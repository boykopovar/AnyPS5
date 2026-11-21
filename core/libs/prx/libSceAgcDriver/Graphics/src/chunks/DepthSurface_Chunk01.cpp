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
    std::shared_ptr<Buffer> importedSamples;
    bool importedPerSample = false;
    bool importedByCopy = false;

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

std::uint64_t newestFillNote(const Recorder& recorder, const GuestMemory::UnchangedQuery& range, std::uint64_t floor, std::uint32_t word) {
    std::uint64_t newest = 0u;
    bool proves = false;
    for (const auto& write : recorder.PendingWritesOver(range.address, range.bytes)) {
        if (write.note <= newest) continue;
        newest = write.note;
        proves = write.note >= floor && write.begin <= range.address && write.end >= range.address + range.bytes && write.word == static_cast<std::int64_t>(word);
    }
    return proves ? newest : 0u;
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

bool bareDepthTarget(const DepthTarget& target) {
    return target.samples != 1u && target.format == VK_FORMAT_D32_SFLOAT && target.stencilAddress == 0u && target.htileAddress == 0u && !target.htileStencil;
}

constexpr VkImageUsageFlags importedSampleUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constexpr VkImageUsageFlags copiedSampleUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr VkImageCreateFlags importedSampleFlags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

bool detilesDepthSamples(const Context& context, const DepthTarget& target, VkImageUsageFlags sampleUsage) {
    if (context.detiler == nullptr || context.imageFormatProperties == nullptr || !context.storageImageMultisample || !context.storageImageWriteWithoutFormat) return false;
    const auto supports = [&](VkFormat format, VkImageUsageFlags usage, VkImageCreateFlags flags) {
        VkImageFormatProperties properties{};
        return context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, flags, &properties) == VK_SUCCESS && (properties.sampleCounts & target.samples) != 0u && target.extent.width <= properties.maxExtent.width && target.extent.height <= properties.maxExtent.height;
    };
    return supports(TextureDetiler::ImageElementFormat(4u), VK_IMAGE_USAGE_STORAGE_BIT, 0u) && supports(VK_FORMAT_R32_SFLOAT, sampleUsage, importedSampleFlags);
}

bool importsDepthSamples(const Context& context, const DepthTarget& target) {
    return context.sampleRateShading && detilesDepthSamples(context, target, importedSampleUsage);
}

bool copiesDepthSamples(const Context& context, const DepthTarget& target) {
    return context.depthRangeUnrestricted && DirectDepthCopy(context, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FORMAT_R32_SFLOAT) && detilesDepthSamples(context, target, copiedSampleUsage);
}

const char* readDepthPlane(const Context& context, const DepthTarget& target, const MultisampleTextureLayout& layout, DepthSamplingState& state) {
    const auto& plane = state.initial.front();
    std::vector<std::byte> tiled(plane.bytes);
    GuestMemory::Read(plane.address, std::span(tiled));
    const auto linearBytes = static_cast<std::size_t>(layout.LinearBytes());
    auto samples = std::make_shared<Buffer>(context, linearBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto linear = samples->Bytes().first(linearBytes);
    layout.Detile(tiled, linear);
    std::uint32_t first = 0u;
    std::memcpy(&first, linear.data(), sizeof(first));
    bool uniform = true;
    bool exact = true;
    for (std::size_t offset = 0u; offset < linearBytes; offset += sizeof(std::uint32_t)) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, linear.data() + offset, sizeof(bits));
        const auto depth = std::bit_cast<float>(bits);
        uniform = uniform && bits == first;
        exact = exact && (bits == 0u || (std::isnormal(depth) && depth > 0.0f && depth <= 1.0f));
    }
    const auto value = std::bit_cast<float>(first);
    if (uniform && value >= 0.0f && value <= 1.0f) {
        state.initialDepth = value;
        return nullptr;
    }
    const bool drawn = exact && importsDepthSamples(context, target);
    if (!drawn && !copiesDepthSamples(context, target)) {
        if (!exact) return "native multisample depth plane without HTILE holds a sample other than +0 or a normal depth in (0, 1], which only a bit copy through VK_KHR_maintenance8 with VK_EXT_depth_range_unrestricted imports";
        return "importing an uneven native multisample depth plane requires multisampled storage image writes without format, sample-rate shading and a sampled multisampled R32 float image";
    }
    state.importedSamples = std::move(samples);
    state.importedPerSample = true;
    state.importedByCopy = !drawn;
    return nullptr;
}

DepthSamplingState prepareDepthPlaneSampling(const Context& context, const DepthTarget& target, DepthSamplingState state) {
    const auto refuse = [&](const char* reason) {
        state.refusal = reason;
        state.importedSamples.reset();
        state.importedPerSample = false;
        state.importedByCopy = false;
        return state;
    };
    if (target.depthSwizzle != 24u || target.firstSlice != 0u || target.lastSlice != 0u) return refuse("native multisample depth arrays or non-Z_X layout are not implemented");
    if ((target.pipeBankXor & ~0xcf00u) != 0u) return refuse("unsupported native multisample depth XOR");
    if (target.address == 0u || (target.address & 65535u) != 0u) return refuse("native multisample depth base is not aligned");
    state.initial = {{target.address, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(target.extent, 4u, target.samples)), 0u}};
    auto& plane = state.initial.front();
    if (plane.address > std::numeric_limits<std::uint64_t>::max() - plane.bytes) return refuse("native depth import range overflows");
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(plane.address), plane.bytes) || !GuestMemory::Watched(plane.address, plane.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
    const MultisampleTextureLayout layout(TextureTileMode::kZ64KBX, 4u, target.samples, target.extent.width, target.extent.height, 1u, target.pipeBankXor);
    if (layout.Bytes() != plane.bytes) return refuse("native multisample depth plane layout differs from its guarded range");
    if (const auto alias = MultisampleColorTargetOver(plane.address, plane.bytes)) {
        const auto depthValue = aliasedDepth(*alias, plane, target.samples);
        if (!depthValue) {
            reportAlias(*alias, plane, "depth");
            return refuse(depthAliasRefusal);
        }
        state.aliasDepth = *depthValue;
    }
    if (auto* recorder = RecorderFor(context)) recorder->SyncThrough(plane.address, plane.bytes);
    StorageTexture::FlushPending(plane.address, plane.bytes);
    GuestMemory::FlushGpuWrites(plane.address, plane.bytes);
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(plane.address), plane.bytes) || !GuestMemory::Watched(plane.address, plane.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
    plane.generation = GuestMemory::CollectWritesUncached(plane.address, plane.bytes);
    if (plane.generation == 0u) return refuse("native multisample depth import requires tracked generations");
    if (state.aliasDepth) state.initialDepth = *state.aliasDepth;
    else if (const auto* reason = readDepthPlane(context, target, layout, state)) return refuse(reason);
    GuestMemory::CollectWritesUncached(plane.address, plane.bytes);
    if (!GuestMemory::UnchangedSinceAll(state.initial)) return refuse("guest writes raced native multisample depth import");
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
    if (bareDepthTarget(target)) return prepareDepthPlaneSampling(context, target, std::move(state));
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

}
}
