#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PatchVertexLayer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <mutex>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
void Pipeline::Begin(CommandState& recording, const Framebuffer& framebuffer, VkExtent2D extent, const State& state, const State* masked) const {
    const auto commands = recording.Commands();
    if (dynamicRendering) {
        const auto views = framebuffer.Views();
        Require(views.size() == attachments, "render targets do not match the pipeline's attachments");
        std::vector<VkRenderingAttachmentInfoKHR> colors(colorSlots.size(), VkRenderingAttachmentInfoKHR{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR});
        for (std::size_t slot = 0; slot < colorSlots.size(); ++slot) {
            if (colorSlots[slot] == ~0u) continue;
            colors[slot].imageView = views[colorSlots[slot]];
            colors[slot].imageLayout = attachmentLayout;
            colors[slot].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            colors[slot].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        }
        VkRenderingAttachmentInfoKHR depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
        if (depthFormat != VK_FORMAT_UNDEFINED) {
            depth.imageView = views.back();
            depth.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        }
        VkRenderingInfoKHR rendering{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
        rendering.renderArea = {{0, 0}, extent};
        rendering.layerCount = state.renderLayers;
        const bool outputs = !state.rasterizerDiscard;
        rendering.colorAttachmentCount = outputs ? static_cast<std::uint32_t>(colors.size()) : 0u;
        rendering.pColorAttachments = outputs && !colors.empty() ? colors.data() : nullptr;
        if (outputs && depthFormat != VK_FORMAT_UNDEFINED) rendering.pDepthAttachment = &depth;
        if (outputs && depthFormat != VK_FORMAT_UNDEFINED && HasStencil(depthFormat)) rendering.pStencilAttachment = &depth;
        context.Resolved(&DeviceFunctions::cmdBeginRendering, "vkCmdBeginRenderingKHR")(commands, &rendering);
        Continue(recording, state, masked);
        return;
    }
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = renderPass;
    begin.framebuffer = framebuffer.Handle();
    begin.renderArea = {{0, 0}, extent};
    const auto locations = SampleLocations(state);
    const auto grid = sampleLocationGrid;
    const VkSampleLocationsInfoEXT info{VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, static_cast<VkSampleCountFlagBits>(state.samples), grid, state.samples * grid.width * grid.height, locations.data()};
    const VkAttachmentSampleLocationsEXT initial{boundColorAttachments, info};
    const VkSubpassSampleLocationsEXT post{0, info};
    VkRenderPassSampleLocationsBeginInfoEXT sampleLocations{VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT};
    sampleLocations.attachmentInitialSampleLocationsCount = state.depth ? 1 : 0;
    sampleLocations.pAttachmentInitialSampleLocations = state.depth ? &initial : nullptr;
    sampleLocations.postSubpassSampleLocationsCount = 1;
    sampleLocations.pPostSubpassSampleLocations = &post;
    if (sampleLocationsEnabled) begin.pNext = &sampleLocations;
    context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    Continue(recording, state, masked);
}

void Pipeline::setViewports(CommandState& recording, const State& state) const {
    if (state.extraViewports.empty()) {
        recording.SetViewport(context, state.viewport);
        recording.SetScissor(context, state.scissor);
        return;
    }
    Require(state.viewportScissors.size() == ViewportCount(state), "every viewport needs a scissor");
    std::array<VkViewport, MaxViewports> viewports{};
    viewports[0] = state.viewport;
    std::copy(state.extraViewports.begin(), state.extraViewports.end(), viewports.begin() + 1);
    recording.SetViewports(context, std::span(viewports.data(), ViewportCount(state)), state.viewportScissors);
}

void Pipeline::Continue(CommandState& recording, const State& state, const State* masked) const {
    if (optimized != nullptr) optimized->Schedule();
    const auto best = optimized != nullptr ? optimized->handle.load() : VK_NULL_HANDLE;
    recording.BindPipeline(context, best != VK_NULL_HANDLE ? best : pipeline, dynamicSlots);
    setViewports(recording, state);
    setColorWriteMasks(recording, state, masked, false);
    if (state.stencilReplayReference) recording.SetStencilReference(context, VK_STENCIL_FACE_FRONT_AND_BACK, *state.stencilReplayReference);
    if (libraries) {
        setLibraryState(recording, state);
        return;
    }
    if (depthBias) recording.SetDepthBias(context, HostDepthBiasConstant(context, state.depth->format, state.depthBiasConstant), state.depthBiasClamp, state.depthBiasSlope);
    if (!depthBounds) return;
    recording.SetDepthBounds(context, state.minDepthBounds, state.maxDepthBounds);
}

void Pipeline::setLibraryState(CommandState& recording, const State& state) const {
    const bool depth = state.depth.has_value();
    recording.SetCullMode(context, state.cullMode);
    recording.SetFrontFace(context, state.frontFace);
    recording.SetDepthBias(context, depthBias ? HostDepthBiasConstant(context, state.depth->format, state.depthBiasConstant) : 0.0f, depthBias ? state.depthBiasClamp : 0.0f, depthBias ? state.depthBiasSlope : 0.0f);
    if (dynamicDepthBiasEnable) recording.SetDepthBiasEnable(context, depthBias);
    recording.SetDepthBounds(context, depthBounds ? state.minDepthBounds : 0.0f, depthBounds ? state.maxDepthBounds : 1.0f);
    recording.SetDepthTestEnable(context, depth && state.depthTest);
    recording.SetDepthWriteEnable(context, depth && state.depthWrite);
    recording.SetDepthCompareOp(context, depth ? state.depthCompare : VK_COMPARE_OP_ALWAYS);
    recording.SetDepthBoundsTestEnable(context, depthBounds);
    recording.SetStencilTestEnable(context, depth && state.stencilTest);
    const auto stencil = [&](VkStencilFaceFlags face, const VkStencilOpState& op) {
        recording.SetStencilOp(context, face, op.failOp, op.passOp, op.depthFailOp, op.compareOp);
        recording.SetStencilCompareMask(context, face, op.compareMask);
        recording.SetStencilWriteMask(context, face, op.writeMask);
        recording.SetStencilReference(context, face, op.reference);
    };
    const auto front = depth ? state.stencilFront : VkStencilOpState{};
    const auto back = depth ? state.stencilBack : VkStencilOpState{};
    if (std::memcmp(&front, &back, sizeof(front)) == 0) {
        stencil(VK_STENCIL_FACE_FRONT_AND_BACK, front);
        return;
    }
    stencil(VK_STENCIL_FACE_FRONT_BIT, front);
    stencil(VK_STENCIL_FACE_BACK_BIT, back);
}

void Pipeline::ContinueSecondChannel(CommandState& recording, const State& state, const State* masked) const {
    Require(secondChannel != VK_NULL_HANDLE, "the pipeline blends every channel in one pass");
    recording.BindPipeline(context, secondChannel, dynamicSlots);
    setColorWriteMasks(recording, state, masked, true);
}

void Pipeline::setColorWriteMasks(CommandState& recording, const State& state, const State* masked, bool secondChannel) const {
    if (!dynamicColorWriteMask || state.blends.empty()) return;
    const auto masks = channelWriteMasks(state, masked != nullptr ? std::span(masked->blends) : std::span(state.blends), secondChannel);
    recording.SetColorWriteMasks(context, std::span(masks).first(state.blends.size()));
}

void Pipeline::ContinueBackFaces(CommandState& recording, const State& state, const State* masked) const {
    Require(backFaces != VK_NULL_HANDLE, "the pipeline draws both faces in one pass");
    recording.BindPipeline(context, backFaces, dynamicSlots);
    setColorWriteMasks(recording, state, masked, false);
    setViewports(recording, state);
    recording.SetDepthBias(context, HostDepthBiasConstant(context, state.depth->format, state.backDepthBiasConstant), state.depthBiasClamp, state.backDepthBiasSlope);
    if (depthBounds) recording.SetDepthBounds(context, state.minDepthBounds, state.maxDepthBounds);
}

void Pipeline::PushConstants(VkCommandBuffer commands, VkShaderStageFlags stages, std::span<const std::byte, PipelinePushConstantBytes> bytes) const {
    if (stages == 0) return;
    if (dynamicRendering) {
        context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, layout, pushStagesOf(context), 0, PipelinePushSlotBytes, bytes.data());
        context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, layout, VK_SHADER_STAGE_FRAGMENT_BIT, PipelinePushSlotBytes, PipelinePushSlotBytes, bytes.data() + PipelinePushSlotBytes);
        return;
    }
    context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, layout, stages, 0, PipelinePushSlotBytes, bytes.data());
}

