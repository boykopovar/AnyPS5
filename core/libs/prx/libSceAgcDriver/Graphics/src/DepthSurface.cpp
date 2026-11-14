#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/PrimedSharedMutex.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuardSpans.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DepthToStorage_spv.h"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <shared_mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

std::uint64_t depthWrites = 0;
std::uint64_t mirrorEpoch = 0;
std::atomic<void (*)(DepthPublicationStage)> depthPublicationHook{nullptr};

std::uint64_t RegisteredBacking(std::uint64_t address) {
    const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(static_cast<std::uintptr_t>(address), false);
    return range != nullptr ? range->backingSerial : 0;
}

std::pair<std::uint32_t, std::uint32_t> DepthBlockExtent(std::uint32_t bytesPerTexel) {
    return {bytesPerTexel == 4 ? 128u : 256u, bytesPerTexel == 1 ? 256u : 128u};
}

Recorder* RecorderFor(const Context& context) {
    auto* recorder = Recorder::Active();
    return recorder != nullptr && recorder->Device() == context.device ? recorder : nullptr;
}

bool storageFits(const DepthTarget& target, const GuestTextureResource& descriptor) {
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = ResolveTextureFormat(descriptor.format);
    const bool sized = d16 ? (format == VK_FORMAT_R16_UINT || format == VK_FORMAT_R16_UNORM || format == VK_FORMAT_R16_SINT || format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R16_SFLOAT) : (format == VK_FORMAT_R32_SFLOAT || format == VK_FORMAT_R32_UINT || format == VK_FORMAT_R32_SINT);
    return sized && descriptor.width == target.extent.width && descriptor.height == target.extent.height && descriptor.mipCount == 1 && (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k2DArray);
}

std::atomic<bool> directDepthCopy{std::getenv("APS5_NO_DIRECT_DEPTH_COPY") == nullptr};
std::atomic<bool> forcedDepthUploadImage{false};
std::atomic<std::uint64_t> depthUploads{0};
std::atomic<std::uint64_t> directDepthCopies{0};

std::uint32_t aspectCopyBytes(VkFormat format, VkImageAspectFlagBits aspect) {
    if (aspect != VK_IMAGE_ASPECT_DEPTH_BIT) return 0;
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) return 4;
    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT) return 2;
    return 0;
}

bool singleComponent(VkFormat format, std::uint32_t bytes) {
    switch (bytes) {
        case 2: return format == VK_FORMAT_R16_SFLOAT || format == VK_FORMAT_R16_UNORM || format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R16_UINT || format == VK_FORMAT_R16_SINT;
        case 4: return format == VK_FORMAT_R32_SFLOAT || format == VK_FORMAT_R32_SINT || format == VK_FORMAT_R32_UINT;
        default: return false;
    }
}

std::atomic<bool> depthTransferPasses{std::getenv("APS5_NO_DEPTH_TRANSFER_PASS") == nullptr};
std::atomic<std::uint64_t> depthTransferPassCount{0};

bool depthToStoragePass(const Context& context, VkFormat depthFormat, VkFormat colorFormat) {
    if (!context.pushDescriptors || !depthTransferPasses.load(std::memory_order_relaxed)) return false;
    return aspectCopyBytes(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT) == 4 && singleComponent(colorFormat, 4);
}

struct RectPush {
    std::array<std::uint32_t, 2> offset;
    std::array<std::uint32_t, 2> extent;
};

class DepthToStoragePass {
public:
    explicit DepthToStoragePass(const Context& context) : context(context) {
        this->context.bufferPool.reset();
        try {
            pushSet = context.Function<PFN_vkCmdPushDescriptorSetKHR>("vkCmdPushDescriptorSetKHR");
            const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
            setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
            setInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout depth to storage");
            const VkPushConstantRange rect{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RectPush)};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &rect;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout depth to storage");
            const auto compute = CreateShaderModule(context, DEPTH_TO_STORAGE_SPV, sizeof(DEPTH_TO_STORAGE_SPV), "vkCreateShaderModule depth to storage");
            VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, compute, "main", nullptr};
            info.layout = layout;
            VkResult created;
            {
                auto pipelineCacheLock = context.LockPipelineCacheForUse();
                created = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &info, nullptr, &pipeline);
            }
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, compute, nullptr);
            Check(created, "vkCreateComputePipelines depth to storage");
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthToStoragePass() { release(); }
    DepthToStoragePass(const DepthToStoragePass&) = delete;
    DepthToStoragePass& operator=(const DepthToStoragePass&) = delete;

    VkDevice Device() const { return context.device; }

    void Record(VkCommandBuffer commands, VkImageView depthPlane, VkImageView texels, std::span<const VkRect2D> rects) const {
        const VkDescriptorImageInfo depthInfo{VK_NULL_HANDLE, depthPlane, VK_IMAGE_LAYOUT_GENERAL};
        const VkDescriptorImageInfo texelInfo{VK_NULL_HANDLE, texels, VK_IMAGE_LAYOUT_GENERAL};
        const std::array<VkWriteDescriptorSet, 2> writes{imageWrite(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, depthInfo), imageWrite(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, texelInfo)};
        context.BindComputePipeline(commands, pipeline);
        pushSet(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, static_cast<std::uint32_t>(writes.size()), writes.data());
        const auto pushConstants = context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants");
        const auto dispatch = context.Resolved(&DeviceFunctions::cmdDispatch, "vkCmdDispatch");
        for (const auto& rect : rects) {
            const RectPush push{{static_cast<std::uint32_t>(rect.offset.x), static_cast<std::uint32_t>(rect.offset.y)}, {rect.extent.width, rect.extent.height}};
            pushConstants(commands, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            dispatch(commands, (rect.extent.width + 7u) / 8u, (rect.extent.height + 7u) / 8u, 1);
        }
    }

private:
    static VkWriteDescriptorSet imageWrite(std::uint32_t binding, VkDescriptorType type, const VkDescriptorImageInfo& info) {
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pImageInfo = &info;
        return write;
    }

    void release() noexcept {
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
        pipeline = VK_NULL_HANDLE;
        layout = VK_NULL_HANDLE;
        setLayout = VK_NULL_HANDLE;
    }

    Context context;
    PFN_vkCmdPushDescriptorSetKHR pushSet = nullptr;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

std::mutex& depthToStoragePassesMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<DepthToStoragePass>>& depthToStoragePasses() {
    static auto* list = new std::vector<std::unique_ptr<DepthToStoragePass>>();
    return *list;
}

DepthToStoragePass& depthToStoragePassFor(const Context& context) {
    std::lock_guard lock(depthToStoragePassesMutex());
    auto& list = depthToStoragePasses();
    for (const auto& pass : list) {
        if (pass->Device() == context.device) return *pass;
    }
    list.push_back(std::make_unique<DepthToStoragePass>(context));
    return *list.back();
}

struct DepthGuard {
    VkDevice device;
    std::uint64_t address;
    std::size_t bytes;
    std::shared_ptr<std::atomic<bool>> active;
    std::vector<GuestMemory::UnchangedQuery> live;
    std::shared_ptr<std::atomic<bool>> hostWritten;
    std::shared_ptr<std::atomic<bool>> hostEverWritten;
};

std::shared_mutex& depthGuardMutex() {
    static PrimedSharedMutex mutex;
    return mutex;
}

std::vector<DepthGuard>& depthGuards() {
    static std::vector<DepthGuard> ranges;
    return ranges;
}

std::atomic<bool>& anyDepthGuard() {
    static std::atomic<bool> any{false};
    return any;
}

std::atomic<std::uint64_t>& depthGuardGeneration() {
    static std::atomic<std::uint64_t> generation{0};
    return generation;
}

void publishDepthGuards(bool changed) {
    if (changed) depthGuardGeneration().fetch_add(1, std::memory_order_release);
    anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
}

bool mayOverlapDepthGuard(std::uint64_t address, std::uint64_t bytes) {
    if (RuntimeSwitches::Set(RuntimeSwitch::NoGuardSpans)) return true;
    struct SpansStorage {};
    auto& spans = HostThreadLocal<GuardSpans, SpansStorage>();
    if (spans.Generation() != depthGuardGeneration().load(std::memory_order_acquire)) {
        std::shared_lock lock(depthGuardMutex());
        spans.Fill(depthGuardGeneration().load(std::memory_order_relaxed), depthGuards());
        NoteGuardSpans(GuardSpanEvent::Refill);
    }
    if (spans.Overlaps(address, bytes)) {
        NoteGuardSpans(GuardSpanEvent::Overlap);
        return true;
    }
    NoteGuardSpans(GuardSpanEvent::Clear);
    if (!VerifyGuardSpans()) return false;
    std::shared_lock lock(depthGuardMutex());
    const bool overlapped = std::ranges::any_of(depthGuards(), [&](const auto& range) { return address < range.address + range.bytes && range.address < address + bytes; });
    const auto generation = depthGuardGeneration().load(std::memory_order_relaxed);
    if (overlapped && generation != spans.Generation()) spans.Fill(generation, depthGuards());
    if (overlapped && !spans.Overlaps(address, bytes)) ReportGuardSpanMiss("depth", address, bytes);
    else NoteGuardSpans(GuardSpanEvent::Verified);
    return overlapped;
}

std::atomic<std::uint32_t>& pendingOverwriteProofs() {
    static std::atomic<std::uint32_t> count{0u};
    return count;
}

struct DepthSamplingState {
    std::vector<GuestMemory::UnchangedQuery> initial;
    std::optional<float> initialDepth;
    std::optional<std::uint8_t> initialStencil;
    std::string refusal;
    std::optional<float> aliasDepth;
    std::optional<std::uint8_t> aliasStencil;
    std::optional<MultisampleColorAlias> unprovenStencil;
    std::optional<MultisampleColorAlias> depthAlias;
    bool unevenPlanes = false;
    std::optional<std::uint8_t> overwriteStencil;
    std::optional<MultisampleColorAlias> stencilAlias;

    void Refuse(const char* reason) {
        if (refusal.empty()) refusal = reason;
    }
};

constexpr const char* stencilAliasRefusal = "native multisample stencil plane aliases a multisampled colour target whose samples are only on the host";
constexpr const char* depthAliasRefusal = "native multisample depth or HTILE aliases a multisampled colour target whose samples are only on the host";

std::optional<MultisampleColorAlias> soleDepthAlias(const GuestMemory::UnchangedQuery& plane, std::uint32_t samples) {
    const auto aliases = MultisampleColorTargetsOver(plane.address, plane.bytes);
    if (aliases.size() != 1u || aliases.front().samples != samples || aliases.front().active == nullptr) return std::nullopt;
    return aliases.front();
}

bool sameAliasValue(const MultisampleColorAlias& recorded, const MultisampleColorAlias& current) {
    return recorded.value.has_value() == current.value.has_value() && (!recorded.value || std::memcmp(&*recorded.value, &*current.value, sizeof(VkClearColorValue)) == 0);
}

bool unchangedAlias(const MultisampleColorAlias& recorded, const GuestMemory::UnchangedQuery& plane) {
    const auto aliases = MultisampleColorTargetsOver(plane.address, plane.bytes);
    if (aliases.size() != 1u) return false;
    const auto& current = aliases.front();
    return current.active == recorded.active && current.contents == recorded.contents && current.newest == recorded.newest && current.fills == recorded.fills && sameAliasValue(recorded, current);
}

bool sameDepthAlias(const MultisampleColorAlias& recorded, const GuestMemory::UnchangedQuery& plane) {
    const auto aliases = MultisampleColorTargetsOver(plane.address, plane.bytes);
    if (aliases.size() != 1u) return false;
    const auto& current = aliases.front();
    const auto sameValue = sameAliasValue(recorded, current);
    const bool refilled = recorded.newest == MultisampleColorContent::Clear && current.newest == MultisampleColorContent::Fill && current.fills > recorded.fills && current.fills - recorded.fills == current.sameRun && current.filledBytes >= current.bytes;
    return current.active == recorded.active && current.contents == recorded.contents && (current.newest == recorded.newest || refilled) && (current.fills == recorded.fills || refilled) && sameValue;
}

std::optional<std::uint8_t> aliasedStencil(const MultisampleColorAlias& alias, const GuestMemory::UnchangedQuery& plane, std::uint32_t samples) {
    const bool r8 = alias.format == VK_FORMAT_R8_UNORM || alias.format == VK_FORMAT_R8_UINT;
    const bool known = alias.newest == MultisampleColorContent::Fill || alias.newest == MultisampleColorContent::FastClear || alias.newest == MultisampleColorContent::Clear;
    if (!r8 || !known || alias.samples != samples || alias.address != plane.address || alias.bytes != plane.bytes) return std::nullopt;
    return alias.byte;
}

std::optional<float> aliasedDepth(const MultisampleColorAlias& alias, const GuestMemory::UnchangedQuery& plane, std::uint32_t samples) {
    const bool r32 = alias.format == VK_FORMAT_R32_SFLOAT;
    const bool known = alias.newest == MultisampleColorContent::Fill || alias.newest == MultisampleColorContent::FastClear || alias.newest == MultisampleColorContent::Clear;
    if (!r32 || !known || !alias.value || alias.samples != samples || alias.address != plane.address || alias.bytes != plane.bytes) return std::nullopt;
    const auto value = alias.value->float32[0];
    if (!(value >= 0.0f && value <= 1.0f)) return std::nullopt;
    return value;
}

void reportAlias(const MultisampleColorAlias& alias, const GuestMemory::UnchangedQuery& range, const char* plane) {
    static std::atomic<int> reports{0};
    if (reports.fetch_add(1) >= 8) return;
    char newest[96];
    const char* operation = alias.newest == MultisampleColorContent::Fill ? "fill" : alias.newest == MultisampleColorContent::FastClear ? "CMASK fast clear" : alias.newest == MultisampleColorContent::Clear ? "clear" : alias.newest == MultisampleColorContent::Drawn ? "drawn" : "unknown";
    if (alias.byte) std::snprintf(newest, sizeof(newest), "%s 0x%02x", operation, *alias.byte);
    else if (alias.value) std::snprintf(newest, sizeof(newest), "%s 0x%08x 0x%08x 0x%08x 0x%08x", operation, alias.value->uint32[0], alias.value->uint32[1], alias.value->uint32[2], alias.value->uint32[3]);
    else std::snprintf(newest, sizeof(newest), "%s", operation);
    std::fprintf(stderr, "[gpu] native multisample %s 0x%llx+0x%zx aliases multisampled colour target 0x%llx+0x%llx (vk format %d, %u samples), newest %s\n", plane, static_cast<unsigned long long>(range.address), range.bytes, static_cast<unsigned long long>(alias.address), static_cast<unsigned long long>(alias.bytes), static_cast<int>(alias.format), alias.samples, newest);
}

bool htileProvesStencilClear(std::uint32_t word) {
    return ((word >> 8u) & 3u) == 0u;
}

bool htileProvesClear(std::uint32_t word, bool depthOnly, float clearDepth) {
    if ((word & 15u) == 15u) return false;
    const auto depth = depthOnly ? word : word & 0xfffff00fu;
    if (depth == 0u) return clearDepth == 0.0f;
    if (depth == (depthOnly ? 0xfffffff0u : 0xfffc0000u)) return clearDepth == 1.0f;
    return false;
}

std::uint64_t fillNoteAfter(const Recorder& recorder, const GuestMemory::UnchangedQuery& range, std::uint64_t floor, std::uint32_t word) {
    std::uint64_t found = 0u;
    for (const auto& write : recorder.PendingWritesOver(range.address, range.bytes)) {
        if (write.note <= floor) continue;
        if (found != 0u || write.begin > range.address || write.end < range.address + range.bytes || write.word != static_cast<std::int64_t>(word)) return 0u;
        found = write.note;
    }
    return found;
}

bool uniformPlaneTarget(const DepthTarget& target) {
    return target.format == VK_FORMAT_D32_SFLOAT_S8_UINT && target.stencilAddress != 0u && target.htileAddress == 0u;
}

template <typename Element>
std::optional<Element> uniformPlaneValue(std::uint64_t address, std::size_t bytes) {
    std::array<Element, 16384> chunk{};
    std::optional<Element> value;
    for (std::size_t offset = 0u; offset < bytes;) {
        const auto count = std::min(chunk.size(), (bytes - offset) / sizeof(Element));
        const auto elements = std::span(chunk).first(count);
        GuestMemory::Read(address + offset, std::as_writable_bytes(elements));
        if (!value) value = elements.front();
        if (std::ranges::any_of(elements, [&](Element element) { return element != *value; })) return std::nullopt;
        offset += count * sizeof(Element);
    }
    return value;
}

DepthSamplingState prepareUniformPlaneSampling(const Context& context, const DepthTarget& target, DepthSamplingState state) {
    const auto refuse = [&](const char* reason) {
        state.refusal = reason;
        return state;
    };
    if (target.depthSwizzle != 24u || target.firstSlice != 0u || target.lastSlice != 0u) return refuse("native multisample depth arrays or non-Z_X layout are not implemented");
    if ((target.pipeBankXor & ~0xcf00u) != 0u) return refuse("unsupported native multisample depth XOR");
    if (target.address == 0u || (target.address & 65535u) != 0u || (target.stencilAddress & 65535u) != 0u) return refuse("native multisample depth base is not aligned");
    if (target.htileStencil) return refuse("native multisample stencil import is not implemented");
    state.initial = {{target.address, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(target.extent, 4u, target.samples)), 0u}, {target.stencilAddress, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(target.extent, 1u, target.samples)), 0u}};
    for (const auto& range : state.initial) {
        if (range.address > std::numeric_limits<std::uint64_t>::max() - range.bytes) return refuse("native depth import range overflows");
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) || !GuestMemory::Watched(range.address, range.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
    }
    const auto& depth = state.initial[0];
    const auto& stencil = state.initial[1];
    if (depth.address + depth.bytes > stencil.address && stencil.address + stencil.bytes > depth.address) return refuse("native depth and stencil initial ranges overlap");
    for (std::size_t index = 0u; index < state.initial.size(); ++index) {
        const auto alias = MultisampleColorTargetOver(state.initial[index].address, state.initial[index].bytes);
        if (!alias) continue;
        if (index == 0u) {
            if (const auto depthValue = aliasedDepth(*alias, state.initial[index], target.samples)) {
                state.aliasDepth = *depthValue;
                continue;
            }
        }
        reportAlias(*alias, state.initial[index], index == 1u ? "stencil plane" : "depth");
        return refuse(index == 1u ? stencilAliasRefusal : depthAliasRefusal);
    }
    for (auto& range : state.initial) {
        if (auto* recorder = RecorderFor(context)) recorder->SyncThrough(range.address, range.bytes);
        StorageTexture::FlushPending(range.address, range.bytes);
        GuestMemory::FlushGpuWrites(range.address, range.bytes);
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) || !GuestMemory::Watched(range.address, range.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
        range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        if (range.generation == 0u) return refuse("native multisample depth import requires tracked generations");
    }
    state.unevenPlanes = true;
    if (state.aliasDepth) {
        state.initialDepth = *state.aliasDepth;
    } else {
        const auto depthWord = uniformPlaneValue<std::uint32_t>(depth.address, depth.bytes);
        const auto depthValue = depthWord ? std::bit_cast<float>(*depthWord) : 0.0f;
        if (!depthWord || !(depthValue >= 0.0f && depthValue <= 1.0f)) return refuse("native multisample depth plane without HTILE does not hold one depth value in [0, 1]");
        state.initialDepth = depthValue;
    }
    const auto stencilByte = uniformPlaneValue<std::uint8_t>(stencil.address, stencil.bytes);
    if (!stencilByte) return refuse("native multisample stencil plane without HTILE does not hold one stencil value");
    state.unevenPlanes = false;
    for (const auto& range : state.initial) GuestMemory::CollectWritesUncached(range.address, range.bytes);
    if (!GuestMemory::UnchangedSinceAll(state.initial)) return refuse("guest writes raced native multisample depth import");
    state.initialStencil = *stencilByte;
    return state;
}

DepthSamplingState prepareDepthSampling(const Context& context, const DepthTarget& target) {
    DepthSamplingState state;
    const auto refuse = [&](const char* reason) {
        state.refusal = reason;
        return state;
    };
    if (target.samples != 2u && target.samples != 4u && target.samples != 8u) return refuse("unsupported native depth sample count");
    if (uniformPlaneTarget(target)) return prepareUniformPlaneSampling(context, target, std::move(state));
    const bool stencil = target.format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    const bool stencilPlane = stencil && !target.htileStencil && target.htileDepthOnly;
    if (target.format != VK_FORMAT_D16_UNORM && target.format != VK_FORMAT_D32_SFLOAT && !stencil) return refuse("native multisample stencil or depth format import is not implemented");
    if (target.depthSwizzle != 24u || target.firstSlice != 0u || target.lastSlice != 0u) return refuse("native multisample depth arrays or non-Z_X layout are not implemented");
    if ((target.pipeBankXor & ~0xcf00u) != 0u) return refuse("unsupported native multisample depth XOR");
    if (target.address == 0u || (target.address & 65535u) != 0u) return refuse("native multisample depth base is not aligned");
    if (stencil != (target.stencilAddress != 0u) || (!stencil && target.htileStencil)) return refuse("native multisample stencil import is not implemented");
    if (stencil && !stencilPlane && (!target.htileStencil || target.htileDepthOnly)) return refuse("native multisample stencil import requires HTILE stencil metadata");
    if (stencil && (target.stencilAddress & 65535u) != 0u) return refuse("native multisample stencil base is not aligned");
    if (target.htileAddress == 0u || (target.htileAddress & 255u) != 0u || (!target.htilePipeAligned && !target.htileDepthOnly) || target.htileVrsEncoding != 0u) return refuse("native multisample depth initial import requires pipe-aligned or depth-only HTILE without VRS");
    if (target.clearDepth != 0.0f && target.clearDepth != 1.0f) return refuse("native multisample depth initial clear value is not implemented");
    state.initial = {{target.address, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(target.extent, target.format == VK_FORMAT_D16_UNORM ? 2u : 4u, target.samples)), 0u}, {target.htileAddress, static_cast<std::size_t>(HtileMetadataBytes(target.extent)), 0u}};
    if (stencil) state.initial.push_back({target.stencilAddress, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(target.extent, 1u, target.samples)), 0u});
    for (const auto& range : state.initial) {
        if (range.address > std::numeric_limits<std::uint64_t>::max() - range.bytes) return refuse("native depth import range overflows");
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) || !GuestMemory::Watched(range.address, range.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
    }
    for (std::size_t first = 0u; first < state.initial.size(); ++first) {
        for (std::size_t second = first + 1u; second < state.initial.size(); ++second) {
            const auto& a = state.initial[first];
            const auto& b = state.initial[second];
            if (a.address + a.bytes > b.address && b.address + b.bytes > a.address) return refuse("native depth, stencil and HTILE initial ranges overlap");
        }
    }
    for (std::size_t index = 0u; index < state.initial.size(); ++index) {
        const auto& range = state.initial[index];
        const auto alias = MultisampleColorTargetOver(range.address, range.bytes);
        if (!alias) continue;
        std::optional<float> depthValue;
        std::optional<std::uint8_t> stencilValue;
        if (index == 0u) {
            depthValue = aliasedDepth(*alias, range, target.samples);
            if (const auto sole = soleDepthAlias(range, target.samples)) {
                reportAlias(*sole, range, "depth");
                state.depthAlias = sole;
                if (!depthValue) continue;
            }
        } else if (index == 2u && stencilPlane) stencilValue = aliasedStencil(*alias, range, target.samples);
        if (!depthValue && !stencilValue) {
            reportAlias(*alias, range, index == 2u ? "stencil plane" : index == 1u ? "HTILE" : "depth");
            if (index == 2u && stencilPlane) {
                state.unprovenStencil = alias;
                continue;
            }
            return refuse(index == 2u ? stencilAliasRefusal : depthAliasRefusal);
        }
        if (depthValue) state.aliasDepth = *depthValue;
        else {
            state.aliasStencil = *stencilValue;
            state.stencilAlias = *alias;
        }
    }
    const auto& htile = state.initial[1];
    for (auto& range : state.initial) {
        if (auto* recorder = RecorderFor(context)) recorder->SyncThrough(range.address, range.bytes);
        StorageTexture::FlushPending(range.address, range.bytes);
        GuestMemory::FlushGpuWrites(range.address, range.bytes);
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) || !GuestMemory::Watched(range.address, range.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
        range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        if (range.generation == 0u) return refuse("native multisample depth import requires tracked generations");
    }
    std::vector<std::uint32_t> metadata(htile.bytes / sizeof(std::uint32_t));
    GuestMemory::Read(target.htileAddress, std::as_writable_bytes(std::span(metadata)), 256u);
    if (!target.htilePipeAligned && std::adjacent_find(metadata.begin(), metadata.end(), [](std::uint32_t first, std::uint32_t second) { return first != second; }) != metadata.end()) return refuse("native multisample depth HTILE that is not pipe-aligned holds more than one word");
    if (state.aliasDepth && state.depthAlias && !std::all_of(metadata.begin(), metadata.end(), [&](std::uint32_t word) { return htileProvesClear(word, target.htileDepthOnly, *state.aliasDepth); })) state.aliasDepth.reset();
    const float depthClear = state.aliasDepth.value_or(target.clearDepth);
    for (const auto word : metadata) {
        if (!htileProvesClear(word, target.htileDepthOnly, depthClear)) return refuse("native multisample depth initial HTILE does not prove its clear value");
        if (stencil && !stencilPlane && !htileProvesStencilClear(word)) return refuse("native multisample stencil initial HTILE does not prove its clear value");
    }
    const bool provedPlane = stencilPlane && !state.aliasStencil && !state.unprovenStencil;
    if (provedPlane) {
        std::vector<std::uint8_t> plane(state.initial[2].bytes);
        GuestMemory::Read(target.stencilAddress, std::as_writable_bytes(std::span(plane)));
        if (const auto hook = depthPublicationHook.load(std::memory_order_acquire)) hook(DepthPublicationStage::StencilPlaneRead);
        if (std::any_of(plane.begin(), plane.end(), [&](std::uint8_t value) { return value != target.clearStencil; })) return refuse("native multisample stencil plane does not hold its clear value");
    }
    for (const auto& range : state.initial) GuestMemory::CollectWritesUncached(range.address, range.bytes);
    std::vector<GuestMemory::UnchangedQuery> unchanged;
    for (std::size_t index = 0u; index < state.initial.size(); ++index) {
        if (!provedPlane || index != 2u) unchanged.push_back(state.initial[index]);
    }
    if (!GuestMemory::UnchangedSinceAll(unchanged)) return refuse("guest writes raced native multisample depth import");
    if (provedPlane) {
        auto& range = state.initial[2];
        range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        if (range.generation == 0u) return refuse("native multisample depth import requires tracked generations");
        std::vector<std::uint8_t> plane(range.bytes);
        GuestMemory::Read(target.stencilAddress, std::as_writable_bytes(std::span(plane)));
        if (std::any_of(plane.begin(), plane.end(), [&](std::uint8_t value) { return value != target.clearStencil; })) return refuse("guest writes raced native multisample depth import");
    }
    if (state.aliasDepth) state.initialDepth = *state.aliasDepth;
    return state;
}

std::atomic<bool> depthPlaneClearEnabled{true};

enum DepthClearDecline : std::size_t { DeclineLayers, DeclineFormat, DeclineValue, DeclineColor, DeclinePending, DeclineCoverage, DeclineSize, DeclineCount };
static_assert(DeclineCount <= 8, "DepthClearStatistics::declined holds eight reasons");

struct DepthClearCounters {
    std::atomic<std::uint64_t> matched{0};
    std::atomic<std::uint64_t> taken{0};
    std::atomic<std::uint64_t> copiesSkipped{0};
    std::atomic<std::uint64_t> clears{0};
    std::atomic<std::uint64_t> bytesSkipped{0};
    std::array<std::atomic<std::uint64_t>, 8> declined{};
};

DepthClearCounters& depthClearCounters() {
    static DepthClearCounters counters;
    return counters;
}

