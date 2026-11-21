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
namespace {
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
            else {
                surface->clearGeneration = 0u;
                surface->clearFillFloor = 0u;
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
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress) continue;
        if (bytes < surface->sampling.initial[1].bytes) continue;
        ++surface->clearDispatchesPending;
        if (surface->clearGeneration == 0u || !surface->sampling.refusal.empty()) continue;
        surface->clearCompletionQueued = true;
    }
}

void CompleteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress) continue;
        const auto& metadata = surface->sampling.initial[1];
        if (bytes < metadata.bytes) continue;
        if (surface->clearDispatchesPending != 0u && --surface->clearDispatchesPending == 0u) surface->clearCompletionQueued = false;
        if (surface->clearGeneration == 0u) continue;
        Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, surface->clearGeneration), "CPU writes raced native multisample depth clear");
        GuestMemory::MarkWritten(metadata.address, metadata.bytes);
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

}
}
}