namespace {

class KeyWriter {
public:
    KeyWriter(std::vector<std::byte>& storage, std::size_t capacity) {
        if (storage.size() < capacity) storage.resize(capacity);
        begin = out = storage.data();
        end = begin + capacity;
    }

    template<typename TValue>
    void Put(const TValue& value) {
        static_assert(std::is_trivially_copyable_v<TValue>);
        reserve(sizeof(TValue));
        std::memcpy(out, &value, sizeof(TValue));
        out += sizeof(TValue);
    }

    void PutBytes(const void* data, std::size_t bytes) {
        if (bytes == 0) return;
        reserve(bytes);
        std::memcpy(out, data, bytes);
        out += bytes;
    }

    std::size_t Length() const { return static_cast<std::size_t>(out - begin); }

private:
    void reserve(std::size_t bytes) const {
        if (bytes > static_cast<std::size_t>(end - out)) [[unlikely]] throw std::length_error("pipeline key larger than its buffer");
    }

    std::byte* begin;
    std::byte* out;
    std::byte* end;
};

template<typename TValue>
void append(KeyWriter& key, const TValue& value) {
    key.Put(value);
}

// Everything the Pipeline objects are built from, or empty when a stage's result has no module
// (nothing else may share its pipeline). The stage identities and the state fields are the same on
// every run (CanonicalPipelineState keeps exactly the fields read here), which lets the journal's
// replay insert an entry a later draw finds; a generated stage (rect-list pair, geometry) is keyed
// by its identity like the others, so a replayed pipeline built from another device's generated
// module is missed rather than used.
std::size_t pipelineKey(std::vector<std::byte>& storage, const Context& context, const State& state, const VertexInputLayout& input, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout, bool narrowLibraryKeys) {
    const auto capacity = 512 + sizeof(State) + sizeof(Context) + shaders.size() * 64 + input.bindings.size() * sizeof(VkVertexInputBindingDescription) + input.attributes.size() * sizeof(VkVertexInputAttributeDescription) + resources.mainKey.size_bytes() + resources.fragmentKey.size_bytes() + state.blends.size() * sizeof(VkPipelineColorBlendAttachmentState) + state.colors.size() * 64;
    KeyWriter key(storage, capacity);
    append(key, context.device);
    append(key, attachmentLayout);
    append(key, shaders.size());
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        const auto identity = shader.program->Identity();
        if (identity.Empty()) return 0;
        append(key, shader.stage);
        append(key, identity.low);
        append(key, identity.high);
        // Where the stage's push constants sit in the block (AssemblePushConstants).
        append(key, KeyedPushOffset(shader));
    }
    append(key, state.meshLayerBase);
    append(key, state.meshLayerCount);
    append(key, PushConstantStages(shaders));
    append(key, input.bindings.size());
    static_assert(sizeof(VkVertexInputBindingDescription) == 3 * sizeof(std::uint32_t) && offsetof(VkVertexInputBindingDescription, binding) == 0 && offsetof(VkVertexInputBindingDescription, stride) == 4 && offsetof(VkVertexInputBindingDescription, inputRate) == 8 && sizeof(VkVertexInputRate) == sizeof(std::uint32_t));
    key.PutBytes(input.bindings.data(), input.bindings.size() * sizeof(VkVertexInputBindingDescription));
    append(key, input.attributes.size());
    static_assert(sizeof(VkVertexInputAttributeDescription) == 4 * sizeof(std::uint32_t) && offsetof(VkVertexInputAttributeDescription, location) == 0 && offsetof(VkVertexInputAttributeDescription, binding) == 4 && offsetof(VkVertexInputAttributeDescription, format) == 8 && offsetof(VkVertexInputAttributeDescription, offset) == 12 && sizeof(VkFormat) == sizeof(std::uint32_t));
    key.PutBytes(input.attributes.data(), input.attributes.size() * sizeof(VkVertexInputAttributeDescription));
    append(key, resources.mainKey.size());
    key.PutBytes(resources.mainKey.data(), resources.mainKey.size_bytes());
    append(key, resources.fragmentKey.size());
    key.PutBytes(resources.fragmentKey.data(), resources.fragmentKey.size_bytes());
    append(key, state.hasColorTarget);
    append(key, state.rectList);
    append(key, state.topology);
    append(key, state.primitiveRestart);
    append(key, state.cullMode);
    append(key, state.polygonMode);
    append(key, state.lineWidth);
    append(key, state.frontFace);
    append(key, state.provokingVertexMode);
    append(key, ViewportCount(state));
    append(key, state.negativeOneToOne);
    append(key, state.depthClamp && context.depthClamp);
    append(key, state.conservativeRasterization);
    append(key, state.blends.size());
    const bool dynamicColorWriteMask = context.dynamicColorWriteMask && !FragmentInterlock(shaders);
    for (const auto& blend : state.blends) append(key, keyedBlend(dynamicColorWriteMask, blend));
    key.Put(state.colorRop);
    append(key, narrowLibraryKeys);
    key.PutBytes(state.blendConstants.data(), sizeof(state.blendConstants));
    append(key, state.samples);
    append(key, state.perSampleShading);
    append(key, state.mergedCoverage);
    append(key, state.truncatedCoverage);
    append(key, state.customSampleLocations);
    if (state.customSampleLocations) {
        append(key, state.sampleLocationGrid);
        append(key, state.sampleLocations);
    }
    append(key, state.colors.size());
    for (const auto& color : state.colors) {
        append(key, color.format);
        append(key, color.componentMapping);
        append(key, color.partialWriteMask != 0u);
    }
    if (state.blends.size() != state.colors.size()) {
        for (const auto& color : state.colors) append(key, color.exportIndex);
    }
    append(key, state.depth.has_value());
    if (state.depth) {
        append(key, state.depth->format);
        append(key, state.depth->samples);
        append(key, state.depthTest);
        append(key, state.depthWrite);
        append(key, state.depthCompare);
        append(key, state.depthBoundsTest);
        append(key, state.depthBias);
        append(key, state.depthBias && state.depthBiasClamp != 0.0f);
        append(key, state.depthBiasPerFace);
        append(key, state.stencilTest);
        auto front = state.stencilFront;
        auto back = state.stencilBack;
        if (state.stencilReplayReference) front.reference = back.reference = 0u;
        append(key, state.stencilReplayReference.has_value());
        append(key, front);
        append(key, back);
    }
    append(key, state.rasterizerDiscard);
    append(key, state.stages.mesh.has_value());
    if (state.stages.mesh) {
        const auto& mesh = *state.stages.mesh;
        append(key, mesh.inputPrimitive);
        append(key, mesh.primitivesPerGroup);
        append(key, mesh.verticesPerGroup);
        append(key, mesh.maxVertices);
        append(key, mesh.maxPrimitives);
        append(key, mesh.threadsPerGroup);
        append(key, mesh.ldsSizeDwords);
        append(key, mesh.provokingVertex);
        append(key, mesh.outputPrimitiveSize);
    }
    append(key, state.stages.tessellation.has_value());
    if (state.stages.tessellation) {
        const auto& tessellation = *state.stages.tessellation;
        append(key, tessellation.inputControlPoints);
        append(key, tessellation.outputControlPoints);
        append(key, tessellation.domain);
        append(key, tessellation.partitioning);
        append(key, tessellation.outputTopology);
        append(key, tessellation.patchesPerGroup);
        append(key, tessellation.primitiveGroupSize);
        append(key, tessellation.breakAtInstance);
    }
    append(key, HostSampleMask(state));
    append(key, context.sampleLocationSampleCounts);
    append(key, context.sampleLocationGridSampleCounts);
    append(key, context.storageImageMultisample);
    append(key, context.shaderStencilExport);
    return key.Length();
}