struct DepthPlaneStoreCounters {
    std::atomic<std::uint64_t> stores{0};
    std::atomic<std::uint64_t> units{0};
    std::atomic<std::uint64_t> bytes{0};
};

DepthPlaneStoreCounters& depthPlaneStoreCounters() {
    static DepthPlaneStoreCounters counters;
    return counters;
}

bool anyUnit(const std::vector<bool>& units) {
    return std::find(units.begin(), units.end(), true) != units.end();
}

thread_local DepthPlaneOverwrite* currentOverwrite = nullptr;

std::optional<DepthPlaneOverwrite> decodeOverwrite(const StorageImageClear& clear) {
    auto& counters = depthClearCounters();
    ++counters.matched;
    const auto decline = [&](DepthClearDecline why) -> std::optional<DepthPlaneOverwrite> {
        ++counters.declined[why];
        return std::nullopt;
    };
    DepthPlaneOverwrite overwrite;
    overwrite.image = clear.image;
    try {
        overwrite.view = DecodeTextureResource(clear.image);
    } catch (...) {
        return decline(DeclineFormat);
    }
    const auto& view = overwrite.view;
    const bool oneLayer = view.dimension == TextureDimension::k2D || (view.dimension == TextureDimension::k2DArray && view.baseArray == 0 && view.depthOrLastArray == 0);
    if (!oneLayer || view.baseLevel != 0) return decline(DeclineLayers);
    VkFormat format = VK_FORMAT_UNDEFINED;
    try {
        format = ResolveTextureFormat(view.format);
    } catch (...) {
        return decline(DeclineFormat);
    }
    enum class Kind { Unorm16, Uint16, Float32, Uint32 };
    Kind kind;
    switch (format) {
        case VK_FORMAT_R16_UNORM: kind = Kind::Unorm16; break;
        case VK_FORMAT_R16_UINT: kind = Kind::Uint16; break;
        case VK_FORMAT_R32_SFLOAT: kind = Kind::Float32; break;
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT: kind = Kind::Uint32; break;
        default: return decline(DeclineFormat);
    }
    const std::uint64_t base = clear.color[0] | (static_cast<std::uint64_t>(clear.color[1] & 0xffffu) << 32u);
    const std::uint64_t stride = (clear.color[1] >> 16u) & 0x3fffu;
    const std::uint64_t extent = stride == 0 ? clear.color[2] : static_cast<std::uint64_t>(clear.color[2]) * stride;
    if (base == 0 || (base & 3u) != 0u || extent < 16u || !GuestMemory::Accessible(reinterpret_cast<const void*>(base), 16)) return decline(DeclineColor);
    if (const auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(base, 16)) return decline(DeclinePending);
    const std::pair<std::uint64_t, std::uint64_t> colorRange{base, base + 16u};
    if (StorageTexture::AnyPendingOverlaps(std::span(&colorRange, 1))) return decline(DeclinePending);
    std::uint32_t c0 = 0;
    try {
        GuestMemory::Read(base, std::as_writable_bytes(std::span(&c0, 1)), 4);
    } catch (...) {
        return decline(DeclineColor);
    }
    switch (kind) {
        case Kind::Unorm16:
            if (c0 == 0u || c0 == 0x80000000u) overwrite.raw = 0u;
            else if (c0 == 0x3f800000u) overwrite.raw = 0xffffu;
            else return decline(DeclineValue);
            overwrite.bits = 16u;
            overwrite.depth = static_cast<float>(overwrite.raw) / 65535.0f;
            break;
        case Kind::Uint16:
            if (c0 != 0u && c0 != 0xffffu) return decline(DeclineValue);
            overwrite.raw = c0;
            overwrite.bits = 16u;
            overwrite.depth = static_cast<float>(overwrite.raw) / 65535.0f;
            break;
        case Kind::Float32:
        case Kind::Uint32: {
            const auto depth = std::bit_cast<float>(c0);
            if (c0 != 0u && !(std::isnormal(depth) && depth > 0.0f && depth <= 1.0f)) return decline(DeclineValue);
            overwrite.raw = c0;
            overwrite.bits = 32u;
            overwrite.depth = depth;
            break;
        }
    }
    overwrite.colorAddress = base;
    overwrite.colorBits = c0;
    overwrite.width = clear.width;
    overwrite.height = clear.height;
    return overwrite;
}

class DepthSurface;
std::pair<std::uint64_t, std::uint64_t> staleStencilRange(const DepthSurface& surface);
void noteStalePlane(std::uint64_t begin, std::uint64_t end);
std::uint64_t nextPlaneSerial();

class DepthSurface {
public:
    DepthSurface(const Context& context, const DepthTarget& target, DepthSamplingState sampling = {}) : context(context), target(target), hostFormat(HostDepthFormat(context, target.format)), sampling(std::move(sampling)) {
        this->context.bufferPool.reset();
        VkFormatProperties properties{};
        context.formatProperties(context.physical, hostFormat, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0, "depth/stencil format " + std::to_string(hostFormat) + " cannot be an attachment on this device");
        Require(target.samples != 1u || (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0, "depth/stencil format " + std::to_string(hostFormat) + " cannot be sampled on this device");
        Require(target.extent.width <= context.limits.maxFramebufferWidth && target.extent.height <= context.limits.maxFramebufferHeight, "depth target exceeds framebuffer limits");
        const VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT | (target.stencilAddress != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            if (SampleLocationsCompatibleDepth(context, hostFormat, target.samples)) info.flags = VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = hostFormat;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            sampleable = target.samples == 1;
            if (target.samples != 1) {
                Require(context.imageFormatProperties != nullptr, "native multisample depth requires image format queries");
                VkImageFormatProperties shadowProperties{};
                VkImageFormatProperties directProperties{};
                const auto sampledUsage = sampleShadowUsage();
                const auto shadowResult = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, sampledUsage, 0, &shadowProperties);
                if (shadowResult != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(shadowResult, "vkGetPhysicalDeviceImageFormatProperties sampled depth shadow");
                const auto directResult = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, info.usage | VK_IMAGE_USAGE_SAMPLED_BIT, info.flags, &directProperties);
                if (directResult != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(directResult, "vkGetPhysicalDeviceImageFormatProperties sampled depth attachment");
                const auto supports = [&](VkResult result, const VkImageFormatProperties& supported) {
                    return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && result == VK_SUCCESS && (supported.sampleCounts & info.samples) != 0 && supported.maxMipLevels >= 1u && supported.maxArrayLayers >= 1u && supported.maxExtent.depth >= 1u && info.extent.width <= supported.maxExtent.width && info.extent.height <= supported.maxExtent.height;
                };
                useSampleShadow = supports(shadowResult, shadowProperties);
                const bool directSampleable = supports(directResult, directProperties);
                sampleable = useSampleShadow || directSampleable;
                if (!useSampleShadow && directSampleable) info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            }
            if (target.samples == 1 && sampleable) info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory depth target");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory depth");
            if (target.samples != 1u) owned = std::make_shared<OwnedImage>(this->context, image, memory);
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = hostFormat;
            viewInfo.subresourceRange = {aspects, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth");
            if (target.samples != 1u && sampleable && this->sampling.refusal.empty()) CheckCurrent();
            auto* recorder = RecorderFor(context);
            if (recorder != nullptr && owned != nullptr) recorder->Keep(owned);
            std::unique_ptr<CommandBatch> batch;
            if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
            const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
            VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.image = image;
            toGeneral.subresourceRange = viewInfo.subresourceRange;
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            const VkClearDepthStencilValue clear{this->sampling.initialDepth.value_or(target.clearDepth), this->sampling.initialStencil.value_or(this->sampling.aliasStencil.value_or(target.clearStencil))};
            context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &toGeneral.subresourceRange);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        } catch (...) {
            release();
            throw;
        }
        if (this->sampling.overwriteStencil) pendingOverwriteProofs().fetch_add(1u, std::memory_order_acq_rel);
    }

    ~DepthSurface() {
        if (sampling.overwriteStencil) pendingOverwriteProofs().fetch_sub(1u, std::memory_order_acq_rel);
        release();
    }
    DepthSurface(const DepthSurface&) = delete;
    DepthSurface& operator=(const DepthSurface&) = delete;