std::uint64_t hashKey(std::span<const std::byte> key) {
    return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(key.data()), key.size()));
}

struct PipelineStore {
    struct Entry {
        VkDevice device;
        // The device's buffer pool at insertion: it is made and reset with the device, so it tells
        // the device instance apart from a later one the loader gave the same handle value.
        std::weak_ptr<BufferPool> pool;
        bool tracksPool;
        std::uint64_t hash;
        std::vector<std::byte> key;
        std::shared_ptr<Pipeline> pipeline;
    };
    AgcDriver::Mutex mutex;
    // Least recently used first.
    std::list<Entry> entries;
    std::unordered_map<std::uint64_t, std::list<Entry>::iterator> index;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t uncached = 0;
    std::uint64_t evicted = 0;
    PipelineStoreCounters totals;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    std::size_t capacity = 0;
    bool fixedCapacity = false;
    std::list<std::uint64_t> evictedOrder;
    std::unordered_map<std::uint64_t, std::list<std::uint64_t>::iterator> evictedKeys;
    std::size_t evictionsSinceCheck = 0;
    std::size_t returnsSinceCheck = 0;
    static constexpr std::size_t BaseEntries = 1024;
    static constexpr std::size_t MaximumEntries = 4096;
};

// Never destroyed: the pipelines belong to a device that may already be gone when statics die, and
// the device's teardown (ClearCachedPipelines) is the place to destroy them.
PipelineStore& Pipelines() {
    static auto* store = new PipelineStore();
    return *store;
}