    void CheckCurrent() {
        if (target.samples == 1u) return;
        ProveOverwrite();
        const auto clearLease = clearGeneration == 0u && stencilClearGeneration == 0u ? GuestAllocations::Lease{} : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
        Require(active->load(std::memory_order_acquire), "the native multisample depth owner was released");
        Require(sampling.refusal.empty(), sampling.refusal);
        Require(!clearPending, "native multisample depth clear has not been recorded");
        const auto* recorder = RecorderFor(context);
        if (recorder != nullptr && clearGeneration != 0u && clearPattern.has_value() && clearFillNote == 0u) clearFillNote = fillNoteAfter(*recorder, sampling.initial[1], clearFillFloor, *clearPattern);
        if (recorder != nullptr && stencilClearGeneration != 0u && stencilFillNote == 0u) stencilFillNote = fillNoteAfter(*recorder, sampling.initial[2], stencilFillFloor, *stencilClearValue * 0x01010101u);
        if (!publication.empty()) {
            if (auto* pending = RecorderFor(context)) {
                if (clearFillNote != 0u) pending->SyncThrough(sampling.initial[1].address, sampling.initial[1].bytes);
                if (stencilFillNote != 0u) pending->SyncThrough(sampling.initial[2].address, sampling.initial[2].bytes);
            }
        }
        const auto newestHtileNote = recorder != nullptr ? recorder->NewestStoreNote(sampling.initial[1].address, sampling.initial[1].bytes) : 0u;
        const bool queuedFill = clearPattern.has_value() && clearFillNote != 0u && newestHtileNote == clearFillNote;
        const auto newestStencilNote = recorder != nullptr && sampling.initial.size() > 2u ? recorder->NewestStoreNote(sampling.initial[2].address, sampling.initial[2].bytes) : 0u;
        const bool queuedStencilFill = stencilClearValue.has_value() && stencilFillNote != 0u && newestStencilNote == stencilFillNote;
        const bool separatePlane = target.stencilAddress != 0u && !target.htileStencil && target.htileDepthOnly && sampling.initial.size() > 2u;
        bool stencilPlaneAliased = false;
        for (std::size_t index = 0u; index < sampling.initial.size(); ++index) {
            const auto& range = sampling.initial[index];
            Require(GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) && GuestMemory::Watched(range.address, range.bytes), "native multisample depth requires watched accessible guest memory");
            if (recorder != nullptr && !(index == 1u && queuedFill) && !(index == 2u && queuedStencilFill) && recorder->PendingWriteOverlaps(range.address, range.bytes)) {
                Require(!recorder->PendingStoreOverlaps(range.address, range.bytes), "external GPU writes over native multisample depth are not implemented");
                const auto settled = GuestMemory::TrackerGeneration();
                RecorderFor(context)->SyncThrough(range.address, range.bytes);
                Require(!GuestMemory::WrittenSince(range.address, range.bytes, settled), "external GPU writes over native multisample depth are not implemented");
            }
            GuestMemory::CollectWritesUncached(range.address, range.bytes);
            const auto alias = MultisampleColorTargetOver(range.address, range.bytes);
            if (!alias || (index == 2u && sampling.unprovenStencil && stencilClearGeneration == 0u)) continue;
            if (index == 0u && sampling.depthAlias && sameDepthAlias(*sampling.depthAlias, range)) continue;
            const bool uniform = uniformPlaneTarget(target);
            std::optional<float> depthValue;
            std::optional<std::uint8_t> stencilValue;
            if (index == 0u) depthValue = aliasedDepth(*alias, range, target.samples);
            else if (index == 2u && separatePlane) stencilValue = aliasedStencil(*alias, range, target.samples);
            bool kept = false;
            if (depthValue) kept = std::bit_cast<std::uint32_t>(*depthValue) == std::bit_cast<std::uint32_t>(sampling.initialDepth.value_or(target.clearDepth));
            else if (stencilValue) kept = stencilClearGeneration != 0u ? *stencilValue == *stencilClearValue : *stencilValue == sampling.aliasStencil;
            const bool stencilRange = index == 2u || (index == 1u && uniform);
            if (!kept) reportAlias(*alias, range, stencilRange ? "stencil plane" : index == 1u ? "HTILE" : "depth");
            Require(kept, stencilRange ? stencilAliasRefusal : depthAliasRefusal);
            if (index == 2u && stencilClearGeneration != 0u) sampling.stencilAlias = *alias;
            if (stencilValue) stencilPlaneAliased = true;
        }
        if (stencilClearGeneration != 0u) {
            auto& plane = sampling.initial[2];
            const bool gpuFill = stencilFillNote != 0u && (queuedStencilFill || newestStencilNote == 0u);
            Require(GuestMemory::UnchangedSinceCollected(plane.address, plane.bytes, stencilClearGeneration), "CPU writes raced native multisample stencil plane fill");
            if (stencilPlaneAliased) sampling.aliasStencil = *stencilClearValue;
            else Require(gpuFill || stencilStored || GuestMemory::StoredAllSince_nid_no_patch(plane.address, plane.bytes, stencilClearGeneration), "native multisample stencil plane fill has no complete tracked store");
            if (!queuedStencilFill && !stencilPlaneAliased) {
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(plane.address);
                Require(std::all_of(bytes, bytes + plane.bytes, [&](std::uint8_t value) { return value == *stencilClearValue; }), "native multisample stencil plane does not hold its fill value");
            }
            const auto generation = GuestMemory::CollectWritesUncached(plane.address, plane.bytes);
            Require(GuestMemory::UnchangedSinceCollected(plane.address, plane.bytes, stencilClearGeneration), "CPU writes raced native multisample stencil plane fill");
            stencilFill = *stencilClearValue;
            RecordClear(VK_IMAGE_ASPECT_STENCIL_BIT);
            plane.generation = generation;
            RebasePublication(2u);
            stencilClearGeneration = 0u;
            sampling.unprovenStencil.reset();
        }
        if (clearGeneration != 0u) {
            const auto& metadata = sampling.initial[1];
            const bool gpuFill = clearPattern.has_value() && clearFillNote != 0u && (queuedFill || newestHtileNote == 0u);
            for (std::size_t index = 0u; index < sampling.initial.size(); ++index) {
                const auto& plane = sampling.initial[index];
                if (index != 1u) Require(GuestMemory::UnchangedSince(plane.address, plane.bytes, plane.generation), "guest writes over native multisample depth are not implemented");
            }
            Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, clearGeneration), "CPU writes raced native multisample depth clear");
            Require(gpuFill || clearStored || GuestMemory::StoredAllSince_nid_no_patch(metadata.address, metadata.bytes, clearGeneration), "native multisample depth clear has no complete tracked HTILE store");
            Require(clearDepth == 0.0f || clearDepth == 1.0f, "native multisample depth clear value is not implemented");
            auto current = sampling.initial;
            current[1].generation = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
            std::vector<std::uint32_t> words(queuedFill ? 1u : metadata.bytes / sizeof(std::uint32_t), queuedFill ? *clearPattern : 0u);
            if (!queuedFill) std::memcpy(words.data(), reinterpret_cast<const void*>(metadata.address), metadata.bytes);
            for (const auto word : words) {
                Require(htileProvesClear(word, target.htileDepthOnly, clearDepth), "native multisample depth completed HTILE does not prove its clear value");
                Require((clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT) == 0u || htileProvesStencilClear(word), "native multisample stencil completed HTILE does not prove its clear value");
            }
            for (const auto& range : current) GuestMemory::CollectWritesUncached(range.address, range.bytes);
            Require(GuestMemory::UnchangedSinceAll(current), "guest writes raced native multisample depth clear");
            Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, clearGeneration), "CPU writes raced native multisample depth clear");
            RecordClear(clearAspects);
            sampling.initial = current;
            RebasePublication(1u);
            clearGeneration = 0u;
            clearAspects = 0u;
            pendingClear = 0u;
        }
        Require(GuestMemory::UnchangedSinceAll(sampling.initial), "guest writes over native multisample depth are not implemented");
    }

    void CheckResource(const GuestTextureResource& resource) const {
        if (target.samples == 1u) return;
        const bool stencil = target.stencilAddress != 0u && resource.baseAddress == target.stencilAddress;
        const bool identity = stencil ? resource.format == 5u : resource.baseAddress == target.address && resource.format == (target.format == VK_FORMAT_D16_UNORM ? 7u : 22u);
        Require(identity && resource.width == target.extent.width && resource.height == target.extent.height && resource.samples == target.samples, "native multisample depth sampled identity or format mismatch");
        Require(resource.dimension == TextureDimension::k2DMsaa && resource.tileMode == TextureTileMode::kZ64KBX && resource.depthOrLastArray == 0u && resource.baseArray == 0u && resource.baseLevel == 0u && resource.lastLevel == 0u && resource.mipCount == 1u && resource.allocatedMipCount == 1u && resource.minLod == 0u, "native multisample depth sampled array, mip or layout is not implemented");
        if (uniformPlaneTarget(target)) {
            Require(resource.pipeBankXor == target.pipeBankXor && resource.dccAddress == 0u && !resource.dccAlphaOnMsb, "native multisample depth sampled metadata or XOR mismatch");
            return;
        }
        const bool htile = resource.dccAddress == target.htileAddress && resource.dccPipeAligned == target.htilePipeAligned;
        const bool bare = resource.dccAddress == 0u && !resource.dccPipeAligned;
        if (stencil) {
            Require(resource.pipeBankXor == target.pipeBankXor && bare && !resource.dccAlphaOnMsb && !target.htileStencil && target.htileDepthOnly, "native multisample depth sampled metadata or XOR mismatch");
            return;
        }
        const bool layout = resource.pipeBankXor == target.pipeBankXor && resource.iterate256 == target.iterate256 && !resource.dccAlphaOnMsb;
        if (layout && bare && !htile && !expanded) {
            static std::atomic<int> reports{0};
            if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] native multisample depth 0x%llx sampled through a descriptor without metadata before an in-place expand (last DB_RENDER_CONTROL 0x%08x)\n", static_cast<unsigned long long>(target.address), renderControl);
        }
        Require(layout && (htile || (bare && expanded)), "native multisample depth sampled metadata or XOR mismatch");
    }

    void RequireStencilProven() const {
        if (sampling.unprovenStencil) {
            const auto& plane = sampling.initial[2];
            reportAlias(MultisampleColorTargetOver(plane.address, plane.bytes).value_or(*sampling.unprovenStencil), plane, "unproven stencil plane");
        }
        Require(!sampling.unprovenStencil, stencilAliasRefusal);
    }

    void ApplyFastClear() {
        if (samplingActive) {
            CheckCurrent();
            Require(pendingClear == 0u, "native multisample depth fast-clear transition is not implemented");
        }
        const auto aspects = pendingClear & (VK_IMAGE_ASPECT_DEPTH_BIT | (target.stencilAddress != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u));
        pendingClear = 0;
        if (aspects == 0) return;
        RecordClear(aspects);
    }

    void RecordClear(VkImageAspectFlags aspects) {
        const float depth = depthFill.value_or(clearDepth);
        const std::uint8_t stencil = stencilFill.value_or(clearStencil);
        depthFill.reset();
        stencilFill.reset();
        RecordClearValue(aspects, depth, stencil);
    }

    void RecordClearValue(VkImageAspectFlags aspects, float depth, std::uint8_t stencil) {
        written = ++depthWrites;
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        const VkImageSubresourceRange range{aspects, 0, 1, 0, 1};
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearDepthStencilValue clear{depth, stencil};
        context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, access);
        if (batch) batch->SubmitAndWait();
        else {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            recorder->Keep(owned);
        }
    }

    bool UnormStencilView(const GuestTextureResource& resource) const {
        return target.samples == 1 && target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress && ResolveTextureFormat(resource.format) == VK_FORMAT_R8_UNORM;
    }

    bool SampledAccepts(std::span<const std::uint32_t> words, const GuestTextureResource& resource) const {
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        const bool oneSlice = target.samples != 1 ? resource.dimension == TextureDimension::k2DMsaa : resource.dimension == TextureDimension::k2D || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray == 0);
        return (ResolveTextureFormat(resource.format) == expected || depthBits || UnormStencilView(resource)) && oneSlice && resource.width == target.extent.width && resource.height == target.extent.height && resource.baseLevel == 0 && resource.lastLevel == 0 && resource.baseArray == 0;
    }

    std::shared_ptr<Texture> Sampled(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, bool drawn = false, VkImageViewType multisampleViewType = VK_IMAGE_VIEW_TYPE_2D, bool copySource = false) {
        if (!copySource) CheckCurrent();
        CheckResource(resource);
        std::array<std::uint32_t, 13> key{};
        std::copy_n(words.begin(), std::min<std::size_t>(words.size(), 8), key.begin());
        key[8] = components.r;
        key[9] = components.g;
        key[10] = components.b;
        key[11] = components.a;
        key[12] = static_cast<std::uint32_t>(multisampleViewType);
        const bool multisampled = target.samples != 1;
        Require(!multisampled || sampleable, "the device cannot sample the native depth target's format and sample count");
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const auto format = ResolveTextureFormat(resource.format);
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        if (multisampled) {
            Require(!depthBits && format == expected, "native multisampled depth sampling needs a numeric depth-plane format");
            Require(!stencil || (context.limits.sampledImageStencilSampleCounts & target.samples) != 0u, "the device cannot sample the native depth target's stencil at its sample count");
            Require(resource.samples == target.samples, "the sampled depth descriptor's sample count differs from its native target");
            Require(hostFormat == target.format, "sampling a native multisampled depth plane that the device stores in another depth format is not implemented");
        }
        if (stencil && UnormStencilView(resource) && SampledAccepts(words, resource)) {
            return unormStencil(key, components, resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, drawn);
        }
        if (!SampledAccepts(words, resource)) {
            char text[448];
            std::snprintf(text, sizeof(text), "AGC graphics: sampling the %s plane of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), tile mode %u, dimension %d, levels %u-%u, slice %u is not implemented (T# %08x %08x %08x %08x %08x %08x %08x %08x)",
                          stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), target.extent.width, target.extent.height, static_cast<int>(target.format), resource.width, resource.height, resource.format, static_cast<int>(format),
                          static_cast<unsigned>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseLevel, resource.lastLevel, resource.baseArray, key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
            throw std::runtime_error(text);
        }
        if (multisampled && useSampleShadow) {
            if (sampleShadow == nullptr) createSampleShadow();
            refreshSampleShadow(drawn, stencil);
        }
        if (const auto found = textures.find(key); found != textures.end()) return found->second;
        const auto viewType = multisampled ? multisampleViewType : resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        const auto sampledImage = useSampleShadow ? sampleShadow : owned;
        auto texture = std::make_shared<Texture>(context, useSampleShadow ? sampleShadow->image : image, hostFormat, stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, components, viewType, 0, sampledImage);
        if (multisampled) {
            Require(copySource || samplingActive, "native multisample depth sampling requires its creation-time owner");
            texture->MarkRefreshedPerUse();
        }
        textures.emplace(key, texture);
        return texture;
    }

    std::shared_ptr<Texture> FirstSample(const Texture& source, VkComponentMapping components) {
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "a native multisampled depth target is sampled only by recorded draws and dispatches");
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto format = d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        if (firstSample == nullptr) firstSample = std::make_shared<FirstSampleTextures>(context, format, target.extent);
        firstSample->copy.Record(*recorder, source.View(), source.Layout());
        const std::array<std::uint32_t, 4> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a)};
        auto& texture = firstSample->textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, firstSample->copy.Image(), format, VK_IMAGE_ASPECT_COLOR_BIT, components);
            texture->MarkRefreshedPerUse();
        }
        return {firstSample, texture.get()};
    }

    void ForgetClear(VkImageAspectFlags aspects) {
        pendingClear &= ~aspects;
        if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) depthFill.reset();
        if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) stencilFill.reset();
    }

    void MarkStencilStale() {
        if (!StencilMirrorable()) return;
        stencilStale = true;
        stencilMark = nextPlaneSerial();
        const auto [begin, end] = staleStencilRange(*this);
        noteStalePlane(begin, end);
    }

    void NoteHtileClear(VkImageAspectFlags aspects) {
        ForgetClear(aspects);
        pendingClear |= aspects;
        if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0u) NoteDepthWork();
        if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u) MarkStencilStale();
    }

    void NotePlaneFill(VkImageAspectFlagBits aspect, float depth, std::uint8_t stencil) {
        pendingClear |= aspect;
        if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
            depthFill = depth;
            NoteDepthWork();
        } else {
            stencilFill = stencil;
            MarkStencilStale();
        }
    }

    void CopyAspectFrom(const DepthSurface& source, VkImageAspectFlagBits aspect) {
        ForgetClear(aspect);
        NoteHostWrite();
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        constexpr VkAccessFlags all = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, all, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy region{};
        region.srcSubresource = {static_cast<VkImageAspectFlags>(aspect), 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, all);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
    }

    bool Holds(const Texture* texture) const {
        const auto held = [&](const auto& entry) { return entry.second.get() == texture; };
        return std::any_of(textures.begin(), textures.end(), held) || std::any_of(stencilCopy.textures.begin(), stencilCopy.textures.end(), held);
    }

    bool SampledElsewhere() const {
        return std::any_of(textures.begin(), textures.end(), [](const auto& entry) { return entry.second.use_count() > 1; });
    }

    void NoteHostWrite() {
        if (samplingActive && !hostWritten->load(std::memory_order_acquire)) capturePublication();
        hostEverWritten->store(true, std::memory_order_release);
        hostWritten->store(true, std::memory_order_release);
    }

    void ProveOverwrite() {
        if (!sampling.overwriteStencil || !hostEverWritten->load(std::memory_order_acquire) || Recorder::InCompletion()) return;
        const auto value = *sampling.overwriteStencil;
        sampling.overwriteStencil.reset();
        pendingOverwriteProofs().fetch_sub(1u, std::memory_order_acq_rel);
        if (!sampleable || context.detiler == nullptr || !active->load(std::memory_order_acquire)) return;
        if (auto* recorder = RecorderFor(context)) recorder->Sync();
        for (const auto& range : sampling.initial) {
            StorageTexture::FlushPending(range.address, range.bytes);
            GuestMemory::FlushGpuWrites(range.address, range.bytes);
        }
        if (useSampleShadow) {
            if (sampleShadow == nullptr) createSampleShadow();
            refreshSampleShadow(true, true);
            if (auto* recorder = RecorderFor(context)) recorder->Sync();
        }
        const auto samples = static_cast<std::uint64_t>(target.extent.width) * target.extent.height * target.samples;
        const auto stencil = readBackSamples(VK_IMAGE_ASPECT_STENCIL_BIT, samples);
        const auto bytes = stencil->Bytes().first(static_cast<std::size_t>(samples));
        if (std::ranges::any_of(bytes, [&](std::byte stored) { return stored != static_cast<std::byte>(value); })) return;
        for (const auto& range : sampling.initial) GuestMemory::CollectWritesUncached(range.address, range.bytes);
        if (!GuestMemory::UnchangedSinceAll(sampling.initial)) return;
        sampling.refusal.clear();
        {
            std::unique_lock guardLock(depthGuardMutex());
            for (const auto& range : sampling.initial) depthGuards().push_back({context.device, range.address, range.bytes, active, {{range.address, range.bytes, 0u}}, hostWritten, hostEverWritten});
            publishDepthGuards(true);
        }
        InstallResidentImageReadGuard_nid_no_patch();
        samplingActive = true;
        capturePublication();
    }

    bool GuestHoldsSamples() const {
        return target.samples != 1u && uniformPlaneTarget(target) && samplingActive && sampling.refusal.empty() && !hostWritten->load(std::memory_order_acquire);
    }

    void PublishToGuest();

    std::uint64_t written = ++depthWrites;
    std::shared_ptr<std::atomic<bool>> hostWritten = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> hostEverWritten = std::make_shared<std::atomic<bool>>(false);
    bool retired = false;

    bool Overlaps(std::uint64_t address, std::uint64_t bytes) const {
        const auto end = address + bytes;
        if (address < target.address + depthBytes() && target.address < end) return true;
        return target.stencilAddress != 0 && address < target.stencilAddress + stencilBytes() && target.stencilAddress < end;
    }

    bool PlaneCoveredBy(bool stencilPlane, std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) const {
        const auto reaches = [&](std::uint64_t address, std::uint32_t bytesPerTexel) {
            const auto [blockWidth, blockHeight] = DepthBlockExtent(bytesPerTexel);
            const std::array<std::uint32_t, 3> block{65536u, blockWidth, blockHeight};
            const auto blocksPerRow = (target.extent.width + blockWidth - 1u) / blockWidth;
            return std::any_of(extents.begin(), extents.end(), [&](const auto& extent) {
                return extent.address == address && extent.bytesPerElement == bytesPerTexel && extent.block == block && extent.blocksPerRow == blocksPerRow && extent.width >= target.extent.width && extent.height >= target.extent.height;
            });
        };
        const auto covers = [&](std::uint64_t begin, std::uint64_t end) {
            for (const auto& [writtenBegin, writtenEnd] : written) {
                if (writtenEnd <= begin) continue;
                if (writtenBegin > begin) return false;
                begin = writtenEnd;
                if (begin >= end) return true;
            }
            return false;
        };
        if (stencilPlane) return target.stencilAddress != 0 && ((target.samples == 1u && reaches(target.stencilAddress, 1u)) || covers(target.stencilAddress, target.stencilAddress + stencilBytes()));
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return (target.samples == 1u && reaches(target.address, d16 ? 2u : 4u)) || covers(target.address, target.address + depthBytes());
    }

    bool CoveredBy(std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) const {
        return PlaneCoveredBy(false, written, extents) || PlaneCoveredBy(true, written, extents);
    }

    bool StencilWrittenInMemory() const {
        if (target.stencilAddress == 0) return false;
        GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes()));
        return GuestMemory::WrittenSince(target.stencilAddress, static_cast<std::size_t>(stencilBytes()), stencilMemory);
    }

    void NoteMemory() {
        depthMemory = GuestMemory::CollectWrites(target.address, static_cast<std::size_t>(depthBytes()));
        stencilMemory = target.stencilAddress != 0 ? GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes())) : 0;
    }

    bool OverwrittenOnGpu() const {
        std::array<std::pair<std::uint64_t, std::uint64_t>, 2> ranges{{{target.address, target.address + depthBytes()}, {target.stencilAddress, target.stencilAddress + stencilBytes()}}};
        return StorageTexture::AnyPendingOverlaps(std::span(ranges).first(target.stencilAddress != 0 ? 2u : 1u));
    }

    bool OverwrittenInMemory() const {
        GuestMemory::CollectWrites(target.address, static_cast<std::size_t>(depthBytes()));
        if (GuestMemory::WrittenSince(target.address, static_cast<std::size_t>(depthBytes()), depthMemory)) return true;
        if (target.stencilAddress == 0) return false;
        GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes()));
        return GuestMemory::WrittenSince(target.stencilAddress, static_cast<std::size_t>(stencilBytes()), stencilMemory);
    }

    VkImageAspectFlags pendingClear = 0;
    float clearDepth = 0.0f;
    std::uint8_t clearStencil = 0;
    std::optional<float> depthFill;
    std::optional<std::uint8_t> stencilFill;

    bool StorageViewMatches(std::span<const std::uint32_t> words, const GuestTextureResource& resource) const {
        if (resource.width != target.extent.width || resource.height != target.extent.height) return false;
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        return ResolveTextureFormat(resource.format) == expected || depthBits;
    }

    void RequireStorable(const GuestTextureResource& resource) const {
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        Require(target.samples == 1, "storage image access to a multisampled depth surface is not implemented");
        if (!stencil && hostFormat != target.format) {
            char text[224];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image access to the depth plane of depth surface 0x%llx (vk format %d) stored as vk format %d is not implemented", static_cast<unsigned long long>(target.address), static_cast<int>(target.format), static_cast<int>(hostFormat));
            throw std::runtime_error(text);
        }
    }

    std::shared_ptr<StorageTexture> Storage(std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip) {
        if (!StorageViewMatches(words, resource)) return nullptr;
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool oneLayer = resource.dimension == TextureDimension::k2DArray && resource.baseArray == 0 && resource.depthOrLastArray == 0;
        if (resource.dimension != TextureDimension::k2D && !oneLayer) {
            char text[256];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image access to depth/stencil surface 0x%llx is not implemented (storage=%ux%u guest format=%u mip=%u dimension=%d tile=%d)", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, mip, static_cast<int>(resource.dimension), static_cast<int>(resource.tileMode));
            throw std::runtime_error(text);
        }
        RequireStorable(resource);
        auto* overwrite = stencil ? nullptr : PlaneOverwritten(words);
        ApplyFastClear();
        auto& plane = stencil ? stencilPlane : depthPlane;
        if (plane.storage == nullptr || plane.storage->Descriptor().format != resource.format || plane.storage->Descriptor().tileMode != resource.tileMode || plane.storage->Descriptor().baseAddress != resource.baseAddress || plane.storage->Descriptor().dimension != resource.dimension) {
            std::erase_if(plane.replaced, [](const auto& old) { return old.expired(); });
            if (plane.storage != nullptr) plane.replaced.push_back(plane.storage);
            plane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        }
        if (overwrite != nullptr) {
            ++depthClearCounters().copiesSkipped;
        } else {
            copyPlane(plane, plane.storage, stencil, true);
        }
        return plane.storage;
    }

    bool OverwritesWholePlane(const DepthPlaneOverwrite& overwrite) const {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return overwrite.bits == (d16 ? 16u : 32u) && overwrite.width >= target.extent.width && overwrite.height >= target.extent.height;
    }

    DepthPlaneOverwrite* PlaneOverwritten(std::span<const std::uint32_t> words) const {
        auto* overwrite = currentOverwrite;
        if (overwrite == nullptr || words.size() < overwrite->image.size() || !std::equal(overwrite->image.begin(), overwrite->image.end(), words.begin())) return nullptr;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        auto& declined = depthClearCounters().declined;
        if (overwrite->bits != (d16 ? 16u : 32u)) {
            ++declined[DeclineSize];
            return nullptr;
        }
        if (overwrite->width < target.extent.width || overwrite->height < target.extent.height) {
            ++declined[DeclineCoverage];
            return nullptr;
        }
        return overwrite;
    }

    bool StoreOverwrite(const StorageTexture& mirror) {
        auto* overwrite = currentOverwrite;
        if (overwrite == nullptr || !OverwritesWholePlane(*overwrite)) return false;
        const auto& described = mirror.Descriptor();
        const auto& view = overwrite->view;
        if (described.baseAddress != view.baseAddress || described.format != view.format || described.tileMode != view.tileMode || described.dimension != view.dimension) return false;
        std::uint32_t current = 0;
        try {
            GuestMemory::Read(overwrite->colorAddress, std::as_writable_bytes(std::span(&current, 1)), 4);
        } catch (...) {
            return false;
        }
        if (current != overwrite->colorBits) return false;
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "depth surface storage clears need the recorder of the surface's device");
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * (d16 ? 2u : 4u);
        recorder->Commands();
        const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
        RecordClearValue(VK_IMAGE_ASPECT_DEPTH_BIT, overwrite->depth, clearStencil);
        recorder->EndGpuTiming(timing, bytes);
        auto& counters = depthClearCounters();
        ++counters.clears;
        counters.bytesSkipped += 2u * bytes;
        return true;
    }

    bool StoreBack(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            auto& plane = stencil ? stencilPlane : depthPlane;
            if (plane.storage.get() == &candidate) {
                if (stencil || !StoreOverwrite(candidate)) copyPlane(plane, plane.storage, stencil, false);
                written = ++depthWrites;
                NoteHostWrite();
                return true;
            }
            if (std::none_of(plane.replaced.begin(), plane.replaced.end(), [&](const auto& old) { return old.lock().get() == &candidate; })) continue;
            char text[288];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image writes to the %s plane of depth surface 0x%llx through a view that another view of the plane (guest format %u, tile mode %d) replaced are not implemented", stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), plane.storage->Descriptor().format, static_cast<int>(plane.storage->Descriptor().tileMode));
            throw std::runtime_error(text);
        }
        return false;
    }

    void CopyDepthTo(const std::shared_ptr<StorageTexture>& storage) {
        ApplyFastClear();
        copyPlane(depthPlane, storage, false, true);
    }

    void CopyStencilTo(const std::shared_ptr<StorageTexture>& storage) {
        ApplyFastClear();
        copyPlane(stencilPlane, storage, true, true);
    }

    bool StoreBackCovering(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            if (!CoversPlane(candidate, stencil)) continue;
            auto& plane = stencil ? stencilPlane : depthPlane;
            const auto source = std::const_pointer_cast<StorageTexture>(candidate.shared_from_this());
            copyPlane(plane, source, stencil, false);
            ForgetClear(stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);
            written = ++depthWrites;
            NoteHostWrite();
            return true;
        }
        return false;
    }

    void TakeStencilStorage() {
        if (target.samples != 1 || target.stencilAddress == 0) return;
        const auto writes = StorageTexture::EightBitWrites();
        if (writes == stencilWritesSeen) return;
        stencilWritesSeen = writes;
        const auto bytes = DepthSliceBytes(target.extent, 1u);
        auto storage = StorageTexture::FindPending(target.stencilAddress, bytes);
        if (storage == nullptr || storage == stencilMirror.storage || storage->Device() != context.device) return;
        const auto& described = storage->Descriptor();
        const auto format = ResolveTextureFormat(described.format);
        if (described.baseAddress != target.stencilAddress || described.width != target.extent.width || described.height != target.extent.height || (format != VK_FORMAT_R8_UINT && format != VK_FORMAT_R8_UNORM)) return;
        if (stencilTaken.lock() == storage && stencilTakenVersion == storage->Version()) return;
        storage->Refresh();
        copyPlane(stencilPlane, storage, true, false, Recorder::Active() == nullptr);
        stencilTaken = storage;
        stencilTakenVersion = storage->Version();
        written = ++depthWrites;
        NoteHostWrite();
    }

    void TakeStorage() {
        if (target.samples != 1 || hostFormat != target.format) return;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = DepthSliceBytes(target.extent, d16 ? 2u : 4u);
        auto storage = StorageTexture::FindPending(target.address, bytes);
        if (storage == nullptr) storage = StorageTexture::FindLive(target.address, bytes);
        if (storage == nullptr || !storageFits(target, storage->Descriptor())) return;
        storage->Refresh();
        const bool direct = DirectDepthCopy(context, hostFormat, VK_IMAGE_ASPECT_DEPTH_BIT, storage->ImageFormat());
        if (!direct && depthPlane.staging == nullptr) depthPlane.staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(target.extent.width) * target.extent.height * (d16 ? 2u : 4u), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(storage);
        VkBufferImageCopy depthRegion{0, 0, 0, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        VkBufferImageCopy colorRegion = depthRegion;
        colorRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (direct) {
            const VkImageCopy copy{{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
            const bool discarding = target.stencilAddress == 0;
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            if (discarding) {
                VkImageMemoryBarrier discard{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                discard.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                discard.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                discard.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                discard.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                discard.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                discard.image = image;
                discard.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
                const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
                barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &discard);
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
                VkImageMemoryBarrier toGeneral = discard;
                toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toGeneral.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            } else {
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            }
            directDepthCopies.fetch_add(1, std::memory_order_relaxed);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Copy, discarding ? 3 : 2);
            return;
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, depthPlane.staging->Handle(), 1, &colorRegion);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        if (target.stencilAddress == 0) {
            VkImageMemoryBarrier discard{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            discard.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            discard.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            discard.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            discard.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            discard.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            discard.image = image;
            discard.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &discard);
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, depthPlane.staging->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &depthRegion);
            VkImageMemoryBarrier toGeneral = discard;
            toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        } else {
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, depthPlane.staging->Handle(), image, VK_IMAGE_LAYOUT_GENERAL, 1, &depthRegion);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        }
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Copy, target.stencilAddress == 0 ? 4 : 3);
    }

    std::shared_ptr<StorageTexture> Mirror(const GuestTextureResource& resource) {
        if (mirrorPlane.storage == nullptr) {
            mirrorPlane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
            ++mirrorEpoch;
        }
        return mirrorPlane.storage;
    }

    bool StoresUnits(const StorageTexture& mirror) const {
        return mirror.BlockUnits() && mirror.TrackedUnits() == staleUnits.size();
    }

    std::shared_ptr<StorageTexture> StoreDepthPlane(const GuestTextureResource& resource, std::vector<bool>& units) {
        ApplyFastClear();
        auto mirror = Mirror(resource);
        auto& counters = depthPlaneStoreCounters();
        ++counters.stores;
        const auto planeBytes = mirror->GuestBytes();
        if (!StoresUnits(*mirror)) {
            units.assign(units.size(), true);
            copyPlane(mirrorPlane, mirror, false, true, true);
            mirror->MarkWholeWritten();
            counters.units += units.size();
            counters.bytes += planeBytes;
            return mirror;
        }
        const auto rects = mirror->UnitRects(units);
        copyPlane(mirrorPlane, mirror, false, true, true, &rects);
        mirror->MarkUnitsWritten(units);
        for (std::size_t unit = 0; unit < units.size(); ++unit) {
            if (!units[unit]) continue;
            ++counters.units;
            counters.bytes += std::min<std::uint64_t>(unitBytes, planeBytes - unit * unitBytes);
        }
        return mirror;
    }

    void SizeUnits(std::uint64_t planeBytes) {
        static const bool whole = std::getenv("APS5_DEPTH_PLANE_WHOLE_STORE") != nullptr;
        const std::uint64_t bytes = !whole && target.address % 65536u == 0 ? std::uint64_t{65536} : std::max<std::uint64_t>(planeBytes, 1u);
        const auto count = static_cast<std::size_t>(std::max<std::uint64_t>((planeBytes + bytes - 1u) / bytes, 1u));
        if (unitBytes == bytes && staleUnits.size() == count) return;
        unitBytes = bytes;
        staleUnits.assign(count, false);
        staleCount = 0;
        unitGeneration.assign(count, 0);
        unitBacking.assign(count, 0);
        backingGeneration = 0;
    }

    bool AnyStale() const {
        return staleCount != 0;
    }

    void NoteDepthWork() {
        depthMark = nextPlaneSerial();
    }

    void MarkAllStale(std::uint64_t planeBytes) {
        SizeUnits(planeBytes);
        staleUnits.assign(staleUnits.size(), true);
        staleCount = staleUnits.size();
    }

    void ClearStale() {
        staleUnits.assign(staleUnits.size(), false);
        staleCount = 0;
    }

    void ClearStale(const std::vector<bool>& units) {
        for (std::size_t unit = 0; unit < units.size() && unit < staleUnits.size(); ++unit) {
            if (!units[unit] || !staleUnits[unit]) continue;
            staleUnits[unit] = false;
            --staleCount;
        }
    }

    std::vector<bool> StaleUnitsIn(std::uint64_t planeBytes, std::uint64_t begin, std::uint64_t end) const {
        std::vector<bool> units(staleUnits.size(), false);
        const auto plane = target.address;
        if (units.empty() || unitBytes == 0 || begin >= end || begin >= plane + planeBytes || plane >= end) return units;
        const auto from = std::max(begin, plane) - plane;
        const auto to = std::min(end, plane + planeBytes) - plane;
        const auto last = std::min<std::uint64_t>((to - 1u) / unitBytes, units.size() - 1u);
        for (auto unit = from / unitBytes; unit <= last; ++unit) units[static_cast<std::size_t>(unit)] = staleUnits[static_cast<std::size_t>(unit)];
        return units;
    }

    std::pair<std::uint64_t, std::uint64_t> UnitHull(const std::vector<bool>& units, std::uint64_t planeBytes) const {
        std::uint64_t first = planeBytes;
        std::uint64_t last = 0;
        for (std::size_t unit = 0; unit < units.size(); ++unit) {
            if (!units[unit]) continue;
            first = std::min<std::uint64_t>(first, unit * unitBytes);
            last = std::max<std::uint64_t>(last, std::min<std::uint64_t>(planeBytes, (unit + 1u) * unitBytes));
        }
        return {target.address + first, target.address + std::max(first, last)};
    }

    void NoteStored(const std::vector<bool>& units, std::uint64_t generation) {
        for (std::size_t unit = 0; unit < units.size() && unit < unitGeneration.size(); ++unit) {
            if (units[unit]) unitGeneration[unit] = generation;
        }
    }

    std::vector<bool> WrittenUnits(const GuestTextureResource& resource, std::uint64_t& generation) {
        const auto bytes = static_cast<std::size_t>(MirrorBytes());
        SizeUnits(bytes);
        const auto collected = GuestMemory::CollectWrites(resource.baseAddress, bytes);
        generation = GuestMemory::TrackerGeneration();
        std::vector<bool> changed;
        const auto mark = [&](std::size_t unit) {
            if (changed.empty()) changed.assign(unitGeneration.size(), false);
            changed[unit] = true;
        };
        if (collected == 0u) {
            for (std::size_t unit = 0; unit < unitGeneration.size(); ++unit) {
                const auto offset = unit * unitBytes;
                const auto address = resource.baseAddress + offset;
                const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(unitBytes, bytes - offset));
                if (GuestMemory::CollectWritesUncached(address, length) != 0u) {
                    if (unitGeneration[unit] != 0u && GuestMemory::WrittenSince(address, length, unitGeneration[unit])) mark(unit);
                    continue;
                }
                constexpr std::uint64_t pageBytes = 4096u;
                for (std::size_t page = 0; page < length;) {
                    const auto pageAddress = address + page;
                    const auto pageLength = static_cast<std::size_t>(std::min<std::uint64_t>(length - page, pageBytes - pageAddress % pageBytes));
                    if (GuestMemory::CollectWritesUncached(pageAddress, pageLength) != 0u && unitGeneration[unit] != 0u && GuestMemory::WrittenSince(pageAddress, pageLength, unitGeneration[unit])) mark(unit);
                    page += pageLength;
                }
            }
            generation = GuestMemory::TrackerGeneration();
            return changed;
        }
        if (unitGeneration.size() == 1u) {
            if (GuestMemory::WrittenSince(resource.baseAddress, bytes, unitGeneration[0])) mark(0);
            return changed;
        }
        const auto oldest = *std::min_element(unitGeneration.begin(), unitGeneration.end());
        if (oldest != 0u && !GuestMemory::WrittenSince(resource.baseAddress, bytes, oldest)) return changed;
        std::vector<std::uint8_t> stamped(unitGeneration.size(), GuestMemory::BlockUnchanged);
        if (!GuestMemory::ChangedBlocks(resource.baseAddress, bytes, unitGeneration, stamped)) return changed;
        for (std::size_t unit = 0; unit < stamped.size(); ++unit) {
            if (unitGeneration[unit] != 0u && stamped[unit] == GuestMemory::BlockWritten) mark(unit);
        }
        return changed;
    }

    std::vector<bool> ChangedBackingUnits() const {
        std::vector<bool> changed(unitBacking.size(), false);
        if (backingGeneration == GuestAllocations::GuestAllocationsGeneration_nid_postfix()) return changed;
        for (std::size_t unit = 0; unit < unitBacking.size(); ++unit) {
            changed[unit] = unitBacking[unit] != 0 && RegisteredBacking(target.address + unit * unitBytes) != unitBacking[unit];
        }
        return changed;
    }

    void SyncFromMemory(const GuestTextureResource& resource, bool storing = false) {
        std::uint64_t generation = 0;
        const auto changed = WrittenUnits(resource, generation);
        // The registry generation moves on every mapping mutation, so an unchanged value means every recorded backing is still current. Read it before the lookups: a mutation racing them leaves the stored value old and the next sync refreshes again.
        if (const auto registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix(); registryGeneration != backingGeneration) {
            for (std::size_t unit = 0; unit < unitBacking.size(); ++unit) unitBacking[unit] = RegisteredBacking(target.address + unit * unitBytes);
            backingGeneration = registryGeneration;
        }
        const bool clearing = (pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
        if ((clearing && !storing) || !anyUnit(changed)) {
            unitGeneration.assign(unitGeneration.size(), generation);
            return;
        }
        if (clearing) {
            ClearStale();
            ForgetClear(VK_IMAGE_ASPECT_DEPTH_BIT);
        } else {
            ClearStale(changed);
        }
        {
            struct Syncing {
                bool& flag;
                ~Syncing() { flag = false; }
            } guard{syncing};
            syncing = true;
            if (mirrorPlane.storage == nullptr) {
                mirrorPlane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
                ++mirrorEpoch;
            } else {
                static_cast<void>(mirrorPlane.storage->Refresh());
            }
        }
        unitGeneration.assign(unitGeneration.size(), generation);
        const auto mirror = mirrorPlane.storage;
        if (StoresUnits(*mirror)) {
            const auto rects = mirror->UnitRects(changed);
            copyPlane(mirrorPlane, mirror, false, false, true, &rects);
        } else {
            ClearStale();
            copyPlane(mirrorPlane, mirror, false, false, true);
        }
        written = ++depthWrites;
    }

    void DropMirror() {
        mirrorPlane = {};
    }

    bool StencilMirrorable() const {
        return target.samples == 1u && target.stencilAddress != 0u && target.depthSwizzle == XorSwizzleMode(TextureTileMode::kZ64KBX);
    }

    void SyncStencilFromMemory(const GuestTextureResource& resource) {
        const auto bytes = static_cast<std::size_t>(StencilMirrorBytes());
        GuestMemory::CollectWrites(resource.baseAddress, bytes);
        const auto generation = GuestMemory::TrackerGeneration();
        const bool changed = stencilGeneration != 0u && GuestMemory::WrittenSince(resource.baseAddress, bytes, stencilGeneration);
        stencilGeneration = generation;
        if (!changed || (pendingClear & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) return;
        stencilStale = false;
        {
            struct Syncing {
                bool& flag;
                ~Syncing() { flag = false; }
            } guard{syncing};
            syncing = true;
            if (stencilMirror.storage == nullptr) stencilMirror.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
            else static_cast<void>(stencilMirror.storage->Refresh());
        }
        copyPlane(stencilMirror, stencilMirror.storage, true, false, true);
        written = ++depthWrites;
    }

    std::shared_ptr<StorageTexture> StoreStencilPlane(const GuestTextureResource& resource) {
        ApplyFastClear();
        if (stencilMirror.storage == nullptr) stencilMirror.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        copyPlane(stencilMirror, stencilMirror.storage, true, true, true);
        stencilMirror.storage->MarkWholeWritten();
        stencilStale = false;
        return stencilMirror.storage;
    }

    void NoteStencilStored(std::uint64_t generation) {
        stencilGeneration = generation;
    }

    bool stencilStale = false;
    std::uint64_t stencilGeneration = 0;

    std::vector<bool> staleUnits;
    std::size_t staleCount = 0;
    std::uint64_t depthMark = 0;
    std::uint64_t stencilMark = 0;
    std::vector<std::uint64_t> unitBacking;
    std::uint64_t backingGeneration = 0;
    std::vector<std::uint64_t> unitGeneration;
    std::uint64_t unitBytes = 0;
    bool syncing = false;
    std::uint64_t scannedEpoch = 0;
    mutable std::uint64_t mirrorBytes = 0;
    mutable std::uint64_t stencilMirrorBytes = 0;

    std::uint64_t MirrorBytes() const;
    std::uint64_t StencilMirrorBytes() const;

    const Context context;
    const DepthTarget target;
    const VkFormat hostFormat;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::shared_ptr<std::atomic<bool>> active = std::make_shared<std::atomic<bool>>(true);
    bool sampleable = true;
    bool samplingActive = false;
    DepthSamplingState sampling;
    bool clearPending = false;
    std::uint64_t clearGeneration = 0u;
    bool clearCompletionQueued = false;
    VkImageAspectFlags clearAspects = 0u;
    std::optional<std::uint32_t> clearPattern;
    std::uint64_t clearFillNote = 0u;
    std::uint64_t clearFillFloor = 0u;
    std::uint64_t clearStoreStamp = 0u;
    bool clearStored = false;
    std::uint64_t stencilClearGeneration = 0u;
    std::optional<std::uint8_t> stencilClearValue;
    std::uint64_t stencilFillNote = 0u;
    std::uint64_t stencilFillFloor = 0u;
    bool stencilStored = false;
    bool expanded = false;
    std::uint32_t renderControl = 0u;
    std::shared_ptr<OwnedImage> owned;
    std::shared_ptr<OwnedImage> sampleShadow;
    bool useSampleShadow = false;
    std::uint64_t sampleShadowWritten = 0u;
    bool sampleShadowStencil = false;

private:
    struct PublicationPlane {
        static constexpr std::size_t PageBytes = 4096u;
        static constexpr std::size_t Repeated = std::numeric_limits<std::size_t>::max();
        GuestMemory::UnchangedQuery range;
        std::vector<std::uint32_t> words;
        std::vector<std::size_t> starts;
        std::vector<std::byte> stored;

        std::byte Byte(std::size_t offset) const {
            const auto page = offset / PageBytes;
            if (starts[page] != Repeated) return stored[starts[page] + offset % PageBytes];
            return static_cast<std::byte>(words[page] >> (offset % 4u * 8u));
        }

        std::vector<std::byte> Expand() const {
            std::vector<std::byte> bytes(range.bytes);
            for (std::size_t offset = 0u; offset < bytes.size(); ++offset) bytes[offset] = Byte(offset);
            return bytes;
        }
    };
    std::vector<PublicationPlane> publication;
    bool publicationInProgress = false;
    bool publicationFailed = false;
    std::exception_ptr publicationFailure;

    void capturePublication() {
        if (target.format != VK_FORMAT_D32_SFLOAT_S8_UINT || context.detiler == nullptr) return;
        try {
            const auto lease = GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
            CheckCurrent();
            std::vector<PublicationPlane> captured;
            for (const auto& range : sampling.initial) captured.push_back(CapturePublication(range));
            for (const auto& plane : captured) {
                Require(GuestMemory::UnchangedSinceCollected(plane.range.address, plane.range.bytes, plane.range.generation), "guest writes raced native multisample depth publication capture");
                Require(GuestMemory::UnchangedSince(plane.range.address, plane.range.bytes, plane.range.generation), "guest writes raced native multisample depth publication capture");
            }
            for (const auto& range : sampling.initial) Require(GuestMemory::UnchangedSinceCollected(range.address, range.bytes, range.generation), "guest writes raced native multisample depth publication capture");
            Require(GuestMemory::UnchangedSinceAll(sampling.initial), "guest writes raced native multisample depth publication capture");
            publication = std::move(captured);
        } catch (...) {
            publicationFailure = std::current_exception();
            publication.clear();
        }
    }

    std::unique_ptr<Buffer> readBackSamples(VkImageAspectFlagBits aspect, std::uint64_t elements) {
        const bool depth = aspect == VK_IMAGE_ASPECT_DEPTH_BIT;
        const auto linearBytes = (elements * (depth ? 4u : 1u) + 3u) & ~std::uint64_t{3u};
        Require(linearBytes <= context.limits.maxStorageBufferRange, "native multisample depth publication exceeds the device's readback range");
        auto linear = std::make_unique<Buffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        VkImageView arrayView = VK_NULL_HANDLE;
        try {
            VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            info.image = useSampleShadow ? sampleShadow->image : image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            info.format = hostFormat;
            info.subresourceRange = {static_cast<VkImageAspectFlags>(aspect), 0u, 1u, 0u, 1u};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &arrayView), "vkCreateImageView depth publication");
            CommandBatch batch(context);
            constexpr VkAccessFlags writes = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, writes, VK_ACCESS_SHADER_READ_BIT);
            context.detiler->DispatchMultisampleReadback(batch.Handle(), depth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8_UINT, linear->Handle(), 0u, linearBytes, arrayView, target.extent.width, target.extent.height, 1u, target.samples, 0u, static_cast<std::uint32_t>(elements));
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            batch.SubmitAndWait();
        } catch (...) {
            if (arrayView != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, arrayView, nullptr);
            throw;
        }
        context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, arrayView, nullptr);
        linear->Invalidate();
        return linear;
    }

    PublicationPlane CapturePublication(const GuestMemory::UnchangedQuery& range) {
        PublicationPlane plane;
        plane.range = range;
        plane.range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        const auto pages = (range.bytes + PublicationPlane::PageBytes - 1u) / PublicationPlane::PageBytes;
        plane.words.assign(pages, 0u);
        plane.starts.assign(pages, PublicationPlane::Repeated);
        std::array<std::byte, PublicationPlane::PageBytes> page;
        for (std::size_t index = 0u; index < pages; ++index) {
            const auto first = index * PublicationPlane::PageBytes;
            const auto length = std::min(PublicationPlane::PageBytes, range.bytes - first);
            GuestMemory::CopyGuestBytes(page.data(), reinterpret_cast<const void*>(range.address + first), length);
            if (length <= 4u || std::equal(page.begin() + 4u, page.begin() + length, page.begin())) {
                std::memcpy(&plane.words[index], page.data(), std::min<std::size_t>(length, 4u));
            } else {
                plane.starts[index] = plane.stored.size();
                plane.stored.insert(plane.stored.end(), page.begin(), page.begin() + length);
            }
        }
        Require(GuestMemory::UnchangedSinceCollected(range.address, range.bytes, plane.range.generation) && GuestMemory::UnchangedSince(range.address, range.bytes, plane.range.generation), "guest writes raced native multisample depth publication capture");
        return plane;
    }

    void RebasePublication(std::size_t index) {
        if (publication.empty()) return;
        try {
            publication[index] = CapturePublication(sampling.initial[index]);
        } catch (...) {
            publicationFailure = std::current_exception();
            publication.clear();
        }
    }

    struct PlaneStorage {
        std::shared_ptr<StorageTexture> storage;
        std::vector<std::weak_ptr<StorageTexture>> replaced;
        std::unique_ptr<DeviceBuffer> staging;
    };
    struct StencilCopy {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::unique_ptr<DeviceBuffer> staging;
        std::map<std::array<std::uint32_t, 13>, std::shared_ptr<Texture>> textures;
        std::uint64_t written = 0;
        bool filled = false;
    };
    std::map<std::array<std::uint32_t, 13>, std::shared_ptr<Texture>> textures;
    struct FirstSampleTextures {
        FirstSampleTextures(const Context& context, VkFormat format, VkExtent2D extent) : copy(context, format, extent) {}
        FirstSampleCopy copy;
        std::map<std::array<std::uint32_t, 4>, std::shared_ptr<Texture>> textures;
    };
    std::shared_ptr<FirstSampleTextures> firstSample;
    PlaneStorage depthPlane;
    PlaneStorage stencilPlane;
    std::weak_ptr<StorageTexture> stencilTaken;
    std::uint64_t stencilTakenVersion = 0;
    std::uint64_t stencilWritesSeen = 0;
    std::uint64_t depthMemory = 0;
    std::uint64_t stencilMemory = 0;
    StencilCopy stencilCopy;
    VkImageView transferView = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> depthUpload;

    const std::shared_ptr<OwnedImage>& depthUploadImage() {
        if (depthUpload != nullptr) return depthUpload;
        VkImage uploadImage = VK_NULL_HANDLE;
        VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = hostFormat;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &uploadImage), "vkCreateImage depth upload");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, uploadImage, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(MemoryAccounting::Allocate(context, allocation, "depth upload", &uploadMemory), "vkAllocateMemory depth upload");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, uploadImage, uploadMemory, 0), "vkBindImageMemory depth upload");
            depthUpload = std::make_shared<OwnedImage>(context, uploadImage, uploadMemory);
        } catch (...) {
            if (uploadImage != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, uploadImage, nullptr);
            if (uploadMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, uploadMemory);
            throw;
        }
        return depthUpload;
    }

    VkImageUsageFlags sampleShadowUsage() const {
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (HasStencil(hostFormat) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0u);
    }

    void createSampleShadow() {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = hostFormat;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = sampleShadowUsage();
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage sampled depth shadow");
            auto owned = std::make_shared<OwnedImage>(context, image, VK_NULL_HANDLE);
            image = VK_NULL_HANDLE;
            owned->Allocate(context, info, "vkAllocateMemory sampled depth shadow");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, owned->image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory sampled depth shadow");
            sampleShadow = std::move(owned);
            auto* recorder = RecorderFor(context);
            if (recorder != nullptr) recorder->Keep(sampleShadow);
            std::unique_ptr<CommandBatch> batch;
            if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
            const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = sampleShadow->image;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencil(hostFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        } catch (...) {
            if (image != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory != VK_NULL_HANDLE) MemoryAccounting::Free(context, memory);
            throw;
        }
    }

    void refreshSampleShadow(bool drawn, bool stencilRead = false) {
        Require(sampleShadow != nullptr, "native multisample depth shadow was not created");
        if (stencilRead && !sampleShadowStencil) {
            sampleShadowStencil = true;
            sampleShadowWritten = 0u;
        }
        if (!drawn && sampleShadowWritten == written) return;
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(sampleShadow);
        constexpr VkAccessFlags accesses = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, accesses, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT | (sampleShadowStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, sampleShadow->image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, accesses);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        sampleShadowWritten = written;
    }

    std::shared_ptr<Texture> unormStencil(const std::array<std::uint32_t, 13>& key, VkComponentMapping components, VkImageViewType viewType, bool drawn) {
        if (stencilCopy.image == VK_NULL_HANDLE) {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = VK_FORMAT_R8_UNORM;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            try {
                Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &stencilCopy.image), "vkCreateImage stencil copy");
                VkMemoryRequirements requirements{};
                context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, stencilCopy.image, &requirements);
                VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                allocation.allocationSize = requirements.size;
                allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                stencilCopy.memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory stencil copy");
                Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, stencilCopy.image, stencilCopy.memory, 0), "vkBindImageMemory stencil copy");
                stencilCopy.staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(target.extent.width) * target.extent.height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            } catch (...) {
                releaseStencilCopy();
                throw;
            }
        }
        if (drawn || !stencilCopy.filled || stencilCopy.written != written) {
            copyStencil();
            stencilCopy.written = drawn ? 0 : written;
        }
        auto& texture = stencilCopy.textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, stencilCopy.image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, components, viewType);
            texture->MarkRefreshedPerUse();
        }
        return texture;
    }

    void copyStencil() {
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy region{0, 0, 0, {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_GENERAL, stencilCopy.staging->Handle(), 1, &region);
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = stencilCopy.filled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = stencilCopy.image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const VkBufferMemoryBarrier staged{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, stencilCopy.staging->Handle(), 0, VK_WHOLE_SIZE};
        barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &staged, 1, &toTransfer);
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, stencilCopy.staging->Handle(), stencilCopy.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        stencilCopy.filled = true;
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
    }

    void releaseStencilCopy() noexcept {
        stencilCopy.textures.clear();
        stencilCopy.staging.reset();
        if (stencilCopy.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, stencilCopy.image, nullptr);
        if (stencilCopy.memory) MemoryAccounting::Free(context, stencilCopy.memory);
        stencilCopy = {};
    }

    std::uint64_t depthBytes() const {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return DepthSliceBytes(target.extent, d16 ? 2u : 4u);
    }

    std::uint64_t stencilBytes() const {
        return DepthSliceBytes(target.extent, 1u);
    }
    PlaneStorage mirrorPlane;
    PlaneStorage stencilMirror;

    bool CoversPlane(const StorageTexture& storage, bool stencil) const {
        const auto& resource = storage.Descriptor();
        const auto address = stencil ? target.stencilAddress : target.address;
        if (address == 0 || resource.baseAddress != address || resource.width != target.extent.width || resource.height != target.extent.height || resource.baseLevel != 0 || resource.mipCount != 1) return false;
        if (resource.dimension != TextureDimension::k2D && (resource.dimension != TextureDimension::k2DArray || resource.baseArray != 0 || resource.depthOrLastArray != 0)) return false;
        if (target.samples != 1 || (!stencil && hostFormat != target.format)) return false;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return ResolveTextureFormat(resource.format) == (stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT);
    }

    void copyPlane(PlaneStorage& plane, const std::shared_ptr<StorageTexture>& storage, bool stencil, bool toStorage, bool ownBatch = false, const std::vector<VkRect2D>* rects = nullptr) {
        if (rects != nullptr && rects->empty()) return;
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr || ownBatch, "depth surface storage copies need an active recorder");
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const std::uint32_t texelBytes = stencil ? 1u : d16 ? 2u : 4u;
        const auto bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * texelBytes;
        const auto aspect = stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
        const bool direct = !stencil && DirectDepthCopy(context, hostFormat, aspect, storage->ImageFormat());
        const bool passes = toStorage && !stencil && target.samples == 1 && depthToStoragePass(context, hostFormat, storage->ImageFormat());
        if (!direct && !passes && plane.staging == nullptr) plane.staging = std::make_unique<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(storage);
        const std::uint32_t timing = recorder != nullptr ? recorder->BeginGpuTiming(Recorder::CommandClass::Copy) : 0u;
        const VkBufferImageCopy planeRegion{0, 0, 0, {stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        std::vector<VkBufferImageCopy> planeRegions;
        std::size_t moved = bytes;
        if (rects == nullptr) {
            planeRegions.push_back(planeRegion);
        } else {
            moved = 0;
            for (const auto& rect : *rects) {
                auto region = planeRegion;
                region.bufferOffset = (static_cast<VkDeviceSize>(rect.offset.y) * target.extent.width + static_cast<VkDeviceSize>(rect.offset.x)) * texelBytes;
                region.bufferRowLength = target.extent.width;
                region.imageOffset = {rect.offset.x, rect.offset.y, 0};
                region.imageExtent = {rect.extent.width, rect.extent.height, 1};
                planeRegions.push_back(region);
                moved += static_cast<std::size_t>(rect.extent.width) * rect.extent.height * texelBytes;
            }
        }
        if (passes) {
            const std::vector<VkRect2D> whole{{{0, 0}, target.extent}};
            const auto& areas = rects != nullptr ? *rects : whole;
            const auto texels = storage->AtomicView(0, storage->Descriptor().dimension == TextureDimension::k2DArray);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            depthToStoragePassFor(context).Record(commands, depthTransferView(), texels, areas);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            depthTransferPassCount.fetch_add(1, std::memory_order_relaxed);
            if (batch) {
                batch->SubmitAndWait();
                return;
            }
            Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
            recorder->EndGpuTiming(timing, moved);
            return;
        }
        auto colorRegions = planeRegions;
        for (auto& region : colorRegions) region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        const auto count = static_cast<std::uint32_t>(planeRegions.size());
        const auto sourceImage = toStorage ? image : storage->Image();
        const auto destinationImage = toStorage ? storage->Image() : image;
        if (direct) {
            std::vector<VkImageCopy> copies;
            for (const auto& region : planeRegions) {
                const VkImageSubresourceLayers planeLayers{static_cast<VkImageAspectFlags>(aspect), 0, 0, 1};
                const VkImageSubresourceLayers colorLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copies.push_back({toStorage ? planeLayers : colorLayers, region.imageOffset, toStorage ? colorLayers : planeLayers, region.imageOffset, region.imageExtent});
            }
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, sourceImage, VK_IMAGE_LAYOUT_GENERAL, destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, copies.data());
            directDepthCopies.fetch_add(1, std::memory_order_relaxed);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            if (batch) {
                batch->SubmitAndWait();
                return;
            }
            Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
            recorder->EndGpuTiming(timing, moved);
            return;
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, sourceImage, VK_IMAGE_LAYOUT_GENERAL, plane.staging->Handle(), count, toStorage ? planeRegions.data() : colorRegions.data());
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const bool throughUpload = !toStorage && !stencil && DepthUploadImage(context);
        if (throughUpload) {
            const auto& upload = depthUploadImage();
            if (recorder != nullptr) recorder->Keep(upload);
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image = upload->image;
            toTransfer.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencil(hostFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 1, 0, 1};
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, plane.staging->Handle(), upload->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, count, planeRegions.data());
            VkImageMemoryBarrier toSource = toTransfer;
            toSource.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toSource.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
            std::vector<VkImageCopy> copies;
            copies.reserve(planeRegions.size());
            for (const auto& region : planeRegions) copies.push_back({region.imageSubresource, region.imageOffset, region.imageSubresource, region.imageOffset, region.imageExtent});
            context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, upload->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, copies.data());
            depthUploads.fetch_add(1, std::memory_order_relaxed);
        } else {
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, plane.staging->Handle(), destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, toStorage ? colorRegions.data() : planeRegions.data());
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        if (batch) {
            batch->SubmitAndWait();
            return;
        }
        Recorder::CountBarriers(Recorder::CommandClass::Copy, throughUpload ? 5 : 3);
        recorder->EndGpuTiming(timing, moved);
    }

    VkImageView depthTransferView() {
        if (transferView != VK_NULL_HANDLE) return transferView;
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = hostFormat;
        info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &transferView), "vkCreateImageView depth to storage");
        return transferView;
    }

    void release() noexcept {
        depthPlane = {};
        stencilPlane = {};
        releaseStencilCopy();
        depthUpload.reset();
        mirrorPlane = {};
        stencilMirror = {};
        textures.clear();
        if (transferView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, transferView, nullptr);
        transferView = VK_NULL_HANDLE;
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (owned != nullptr) owned.reset();
        else {
            if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) MemoryAccounting::Free(context, memory);
        }
        view = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