// Whether the entry's objects belong to the device the context names. A context without a buffer
// pool (tests) is identified by the handle alone.
bool alive(PipelineStore::Entry& entry, const Context& context) {
    if (entry.device != context.device) return false;
    if (!entry.tracksPool && context.bufferPool != nullptr) {
        entry.pool = context.bufferPool;
        entry.tracksPool = true;
    }
    return context.bufferPool == nullptr || entry.pool.lock() == context.bufferPool;
}

// Drops an entry of a device that is gone: its objects went with the device, so they are forgotten,
// not destroyed.
std::list<PipelineStore::Entry>::iterator abandon(PipelineStore& store, std::list<PipelineStore::Entry>::iterator it) {
    it->pipeline->Abandon();
    store.index.erase(it->hash);
    return store.entries.erase(it);
}

std::size_t storeBound(PipelineStore& store) {
    if (store.capacity == 0) {
        const auto set = PipelineSwitch("APS5_PIPELINE_STORE_ENTRIES", 0u);
        store.fixedCapacity = set != 0;
        store.capacity = store.fixedCapacity ? static_cast<std::size_t>(set) : PipelineStore::BaseEntries;
    }
    return store.capacity;
}

void noteStoreEviction(PipelineStore& store, std::uint64_t hash) {
    if (store.fixedCapacity) return;
    if (const auto found = store.evictedKeys.find(hash); found != store.evictedKeys.end()) store.evictedOrder.erase(found->second);
    store.evictedOrder.push_front(hash);
    store.evictedKeys[hash] = store.evictedOrder.begin();
    while (store.evictedOrder.size() > store.capacity) {
        store.evictedKeys.erase(store.evictedOrder.back());
        store.evictedOrder.pop_back();
    }
    if (++store.evictionsSinceCheck < store.capacity / 32u) return;
    const bool churning = store.returnsSinceCheck * 2u > store.evictionsSinceCheck;
    store.evictionsSinceCheck = 0;
    store.returnsSinceCheck = 0;
    if (!churning || store.capacity >= PipelineStore::MaximumEntries) return;
    const auto previous = store.capacity;
    store.capacity = std::min(store.capacity * 2u, PipelineStore::MaximumEntries);
    std::fprintf(stderr, "[pipecache] bound %zu -> %zu pipelines: pipelines it evicted keep coming back\n", previous, store.capacity);
}