struct DepthPlaneSlice {
    VkImage image = VK_NULL_HANDLE;
    std::uint64_t written = 0;
};

bool DepthPlaneReuse() {
    static const bool enabled = std::getenv("APS5_NO_DEPTH_PLANE_REUSE") == nullptr;
    return enabled;
}

class DepthPlaneCopy {
public:
    DepthPlaneCopy(const Context& context, VkExtent2D extent, VkFormat format, std::uint32_t layers) : context(context), extent(extent), format(format), layers(layers), guestLayers(layers), surfaceLayers(layers) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags = layers % 6u == 0 && extent.width == extent.height ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {extent.width, extent.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = layers;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        try {
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth plane copy");
            owned = std::make_shared<OwnedImage>(context, image, VK_NULL_HANDLE);
            owned->Allocate(context, info, "vkAllocateMemory depth plane copy");
            memory = owned->allocation.Memory();
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, owned->allocation.Offset()), "vkBindImageMemory depth plane copy");
            imageBytes = owned->memoryBytes;
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthPlaneCopy() { release(); }
    DepthPlaneCopy(const DepthPlaneCopy&) = delete;
    DepthPlaneCopy& operator=(const DepthPlaneCopy&) = delete;

    std::shared_ptr<Texture> Refresh(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType, std::uint32_t baseLayer) {
        const auto requested = static_cast<std::uint32_t>(slices.size());
        Require(requested != 0 && requested <= layers, "depth plane copy slices exceed its layers");
        Require(baseLayer < requested, "a depth plane copy view starts past the slices it was given");
        const auto geometry = DescribeSurface(resource);
        Require(!geometry.levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
        const bool guestFill = std::any_of(slices.begin(), slices.end(), [](const auto& slice) { return slice.image == VK_NULL_HANDLE; });
        if (guestFill && (geometry.layers != requested || geometry.sliceLinearBytes < sliceBytes() || geometry.mips.empty())) {
            char text[320];
            std::snprintf(text, sizeof(text), "AGC graphics: depth plane copy geometry does not match its layers (copy %ux%u, %u layers, %llu bytes a slice; texture %ux%u guest format %u, dimension %d, tile mode %d, %u layers, %llu bytes a slice, %zu mips)", extent.width, extent.height, layers, static_cast<unsigned long long>(sliceBytes()), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), static_cast<int>(resource.tileMode), geometry.layers, static_cast<unsigned long long>(geometry.sliceLinearBytes), geometry.mips.size());
            throw std::runtime_error(text);
        }
        const auto stagingBytes = std::max<std::size_t>(static_cast<std::size_t>(geometry.sliceLinearBytes), static_cast<std::size_t>(sliceBytes())) * requested;
        std::vector<std::shared_ptr<Buffer>> uploads;
        std::vector<std::uint32_t> uploadLayers;
        std::vector<std::uint32_t> copiedLayers;
        for (std::uint32_t layer = 0; layer < requested; ++layer) {
            auto& held = guestLayers[layer];
            auto& surface = surfaceLayers[layer];
            if (slices[layer].image != VK_NULL_HANDLE) {
                held = {};
                if (slices[layer].written != 0 && surface.image == slices[layer].image && surface.written == slices[layer].written) continue;
                surface = slices[layer];
                copiedLayers.push_back(layer);
                continue;
            }
            surface = {};
            GuestLayer current{resource.baseAddress + geometry.GuestLayerOffset(layer), geometry.layerBytes, resource.tileMode, geometry.thick, GuestMemory::CollectWrites(resource.baseAddress + geometry.GuestLayerOffset(layer), static_cast<std::size_t>(geometry.layerBytes))};
            current.uncommitted = current.generation == 0 && GuestMemory::CommittedRanges(current.address, static_cast<std::size_t>(current.bytes)).empty();
            const bool unchanged = current.uncommitted ? held.uncommitted : held.generation != 0 && GuestMemory::UnchangedSince(current.address, static_cast<std::size_t>(current.bytes), held.generation);
            if (held.address == current.address && held.bytes == current.bytes && held.tileMode == current.tileMode && held.thick == current.thick && !Recorder::SnapshotWriteOverlaps(current.address, static_cast<std::size_t>(current.bytes)) && unchanged) {
                held.generation = current.generation;
                continue;
            }
            auto upload = std::make_shared<Buffer>(context, static_cast<std::size_t>(geometry.layerBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            GuestMemory::ReadCommitted(current.address, upload->Bytes().first(static_cast<std::size_t>(geometry.layerBytes)));
            uploads.push_back(std::move(upload));
            uploadLayers.push_back(layer);
            held = current;
        }
        if (!copiedLayers.empty() || !uploads.empty()) {
            if (staging == nullptr || stagingBytes > staging->Size()) staging = std::make_shared<DeviceBuffer>(context, stagingBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            record(slices, resource, geometry, copiedLayers, uploads, uploadLayers, staging);
        }
        const auto viewLayers = viewType == VK_IMAGE_VIEW_TYPE_2D ? 1u : requested - baseLayer;
        const std::array<std::uint32_t, 7> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a), static_cast<std::uint32_t>(viewType), baseLayer, viewLayers};
        auto& texture = textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, image, format, aspect(), components, viewType, baseLayer, owned, 0, viewLayers);
            if (!DepthPlaneReuse()) texture->MarkRefreshedPerUse();
        }
        return texture;
    }

    const Context context;
    const VkExtent2D extent;
    const VkFormat format;
    const std::uint32_t layers;
    VkDeviceSize imageBytes = 0;
    std::uint64_t lastUse = 0;
    std::uint64_t lastPresent = 0;
    VkDeviceSize HeldBytes() const { return imageBytes + (staging != nullptr ? staging->Size() : 0u); }

private:
    struct GuestLayer {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        TextureTileMode tileMode = TextureTileMode::kLinear;
        bool thick = false;
        std::uint64_t generation = 0;
        bool uncommitted = false;
    };

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> owned;
    std::shared_ptr<DeviceBuffer> staging;
    std::map<std::array<std::uint32_t, 7>, std::shared_ptr<Texture>> textures;
    std::vector<GuestLayer> guestLayers;
    std::vector<DepthPlaneSlice> surfaceLayers;
    bool filled = false;

    void record(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, const SurfaceGeometry& geometry, std::span<const std::uint32_t> copiedLayers, std::span<const std::shared_ptr<Buffer>> uploads, std::span<const std::uint32_t> uploadLayers, const std::shared_ptr<DeviceBuffer>& staging) {
        if (!uploads.empty()) {
            Require(context.detiler != nullptr, "depth plane copy requires a texture detiler");
            context.detiler->BeginBatch();
        }
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        std::vector<VkBufferImageCopy> regions;
        const auto& mip = geometry.mips.front();
        const auto region = [&](std::uint32_t layer, VkImageAspectFlags aspectMask, std::uint32_t imageLayer) {
            VkBufferImageCopy copy{};
            copy.bufferOffset = geometry.LinearLayerOffset(layer) + mip.linearOffset;
            copy.bufferRowLength = mip.pitchBytes / (format == VK_FORMAT_D32_SFLOAT ? 4u : 2u);
            copy.imageSubresource = {aspectMask, 0, imageLayer, 1};
            copy.imageExtent = {extent.width, extent.height, 1};
            return copy;
        };
        for (const auto layer : copiedLayers) {
            const auto copy = region(layer, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, slices[layer].image, VK_IMAGE_LAYOUT_GENERAL, staging->Handle(), 1, &copy);
            regions.push_back(region(layer, aspect(), layer));
        }
        if (!uploads.empty()) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            for (std::size_t upload = 0; upload < uploads.size(); ++upload) {
                const auto layer = uploadLayers[upload];
                context.detiler->Dispatch(commands, resource.tileMode, format == VK_FORMAT_D32_SFLOAT ? 4u : 2u, uploads[upload]->Handle(), 0, staging->Handle(), geometry.LinearLayerOffset(layer), geometry.mips.front(), false, layer, geometry.thick);
                regions.push_back(region(layer, aspect(), layer));
            }
            if (recorder != nullptr) {
                for (const auto& upload : uploads) recorder->Keep(upload, upload->Bytes().size());
            }
        }
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = filled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {aspect(), 0, 1, 0, layers};
        const VkBufferMemoryBarrier staged{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, staging->Handle(), 0, VK_WHOLE_SIZE};
        barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &staged, 1, &toTransfer);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        filled = true;
        if (batch) batch->SubmitAndWait();
        else {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 3);
            recorder->Keep(staging, staging->Size());
            if (owned != nullptr) recorder->Keep(owned);
        }
    }

    VkDeviceSize sliceBytes() const {
        return static_cast<VkDeviceSize>(extent.width) * extent.height * (format == VK_FORMAT_D32_SFLOAT ? 4u : 2u);
    }

    VkImageAspectFlags aspect() const {
        return format == VK_FORMAT_R16_UINT ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    void release() noexcept {
        textures.clear();
        staging.reset();
        if (owned != nullptr) owned.reset();
        else {
            if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) MemoryAccounting::Free(context, memory);
        }
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat>, std::unique_ptr<DepthPlaneCopy>>& planeCopies() {
    static auto* copies = new std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat>, std::unique_ptr<DepthPlaneCopy>>();
    return *copies;
}

std::uint64_t& planeCopyClock() {
    static std::uint64_t clock = 0;
    return clock;
}

void evictPlaneCopies(const std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat>& keep) {
    static const VkDeviceSize budget = [] {
        const char* text = std::getenv("APS5_DEPTH_PLANE_COPY_MIB");
        return static_cast<VkDeviceSize>(text != nullptr ? std::strtoull(text, nullptr, 10) : 256ull) << 20u;
    }();
    if (budget == 0) return;
    auto& copies = planeCopies();
    VkDeviceSize total = 0;
    for (const auto& entry : copies) total += entry.second->HeldBytes();
    const auto present = Recorder::Presents();
    const auto ceiling = budget + std::min(budget, std::numeric_limits<VkDeviceSize>::max() - budget);
    while (total > budget) {
        const bool over = total > ceiling;
        auto oldest = copies.end();
        for (auto it = copies.begin(); it != copies.end(); ++it) {
            if (it->first == keep || (!over && it->second->lastPresent + 1u >= present)) continue;
            if (oldest == copies.end() || it->second->lastUse < oldest->second->lastUse) oldest = it;
        }
        if (oldest == copies.end()) break;
        total -= oldest->second->HeldBytes();
        copies.erase(oldest);
    }
}

struct LayeredSlice {
    DepthSurface* surface;
    std::uint32_t layer;
};

class LayeredDepthStorage {
public:
    LayeredDepthStorage(const Context& context, const GuestTextureResource& resource, const DepthTarget& base, std::uint32_t bytesPerTexel, std::uint32_t firstLayer, std::uint32_t layers) : context(context), base(base), extent(base.extent), bytesPerTexel(bytesPerTexel), firstLayer(firstLayer), layers(layers) {
        const auto bytes = DescribeSurface(resource).guestBytes;
        layerPitchBytes = DescribeSurface(resource).GuestLayerOffset(1);
        StorageTexture::FlushPending(resource.baseAddress, static_cast<std::size_t>(bytes), nullptr, "storage image creation", PublishScope::None);
        storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        Require(storage->ImageLayers() == layers, "a layered depth storage view's image does not hold its layers");
        if (!DirectDepthCopy(context, base.format, VK_IMAGE_ASPECT_DEPTH_BIT, storage->ImageFormat())) staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(sliceBytes()) * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    void Copy(std::span<const LayeredSlice> slices, bool toStorage) {
        if (slices.empty()) return;
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "depth surface storage copies need an active recorder");
        const auto commands = recorder->Commands();
        recorder->Keep(storage);
        const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
        std::vector<VkBufferImageCopy> storageRegions;
        const auto region = [&](std::uint32_t layer, VkImageAspectFlags aspect, std::uint32_t imageLayer) {
            VkBufferImageCopy copy{};
            copy.bufferOffset = sliceBytes() * layer;
            copy.imageSubresource = {aspect, 0, imageLayer, 1};
            copy.imageExtent = {extent.width, extent.height, 1};
            return copy;
        };
        for (const auto& slice : slices) storageRegions.push_back(region(slice.layer, VK_IMAGE_ASPECT_COLOR_BIT, slice.layer));
        if (DirectDepthCopy(context, base.format, VK_IMAGE_ASPECT_DEPTH_BIT, storage->ImageFormat())) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            for (const auto& slice : slices) {
                const VkImageSubresourceLayers depthLayers{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
                const VkImageSubresourceLayers colorLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, slice.layer, 1};
                const VkImageCopy copy{toStorage ? depthLayers : colorLayers, {0, 0, 0}, toStorage ? colorLayers : depthLayers, {0, 0, 0}, {extent.width, extent.height, 1}};
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, toStorage ? slice.surface->image : storage->Image(), VK_IMAGE_LAYOUT_GENERAL, toStorage ? storage->Image() : slice.surface->image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
                directDepthCopies.fetch_add(1, std::memory_order_relaxed);
            }
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
            recorder->EndGpuTiming(timing, sliceBytes() * slices.size());
            return;
        }
        if (staging == nullptr) staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(sliceBytes()) * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copyToBuffer = context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer");
        const auto copyToImage = context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage");
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        if (toStorage) {
            for (const auto& slice : slices) {
                const auto copy = region(slice.layer, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
                copyToBuffer(commands, slice.surface->image, VK_IMAGE_LAYOUT_GENERAL, staging->Handle(), 1, &copy);
            }
        } else {
            copyToBuffer(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, staging->Handle(), static_cast<std::uint32_t>(storageRegions.size()), storageRegions.data());
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        if (toStorage) {
            copyToImage(commands, staging->Handle(), storage->Image(), VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(storageRegions.size()), storageRegions.data());
        } else {
            for (const auto& slice : slices) {
                const auto copy = region(slice.layer, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
                copyToImage(commands, staging->Handle(), slice.surface->image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
            }
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
        recorder->EndGpuTiming(timing, sliceBytes() * slices.size());
    }

    const Context context;
    const DepthTarget base;
    const VkExtent2D extent;
    const std::uint32_t bytesPerTexel;
    const std::uint32_t firstLayer;
    const std::uint32_t layers;
    std::uint64_t layerPitchBytes = 0;
    std::shared_ptr<StorageTexture> storage;

private:
    std::unique_ptr<DeviceBuffer> staging;

    VkDeviceSize sliceBytes() const {
        return static_cast<VkDeviceSize>(extent.width) * extent.height * bytesPerTexel;
    }
};

std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, int, std::uint32_t>, std::unique_ptr<LayeredDepthStorage>>& layeredStorages() {
    static auto* storages = new std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, int, std::uint32_t>, std::unique_ptr<LayeredDepthStorage>>();
    return *storages;
}

bool sameSurface(const DepthTarget& a, const DepthTarget& b) {
    return a.address == b.address && a.stencilAddress == b.stencilAddress && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.format == b.format && a.samples == b.samples && (a.samples == 1u || (a.htileAddress == b.htileAddress && a.htileStencil == b.htileStencil && a.depthSwizzle == b.depthSwizzle && a.pipeBankXor == b.pipeBankXor && a.firstSlice == b.firstSlice && a.lastSlice == b.lastSlice && a.iterate256 == b.iterate256 && a.htilePipeAligned == b.htilePipeAligned && a.htileVrsEncoding == b.htileVrsEncoding && a.htileDepthOnly == b.htileDepthOnly));
}

bool sameLayoutWithin(const DepthTarget& owner, const DepthTarget& target) {
    if (owner.samples == 1u || owner.htileAddress != 0u || owner.depthSwizzle != 24u || target.extent.width > owner.extent.width || target.extent.height > owner.extent.height) return false;
    auto resized = target;
    resized.extent = owner.extent;
    if (!sameSurface(owner, resized)) return false;
    const auto depthBytes = owner.format == VK_FORMAT_D16_UNORM || owner.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2u : 4u;
    for (const auto elementBytes : {depthBytes, owner.stencilAddress != 0u ? 1u : depthBytes}) {
        const MultisampleTextureLayout held(TextureTileMode::kZ64KBX, elementBytes, owner.samples, owner.extent.width, owner.extent.height, 1u, owner.pipeBankXor);
        const MultisampleTextureLayout bound(TextureTileMode::kZ64KBX, elementBytes, owner.samples, target.extent.width, target.extent.height, 1u, owner.pipeBankXor);
        if (held.Pitch() != bound.Pitch() || held.PaddedHeight() != bound.PaddedHeight() || held.Bytes() != bound.Bytes()) return false;
    }
    return true;
}

bool holdsTarget(const DepthTarget& owner, const DepthTarget& target) {
    return sameSurface(owner, target) || sameLayoutWithin(owner, target);
}

std::recursive_mutex& surfacesMutex() {
    static std::recursive_mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<DepthSurface>>& surfaces() {
    static auto* list = new std::vector<std::unique_ptr<DepthSurface>>();
    return *list;
}

struct PlaneOwner {
    DepthSurface* surface = nullptr;
    std::uint64_t serial = 0;
};

using PlaneOwners = std::map<std::pair<VkDevice, std::uint64_t>, PlaneOwner>;

std::uint64_t planeBinds = 0;

PlaneOwners& depthOwners() {
    static auto* owners = new PlaneOwners();
    return *owners;
}

PlaneOwners& stencilOwners() {
    static auto* owners = new PlaneOwners();
    return *owners;
}

void takePlane(DepthSurface& surface, PlaneOwners& owners, std::uint64_t address, VkImageAspectFlagBits aspect, bool created, VkImageAspectFlags clearAspects) {
    auto& entry = owners[{surface.context.device, address}];
    auto* owner = entry.surface;
    if (owner != nullptr && owner->active->load(std::memory_order_acquire) && owner != &surface && owner->target.format == surface.target.format && owner->target.samples == surface.target.samples && owner->target.extent.width == surface.target.extent.width && owner->target.extent.height == surface.target.extent.height && (!created || (clearAspects & aspect) == 0)) {
        if ((owner->pendingClear & aspect) != 0) {
            if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
                surface.depthFill = owner->depthFill;
                surface.depthMark = std::max(surface.depthMark, owner->depthMark);
            } else {
                surface.stencilFill = owner->stencilFill;
                surface.stencilMark = std::max(surface.stencilMark, owner->stencilMark);
            }
            if (!created || (aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? surface.depthFill.has_value() : surface.stencilFill.has_value())) surface.pendingClear |= aspect;
        } else {
            surface.CopyAspectFrom(*owner, aspect);
        }
    }
    entry = {&surface, ++planeBinds};
}

void takePlanes(DepthSurface& surface, bool created, VkImageAspectFlags clearAspects) {
    takePlane(surface, depthOwners(), surface.target.address, VK_IMAGE_ASPECT_DEPTH_BIT, created, clearAspects);
    if (surface.target.stencilAddress != 0) takePlane(surface, stencilOwners(), surface.target.stencilAddress, VK_IMAGE_ASPECT_STENCIL_BIT, created, clearAspects);
}

DepthSurface* depthSlice(VkDevice device, std::uint64_t address, const DepthTarget& base) {
    if (const auto found = depthOwners().find({device, address}); found != depthOwners().end()) {
        const auto& target = found->second.surface->target;
        if (found->second.surface->active->load(std::memory_order_acquire) && target.extent.width == base.extent.width && target.extent.height == base.extent.height && target.format == base.format) return found->second.surface;
    }
    const auto& list = surfaces();
    const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && surface->context.device == device && !surface->retired && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format; });
    return slice == list.rend() ? nullptr : slice->get();
}

DepthSurface* planeSurface(VkDevice device, std::uint64_t address) {
    const auto depth = depthOwners().find({device, address});
    const auto stencil = stencilOwners().find({device, address});
    const bool hasDepth = depth != depthOwners().end() && depth->second.surface->active->load(std::memory_order_acquire);
    const bool hasStencil = stencil != stencilOwners().end() && stencil->second.surface->active->load(std::memory_order_acquire);
    if (hasDepth && (!hasStencil || depth->second.serial > stencil->second.serial)) return depth->second.surface;
    if (hasStencil) return stencil->second.surface;
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == device && !surface->retired && (surface->target.address == address || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == address));
    });
    return found == list.rend() ? nullptr : found->get();
}