void noteStoreReturn(PipelineStore& store, std::uint64_t hash) {
    if (store.fixedCapacity) return;
    const auto found = store.evictedKeys.find(hash);
    if (found == store.evictedKeys.end()) return;
    store.evictedOrder.erase(found->second);
    store.evictedKeys.erase(found);
    ++store.returnsSinceCheck;
}

void reportPipelines(PipelineStore& store) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - store.lastReport < std::chrono::seconds(10)) return;
    store.lastReport = now;
    const auto lookups = store.hits + store.misses + store.uncached;
    AgcDriver::ProfilePrint_nid_no_patch( "[pipecache] %llu lookups over 10 s: %llu hits (%.0f%%), %llu misses, %llu private (no variant id), %llu evicted, %zu cached\n", static_cast<unsigned long long>(lookups), static_cast<unsigned long long>(store.hits), lookups != 0 ? 100.0 * static_cast<double>(store.hits) / static_cast<double>(lookups) : 0.0, static_cast<unsigned long long>(store.misses), static_cast<unsigned long long>(store.uncached), static_cast<unsigned long long>(store.evicted), store.entries.size());
    store.hits = store.misses = store.uncached = store.evicted = 0;
}

}

std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    return CachedPipeline(context, state, vertexInput, resources.SetLayouts(), shaders, attachmentLayout);
}

std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout, bool* created, std::uint64_t* storeHash) {
    static const bool disabled = std::getenv("APS5_NO_PIPELINE_CACHE") != nullptr;
    if (created != nullptr) *created = false;
    if (storeHash != nullptr) *storeHash = 0;
    if (disabled) return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
    auto& store = Pipelines();
    struct PipelineKeyStorage {};
    auto& storage = HostThreadLocal<std::vector<std::byte>, PipelineKeyStorage>();
    const bool narrowLibraryKeys = narrowLibraryKeysNow();
    const auto length = pipelineKey(storage, context, state, vertexInput, resources, shaders, attachmentLayout, narrowLibraryKeys);
    const std::span<const std::byte> key(storage.data(), length);
    if (key.empty()) {
        std::lock_guard lock(store.mutex);
        ++store.uncached;
        ++store.totals.uncached;
        return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
    }
    const auto hash = hashKey(key);
    // The store's entry of the key, or nullptr with `found` telling a miss (the key is new) from a
    // collision (another configuration owns the hash, so this one stays private).
    const auto lookup = [&](bool& collision) -> std::shared_ptr<Pipeline> {
        collision = false;
        const auto found = store.index.find(hash);
        if (found == store.index.end()) return nullptr;
        const auto it = found->second;
        if (it->key.size() != key.size() || std::memcmp(it->key.data(), key.data(), key.size()) != 0) {
            collision = true;
            return nullptr;
        }
        // The key names the device handle, which the loader may reuse for a device created after
        // this one was destroyed without ClearCachedPipelines: such an entry is a miss.
        if (alive(*it, context)) {
            store.entries.splice(store.entries.end(), store.entries, it);
            return it->pipeline;
        }
        abandon(store, it);
        return nullptr;
    };
    {
        std::lock_guard lock(store.mutex);
        reportPipelines(store);
        bool collision = false;
        if (auto pipeline = lookup(collision)) {
            ++store.hits;
            ++store.totals.hits;
            if (storeHash != nullptr) *storeHash = hash;
            return pipeline;
        }
        if (collision) {
            // A different configuration with the same hash keeps the resident entry; this one stays private.
            ++store.uncached;
            ++store.totals.uncached;
            return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
        }
        ++store.misses;
        ++store.totals.misses;
        noteStoreReturn(store, hash);
    }
    // Built outside the mutex: a draw that finds its pipeline resident is not held behind another
    // thread's build (the journal replay builds here too), and a device's driver compiles in
    // parallel when several threads build at once.
    auto pipeline = std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
    const bool replaying = PipelineJournalReplaying();
    {
        std::lock_guard lock(store.mutex);
        if (pipeline->NarrowLibraryKeys() != narrowLibraryKeys) {
            ++store.uncached;
            ++store.totals.uncached;
            return pipeline;
        }
        bool collision = false;
        if (auto raced = lookup(collision)) {
            if (storeHash != nullptr) *storeHash = hash;
            return raced;
        }
        if (collision) {
            ++store.uncached;
            ++store.totals.uncached;
            return pipeline;
        }
        // Entries of another device belong to one the driver replaced (it does so under the GpuMutex
        // before any draw reaches the new device), whose objects went with it: forget them.
        for (auto it = store.entries.begin(); it != store.entries.end();) {
            it = alive(*it, context) ? std::next(it) : abandon(store, it);
        }
        // A replayed pipeline enters at the least recently used end and never displaces a draw's:
        // the title's own pipelines evict it first, and a full store takes none.
        if (replaying && store.entries.size() >= storeBound(store)) {
            if (store.fixedCapacity || store.capacity >= PipelineStore::MaximumEntries) return pipeline;
            store.capacity = std::min(store.capacity * 2u, PipelineStore::MaximumEntries);
        }
        if (replaying) {
            store.entries.push_front({context.device, context.bufferPool, context.bufferPool != nullptr, hash, std::vector<std::byte>(key.begin(), key.end()), pipeline});
            store.index[hash] = store.entries.begin();
        } else {
            store.entries.push_back({context.device, context.bufferPool, context.bufferPool != nullptr, hash, std::vector<std::byte>(key.begin(), key.end()), pipeline});
            store.index[hash] = std::prev(store.entries.end());
        }
        while (store.entries.size() > storeBound(store)) {
            // Only an entry no recorded draw still holds may go (Kept keeps its shared_ptr until the fence).
            const auto victim = std::find_if(store.entries.begin(), store.entries.end(), [](const PipelineStore::Entry& entry) { return entry.pipeline.use_count() == 1; });
            if (victim == store.entries.end()) break;
            const auto evictedHash = victim->hash;
            store.index.erase(victim->hash);
            store.entries.erase(victim);
            noteStoreEviction(store, evictedHash);
            ++store.evicted;
            ++store.totals.evicted;
        }
        if (storeHash != nullptr && store.index.contains(hash)) *storeHash = hash;
    }
    if (created != nullptr) *created = true;
    if (PipelineJournalEnabled() && !PipelineJournalReplaying() && state.tessellationCapture == nullptr) RecordGraphicsPipeline(MakeGraphicsPipelineRecipe(state, vertexInput, resources, shaders, attachmentLayout), shaders);
    return pipeline;
}

}
}
}
}