void settlePendingHtileClear(const Context& context, std::uint64_t address) {
    if (Recorder::InCompletion()) return;
    auto* recorder = RecorderFor(context);
    if (recorder == nullptr) return;
    std::pair<std::uint64_t, std::size_t> metadata;
    {
        std::lock_guard lock(surfacesMutex());
        const auto* surface = planeSurface(context.device, address);
        if (surface == nullptr || !surface->samplingActive || surface->clearGeneration == 0u || !surface->clearCompletionQueued || !surface->sampling.refusal.empty()) return;
        const auto& range = surface->sampling.initial[1];
        metadata = {range.address, range.bytes};
    }
    if (recorder->PendingWriteOverlaps(metadata.first, metadata.second)) recorder->SyncThrough(metadata.first, metadata.second);
}

std::map<std::uint64_t, std::size_t>& clearedHtiles() {
    static auto* cleared = new std::map<std::uint64_t, std::size_t>();
    return *cleared;
}

std::map<std::uint64_t, std::size_t>::iterator findClearedHtile(const DepthTarget& target) {
    auto& cleared = clearedHtiles();
    if (target.htileAddress == 0) return cleared.end();
    return std::find_if(cleared.begin(), cleared.end(), [&](const auto& entry) { return HtileFillCovers(target.htileAddress, target.extent, entry.first, entry.second); });
}

bool takeClearedHtile(const DepthTarget& target) {
    const auto found = findClearedHtile(target);
    if (found == clearedHtiles().end()) return false;
    clearedHtiles().erase(found);
    return true;
}

struct ExpandWrite {
    const DepthSurface* source = nullptr;
    std::uint64_t written = 0;
    std::weak_ptr<StorageTexture> destination;
    std::uint64_t version = 0;
};

using ExpandKey = std::pair<VkDevice, std::uint64_t>;

std::map<ExpandKey, ExpandWrite>& expandWrites() {
    static auto* writes = new std::map<ExpandKey, ExpandWrite>();
    return *writes;
}

std::map<ExpandKey, std::size_t>& unstoredExpands() {
    static auto* writes = new std::map<ExpandKey, std::size_t>();
    return *writes;
}

std::atomic<std::uint64_t> expandsRun{0};
std::atomic<std::uint64_t> expandsSkipped{0};
std::atomic<std::uint64_t> expandsStored{0};

DepthSurface* drawnDepth = nullptr;

std::atomic<std::uint64_t> surfacesSerial{0};
std::atomic<std::uint64_t> surfacesReleased{0};

void retire(DepthSurface& surface);

void releaseRetired(const Context& context) {
    auto* recorder = Recorder::Active();
    if (recorder != nullptr && recorder->Device() != context.device) return;
    auto& list = surfaces();
    for (auto it = list.begin(); it != list.end();) {
        auto& surface = **it;
        const bool nativeReleased = surface.target.samples != 1u && !surface.active->load(std::memory_order_acquire) && !surface.hostWritten->load(std::memory_order_acquire);
        if ((!surface.retired && !nativeReleased) || surface.context.device != context.device || (surface.target.samples != 1u && !nativeReleased) || surface.SampledElsewhere()) {
            ++it;
            continue;
        }
        if (!surface.retired) retire(surface);
        if (drawnDepth == &surface) drawnDepth = nullptr;
        std::shared_ptr<DepthSurface> released = std::move(*it);
        it = list.erase(it);
        surfacesReleased.fetch_add(1, std::memory_order_release);
        if (recorder != nullptr) recorder->Keep(std::move(released));
    }
}

std::uint32_t expandedHtileWord(std::uint32_t word, bool stencilInHtile, VkImageAspectFlags cleared) {
    if (!stencilInHtile) return (cleared & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 ? 0xfffc000fu : word;
    if ((cleared & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) word = (word & 0x3f0u) | 0xfffff00fu;
    if ((cleared & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) word |= 0x3f0u;
    return word;
}

void DepthSurface::PublishToGuest() {
    Require(!Recorder::InCompletion(), "native multisample depth publication inside a GPU completion is not implemented");
    Require(!publicationInProgress, "recursive native multisample depth publication is not implemented");
    Require(!publicationFailed, "a previous native multisample depth publication store did not complete");
    struct PublicationScope {
        bool& inProgress;
        ~PublicationScope() { inProgress = false; }
    } scope{publicationInProgress};
    publicationInProgress = true;
    if (publicationFailure) std::rethrow_exception(publicationFailure);
    const bool htile = !uniformPlaneTarget(target);
    Require(target.format == VK_FORMAT_D32_SFLOAT_S8_UINT && hostFormat == target.format && publication.size() == (htile ? 3u : 2u), "sample-preserving guest publication of native multisample depth is not implemented for this owner");
    const std::size_t stencilPlane = publication.size() - 1u;
    Require(sampling.refusal.empty(), sampling.refusal);
    RequireStencilProven();
    Require(!clearPending && clearGeneration == 0u && stencilClearGeneration == 0u && pendingClear == 0u, "native multisample depth publication with an unapplied clear is not implemented");
    Require(sampleable && context.detiler != nullptr, "sample-preserving guest publication of native multisample depth requires sampled readback support");
    const auto lease = GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
    std::array<std::vector<GuestMemory::UnchangedQuery>, 3> live;
    {
        std::shared_lock guardLock(depthGuardMutex());
        for (std::size_t index = 0u; index < publication.size(); ++index) {
            const auto& plane = publication[index];
            const auto found = std::ranges::find_if(depthGuards(), [&](const auto& range) { return range.hostWritten == hostWritten && range.address == plane.range.address && range.bytes == plane.range.bytes; });
            Require(found != depthGuards().end(), "native multisample depth publication lost its guarded plane");
            live[index] = found->live;
        }
    }
    const auto supersedes = [&] {
        return std::array{sampling.depthAlias && unchangedAlias(*sampling.depthAlias, sampling.initial[0]), false, sampling.stencilAlias && unchangedAlias(*sampling.stencilAlias, sampling.initial[2])};
    };
    const auto superseded = supersedes();
    for (std::size_t index = 0u; index < live.size(); ++index) {
        for (const auto& part : live[index]) {
            Require(GuestMemory::Accessible(reinterpret_cast<const void*>(part.address), part.bytes, true) && GuestMemory::Watched(part.address, part.bytes), "native multisample depth publication requires watched accessible guest memory");
            Require(superseded[index] || !MultisampleColorTargetOver(part.address, part.bytes), "native multisample depth publication over a resident colour target is not implemented");
            const std::pair<std::uint64_t, std::uint64_t> range{part.address, part.address + part.bytes};
            Require(!StorageTexture::AnyPendingUnitsOverlap(std::span(&range, 1u)), "native multisample depth publication over pending storage image writes is not implemented");
        }
    }
    for (const auto& parts : live) {
        for (const auto& part : parts) GuestMemory::FlushGpuWrites(part.address, part.bytes);
    }
    if (auto* recorder = RecorderFor(context)) recorder->Sync();
    Require(active->load(std::memory_order_acquire), "the native multisample depth owner was released during publication");
    if (useSampleShadow) {
        if (sampleShadow == nullptr) createSampleShadow();
        refreshSampleShadow(true, true);
        if (auto* recorder = RecorderFor(context)) recorder->Sync();
    }
    std::array<std::vector<std::byte>, 3> result;
    for (const std::size_t index : {std::size_t{0u}, stencilPlane}) {
        const auto elementBytes = index == 0u ? 4u : 1u;
        const auto aspect = index == 0u ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
        const MultisampleTextureLayout layout(TextureTileMode::kZ64KBX, elementBytes, target.samples, target.extent.width, target.extent.height, 1u, target.pipeBankXor);
        Require(layout.Bytes() == publication[index].range.bytes, "native multisample depth publication layout differs from its guarded range");
        const auto linear = readBackSamples(aspect, layout.LinearBytes() / elementBytes);
        result[index] = publication[index].Expand();
        layout.Tile(linear->Bytes().first(static_cast<std::size_t>(layout.LinearBytes())), result[index]);
    }
    if (htile) {
        result[1] = publication[1].Expand();
        for (std::size_t offset = 0u; offset < result[1].size(); offset += 4u) {
            std::uint32_t word;
            std::memcpy(&word, result[1].data() + offset, 4u);
            word = expandedHtileWord(word, !target.htileDepthOnly, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
            std::memcpy(result[1].data() + offset, &word, 4u);
        }
    }
    std::array<std::vector<std::byte>, 3> current;
    std::array<bool, 3> tookGuest{};
    std::vector<GuestMemory::UnchangedQuery> observed;
    for (std::size_t index = 0u; index < publication.size(); ++index) {
        const auto& plane = publication[index];
        current[index].resize(plane.range.bytes);
        for (const auto& part : live[index]) {
            observed.push_back({part.address, part.bytes, GuestMemory::CollectWritesUncached(part.address, part.bytes)});
            const auto begin = static_cast<std::size_t>(part.address - plane.range.address);
            const auto end = begin + part.bytes;
            GuestMemory::CopyGuestBytes(current[index].data() + begin, reinterpret_cast<const void*>(part.address), part.bytes);
            for (auto first = begin; first < end;) {
                const auto address = plane.range.address + first;
                const auto bytes = std::min<std::size_t>(4096u - (address & 4095u), end - first);
                if (!GuestMemory::UnchangedSinceLastCollect(address, bytes, plane.range.generation)) {
                    if (GuestMemory::StoredAllSince_nid_no_patch(address, bytes, plane.range.generation)) {
                        if (!std::equal(current[index].begin() + first, current[index].begin() + first + bytes, result[index].begin() + first)) tookGuest[index] = true;
                        std::copy_n(current[index].begin() + first, bytes, result[index].begin() + first);
                    } else for (std::size_t offset = first; offset < first + bytes; ++offset) {
                        if (current[index][offset] != plane.Byte(offset) || current[index][offset] == result[index][offset] || GuestMemory::StoredAllSince_nid_no_patch(plane.range.address + offset, 1u, plane.range.generation)) {
                            if (current[index][offset] != result[index][offset]) tookGuest[index] = true;
                            result[index][offset] = current[index][offset];
                        } else Require(GuestMemory::UnchangedSinceLastCollect(plane.range.address + offset, 1u, plane.range.generation), "native multisample depth publication cannot distinguish an unwritten byte from a same-value guest store");
                    }
                }
                first += bytes;
            }
        }
    }
    if (const auto hook = depthPublicationHook.load(std::memory_order_acquire)) hook(DepthPublicationStage::GuestCopied);
    for (const auto& range : observed) GuestMemory::CollectWritesUncached(range.address, range.bytes);
    for (const auto& range : observed) Require(GuestMemory::UnchangedSinceCollected(range.address, range.bytes, range.generation), "guest writes raced native multisample depth publication");
    Require(GuestMemory::UnchangedSinceAll(observed), "guest writes raced native multisample depth publication");
    Require(active->load(std::memory_order_acquire) && !clearPending && clearGeneration == 0u && stencilClearGeneration == 0u && pendingClear == 0u, "native multisample depth ownership changed during publication");
    Require(supersedes() == superseded, "native multisample depth publication over a resident colour target is not implemented");
    publicationFailed = true;
    if (const auto hook = depthPublicationHook.load(std::memory_order_acquire)) hook(DepthPublicationStage::BeforeCommit);
    auto seen = observed.begin();
    for (std::size_t index = 0u; index < publication.size(); ++index) {
        for (const auto& part : live[index]) {
            const auto offset = static_cast<std::size_t>(part.address - publication[index].range.address);
            const auto lost = GuestMemory::WriteChangedIfHeld(part.address, std::span<const std::byte>(result[index]).subspan(offset, part.bytes), std::span<const std::byte>(current[index]).subspan(offset, part.bytes), (seen++)->generation);
            Require(lost == 0u, "guest writes raced native multisample depth publication");
        }
        if (const auto hook = depthPublicationHook.load(std::memory_order_acquire)) hook(DepthPublicationStage::PlaneCommitted);
    }
    publicationFailed = false;
    for (std::size_t index = 0u; index < publication.size(); ++index) {
        const auto& range = sampling.initial[index];
        const bool whole = live[index].size() == 1u && live[index][0].address == range.address && live[index][0].bytes == range.bytes;
        if (tookGuest[index] || !whole) continue;
        const auto generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        std::vector<std::byte> committed(range.bytes);
        GuestMemory::CopyGuestBytes(committed.data(), reinterpret_cast<const void*>(range.address), range.bytes);
        if (committed == result[index]) sampling.initial[index].generation = generation;
    }
    hostWritten->store(false, std::memory_order_release);
    publication.clear();
    if (superseded[0]) RetireMultisampleColorTarget(sampling.depthAlias->active);
    if (superseded[2]) RetireMultisampleColorTarget(sampling.stencilAlias->active);
}

void publishWrittenDepth(std::span<const std::shared_ptr<std::atomic<bool>>> written) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& flag : written) {
        if (!flag->load(std::memory_order_acquire)) continue;
        const auto found = std::ranges::find_if(surfaces(), [&](const auto& surface) { return surface->hostWritten == flag && surface->active->load(std::memory_order_acquire); });
        if (found == surfaces().end()) continue;
        (*found)->PublishToGuest();
    }
}

std::mutex& htileReadsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::uint64_t, std::uint64_t>& htileReads() {
    static auto* reads = new std::map<std::uint64_t, std::uint64_t>();
    return *reads;
}

using UntrackedHtileStamp = std::array<std::uint64_t, 5>;

struct UntrackedHtileScan {
    std::size_t bytes = 0;
    bool stencil = false;
    UntrackedHtileStamp stamp{};
};

std::map<std::uint64_t, UntrackedHtileScan>& untrackedHtileScans() {
    static auto* scans = new std::map<std::uint64_t, UntrackedHtileScan>();
    return *scans;
}

std::optional<UntrackedHtileStamp> untrackedHtileStamp(const Recorder* recorder, std::uint64_t address, std::size_t bytes) {
    const auto epoch = GuestMemory::CollectEpoch();
    if (epoch == 0) return std::nullopt;
    GuestMemory::FlushGpuWrites(address, bytes);
    const auto hint = GuestMemory::UntrackedHint(address, bytes);
    const UntrackedHtileStamp stamp{hint != 0, hint != 0 ? hint : epoch, GuestMemory::NewestStamp(address, bytes), GuestMemory::ForgetSerial(), recorder != nullptr ? recorder->NewestWriteNote(address, bytes) : 0};
    if ((stamp[3] & 1u) != 0) return std::nullopt;
    return stamp;
}

void forgetHtileReads(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(htileReadsMutex());
    auto& reads = htileReads();
    reads.erase(reads.lower_bound(address), reads.lower_bound(address + bytes));
    auto& scans = untrackedHtileScans();
    scans.erase(scans.lower_bound(address), scans.lower_bound(address + bytes));
}

bool RoutesToNativeDepthOwner(const DepthTarget& target) {
    if (target.samples != 1u) return true;
    if (target.htileAddress == 0 || !anyDepthGuard().load(std::memory_order_acquire)) return false;
    const auto bytes = static_cast<std::uint64_t>(HtileMetadataBytes(target.extent));
    if (!mayOverlapDepthGuard(target.htileAddress, bytes)) return false;
    std::shared_lock lock(depthGuardMutex());
    return std::ranges::any_of(depthGuards(), [&](const auto& range) { return range.active->load(std::memory_order_acquire) && target.htileAddress < range.address + range.bytes && range.address < target.htileAddress + bytes; });
}

std::optional<std::uint32_t> htileAfterPendingFills(const Recorder& recorder, std::uint64_t address, std::size_t bytes) {
    auto writes = recorder.PendingWritesOver(address, bytes);
    std::sort(writes.begin(), writes.end(), [](const Recorder::PendingWrite& a, const Recorder::PendingWrite& b) { return a.note > b.note; });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwritten{{address, address + bytes}};
    std::optional<std::uint32_t> landed;
    for (const auto& write : writes) {
        if (unwritten.empty()) break;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> rest;
        bool covered = false;
        for (const auto& [from, to] : unwritten) {
            if (write.end <= from || to <= write.begin) {
                rest.emplace_back(from, to);
                continue;
            }
            covered = true;
            if (from < write.begin) rest.emplace_back(from, write.begin);
            if (write.end < to) rest.emplace_back(write.end, to);
        }
        unwritten = std::move(rest);
        if (!covered) continue;
        if (write.word < 0 || (landed.has_value() && *landed != static_cast<std::uint32_t>(write.word))) return std::nullopt;
        landed = static_cast<std::uint32_t>(write.word);
    }
    if (!landed.has_value()) return std::nullopt;
    for (const auto& [from, to] : unwritten) {
        if (from % 4u != 0 || to % 4u != 0) return std::nullopt;
        const auto* words = reinterpret_cast<const volatile std::uint32_t*>(from);
        for (std::uint64_t index = 0; index < (to - from) / 4u; ++index) {
            if (words[index] != *landed) return std::nullopt;
        }
    }
    return landed;
}

struct HtileMaskWrite {
    HtileMaskClear clear;
    std::uint64_t note;
    std::uint64_t recorderId;
    VkDevice device;
};

std::mutex& htileMasksMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<HtileMaskWrite>& htileMasks() {
    static auto* masks = new std::vector<HtileMaskWrite>();
    return *masks;
}

std::optional<std::pair<VkImageAspectFlags, std::uint32_t>> htileAfterMaskClear(const Recorder& recorder, const DepthTarget& target, std::size_t bytes) {
    const auto newest = recorder.NewestWriteNote(target.htileAddress, bytes);
    if (newest == 0) return std::nullopt;
    std::optional<HtileMaskClear> clear;
    {
        std::lock_guard lock(htileMasksMutex());
        for (const auto& write : htileMasks()) {
            if (write.recorderId == recorder.Id() && write.note == newest && write.clear.address <= target.htileAddress && write.clear.address + write.clear.bytes >= target.htileAddress + bytes) clear = write.clear;
        }
    }
    if (!clear.has_value() || (clear->mask & 0xfu) != 0 || (clear->value & 0xfu) != 0) return std::nullopt;
    if (target.firstSlice != 0 && clear->mask != 0) return std::nullopt;
    if (!target.htileStencil) return std::pair<VkImageAspectFlags, std::uint32_t>{VK_IMAGE_ASPECT_DEPTH_BIT, 0xfffc000fu};
    if ((clear->mask & 0x300u) != 0) return std::nullopt;
    if (((clear->value >> 8u) & 0x3u) == 0) return std::pair<VkImageAspectFlags, std::uint32_t>{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0xfffff3ffu};
    if ((clear->mask & 0x3f0u) != 0) return std::nullopt;
    return std::pair<VkImageAspectFlags, std::uint32_t>{VK_IMAGE_ASPECT_DEPTH_BIT, (clear->value & 0x3f0u) | 0xfffff00fu};
}

bool storeHtileOnGpu(const Context& context, Recorder& recorder, std::uint64_t address, std::size_t bytes, std::uint32_t word) {
    if (address % 4u != 0 || bytes % 4u != 0) return false;
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) return false;
    recorder.FlushKeyStoresOverlapping(address, bytes);
    recorder.FlushStoresOverlapping(address, bytes);
    const auto commands = recorder.Commands();
    const auto timing = recorder.BeginGpuTiming(Recorder::CommandClass::Fill);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::Fill);
    context.Resolved(&DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer")(commands, import->buffer, address - import->base, bytes, word);
    constexpr VkAccessFlags filledAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, filledAccess);
    Recorder::CountBarriers(Recorder::CommandClass::Fill);
    recorder.EndGpuTiming(timing, bytes);
    recorder.MarkCovered(filledAccess);
    recorder.NotePendingWordFill(address, bytes, word);
    GuestMemory::MarkWritten(address, bytes);
    return true;
}

std::mutex& copyDestinationsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>>& copyDestinations() {
    static auto* ranges = new std::vector<std::pair<std::uint64_t, std::uint64_t>>();
    return *ranges;
}

bool copiedOver(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(copyDestinationsMutex());
    return std::any_of(copyDestinations().begin(), copyDestinations().end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

void forgetCopiesInside(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(copyDestinationsMutex());
    std::erase_if(copyDestinations(), [&](const auto& range) { return address <= range.first && range.second <= address + bytes; });
}

VkImageAspectFlags storeBackClearedHtile(const Context& context, const DepthTarget& target) {
    if (target.htileAddress == 0) return 0;
    const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(target.extent));
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(target.htileAddress), bytes, true)) return 0;
    if (copiedOver(target.htileAddress, bytes)) return 0;
    const auto collected = GuestMemory::CollectWrites(target.htileAddress, bytes);
    auto* recorder = RecorderFor(context);
    const bool pending = recorder != nullptr && recorder->PendingWriteOverlaps(target.htileAddress, bytes);
    const auto untracked = !pending && collected == 0 ? untrackedHtileStamp(recorder, target.htileAddress, bytes) : std::nullopt;
    if (!pending) {
        std::lock_guard lock(htileReadsMutex());
        const auto found = htileReads().find(target.htileAddress);
        if (found != htileReads().end() && found->second != 0 && GuestMemory::UnchangedSince(target.htileAddress, bytes, found->second)) return 0;
        if (untracked) {
            const auto scanned = untrackedHtileScans().find(target.htileAddress);
            if (scanned != untrackedHtileScans().end() && scanned->second.bytes == bytes && scanned->second.stencil == target.htileStencil && scanned->second.stamp == *untracked) return 0;
        }
    }
    static const bool pendingFills = std::getenv("APS5_NO_PENDING_HTILE_FILLS") == nullptr;
    if (pending && pendingFills) {
        std::lock_guard gpu(GuestMemory::GpuMutex());
        if (const auto landed = htileAfterPendingFills(*recorder, target.htileAddress, bytes)) {
            const VkImageAspectFlags cleared = (VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u)) & HtileFillClears(*landed, target.htileStencil);
            if (cleared == 0) {
                std::lock_guard lock(htileReadsMutex());
                htileReads()[target.htileAddress] = collected;
                return 0;
            }
            if (storeHtileOnGpu(context, *recorder, target.htileAddress, bytes, expandedHtileWord(*landed, target.htileStencil, cleared))) {
                const auto generation = GuestMemory::CollectWrites(target.htileAddress, bytes);
                std::lock_guard lock(htileReadsMutex());
                htileReads()[target.htileAddress] = generation;
                return cleared;
            }
        } else if (const auto known = htileAfterMaskClear(*recorder, target, bytes); known.has_value() && storeHtileOnGpu(context, *recorder, target.htileAddress, bytes, known->second)) {
            const auto generation = GuestMemory::CollectWrites(target.htileAddress, bytes);
            std::lock_guard lock(htileReadsMutex());
            htileReads()[target.htileAddress] = generation;
            return known->first;
        }
    }
    std::vector<std::uint32_t> words(bytes / 4u);
    GuestMemory::Read(target.htileAddress, std::as_writable_bytes(std::span(words)), 4);
    VkImageAspectFlags cleared = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
    for (const auto word : words) cleared &= HtileFillClears(word, target.htileStencil);
    auto generation = collected;
    if (cleared != 0) {
        for (auto& word : words) word = expandedHtileWord(word, target.htileStencil, cleared);
        GuestMemory::Write(target.htileAddress, std::as_bytes(std::span(words)), 4);
        generation = GuestMemory::CollectWrites(target.htileAddress, bytes);
    }
    std::lock_guard lock(htileReadsMutex());
    htileReads()[target.htileAddress] = generation;
    if (cleared == 0 && untracked) untrackedHtileScans()[target.htileAddress] = {bytes, target.htileStencil, *untracked};
    else untrackedHtileScans().erase(target.htileAddress);
    return cleared;
}

struct StalePlanes {
    std::mutex mutex;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::atomic<std::uint64_t> begin{std::numeric_limits<std::uint64_t>::max()};
    std::atomic<std::uint64_t> end{0};
};

StalePlanes& stalePlanes() {
    static auto* planes = new StalePlanes();
    return *planes;
}

void publishStaleHull(StalePlanes& planes) {
    auto begin = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t end = 0;
    for (const auto& range : planes.ranges) {
        begin = std::min(begin, range.first);
        end = std::max(end, range.second);
    }
    planes.begin.store(begin, std::memory_order_release);
    planes.end.store(end, std::memory_order_release);
}

void noteStalePlane(std::uint64_t begin, std::uint64_t end) {
    auto& planes = stalePlanes();
    bool first = false;
    {
        std::lock_guard lock(planes.mutex);
        if (std::find(planes.ranges.begin(), planes.ranges.end(), std::pair{begin, end}) != planes.ranges.end()) return;
        first = planes.ranges.empty();
        planes.ranges.emplace_back(begin, end);
        publishStaleHull(planes);
    }
    if (first) UpdateResidentImageUnmapObserver_nid_no_patch();
}

bool overlapsStalePlane(std::uint64_t begin, std::uint64_t end) {
    auto& planes = stalePlanes();
    if (begin >= planes.end.load(std::memory_order_acquire) || planes.begin.load(std::memory_order_acquire) >= end) return false;
    std::lock_guard lock(planes.mutex);
    return std::any_of(planes.ranges.begin(), planes.ranges.end(), [&](const auto& range) { return begin < range.second && range.first < end; });
}

std::atomic<std::uint64_t>& planeSerial() {
    static std::atomic<std::uint64_t> serial{0};
    return serial;
}

std::uint64_t nextPlaneSerial() {
    return planeSerial().fetch_add(1, std::memory_order_acq_rel) + 1;
}

struct UnmappedPlane {
    std::uint64_t begin;
    std::uint64_t end;
    std::uint64_t serial;
};

struct UnmappedPlanes {
    std::mutex mutex;
    std::vector<UnmappedPlane> ranges;
};

UnmappedPlanes& unmappedPlanes() {
    static auto* unmapped = new UnmappedPlanes();
    return *unmapped;
}

void noteUnmappedPlane(std::uint64_t begin, std::uint64_t end) {
    if (!overlapsStalePlane(begin, end)) return;
    auto& unmapped = unmappedPlanes();
    std::lock_guard lock(unmapped.mutex);
    unmapped.ranges.push_back({begin, end, nextPlaneSerial()});
}

bool unmappedSince(std::uint64_t begin, std::uint64_t end, std::uint64_t mark) {
    auto& unmapped = unmappedPlanes();
    std::lock_guard lock(unmapped.mutex);
    return std::any_of(unmapped.ranges.begin(), unmapped.ranges.end(), [&](const auto& range) { return range.serial > mark && range.begin < end && begin < range.end; });
}

std::vector<bool> unmappedUnits(std::uint64_t plane, std::uint64_t planeBytes, std::uint64_t unitBytes, std::size_t count, std::uint64_t mark) {
    std::vector<bool> units(count, false);
    if (unitBytes == 0) return units;
    auto& unmapped = unmappedPlanes();
    std::lock_guard lock(unmapped.mutex);
    for (const auto& range : unmapped.ranges) {
        if (range.serial <= mark || range.begin >= plane + planeBytes || plane >= range.end) continue;
        const auto first = (std::max(range.begin, plane) - plane) / unitBytes;
        const auto last = (std::min(range.end, plane + planeBytes) - 1 - plane) / unitBytes;
        for (auto unit = first; unit <= last && unit < count; ++unit) units[static_cast<std::size_t>(unit)] = true;
    }
    return units;
}

void pruneUnmappedPlanes(const std::vector<UnmappedPlane>& marked) {
    auto& unmapped = unmappedPlanes();
    std::lock_guard lock(unmapped.mutex);
    std::erase_if(unmapped.ranges, [&](const auto& range) { return std::none_of(marked.begin(), marked.end(), [&](const auto& plane) { return plane.serial < range.serial && range.begin < plane.end && plane.begin < range.end; }); });
}

bool mirrorable(const DepthTarget& target) {
    return (target.format == VK_FORMAT_D32_SFLOAT || target.format == VK_FORMAT_D32_SFLOAT_S8_UINT) && target.samples == 1 && target.depthSwizzle == XorSwizzleMode(TextureTileMode::kZ64KBX);
}

GuestTextureResource mirrorResource(const DepthTarget& target) {
    GuestTextureResource resource{};
    resource.baseAddress = target.address;
    resource.width = target.extent.width;
    resource.height = target.extent.height;
    resource.mipCount = 1;
    resource.allocatedMipCount = 1;
    resource.tileMode = TextureTileMode::kZ64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 22;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

std::pair<std::uint64_t, std::uint64_t> stalePlaneRange(const DepthSurface& surface) {
    return {surface.target.address, surface.target.address + surface.MirrorBytes()};
}

GuestTextureResource stencilResource(const DepthTarget& target) {
    auto resource = mirrorResource(target);
    resource.baseAddress = target.stencilAddress;
    resource.format = 5;
    return resource;
}

std::uint64_t DepthSurface::MirrorBytes() const {
    if (mirrorBytes == 0) mirrorBytes = DescribeSurface(mirrorResource(target)).guestBytes;
    return mirrorBytes;
}

std::uint64_t DepthSurface::StencilMirrorBytes() const {
    if (stencilMirrorBytes == 0) stencilMirrorBytes = DescribeSurface(stencilResource(target)).guestBytes;
    return stencilMirrorBytes;
}

std::pair<std::uint64_t, std::uint64_t> staleStencilRange(const DepthSurface& surface) {
    return {surface.target.stencilAddress, surface.target.stencilAddress + surface.StencilMirrorBytes()};
}

void forgetStaleStencil(const DepthSurface& surface) {
    const auto plane = staleStencilRange(surface);
    if (std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& other) { return other->stencilStale && staleStencilRange(*other) == plane; })) return;
    auto& planes = stalePlanes();
    std::lock_guard lock(planes.mutex);
    std::erase(planes.ranges, plane);
    publishStaleHull(planes);
}

void retireStencil(DepthSurface& surface) {
    if (!surface.stencilStale) return;
    surface.stencilStale = false;
    forgetStaleStencil(surface);
}

void storeStaleStencil(DepthSurface& surface) {
    if (!surface.stencilStale || surface.syncing || !surface.StencilMirrorable()) return;
    const auto [begin, end] = staleStencilRange(surface);
    const auto bytes = static_cast<std::size_t>(end - begin);
    if (GuestMemory::CommittedRanges(begin, bytes).empty()) return;
    if (unmappedSince(begin, end, surface.stencilMark)) {
        retireStencil(surface);
        return;
    }
    static_cast<void>(surface.StoreStencilPlane(stencilResource(surface.target)));
    forgetStaleStencil(surface);
    if (!StorageTexture::FlushPending(begin, bytes, nullptr, "stencil plane store")) return;
    GuestMemory::CollectWrites(begin, bytes);
    surface.NoteStencilStored(GuestMemory::TrackerGeneration());
}

void retire(DepthSurface& surface) {
    surface.retired = true;
    for (auto* owners : {&depthOwners(), &stencilOwners()}) {
        std::erase_if(*owners, [&](const auto& entry) { return entry.second.surface == &surface; });
    }
    retireStencil(surface);
    if (!surface.AnyStale()) return;
    surface.ClearStale();
    const auto plane = stalePlaneRange(surface);
    if (std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& other) { return other->AnyStale() && stalePlaneRange(*other) == plane; })) return;
    auto& planes = stalePlanes();
    std::lock_guard lock(planes.mutex);
    std::erase(planes.ranges, plane);
    publishStaleHull(planes);
}

void retireStoringStale(DepthSurface& surface) {
    if (!surface.StencilWrittenInMemory()) storeStaleStencil(surface);
    if (!surface.AnyStale() || !mirrorable(surface.target)) {
        retire(surface);
        return;
    }
    const auto resource = mirrorResource(surface.target);
    const auto planeBytes = surface.MirrorBytes();
    std::uint64_t generation = 0;
    const auto written = surface.WrittenUnits(resource, generation);
    const auto removed = unmappedUnits(surface.target.address, planeBytes, surface.unitBytes, surface.staleUnits.size(), surface.depthMark);
    const auto reused = surface.ChangedBackingUnits();
    auto units = surface.staleUnits;
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        if ((unit < written.size() && written[unit]) || removed[unit] || (unit < reused.size() && reused[unit])) units[unit] = false;
    }
    if ((surface.pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 && anyUnit(written)) units.assign(units.size(), false);
    std::shared_ptr<StorageTexture> mirror;
    if (anyUnit(units) && (!anyUnit(written) || surface.StoresUnits(*surface.Mirror(resource)))) mirror = surface.StoreDepthPlane(resource, units);
    retire(surface);
    if (mirror == nullptr) return;
    const auto [begin, end] = surface.UnitHull(units, planeBytes);
    if (StorageTexture::FlushPending(begin, static_cast<std::size_t>(end - begin), nullptr, "depth plane store")) surface.NoteStored(units, GuestMemory::TrackerGeneration());
}
}

std::uint64_t DepthSurfacesSerial() {
    return surfacesSerial.load(std::memory_order_acquire);
}

std::uint64_t DepthSurfacesReleased() {
    return surfacesReleased.load(std::memory_order_acquire);
}

std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel) {
    const auto [blockWidth, blockHeight] = DepthBlockExtent(bytesPerTexel);
    const auto width = static_cast<std::uint64_t>((extent.width + blockWidth - 1) / blockWidth * blockWidth);
    const auto height = static_cast<std::uint64_t>((extent.height + blockHeight - 1) / blockHeight * blockHeight);
    return width * height * bytesPerTexel;
}

std::uint64_t MultisampledDepthBytes_nid_no_patch(VkExtent2D extent, std::uint32_t bytesPerTexel, std::uint32_t samples) {
    Require(extent.width != 0u && extent.width <= 16384u && extent.height != 0u && extent.height <= 16384u, "invalid multisample depth extent");
    Require((bytesPerTexel == 1u || bytesPerTexel == 2u || bytesPerTexel == 4u) && (samples == 2u || samples == 4u || samples == 8u), "unsupported multisample depth footprint");
    const auto sampleBits = std::countr_zero(samples);
    const auto pixelBits = 16u - std::countr_zero(bytesPerTexel) - sampleBits;
    const auto widthBits = (pixelBits + ((sampleBits & 1u) == 0u ? 1u : 0u)) / 2u;
    const auto blockWidth = 1u << widthBits;
    const auto blockHeight = 1u << (pixelBits - widthBits);
    const auto width = static_cast<std::uint64_t>((extent.width + blockWidth - 1u) / blockWidth * blockWidth);
    const auto height = static_cast<std::uint64_t>((extent.height + blockHeight - 1u) / blockHeight * blockHeight);
    return width * height * bytesPerTexel * samples;
}

void proveOverwrittenPlanes(std::uint64_t address, std::size_t bytes) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->sampling.overwriteStencil && surface->active->load(std::memory_order_acquire) && surface->Overlaps(address, bytes)) surface->ProveOverwrite();
    }
}

void RequireNoResidentDepthRead_nid_no_patch(std::uint64_t address, std::size_t bytes, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear, bool allowPublication) {
    if (bytes != 0u && pendingOverwriteProofs().load(std::memory_order_acquire) != 0u && allowPublication && !HoldsMultisampleTargetsLock()) proveOverwrittenPlanes(address, bytes);
    if (bytes == 0u || !anyDepthGuard().load(std::memory_order_acquire)) return;
    Require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "native depth read range overflows");
    if (!mayOverlapDepthGuard(address, bytes)) return;
    const auto overlaps = [&](const DepthGuard& range) { return address < range.address + range.bytes && range.address < address + bytes; };
    if (htileClear.has_value() && htileClear->second <= std::numeric_limits<std::uint64_t>::max() - htileClear->first && htileClear->first <= address && bytes <= htileClear->second && address - htileClear->first <= htileClear->second - bytes) {
        std::lock_guard lock(surfacesMutex());
        std::shared_lock guardLock(depthGuardMutex());
        const bool accepted = std::ranges::all_of(depthGuards(), [&](const auto& range) {
            if (!overlaps(range)) return true;
            return std::ranges::any_of(surfaces(), [&](const auto& surface) {
                if (surface->active != range.active || !surface->active->load(std::memory_order_acquire) || !surface->samplingActive || !surface->sampling.refusal.empty() || !surface->clearPending) return false;
                const auto& metadata = surface->sampling.initial[1];
                return metadata.address == range.address && metadata.bytes == range.bytes && metadata.address == htileClear->first && metadata.bytes <= htileClear->second;
            });
        });
        if (accepted) return;
    }
    std::vector<std::shared_ptr<std::atomic<bool>>> written;
    {
        std::shared_lock lock(depthGuardMutex());
        if (std::ranges::none_of(depthGuards(), overlaps)) return;
        for (const auto& range : depthGuards()) {
            if (overlaps(range) && range.hostWritten->load(std::memory_order_acquire) && std::ranges::find(written, range.hostWritten) == written.end()) written.push_back(range.hostWritten);
        }
    }
    if (!written.empty()) {
        Require(allowPublication && !HoldsMultisampleTargetsLock(), "sample-preserving guest publication of native multisample depth is not implemented while a multisampled colour target is being acquired");
        if (const auto hook = depthPublicationHook.load(std::memory_order_acquire)) hook(DepthPublicationStage::ReadCaptured);
        publishWrittenDepth(written);
    }
    std::unique_lock lock(depthGuardMutex());
    for (const auto& range : depthGuards()) {
        if (overlaps(range)) Require(!range.hostWritten->load(std::memory_order_acquire), "sample-preserving guest publication of native multisample depth is not implemented");
    }
    for (const auto& range : depthGuards()) {
        if (overlaps(range) && !range.hostEverWritten->load(std::memory_order_acquire)) range.active->store(false, std::memory_order_release);
    }
    const auto removed = std::erase_if(depthGuards(), [](const auto& range) { return !range.active->load(std::memory_order_acquire); });
    publishDepthGuards(removed != 0);
}

void AppendHostWrittenDepthRanges(std::uint64_t address, std::size_t bytes, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
    if (bytes == 0u || !anyDepthGuard().load(std::memory_order_acquire)) return;
    std::shared_lock lock(depthGuardMutex());
    for (const auto& range : depthGuards()) {
        if (address < range.address + range.bytes && range.address < address + bytes && range.hostWritten->load(std::memory_order_acquire)) ranges.emplace_back(range.address, range.bytes);
    }
}

void SetDepthPublicationHookForTest(void (*hook)(DepthPublicationStage)) {
    depthPublicationHook.store(hook, std::memory_order_release);
}

void NoteDepthSurfaceWrite(const Context& context, const DepthTarget& target) {
    if (target.samples == 1u) return;
    std::lock_guard lock(surfacesMutex());
    bool attached = false;
    for (const auto& surface : surfaces()) {
        if (surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && holdsTarget(surface->target, target)) {
            surface->NoteHostWrite();
            attached = true;
        }
    }
    Require(attached, "a native multisample depth attachment was retired while its draw resolved resources");
}

bool HasResidentDepthGuards_nid_no_patch() {
    return anyDepthGuard().load(std::memory_order_acquire);
}

bool ResidentDepthGuardOverlaps_nid_no_patch(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0u || !anyDepthGuard().load(std::memory_order_acquire)) return false;
    Require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "native depth overlap range overflows");
    std::shared_lock lock(depthGuardMutex());
    return std::ranges::any_of(depthGuards(), [&](const DepthGuard& range) {
        return range.active->load(std::memory_order_acquire) && address < range.address + range.bytes && range.address < address + bytes;
    });
}

void RetireUnmappedDepth_nid_no_patch(std::uintptr_t address, std::size_t bytes) {
    if (bytes == 0u) return;
    Require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "native depth release range overflows");
    const auto end = address + bytes;
    noteUnmappedPlane(address, end);
    if (!HasResidentDepthGuards_nid_no_patch()) return;
    std::unique_lock lock(depthGuardMutex());
    for (auto& range : depthGuards()) {
        std::vector<GuestMemory::UnchangedQuery> remaining;
        remaining.reserve(range.live.size() + 1u);
        for (const auto& part : range.live) {
            const auto finish = part.address + part.bytes;
            const auto first = std::max<std::uint64_t>(address, part.address);
            const auto last = std::min<std::uint64_t>(end, finish);
            if (first >= last) remaining.push_back(part);
            else {
                if (part.address < first) remaining.push_back({part.address, static_cast<std::size_t>(first - part.address), 0u});
                if (last < finish) remaining.push_back({last, static_cast<std::size_t>(finish - last), 0u});
            }
        }
        range.live = std::move(remaining);
    }
    for (const auto& range : depthGuards()) {
        if (std::ranges::all_of(depthGuards(), [&](const auto& part) { return part.active != range.active || part.live.empty(); })) range.active->store(false, std::memory_order_release);
    }
    const auto removed = std::erase_if(depthGuards(), [](const auto& range) { return !range.active->load(std::memory_order_acquire); });
    publishDepthGuards(removed != 0);
}

bool SampleLocationsCompatibleDepth(const Context& context, VkFormat format, std::uint32_t samples) {
    if (samples == 0 || (context.sampleLocationSampleCounts & samples) == 0) return false;
    VkImageFormatProperties properties{};
    const auto result = context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT, &properties);
    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) return false;
    Check(result, "vkGetPhysicalDeviceImageFormatProperties sample locations depth");
    return (properties.sampleCounts & samples) != 0;
}

VkFormat HostDepthFormat(const Context& context, VkFormat format) {
    if (format != VK_FORMAT_D16_UNORM_S8_UINT) return format;
    static AgcDriver::Mutex mutex;
    static std::map<VkPhysicalDevice, VkFormat> chosen;
    std::lock_guard lock(mutex);
    if (const auto found = chosen.find(context.physical); found != chosen.end()) return found->second;
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    auto host = format;
    for (const auto candidate : {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        VkFormatProperties properties{};
        context.formatProperties(context.physical, candidate, &properties);
        if ((properties.optimalTilingFeatures & required) != required) continue;
        host = candidate;
        break;
    }
    chosen.emplace(context.physical, host);
    return host;
}

float HostDepthBiasConstant(const Context& context, VkFormat format, float constant) {
    const auto host = HostDepthFormat(context, format);
    if (constant == 0.0f || host == format) return constant;
    Require(host == VK_FORMAT_D24_UNORM_S8_UINT, "a constant depth bias on a 16-bit depth target with stencil that the device stores as 32-bit float depth is not implemented");
    return constant * 256.0f;
}

namespace {

bool resident(std::uint64_t address, std::uint64_t bytes) {
    return GuestMemory::DescribeCommitted(address, static_cast<std::size_t>(bytes)).whole;
}

}

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target, bool depthWritten, bool stencilTested) {
    return DepthSurfaceViewWithStencilWriteIntent(context, target, depthWritten, stencilTested, true);
}

VkImageView DepthSurfaceViewWithStencilWriteIntent(const Context& context, const DepthTarget& target, bool depthWritten, bool stencilTested, bool stencilWritten, std::optional<std::uint8_t> overwrittenStencil) {
    const auto planeBytes = [&](std::uint32_t bytesPerTexel) { return target.samples == 1u ? DepthSliceBytes(target.extent, bytesPerTexel) : MultisampledDepthBytes_nid_no_patch(target.extent, bytesPerTexel, target.samples); };
    if (target.depthPartiallyResident) Require(resident(target.address, planeBytes(target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2u : 4u)) && (target.htileAddress == 0u || resident(target.htileAddress, HtileMetadataBytes(target.extent))), "partially resident depth (DB_Z_INFO) with pages that are not mapped is unsupported");
    if (target.stencilPartiallyResident) Require(resident(target.stencilAddress, planeBytes(1u)), "partially resident stencil (DB_STENCIL_INFO) with pages that are not mapped is unsupported");
    ClaimMultisampleTargetMemory(target.address, planeBytes(target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2u : 4u));
    if (target.stencilAddress != 0u) ClaimMultisampleTargetMemory(target.stencilAddress, planeBytes(1u));
    if (target.htileAddress != 0u) ClaimMultisampleTargetMemory(target.htileAddress, HtileMetadataBytes(target.extent));
    const auto htileCleared = RoutesToNativeDepthOwner(target) ? VkImageAspectFlags{0} : storeBackClearedHtile(context, target);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    settlePendingHtileClear(context, target.address);
    std::unique_lock lock(surfacesMutex());
    const bool cleared = takeClearedHtile(target);
    DepthSurface* surface = nullptr;
    for (const auto& candidate : surfaces()) {
        if (candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameSurface(candidate->target, target)) {
            surface = candidate.get();
            break;
        }
    }
    for (const auto& candidate : surfaces()) {
        if (surface == nullptr && candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameLayoutWithin(candidate->target, target)) surface = candidate.get();
    }
    const bool created = surface == nullptr;
    if (!created) surface->ProveOverwrite();
    GuestAllocations::Lease creationLease;
    if (created) {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto depthBytes = target.samples == 1u ? DepthSliceBytes(target.extent, d16 ? 2u : 4u) : MultisampledDepthBytes_nid_no_patch(target.extent, d16 ? 2u : 4u, target.samples);
        const auto stencilBytes = target.stencilAddress == 0u ? 0u : target.samples == 1u ? DepthSliceBytes(target.extent, 1u) : MultisampledDepthBytes_nid_no_patch(target.extent, 1u, target.samples);
        const auto metadataBytes = target.htileAddress == 0u ? 0u : HtileMetadataBytes(target.extent);
        Require(target.address <= std::numeric_limits<std::uint64_t>::max() - depthBytes && target.stencilAddress <= std::numeric_limits<std::uint64_t>::max() - stencilBytes && target.htileAddress <= std::numeric_limits<std::uint64_t>::max() - metadataBytes, "native depth attachment range overflows");
        lock.unlock();
        RequireNoResidentDepthRead_nid_no_patch(target.address, static_cast<std::size_t>(depthBytes));
        if (stencilBytes != 0u) RequireNoResidentDepthRead_nid_no_patch(target.stencilAddress, static_cast<std::size_t>(stencilBytes));
        if (metadataBytes != 0u) RequireNoResidentDepthRead_nid_no_patch(target.htileAddress, static_cast<std::size_t>(metadataBytes));
        {
            std::shared_lock guardLock(depthGuardMutex());
            for (const auto& range : depthGuards()) {
                const bool overlapsRange = (target.address < range.address + range.bytes && range.address < target.address + depthBytes) || (stencilBytes != 0u && target.stencilAddress < range.address + range.bytes && range.address < target.stencilAddress + stencilBytes) || (metadataBytes != 0u && target.htileAddress < range.address + range.bytes && range.address < target.htileAddress + metadataBytes);
                Require(!overlapsRange || !range.active->load(std::memory_order_acquire) || !range.hostEverWritten->load(std::memory_order_acquire), "a native multisample depth attachment over the planes of a host-written owner of another identity is not implemented");
            }
        }
        auto sampling = target.samples == 1u ? DepthSamplingState{} : prepareDepthSampling(context, target);
        if (sampling.unevenPlanes && overwrittenStencil) {
            sampling.overwriteStencil = overwrittenStencil;
            sampling.initialStencil = static_cast<std::uint8_t>(~*overwrittenStencil);
        }
        creationLease = target.samples == 1u || !sampling.refusal.empty() ? GuestAllocations::Lease{} : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
        lock.lock();
        releaseRetired(context);
        surfacesSerial.fetch_add(1, std::memory_order_release);
        surfaces().push_back(std::make_unique<DepthSurface>(context, target, std::move(sampling)));
        ++mirrorEpoch;
        auto& fresh = *surfaces().back();
        if (target.samples != 1u && fresh.sampleable && fresh.sampling.refusal.empty()) {
            std::unique_lock guardLock(depthGuardMutex());
            for (const auto& range : fresh.sampling.initial) depthGuards().push_back({context.device, range.address, range.bytes, fresh.active, {{range.address, range.bytes, 0u}}, fresh.hostWritten, fresh.hostEverWritten});
            publishDepthGuards(true);
            InstallResidentImageReadGuard_nid_no_patch();
            fresh.samplingActive = true;
        }
        surface = surfaces().back().get();
    }
    surface->retired = false;
    const auto owner = depthOwners().find({context.device, target.address});
    const bool inherits = owner != depthOwners().end() && owner->second.surface->active->load(std::memory_order_acquire) && !owner->second.surface->retired;
    surface->clearDepth = target.clearDepth;
    surface->clearStencil = target.clearStencil;
    const auto stencilOwner = stencilOwners().find({context.device, target.stencilAddress});
    const bool stencilRebound = stencilOwner == stencilOwners().end() || stencilOwner->second.surface != surface;
    takePlanes(*surface, created, htileCleared | (cleared ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u));
    if (cleared || htileCleared != 0u) surface->expanded = false;
    if (created && !inherits && target.htileAddress == 0) surface->TakeStorage();
    if (cleared && (!created || inherits)) {
        if (target.samples != 1u && (!created || !surface->samplingActive)) surface->sampling.Refuse("native multisample depth fast-clear transition is not implemented");
        if (!created) surface->NoteHtileClear(VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    if (!created) surface->NoteHtileClear(htileCleared);
    if (mirrorable(target)) surface->SyncFromMemory(mirrorResource(target));
    if (surface->StencilMirrorable()) surface->SyncStencilFromMemory(stencilResource(target));
    surface->TakeStencilStorage();
    surface->ApplyFastClear();
    if (stencilTested && surface->samplingActive) surface->RequireStencilProven();
    if (drawnDepth != nullptr && drawnDepth != surface) drawnDepth->written = ++depthWrites;
    surface->written = ++depthWrites;
    surface->NoteMemory();
    if (mirrorable(target) && surface->scannedEpoch != mirrorEpoch) {
        surface->scannedEpoch = mirrorEpoch;
        const auto plane = stalePlaneRange(*surface);
        for (const auto& other : surfaces()) {
            if (other.get() == surface || other->context.device != context.device || other->written >= surface->written || !mirrorable(other->target)) continue;
            const auto otherPlane = stalePlaneRange(*other);
            if (otherPlane == plane || otherPlane.first >= plane.second || plane.first >= otherPlane.second) continue;
            other->ClearStale();
            other->DropMirror();
        }
    }
    if ((depthWritten || cleared) && mirrorable(target)) {
        const auto plane = target.address;
        const auto planeBytes = surface->MirrorBytes();
        surface->NoteDepthWork();
        surface->MarkAllStale(planeBytes);
        noteStalePlane(plane, plane + planeBytes);
    }
    if (stencilWritten || created || stencilRebound) surface->MarkStencilStale();
    drawnDepth = surface;
    return surface->view;
}

SampleOrderTarget DepthSurfaceOrderTarget(const Context& context, const DepthTarget& target) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::unique_lock lock(surfacesMutex());
    for (const auto& candidate : surfaces()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device || !holdsTarget(candidate->target, target)) continue;
        Require(candidate->sampleable, "the multisampled depth surface whose samples are put in the guest's order cannot be sampled");
        return {candidate->image, candidate->view, candidate->hostFormat, candidate->target.extent, target.samples, DepthSampleOrderMethod(context, target), candidate->owned};
    }
    throw std::runtime_error("AGC graphics: the multisampled depth surface whose samples are put in the guest's order is not resident");
}

DepthSampleLocations DepthSurfaceSampleLocations(const Context& context, const DepthTarget& target) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::unique_lock lock(surfacesMutex());
    for (const auto& candidate : surfaces()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device || !holdsTarget(candidate->target, target)) continue;
        DepthSampleLocations depth;
        depth.image = candidate->image;
        depth.format = candidate->hostFormat;
        depth.samples = target.samples;
        depth.keep = candidate->owned;
        return depth;
    }
    throw std::runtime_error("AGC graphics: the depth surface drawn at custom sample locations is not resident");
}

bool DepthPlanesStale() {
    auto& planes = stalePlanes();
    return planes.begin.load(std::memory_order_acquire) < planes.end.load(std::memory_order_acquire);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> FlushDepthPlanes(std::uint64_t address, std::size_t bytes, DepthPlaneScope scope) {
    struct FlushingSlot {};
    const bool flushing = HostThreadSlot<bool, FlushingSlot>::Get();
    const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
    if (flushing || !overlapsStalePlane(address, end)) return {};
    if (scope == DepthPlaneScope::Overlapped) {
        std::lock_guard lock(surfacesMutex());
        const auto staleInRange = [&](const auto& surface) {
            if (surface->syncing) return false;
            if (surface->stencilStale) {
                const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
                if (stencilBegin < end && address < stencilEnd) return true;
            }
            return surface->AnyStale() && ((surface->pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 || anyUnit(surface->StaleUnitsIn(surface->MirrorBytes(), address, end)));
        };
        if (std::none_of(surfaces().begin(), surfaces().end(), staleInRange)) return {};
    }
    HostThreadSlot<bool, FlushingSlot>::Set(true);
    struct Reset {
        ~Reset() { HostThreadSlot<bool, FlushingSlot>::Set(false); }
    } reset;
    struct Stored {
        DepthSurface* surface;
        std::shared_ptr<StorageTexture> mirror;
        std::vector<bool> units;
        std::uint64_t begin;
        std::uint64_t end;
        bool flushed;
    };
    std::vector<Stored> stored;
    struct StoredStencil {
        DepthSurface* surface;
        std::shared_ptr<StorageTexture> mirror;
        std::uint64_t begin;
        std::uint64_t end;
        bool flushed;
    };
    std::vector<StoredStencil> storedStencils;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    {
        std::lock_guard gpu(GuestMemory::GpuMutex());
        std::lock_guard lock(surfacesMutex());
        std::vector<std::pair<std::uint64_t, std::uint64_t>> remaining;
        std::vector<UnmappedPlane> marked;
        for (const auto& surface : surfaces()) {
            if (surface->stencilStale && !surface->syncing) {
                const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
                if (stencilBegin < end && address < stencilEnd) {
                    if (GuestMemory::CommittedRanges(stencilBegin, static_cast<std::size_t>(stencilEnd - stencilBegin)).empty() || unmappedSince(stencilBegin, stencilEnd, surface->stencilMark)) {
                        surface->stencilStale = false;
                    } else {
                        auto mirror = surface->StoreStencilPlane(stencilResource(surface->target));
                        ranges.emplace_back(stencilBegin, stencilEnd - stencilBegin);
                        storedStencils.push_back({surface.get(), std::move(mirror), stencilBegin, stencilEnd, false});
                    }
                }
            }
            if (surface->stencilStale) {
                remaining.push_back(staleStencilRange(*surface));
                marked.push_back({remaining.back().first, remaining.back().second, surface->stencilMark});
            }
            if (!surface->AnyStale()) continue;
            const auto resource = mirrorResource(surface->target);
            const auto plane = surface->target.address;
            const auto planeBytes = surface->MirrorBytes();
            const bool overlapped = !surface->syncing && plane < end && address < plane + planeBytes;
            if (overlapped && (surface->pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) surface->MarkAllStale(planeBytes);
            if (overlapped && (scope == DepthPlaneScope::Whole || anyUnit(surface->StaleUnitsIn(planeBytes, address, end)))) {
                const auto removed = unmappedUnits(plane, planeBytes, surface->unitBytes, surface->staleUnits.size(), surface->depthMark);
                const bool uncommitted = GuestMemory::CommittedRanges(plane, static_cast<std::size_t>(planeBytes)).empty();
                if (uncommitted || anyUnit(removed)) {
                    static std::atomic<bool> reported{false};
                    if (!reported.exchange(true)) std::fprintf(stderr, "[gpu] depth plane 0x%llx+0x%llx was unmapped with GPU results pending; they are dropped\n", static_cast<unsigned long long>(plane), static_cast<unsigned long long>(planeBytes));
                    if (uncommitted) surface->ClearStale();
                    else surface->ClearStale(removed);
                    if (!surface->AnyStale()) continue;
                }
                const auto reused = surface->ChangedBackingUnits();
                if (anyUnit(reused)) {
                    static std::atomic<bool> reported{false};
                    if (!reported.exchange(true)) std::fprintf(stderr, "[gpu] depth plane 0x%llx+0x%llx was re-registered with GPU results pending; they are dropped\n", static_cast<unsigned long long>(plane), static_cast<unsigned long long>(planeBytes));
                    surface->ClearStale(reused);
                    if (!surface->AnyStale()) continue;
                }
                GuestMemory::CollectWritesUncached(plane, static_cast<std::size_t>(planeBytes));
                surface->SyncFromMemory(resource, true);
                auto units = scope == DepthPlaneScope::Whole ? surface->staleUnits : surface->StaleUnitsIn(planeBytes, address, end);
                if (anyUnit(units)) {
                    auto mirror = surface->StoreDepthPlane(resource, units);
                    surface->ClearStale(units);
                    const auto hull = surface->UnitHull(units, planeBytes);
                    ranges.emplace_back(hull.first, hull.second - hull.first);
                    stored.push_back({surface.get(), std::move(mirror), std::move(units), hull.first, hull.second, false});
                }
            }
            if (surface->AnyStale()) {
                remaining.emplace_back(plane, plane + planeBytes);
                marked.push_back({plane, plane + planeBytes, surface->depthMark});
            }
        }
        auto& planes = stalePlanes();
        std::lock_guard staleLock(planes.mutex);
        planes.ranges = std::move(remaining);
        publishStaleHull(planes);
        pruneUnmappedPlanes(marked);
    }
    for (auto& entry : stored) entry.flushed = StorageTexture::FlushPending(entry.begin, static_cast<std::size_t>(entry.end - entry.begin), nullptr, "depth plane store");
    for (auto& entry : storedStencils) entry.flushed = StorageTexture::FlushPending(entry.begin, static_cast<std::size_t>(entry.end - entry.begin), nullptr, "stencil plane store");
    const auto generation = GuestMemory::TrackerGeneration();
    std::lock_guard lock(surfacesMutex());
    for (const auto& entry : storedStencils) {
        if (!entry.flushed || std::none_of(surfaces().begin(), surfaces().end(), [&](const auto& candidate) { return candidate.get() == entry.surface; })) continue;
        GuestMemory::CollectWrites(entry.begin, static_cast<std::size_t>(entry.end - entry.begin));
        entry.surface->NoteStencilStored(GuestMemory::TrackerGeneration());
    }
    for (const auto& entry : stored) {
        if (!entry.flushed || std::none_of(surfaces().begin(), surfaces().end(), [&](const auto& candidate) { return candidate.get() == entry.surface; })) continue;
        entry.surface->NoteStored(entry.units, generation);
        entry.surface->NoteMemory();
    }
    return ranges;
}

DepthPlaneStoreStatistics DepthPlaneStoreCounts() {
    const auto& counters = depthPlaneStoreCounters();
    DepthPlaneStoreStatistics statistics;
    statistics.stores = counters.stores.load();
    statistics.units = counters.units.load();
    statistics.bytes = counters.bytes.load();
    return statistics;
}

std::uint64_t HtileDepthClearAddress(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    static constexpr std::array<std::uint32_t, 17> htileMaskKernel{0xd7460000u, 0x04010c06u, 0x34000084u, 0xdc388000u, 0x04020000u, 0xbf8c3f70u, 0xd7710008u, 0x00120805u, 0xd7710009u, 0x00120a05u, 0xd771000au, 0x00120c05u, 0xd771000bu, 0x00120e05u, 0xdc788000u, 0x00020800u, 0xbf810000u};
    if (userData.size() < 6 || numThreads[0] != 64 || numThreads[1] != 1 || numThreads[2] != 1) return 0;
    if (code.size() < htileMaskKernel.size() || !std::equal(htileMaskKernel.begin(), htileMaskKernel.end(), code.begin())) return 0;
    if ((userData[5] & 0xfu) != 0 || (userData[4] & 0xfu) != 0) return 0;
    return userData[2] | (static_cast<std::uint64_t>(userData[3]) << 32u);
}

std::optional<HtileMaskClear> MatchHtileBufferMaskClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::span<const std::uint32_t> packet, std::uint64_t indirectArguments) {
    static constexpr std::array<std::uint32_t, 16> kernel{0xd7460004u, 0x04010c0cu, 0xe00c2000u, 0x80000004u, 0xbf8c3f70u, 0xd7710000u, 0x00120008u, 0xd7710001u, 0x00160209u, 0xd7710002u, 0x001a040au, 0xd7710003u, 0x001e060bu, 0xe01c2000u, 0x80000004u, 0xbf810000u};
    if (userData.size() != 12u || packet.size() < 5u || indirectArguments != 0u) return std::nullopt;
    if (code.size() < kernel.size() || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    if (compute.numThreads != std::array<std::uint32_t, 3>{64u, 1u, 1u} || !compute.groupIdEnable[0] || compute.threadIdComponentCount == 0u || compute.threadIdComponentCount > 3u) return std::nullopt;
    if (packet[1] == 0u || packet[2] != 1u || packet[3] != 1u || (packet[4] & 0x3u) != 0x1u) return std::nullopt;
    for (std::size_t word = 1; word < 4; ++word) {
        if (userData[4 + word] != userData[4] || userData[8 + word] != userData[8]) return std::nullopt;
    }
    if ((userData[8] & 0xfu) != 0u || (userData[4] & 0xfu) != 0u) return std::nullopt;
    const auto stride = (userData[1] >> 16u) & 0x3fffu;
    const bool swizzled = ((userData[1] >> 31u) & 1u) != 0u;
    const bool addTid = ((userData[3] >> 23u) & 1u) != 0u;
    const auto type = userData[3] >> 30u;
    const auto format = (userData[3] >> 12u) & 0x7fu;
    const auto selects = userData[3] & 0xfffu;
    if (stride != 16u || swizzled || addTid || type != 0u || format != 75u || selects != 0xfacu) return std::nullopt;
    const auto address = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
    const std::uint64_t threads = (packet[4] & 0x20u) != 0u ? packet[1] : static_cast<std::uint64_t>(packet[1]) * 64u;
    const auto records = std::min<std::uint64_t>(threads, userData[2]);
    if (address == 0u || (address & 15u) != 0u || records == 0u || records > 0x400000u) return std::nullopt;
    return HtileMaskClear{address, static_cast<std::size_t>(records * 16u), userData[8], userData[4]};
}

std::optional<MemoryValueFill> MatchMemoryValueFill(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads, const std::array<std::uint32_t, 3>& groups) {
    static constexpr std::array<std::uint32_t, 9> kernel{0xd7460000u, 0x04010c08u, 0xf4201a82u, 0xfa000000u, 0xbf8cc07fu, 0x7e02026au, 0xe0102000u, 0x80000100u, 0xbf810000u};
    if (userData.size() < 8 || numThreads != std::array<std::uint32_t, 3>{64u, 1u, 1u} || groups[1] != 1u || groups[2] != 1u) return std::nullopt;
    if (code.size() < kernel.size() || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    const auto stride = (userData[1] >> 16u) & 0x3fffu;
    const bool swizzled = ((userData[1] >> 31u) & 1u) != 0;
    const bool addTid = ((userData[3] >> 23u) & 1u) != 0;
    const auto type = userData[3] >> 30u;
    const auto format = (userData[3] >> 12u) & 0x7fu;
    const auto valueStride = (userData[5] >> 16u) & 0x3fffu;
    const auto valueBytes = valueStride == 0 ? static_cast<std::uint64_t>(userData[6]) : static_cast<std::uint64_t>(userData[6]) * valueStride;
    if (type != 0 || stride != 4 || swizzled || addTid || format < 20u || format > 22u || valueBytes < 4u) return std::nullopt;
    const auto dwords = std::min<std::uint64_t>(static_cast<std::uint64_t>(groups[0]) * 64u, userData[2]);
    const auto address = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
    const auto valueAddress = userData[4] | (static_cast<std::uint64_t>(userData[5] & 0xffffu) << 32u);
    if (dwords == 0 || valueAddress == 0 || (valueAddress & 3u) != 0) return std::nullopt;
    return MemoryValueFill{address, static_cast<std::size_t>(dwords * 4u), valueAddress};
}

void NoteMemoryValueFill(const MemoryValueFill& fill) {
    std::uint32_t value = 0;
    GuestMemory::Read(fill.valueAddress, std::as_writable_bytes(std::span(&value, 1)), 4);
    NoteDepthMetadataFill(fill.address, fill.bytes, value);
}

std::optional<StorageImageClear> MatchStorageImageClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::array<std::uint32_t, 3>& groups) {
    static constexpr std::array<std::uint32_t, 15> kernel{0xd7460004u, 0x0401060cu, 0xf4280204u, 0xfa000000u, 0xd7460005u, 0x0405060du, 0x7e0c020eu, 0xbf8cc07fu, 0x7e000208u, 0x7e020209u, 0x7e04020au, 0x7e06020bu, 0xf0200f28u, 0x00000004u, 0xbf810000u};
    if (code.size() < kernel.size() || code[0] != kernel[0] || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    if (userData.size() != 12u) return std::nullopt;
    if (compute.numThreads != std::array<std::uint32_t, 3>{8u, 8u, 1u}) return std::nullopt;
    if (!compute.groupIdEnable[0] || !compute.groupIdEnable[1] || !compute.groupIdEnable[2]) return std::nullopt;
    if (compute.threadIdComponentCount < 2u) return std::nullopt;
    std::array<std::uint64_t, 3> covered{};
    for (std::size_t axis = 0; axis < 3u; ++axis) covered[axis] = compute.partialThreads[axis] != 0u ? compute.partialThreads[axis] : static_cast<std::uint64_t>(groups[axis]) * compute.numThreads[axis];
    if (covered[0] == 0u || covered[1] == 0u || covered[2] != 1u) return std::nullopt;
    StorageImageClear clear;
    std::copy_n(userData.begin(), clear.image.size(), clear.image.begin());
    std::copy_n(userData.begin() + static_cast<std::ptrdiff_t>(clear.image.size()), clear.color.size(), clear.color.begin());
    clear.width = static_cast<std::uint32_t>(std::min<std::uint64_t>(covered[0], std::numeric_limits<std::uint32_t>::max()));
    clear.height = static_cast<std::uint32_t>(std::min<std::uint64_t>(covered[1], std::numeric_limits<std::uint32_t>::max()));
    return clear;
}

std::uint64_t StorageImageClearAddress(const StorageImageClear& clear) {
    try {
        return DecodeTextureResource(clear.image).baseAddress;
    } catch (...) {
        return 0u;
    }
}

DepthPlaneClear::DepthPlaneClear(const StorageImageClear& clear) {
    if (!depthPlaneClearEnabled.load(std::memory_order_relaxed)) return;
    const auto decoded = decodeOverwrite(clear);
    if (decoded) {
        value = *decoded;
        previous = currentOverwrite;
        currentOverwrite = &value;
        installed = true;
        ++depthClearCounters().taken;
    }
}

DepthPlaneClear::~DepthPlaneClear() {
    if (installed) currentOverwrite = previous;
}

DepthClearStatistics DepthClearCounts() {
    const auto& counters = depthClearCounters();
    DepthClearStatistics statistics;
    statistics.matched = counters.matched.load();
    statistics.taken = counters.taken.load();
    statistics.copiesSkipped = counters.copiesSkipped.load();
    statistics.clears = counters.clears.load();
    statistics.bytesSkipped = counters.bytesSkipped.load();
    for (std::size_t reason = 0; reason < statistics.declined.size(); ++reason) statistics.declined[reason] = counters.declined[reason].load();
    return statistics;
}

void SetDepthPlaneClearForTest(bool enabled) {
    depthPlaneClearEnabled.store(enabled, std::memory_order_relaxed);
}

std::size_t HtileDepthClearBytes_nid_no_patch(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::span<const std::uint32_t> packet, std::uint64_t indirectArguments) {
    if (userData.size() != 6u || packet.size() < 5u || indirectArguments != 0u || compute.PartialGroups() || !compute.groupIdEnable[0] || compute.threadIdComponentCount == 0u || compute.threadIdComponentCount > 3u) return 0u;
    if (packet[1] == 0u || packet[1] > 0x400000u || packet[2] != 1u || packet[3] != 1u || (packet[4] & 0x23u) != 1u) return 0u;
    const auto address = HtileDepthClearAddress(code, userData, compute.numThreads);
    if (address == 0u || (address & 15u) != 0u) return 0u;
    const auto bytes = static_cast<std::uint64_t>(packet[1]) * 1024u;
    return address <= std::numeric_limits<std::uint64_t>::max() - bytes ? static_cast<std::size_t>(bytes) : 0u;
}

void NoteHtileMaskOnGpu(const Context& context, const HtileMaskClear& clear) {
    const auto* recorder = RecorderFor(context);
    if (recorder == nullptr || clear.bytes == 0) return;
    const auto end = clear.address + clear.bytes;
    std::uint64_t note = 0;
    bool covers = false;
    for (const auto& write : recorder->PendingWritesOver(clear.address, clear.bytes)) {
        if (write.note <= note) continue;
        note = write.note;
        covers = write.begin <= clear.address && write.end >= end;
    }
    std::lock_guard lock(htileMasksMutex());
    auto& masks = htileMasks();
    std::erase_if(masks, [&](const HtileMaskWrite& write) { return clear.address < write.clear.address + write.clear.bytes && write.clear.address < end; });
    if (!covers) return;
    constexpr std::size_t KeptMasks = 64;
    if (masks.size() >= KeptMasks) masks.erase(masks.begin());
    masks.push_back({clear, note, recorder->Id(), context.device});
}

void ForgetHtileMasks_nid_no_patch(std::uint64_t recorderId) {
    std::lock_guard lock(htileMasksMutex());
    std::erase_if(htileMasks(), [&](const HtileMaskWrite& write) { return write.recorderId == recorderId; });
}

void NoteCopyDestination(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    std::lock_guard lock(copyDestinationsMutex());
    auto& ranges = copyDestinations();
    std::erase_if(ranges, [&](const auto& range) { return address <= range.first && range.second <= address + bytes; });
    ranges.emplace_back(address, address + bytes);
    if (ranges.size() > 256u) ranges.erase(ranges.begin());
}

bool NoteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes, std::size_t nativeBytes) {
    forgetCopiesInside(htileAddress, bytes);
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(htileAddress, bytes);
    bool existing = false;
    bool completionCandidate = false;
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        if (surface->target.htileAddress != 0u && htileAddress < surface->target.htileAddress + HtileMetadataBytes(surface->target.extent) && surface->target.htileAddress < htileAddress + bytes) surface->expanded = false;
        if (surface->samplingActive && surface->target.htileAddress == htileAddress) {
            const auto& metadata = surface->sampling.initial[1];
            if (surface->target.stencilAddress != 0u) surface->sampling.Refuse("native multisample depth HTILE mask clear of a stencil surface is not implemented");
            else if (nativeBytes != bytes || nativeBytes < metadata.bytes || !GuestMemory::Accessible(reinterpret_cast<const void*>(metadata.address), metadata.bytes) || !GuestMemory::Watched(metadata.address, metadata.bytes)) surface->sampling.Refuse("native multisample depth incomplete or unproven fast-clear transition is not implemented");
            else if (surface->clearPending || surface->clearGeneration != 0u) surface->sampling.Refuse("overlapping native multisample depth clear transitions are not implemented");
            else {
                surface->clearPending = true;
                surface->clearCompletionQueued = false;
                surface->clearAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
                surface->clearPattern.reset();
                surface->clearFillNote = 0u;
                surface->clearStored = false;
                completionCandidate = true;
            }
            existing = true;
            continue;
        }
        if (!HtileFillCovers(surface->target.htileAddress, surface->target.extent, htileAddress, bytes)) continue;
        if (surface->target.samples != 1u) surface->sampling.Refuse("native multisample depth fast-clear transition is not implemented");
        surface->NoteHtileClear(VK_IMAGE_ASPECT_DEPTH_BIT);
        existing = true;
    }
    if (!existing) clearedHtiles()[htileAddress] = bytes;
    return completionCandidate;
}

void ArmHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress || !surface->clearPending) continue;
        const auto& metadata = surface->sampling.initial[1];
        if (bytes < metadata.bytes) continue;
        surface->clearGeneration = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
        surface->clearPending = false;
        if (surface->clearGeneration == 0u) surface->sampling.Refuse("native multisample depth clear requires tracked HTILE generations");
    }
}

void QueueHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress || surface->clearGeneration == 0u || !surface->sampling.refusal.empty()) continue;
        if (bytes >= surface->sampling.initial[1].bytes) surface->clearCompletionQueued = true;
    }
}

void CompleteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress || surface->clearGeneration == 0u) continue;
        const auto& metadata = surface->sampling.initial[1];
        if (bytes < metadata.bytes) continue;
        Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, surface->clearGeneration), "CPU writes raced native multisample depth clear");
        GuestMemory::MarkWritten(metadata.address, metadata.bytes);
        surface->clearCompletionQueued = false;
    }
}

void ClearDepthSurfaces(VkDevice device) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    {
        std::lock_guard lock(htileMasksMutex());
        std::erase_if(htileMasks(), [&](const HtileMaskWrite& write) { return write.device == device; });
    }
    std::lock_guard lock(surfacesMutex());
    drawnDepth = nullptr;
    surfacesSerial.fetch_add(1, std::memory_order_release);
    std::erase_if(depthOwners(), [&](const auto& entry) { return entry.first.first == device; });
    std::erase_if(stencilOwners(), [&](const auto& entry) { return entry.first.first == device; });
    std::erase_if(surfaces(), [&](const auto& surface) { return surface->context.device == device; });
    std::erase_if(expandWrites(), [&](const auto& entry) { return entry.first.first == device; });
    std::erase_if(unstoredExpands(), [&](const auto& entry) { return entry.first.first == device; });
    std::erase_if(planeCopies(), [&](const auto& entry) { return std::get<0>(entry.first) == device; });
    std::erase_if(layeredStorages(), [&](const auto& entry) { return std::get<0>(entry.first) == device; });
    {
        std::lock_guard passesLock(depthToStoragePassesMutex());
        std::erase_if(depthToStoragePasses(), [&](const auto& pass) { return pass->Device() == device; });
    }
    std::unique_lock guardLock(depthGuardMutex());
    const auto removed = std::erase_if(depthGuards(), [&](const auto& range) { return range.device == device; });
    publishDepthGuards(removed != 0);
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

std::optional<VkExtent2D> ActiveDepthExtent(VkDevice device, std::uint64_t address) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->active->load(std::memory_order_acquire) && surface->context.device == device && !surface->retired && surface->target.address == address && surface->target.samples == 1u) return surface->target.extent;
    }
    return std::nullopt;
}

std::size_t DepthPlaneCopyCount() {
    return planeCopies().size();
}

VkDeviceSize DepthPlaneCopyBytes() {
    VkDeviceSize total = 0;
    for (const auto& entry : planeCopies()) total += entry.second->HeldBytes();
    return total;
}

namespace {

thread_local bool depthCopySourceActive = false;

}

std::shared_ptr<Texture> DepthSurfaceFirstSample(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components) {
    Require(resource.dimension == TextureDimension::k2DMsaa, "reading sample 0 of a depth surface needs a multisampled descriptor");
    const auto source = DepthSurfaceTexture(context, words, resource, VkComponentMapping{}, VK_IMAGE_VIEW_TYPE_2D);
    Require(source != nullptr, "reading sample 0 of a multisampled depth surface whose depth plane is not at the descriptor's address is not implemented");
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, resource.baseAddress);
    Require(found != nullptr && found->target.address == resource.baseAddress, "reading sample 0 of a multisampled depth surface whose depth plane is not at the descriptor's address is not implemented");
    return found->FirstSample(*source, components);
}

std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType multisampleViewType) {
    const auto held = [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && !surface->retired && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    };
    {
        std::lock_guard lock(surfacesMutex());
        if (std::none_of(surfaces().begin(), surfaces().end(), held)) return nullptr;
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    settlePendingHtileClear(context, resource.baseAddress);
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, resource.baseAddress);
    if (found == nullptr) return nullptr;
    const auto& base = found->target;
    const bool stencil = base.stencilAddress != 0 && resource.baseAddress == base.stencilAddress;
    if (stencil && BytesPerElement(resource.format) != 1u) return nullptr;
    if (stencil && found->StencilMirrorable()) found->SyncStencilFromMemory(stencilResource(base));
    if (base.samples != 1u && resource.dimension != TextureDimension::k2DMsaa && found->GuestHoldsSamples()) return nullptr;
    if (base.samples != 1u || resource.dimension == TextureDimension::k2DMsaa) {
        Require(base.samples > 1u, "multisample reader overlaps a single-sample depth attachment");
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "a native multisampled depth target is sampled only by recorded draws and dispatches");
        if (depthCopySourceActive) {
            found->CheckResource(resource);
            found->ApplyFastClear();
        } else {
            found->CheckCurrent();
            if (stencil) found->RequireStencilProven();
            found->CheckResource(resource);
            Require(found->pendingClear == 0u, "native multisample depth fast-clear transition is not implemented");
        }
        RecordMemoryBarrier(context, recorder->Commands(), VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        return found->Sampled(words, resource, components, found == drawnDepth, multisampleViewType, depthCopySourceActive);
    }
    const bool exact = resource.width == base.extent.width && resource.height == base.extent.height;
    const bool cropped = !exact && base.gcn && !stencil && found->hostFormat == base.format && (resource.gcnPitch == 0 || resource.gcnPitch == base.extent.width) && resource.width <= base.extent.width && resource.height <= base.extent.height && resource.baseArray == 0 && resource.lastLevel == 0 && resource.dimension == TextureDimension::k2D;
    if ((!exact && !cropped) || resource.baseLevel != 0) return nullptr;
    const bool d16 = base.format == VK_FORMAT_D16_UNORM || base.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = ResolveTextureFormat(resource.format);
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    const bool integer = d16 && format == VK_FORMAT_R16_UINT && !depthBits;
    if (!stencil && format != (d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT) && !depthBits && !integer) return nullptr;
    const bool cube = resource.dimension == TextureDimension::kCube;
    const bool layered = cube || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray != 0);
    const bool sampled = !cropped && (stencil || (!layered && !integer));
    const auto copyFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const bool planes = (format == copyFormat || depthBits) && resource.lastLevel == 0 && !(resource.baseArray != 0 && (cube || !layered || resource.baseArray > resource.depthOrLastArray)) && !(cube && resource.depthOrLastArray != 0 && (resource.depthOrLastArray + 1u) % 6u != 0u) && (resource.dimension == TextureDimension::k2D || resource.dimension == TextureDimension::k2DArray || cube);
    if (!(sampled ? found->SampledAccepts(words, resource) : planes) && (found->OverwrittenInMemory() || found->OverwrittenOnGpu())) {
        retireStoringStale(*found);
        return nullptr;
    }
    if (sampled && !stencil && mirrorable(base)) found->SyncFromMemory(mirrorResource(base));
    found->ApplyFastClear();
    if (sampled) return found->Sampled(words, resource, components, found == drawnDepth);
    Require(found->hostFormat == base.format, "sampling several slices or an integer view of a depth plane that the device stores in another depth format is not implemented");
    const auto layers = cube ? std::max(resource.depthOrLastArray + 1u, 6u) : layered ? resource.depthOrLastArray + 1u : 1u;
    if (!planes) {
        char text[400];
        std::snprintf(text, sizeof(text), "AGC graphics: sampling the depth planes of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), dimension %d, %u slices, levels %u-%u, base slice %u is not implemented (last array %u, depth bits %d, copy format %d)", static_cast<unsigned long long>(base.address), base.extent.width, base.extent.height, static_cast<int>(base.format), resource.width, resource.height, resource.format, static_cast<int>(format), static_cast<int>(resource.dimension), layers, resource.baseLevel, resource.lastLevel, resource.baseArray, resource.depthOrLastArray, depthBits ? 1 : 0, static_cast<int>(copyFormat));
        throw std::runtime_error(text);
    }
    std::vector<DepthPlaneSlice> slices;
    const auto geometry = DescribeSurface(resource);
    Require(!geometry.levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        const auto address = base.address + geometry.GuestLayerOffset(layer);
        auto* slice = depthSlice(context.device, address, base);
        if (slice != nullptr) slice->ApplyFastClear();
        if (slice == nullptr) slices.push_back({});
        else slices.push_back({slice->image, slice == drawnDepth ? 0u : slice->written});
    }
    const auto imageFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
    const VkExtent2D extent{resource.width, resource.height};
    auto& copies = planeCopies();
    const std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat> copyKey{context.device, base.address, extent.width, extent.height, imageFormat};
    auto& copy = copies[copyKey];
    if (copy == nullptr || copy->layers < layers) copy = std::make_unique<DepthPlaneCopy>(context, extent, imageFormat, layers);
    copy->lastUse = ++planeCopyClock();
    copy->lastPresent = Recorder::Presents();
    auto texture = copy->Refresh(slices, resource, components, cube || resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, layered && !cube ? resource.baseArray : 0u);
    if (DepthPlaneReuse() && !texture->HasReuseLookup()) {
        texture->SetReuseLookup([context, lookupWords = std::vector<std::uint32_t>(words.begin(), words.end()), resource, components] { return DepthSurfaceTexture(context, lookupWords, resource, components); });
    }
    evictPlaneCopies(copyKey);
    return texture;
}

bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture) {
    if ((resource.dimension != TextureDimension::k2D && resource.dimension != TextureDimension::k2DMsaa) || resource.baseArray != 0) return false;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    const auto address = resource.baseAddress;
    settlePendingHtileClear(context, address);
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, address);
    if (found == nullptr || !found->Holds(texture)) return false;
    if (!depthCopySourceActive) found->CheckCurrent();
    found->CheckResource(resource);
    return true;
}

void RequireNoNativeDepthFeedback_nid_no_patch(const Context& context, const DepthTarget& target, std::span<const std::shared_ptr<Texture>> sampled) {
    if (target.samples == 1u || sampled.empty()) return;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        if (surface->context.device != context.device || !holdsTarget(surface->target, target)) continue;
        Require(std::none_of(sampled.begin(), sampled.end(), [&](const auto& texture) { return texture != nullptr && surface->Holds(texture.get()); }), "sampling a native multisampled depth target the draw attaches is not implemented");
    }
}

VkImageAspectFlags HtileFillClears(std::uint32_t pattern, bool stencilInHtile) {
    VkImageAspectFlags cleared = (pattern & 0xfu) == 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u;
    if (stencilInHtile && ((pattern >> 8u) & 0x3u) == 0) cleared |= VK_IMAGE_ASPECT_STENCIL_BIT;
    return cleared;
}

std::uint64_t HtileMetadataBytes(VkExtent2D extent) {
    const bool gcn = ShaderRecompiler::GcnGuestShaders();
    const std::uint32_t blockWidth = gcn ? 512u : 1024u;
    const auto blocks = static_cast<std::uint64_t>((extent.width + blockWidth - 1u) / blockWidth) * ((extent.height + 511u) / 512u);
    return blocks * (gcn ? 16384u : 32768u);
}

bool HtileFillCovers(std::uint64_t htile, VkExtent2D extent, std::uint64_t address, std::size_t bytes) {
    if (htile == 0 || htile < address || htile >= address + bytes) return false;
    return address + bytes - htile >= HtileMetadataBytes(extent);
}

bool PlaneFillCovers(std::uint64_t plane, std::uint64_t planeBytes, std::uint64_t address, std::size_t bytes) {
    return plane != 0 && plane >= address && plane + planeBytes <= address + bytes;
}

std::optional<std::uint8_t> StencilFillValue(std::uint32_t pattern) {
    const auto byte = static_cast<std::uint8_t>(pattern & 0xffu);
    if (pattern != byte * 0x01010101u) return std::nullopt;
    return byte;
}

std::optional<float> DepthFillValue(std::uint32_t pattern, bool d16) {
    if (d16) {
        if ((pattern >> 16u) != (pattern & 0xffffu)) return std::nullopt;
        return static_cast<float>(pattern & 0xffffu) / 65535.0f;
    }
    const auto value = std::bit_cast<float>(pattern);
    if (!(value >= 0.0f && value <= 1.0f)) return std::nullopt;
    return value;
}

namespace {

std::optional<float> planeFillDepth(const DepthTarget& target, std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (target.samples != 1u || HtileFillCovers(target.htileAddress, target.extent, address, bytes)) return std::nullopt;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    if (!PlaneFillCovers(target.address, DepthSliceBytes(target.extent, d16 ? 2u : 4u), address, bytes)) return std::nullopt;
    return DepthFillValue(pattern, d16);
}

}

void NoteDepthMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    forgetCopiesInside(address, bytes);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(address, bytes);
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        const auto& target = surface->target;
        const bool touched = std::any_of(surface->sampling.initial.begin(), surface->sampling.initial.end(), [&](const auto& range) { return address < range.address + range.bytes && range.address < address + bytes; });
        if (touched || surface->Overlaps(address, bytes) || (target.htileAddress != 0u && address < target.htileAddress + HtileMetadataBytes(target.extent) && target.htileAddress < address + bytes)) surface->expanded = false;
        if (surface->samplingActive && target.stencilAddress != 0u && !target.htileStencil && target.htileDepthOnly && surface->sampling.initial.size() > 2u) {
            const auto& plane = surface->sampling.initial[2];
            if (address < plane.address + plane.bytes && plane.address < address + bytes) {
                const auto value = StencilFillValue(pattern);
                if (!PlaneFillCovers(plane.address, plane.bytes, address, bytes)) surface->sampling.Refuse("native multisample stencil plane partial fill transition is not implemented");
                else if (!value.has_value()) surface->sampling.Refuse("native multisample stencil plane fill of more than one byte value is not implemented");
                else if (!GuestMemory::Accessible(reinterpret_cast<const void*>(plane.address), plane.bytes) || !GuestMemory::Watched(plane.address, plane.bytes)) surface->sampling.Refuse("native multisample stencil plane fill over unwatched memory is not implemented");
                else {
                    const auto* recorder = RecorderFor(surface->context);
                    const auto refill = surface->stencilClearGeneration != 0u && surface->stencilClearValue == value && surface->stencilFillNote == 0u && recorder != nullptr ? fillNoteAfter(*recorder, plane, surface->stencilFillFloor, *value * 0x01010101u) : 0u;
                    if (surface->stencilClearGeneration != 0u && refill == 0u) surface->sampling.Refuse("overlapping native multisample stencil plane fills are not implemented");
                    else if (refill != 0u) surface->stencilFillNote = refill;
                    else {
                        surface->stencilClearGeneration = GuestMemory::CollectWritesUncached(plane.address, plane.bytes);
                        surface->stencilClearValue = *value;
                        surface->stencilFillNote = recorder != nullptr ? recorder->LastWriteNote(address, bytes) : 0u;
                        surface->stencilFillFloor = recorder != nullptr ? recorder->NewestWriteNote(plane.address, plane.bytes) : 0u;
                        surface->stencilStored = GuestMemory::StoredAllSince_nid_no_patch(plane.address, plane.bytes, plane.generation);
                    }
                }
            }
        }
        const bool overlaps = target.samples != 1u && target.htileAddress != 0u && address < target.htileAddress + HtileMetadataBytes(target.extent) && target.htileAddress < address + bytes;
        if (overlaps && surface->samplingActive && HtileFillCovers(target.htileAddress, target.extent, address, bytes)) {
            const auto& metadata = surface->sampling.initial[1];
            if (!GuestMemory::Accessible(reinterpret_cast<const void*>(metadata.address), metadata.bytes) || !GuestMemory::Watched(metadata.address, metadata.bytes)) surface->sampling.Refuse("native multisample depth metadata fill over unwatched memory is not implemented");
            else {
                const auto* recorder = RecorderFor(surface->context);
                const auto refill = surface->clearGeneration != 0u && surface->clearPattern == pattern && surface->clearFillNote == 0u && recorder != nullptr ? fillNoteAfter(*recorder, metadata, surface->clearFillFloor, pattern) : 0u;
                const auto sameStored = [&] {
                    if (surface->clearGeneration == 0u || surface->clearPattern != pattern) return false;
                    std::vector<std::uint32_t> words(metadata.bytes / sizeof(std::uint32_t));
                    std::memcpy(words.data(), reinterpret_cast<const void*>(metadata.address), metadata.bytes);
                    return std::all_of(words.begin(), words.end(), [&](std::uint32_t word) { return word == pattern; }) && GuestMemory::NewestStamp(metadata.address, metadata.bytes) > surface->clearStoreStamp;
                };
                if (surface->clearPending || (surface->clearGeneration != 0u && !surface->clearPattern.has_value())) surface->sampling.Refuse("native multisample depth metadata fill over a pending HTILE clear dispatch is not implemented");
                else if (surface->clearGeneration != 0u && refill == 0u && !sameStored()) surface->sampling.Refuse("overlapping native multisample depth clear transitions are not implemented");
                else if (refill != 0u) surface->clearFillNote = refill;
                else {
                    surface->clearGeneration = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
                    surface->clearAspects = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
                    surface->clearPattern = pattern;
                    surface->clearStoreStamp = GuestMemory::NewestStamp(metadata.address, metadata.bytes);
                    surface->clearFillNote = recorder != nullptr ? recorder->LastWriteNote(address, bytes) : 0u;
                    surface->clearFillFloor = recorder != nullptr ? recorder->NewestWriteNote(metadata.address, metadata.bytes) : 0u;
                    surface->clearStored = GuestMemory::StoredAllSince_nid_no_patch(metadata.address, metadata.bytes, metadata.generation);
                }
            }
            continue;
        }
        if (overlaps) surface->sampling.Refuse("native multisample depth metadata fill transition is not implemented");
        if (HtileFillCovers(target.htileAddress, target.extent, address, bytes)) {
            const VkImageAspectFlags written = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
            surface->ForgetClear(written);
            surface->NoteHtileClear(HtileFillClears(pattern, target.htileStencil));
            continue;
        }
        if (target.samples != 1) continue;
        if (const auto depth = planeFillDepth(target, address, bytes, pattern)) surface->NotePlaneFill(VK_IMAGE_ASPECT_DEPTH_BIT, *depth, 0);
        if (PlaneFillCovers(target.stencilAddress, DepthSliceBytes(target.extent, 1u), address, bytes)) {
            if (const auto stencil = StencilFillValue(pattern)) surface->NotePlaneFill(VK_IMAGE_ASPECT_STENCIL_BIT, 0.0f, *stencil);
        }
    }
}

std::size_t DiscardFilledDepthPlanes(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (bytes == 0 || !overlapsStalePlane(address, address + bytes)) return 0;
    std::lock_guard lock(surfacesMutex());
    std::size_t discarded = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> remaining;
    for (const auto& surface : surfaces()) {
        if (surface->stencilStale) remaining.push_back(staleStencilRange(*surface));
        if (!surface->AnyStale()) continue;
        const auto& target = surface->target;
        const auto plane = stalePlaneRange(*surface);
        const auto depth = surface->active->load(std::memory_order_acquire) ? planeFillDepth(target, address, bytes, pattern) : std::nullopt;
        if (!surface->syncing && depth.has_value() && PlaneFillCovers(plane.first, plane.second - plane.first, address, bytes)) {
            surface->ClearStale();
            surface->NotePlaneFill(VK_IMAGE_ASPECT_DEPTH_BIT, *depth, 0);
            ++discarded;
        }
        if (surface->AnyStale()) remaining.push_back(plane);
    }
    if (discarded == 0) return 0;
    auto& planes = stalePlanes();
    std::lock_guard staleLock(planes.mutex);
    planes.ranges = std::move(remaining);
    publishStaleHull(planes);
    return discarded;
}

void NoteDepthRenderControl(const DepthRenderControl& draw) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || surface->target.samples == 1u || surface->target.samples != draw.samples || surface->target.address != draw.address) continue;
        surface->renderControl = draw.control;
        const bool depthExpand = (draw.control & 0x40u) != 0u;
        const bool stencilExpand = (draw.control & 0x20u) != 0u;
        if (depthExpand && (stencilExpand || surface->target.stencilAddress == 0u)) surface->expanded = true;
        else if ((draw.control & 3u) != 0u) surface->expanded = false;
    }
}

bool DepthRenderControlDrains(const DepthRenderControl& draw) {
    if (draw.samples == 1u) return false;
    if ((draw.control & 0x60u) != 0u) return true;
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && surface->target.samples == draw.samples && surface->target.address == draw.address; });
}

bool DepthSurfaceAt(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource) {
    std::lock_guard lock(surfacesMutex());
    const auto* found = planeSurface(context.device, resource.baseAddress);
    if (found == nullptr) return false;
    const auto& target = found->target;
    if (resource.width != target.extent.width || resource.height != target.extent.height) return false;
    const bool stencil = target.stencilAddress != 0 && target.stencilAddress == resource.baseAddress;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    return ResolveTextureFormat(resource.format) == expected || depthBits;
}

namespace {

std::vector<LayeredSlice> layeredSlices(const Context& context, const LayeredDepthStorage& storage) {
    const auto& base = storage.base;
    std::vector<LayeredSlice> slices;
    const auto& list = surfaces();
    for (std::uint32_t layer = storage.firstLayer; layer < storage.layers; ++layer) {
        const auto address = base.address + static_cast<std::uint64_t>(layer) * storage.layerPitchBytes;
        const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->context.device == context.device && !surface->retired && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format && surface->target.samples == 1; });
        if (slice != list.rend()) slices.push_back({slice->get(), layer});
    }
    return slices;
}

std::shared_ptr<StorageTexture> layeredDepthStorage(const Context& context, DepthSurface& surface, const GuestTextureResource& resource, std::uint32_t mip) {
    const auto& target = surface.target;
    const auto layers = resource.depthOrLastArray + 1u;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto bytesPerTexel = d16 ? 2u : 4u;
    surface.RequireStorable(resource);
    Require(!DescribeSurface(resource).levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
    auto& entry = layeredStorages()[{context.device, resource.baseAddress, resource.width, resource.height, layers, resource.format, static_cast<int>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseArray}];
    if (entry == nullptr) entry = std::make_unique<LayeredDepthStorage>(context, resource, target, bytesPerTexel, resource.baseArray, layers);
    else static_cast<void>(entry->storage->Refresh());
    const auto slices = layeredSlices(context, *entry);
    for (const auto& slice : slices) slice.surface->ApplyFastClear();
    entry->Copy(slices, true);
    return entry->storage;
}

}

std::shared_ptr<StorageTexture> DepthSurfaceStorage(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip) {
    if (mip != 0 || resource.baseLevel != 0) return nullptr;
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && !surface->retired && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    });
    if (found == list.rend()) return nullptr;
    auto& surface = **found;
    const bool stencil = surface.target.stencilAddress != 0 && surface.target.stencilAddress == resource.baseAddress;
    if (stencil || resource.dimension != TextureDimension::k2DArray || (resource.baseArray == 0 && resource.depthOrLastArray == 0)) return surface.Storage(words, resource, mip);
    if (!surface.StorageViewMatches(words, resource)) return nullptr;
    return layeredDepthStorage(context, surface, resource, mip);
}

DepthCopySourceScope::DepthCopySourceScope(bool active) : previous(depthCopySourceActive) {
    depthCopySourceActive = active;
}

DepthCopySourceScope::~DepthCopySourceScope() {
    depthCopySourceActive = previous;
}

std::array<std::uint32_t, 8> DepthCopySourceWords(const DepthTarget& target) {
    const bool multisampled = target.samples != 1u;
    const auto base = (target.address | target.pipeBankXor) >> 8u;
    const auto metadata = multisampled ? target.htileAddress >> 8u : 0u;
    const auto count = static_cast<std::uint32_t>(std::countr_zero(target.samples));
    const auto width = target.extent.width - 1u, height = target.extent.height - 1u;
    const auto type = multisampled ? 14u : 9u;
    const auto word6 = (multisampled && target.iterate256 ? 1u << 10u : 0u) | (multisampled && target.htilePipeAligned ? 1u << 19u : 0u) | (metadata != 0 ? 1u << 21u : 0u) | (static_cast<std::uint32_t>(metadata & 255u) << 24u);
    return {static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 255u) | (22u << 20u) | ((width & 3u) << 30u), (width >> 2u) | (height << 14u), (type << 28u) | (target.depthSwizzle << 20u) | (count << 16u) | 0x204u, 0u, count << 4u, word6, static_cast<std::uint32_t>(metadata >> 8u)};
}

bool DepthSurfaceStoreBack(const Context& context, const StorageTexture& storage) {
    std::lock_guard lock(surfacesMutex());
    for (auto& [key, entry] : layeredStorages()) {
        if (std::get<0>(key) != context.device || entry->storage.get() != &storage) continue;
        const auto slices = layeredSlices(context, *entry);
        entry->Copy(slices, false);
        for (const auto& slice : slices) {
            slice.surface->written = ++depthWrites;
            slice.surface->NoteHostWrite();
        }
        return true;
    }
    for (const auto& surface : surfaces()) {
        if (surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && surface->StoreBack(storage)) return true;
    }
    for (auto surface = surfaces().rbegin(); surface != surfaces().rend(); ++surface) {
        if ((*surface)->context.device == context.device && (*surface)->StoreBackCovering(storage)) return true;
    }
    return false;
}

bool DepthSurfaceAt(std::uint64_t address) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && !surface->retired && (surface->target.address == address || surface->target.stencilAddress == address); });
}

bool DepthSurfaceOverlaps(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && !surface->retired && surface->context.device == device && surface->Overlaps(address, bytes); });
}

void RetireDepthSurfaces(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || surface->retired || !surface->Overlaps(address, bytes)) continue;
        const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
        if (!(address <= stencilBegin && stencilEnd <= address + bytes)) storeStaleStencil(*surface);
        retire(*surface);
    }
}

namespace {

bool sixteenBitDepth(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT;
}

GuestTextureResource depthPlaneView(std::uint64_t address, VkExtent2D extent, VkFormat format) {
    GuestTextureResource plane{};
    plane.baseAddress = address;
    plane.width = extent.width;
    plane.height = extent.height;
    plane.mipCount = 1;
    plane.tileMode = TextureTileMode::kZ64KBX;
    plane.dimension = TextureDimension::k2D;
    plane.format = FindGuestColorTargetFormat(format, format == VK_FORMAT_R8_UINT ? 1u : format == VK_FORMAT_R16_UNORM ? 2u : 4u).value();
    plane.dstSelX = 4;
    plane.dstSelY = 5;
    plane.dstSelZ = 6;
    plane.dstSelW = 7;
    return plane;
}

void recordPlaneClear(const Context& context, const std::shared_ptr<StorageTexture>& plane, float depth) {
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "depth plane clears need an active recorder");
    const auto commands = recorder->Commands();
    recorder->Keep(plane);
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::FillClear);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkClearColorValue clear{};
    clear.float32[0] = depth;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, plane->Image(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::FillClear, 2);
    recorder->EndGpuTiming(timing, plane->GuestBytes());
}

void recordPlaneCopy(const Context& context, const std::shared_ptr<StorageTexture>& source, const std::shared_ptr<StorageTexture>& destination, VkExtent2D extent) {
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "depth plane copies need an active recorder");
    const auto commands = recorder->Commands();
    recorder->Keep(source);
    recorder->Keep(destination);
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.extent = {extent.width, extent.height, 1};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source->Image(), VK_IMAGE_LAYOUT_GENERAL, destination->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
    recorder->EndGpuTiming(timing, destination->GuestBytes());
}

}

void RunDepthExpandPass(const Context& context, const DepthExpandPass& pass) {
    const auto& source = pass.source;
    const auto failure = [&](std::uint64_t destination, const char* what) {
        char text[288];
        std::snprintf(text, sizeof(text), "AGC graphics: an out-of-place DB expand from 0x%llx (%ux%u) to 0x%llx %s is not implemented", static_cast<unsigned long long>(source.address), source.extent.width, source.extent.height, static_cast<unsigned long long>(destination), what);
        return std::runtime_error(text);
    };
    {
        std::lock_guard lock(surfacesMutex());
        for (const auto address : {pass.destination, pass.stencilDestination}) {
            if (address != 0 && planeSurface(context.device, address) != nullptr) throw failure(address, "into a live depth surface");
        }
    }
    const bool d16 = sixteenBitDepth(source.format);
    expandsRun.fetch_add(1, std::memory_order_relaxed);
    bool copied = false;
    for (const bool stencil : {false, true}) {
        const auto address = stencil ? pass.stencilDestination : pass.destination;
        if (address == 0) continue;
        const auto planeFormat = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const auto destination = CachedStorageSurface(context, depthPlaneView(address, source.extent, planeFormat));
        if (destination == nullptr) throw failure(address, "without a storage image of the write base");
        const ExpandKey key{context.device, address};
        enum class Read { Surface, Clear, GuestBytes } read = Read::Surface;
        ExpandWrite wrote;
        {
            std::lock_guard lock(surfacesMutex());
            const auto owner = depthOwners().find({context.device, source.address});
            if (owner != depthOwners().end()) {
                auto* surface = owner->second.surface;
                const auto& target = surface->target;
                if (sixteenBitDepth(target.format) != d16 || surface->hostFormat != target.format || target.samples != 1 || target.extent.width != source.extent.width || target.extent.height != source.extent.height) throw failure(address, "from a depth surface of another extent, format or sample count");
                if (stencil && target.stencilAddress != source.stencilAddress) throw failure(address, "of a stencil plane its depth surface does not hold");
                surface->ApplyFastClear();
                const auto last = expandWrites().find(key);
                if (last != expandWrites().end() && last->second.source == surface && last->second.written == surface->written && last->second.destination.lock() == destination && last->second.version == destination->Version()) continue;
                if (stencil) surface->CopyStencilTo(destination);
                else surface->CopyDepthTo(destination);
                wrote.source = surface;
                wrote.written = surface->written;
            } else {
                read = !stencil && findClearedHtile(source) != clearedHtiles().end() ? Read::Clear : Read::GuestBytes;
            }
        }
        if (read == Read::Clear) recordPlaneClear(context, destination, source.clearDepth);
        if (read == Read::GuestBytes) {
            const auto plane = CachedStorageSurface(context, depthPlaneView(stencil ? source.stencilAddress : source.address, source.extent, planeFormat));
            if (plane == nullptr) throw failure(address, "without a storage image of the read base");
            recordPlaneCopy(context, plane, destination, source.extent);
        }
        destination->MarkDirty();
        wrote.destination = destination;
        wrote.version = destination->Version();
        {
            std::lock_guard lock(surfacesMutex());
            expandWrites()[key] = std::move(wrote);
            unstoredExpands()[key] = static_cast<std::size_t>(destination->GuestBytes());
        }
        copied = true;
        static bool reported[2][3] = {};
        if (!std::exchange(reported[stencil ? 1 : 0][static_cast<int>(read)], true)) {
            const auto from = static_cast<unsigned long long>(stencil ? source.stencilAddress : source.address);
            const auto written = static_cast<unsigned long long>(address);
            if (read == Read::Surface) std::fprintf(stderr, "[gpu] DB expand: the %s plane of 0x%llx (%ux%u) is written at 0x%llx\n", stencil ? "stencil" : "depth", from, source.extent.width, source.extent.height, written);
            else std::fprintf(stderr, "[gpu] DB expand: 0x%llx (%ux%u) has no depth surface; %s written at 0x%llx\n", from, source.extent.width, source.extent.height, read == Read::Clear ? "DB_DEPTH_CLEAR (an HTILE clear is pending) is" : "its guest bytes are", written);
        }
    }
    if (!copied) expandsSkipped.fetch_add(1, std::memory_order_relaxed);
}

void StoreDepthExpandWrites(const Context& context) {
    std::vector<std::pair<std::uint64_t, std::size_t>> planes;
    {
        std::lock_guard lock(surfacesMutex());
        auto& unstored = unstoredExpands();
        for (auto entry = unstored.begin(); entry != unstored.end();) {
            if (entry->first.first != context.device) {
                ++entry;
                continue;
            }
            planes.emplace_back(entry->first.second, entry->second);
            entry = unstored.erase(entry);
        }
    }
    std::vector<std::pair<std::uint64_t, std::size_t>> kept;
    for (const auto& [address, bytes] : planes) {
        if (!HostImportOverlaps(context, address, bytes)) {
            const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{address, address + bytes}}};
            if (StorageTexture::AnyPendingOverlaps(range)) kept.emplace_back(address, bytes);
            continue;
        }
        if (StorageTexture::FlushPending(address, bytes, nullptr, "DB expand")) expandsStored.fetch_add(1, std::memory_order_relaxed);
    }
    if (kept.empty()) return;
    std::lock_guard lock(surfacesMutex());
    for (const auto& [address, bytes] : kept) unstoredExpands().try_emplace({context.device, address}, bytes);
}

DepthExpandStatistics DepthExpandCounts() {
    return {expandsRun.load(std::memory_order_relaxed), expandsSkipped.load(std::memory_order_relaxed), expandsStored.load(std::memory_order_relaxed)};
}

void RetireCoveredDepthSurfaces(VkDevice device, std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || surface->retired || !surface->CoveredBy(written, extents)) continue;
        if (!surface->PlaneCoveredBy(true, written, extents)) storeStaleStencil(*surface);
        retire(*surface);
        if (surface->target.samples == 1u || !surface->samplingActive || surface->hostWritten->load(std::memory_order_acquire)) continue;
        std::unique_lock guardLock(depthGuardMutex());
        surface->active->store(false, std::memory_order_release);
        std::erase_if(depthGuards(), [&](const auto& range) { return range.active == surface->active; });
        anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
    }
}

bool DirectDepthCopy(const Context& context, VkFormat depthFormat, VkImageAspectFlagBits aspect, VkFormat colorFormat) {
    if (!context.maintenance8 || !directDepthCopy.load(std::memory_order_relaxed)) return false;
    const auto bytes = aspectCopyBytes(depthFormat, aspect);
    return bytes != 0 && singleComponent(colorFormat, bytes);
}

std::uint64_t DirectDepthCopyCount() {
    return directDepthCopies.load(std::memory_order_relaxed);
}

void SetDirectDepthCopy(bool enabled) {
    directDepthCopy.store(enabled && std::getenv("APS5_NO_DIRECT_DEPTH_COPY") == nullptr, std::memory_order_relaxed);
}

void SetDepthTransferPasses(bool enabled) {
    depthTransferPasses.store(enabled && std::getenv("APS5_NO_DEPTH_TRANSFER_PASS") == nullptr, std::memory_order_relaxed);
}

std::uint64_t DepthTransferPassCount() {
    return depthTransferPassCount.load(std::memory_order_relaxed);
}

bool DepthUploadImage(const Context& context) {
    return context.depthUploadImage || forcedDepthUploadImage.load(std::memory_order_relaxed);
}

std::uint64_t DepthUploadCount() {
    return depthUploads.load(std::memory_order_relaxed);
}

void SetDepthUploadImage(bool enabled) {
    forcedDepthUploadImage.store(enabled && std::getenv("APS5_NO_DEPTH_UPLOAD_IMAGE") == nullptr, std::memory_order_relaxed);
}

}
