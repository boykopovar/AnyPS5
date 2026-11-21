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

VkImageView RepeatDepthSurfaceView(const Context& context, const DepthTarget& target, bool depthWritten, bool stencilWritten) {
    if (target.depthPartiallyResident || target.stencilPartiallyResident) return VK_NULL_HANDLE;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    auto* surface = drawnDepth;
    if (surface == nullptr || target.samples != 1u || surface->retired || surface->syncing || surface->pendingClear != 0u || surface->context.device != context.device || !surface->active->load(std::memory_order_acquire) || !sameSurface(surface->target, target)) return VK_NULL_HANDLE;
    surface->written = ++depthWrites;
    if (depthWritten && mirrorable(target)) {
        surface->NoteDepthWork();
        if (!surface->AllStale()) {
            const auto planeBytes = surface->MirrorBytes();
            surface->MarkAllStale(planeBytes);
            noteStalePlane(target.address, target.address + planeBytes);
        }
    }
    if (stencilWritten) surface->MarkStencilStale(true);
    return surface->view;
}

const char* DepthPlanesObligation(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || !surface->Overlaps(address, bytes)) continue;
        if (surface->AnyStale()) return "a depth plane to store";
        if (!surface->retired) return "a depth surface to retire";
    }
    return nullptr;
}

const char* DepthTargetObligation(const Context& context, const DepthTarget& target, bool depthWritten) {
    const bool nativeOwner = RoutesToNativeDepthOwner(target);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    DepthSurface* surface = nullptr;
    for (const auto& candidate : surfaces()) {
        if (candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameSurface(candidate->target, target)) {
            surface = candidate.get();
            break;
        }
    }
    if (surface == nullptr || surface != drawnDepth) return "another depth surface";
    if (surface->pendingClear != 0u) return "a pending depth clear";
    if (depthWritten && mirrorable(target) && !surface->AnyStale()) return "marking a depth write";
    if (target.htileAddress != 0u) {
        const auto& cleared = clearedHtiles();
        if (std::ranges::any_of(cleared, [&](const auto& entry) { return HtileFillCovers(target.htileAddress, target.extent, entry.first, entry.second); })) return "an HTILE clear to take";
        const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(target.extent));
        auto* recorder = RecorderFor(context);
        const VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        if (recorder != nullptr && recorder->PendingWriteOverlaps(target.htileAddress, bytes)) {
            const auto landed = htileAfterPendingFills(*recorder, target.htileAddress, bytes);
            if (landed.has_value() ? (aspects & HtileFillClears(*landed, target.htileStencil)) != 0u : htileAfterMaskClear(*recorder, target, bytes).has_value()) return "a pending HTILE clear";
        } else if (!nativeOwner) {
            const std::span words(reinterpret_cast<const std::uint32_t*>(target.htileAddress), bytes / 4u);
            VkImageAspectFlags clears = aspects;
            for (const auto word : words) clears &= HtileFillClears(word, target.htileStencil);
            if (clears != 0u) return "an HTILE clear to store back";
        }
    }
    const auto owner = depthOwners().find({context.device, target.address});
    if (owner != depthOwners().end() && owner->second.surface != surface && owner->second.surface->active->load(std::memory_order_acquire) && !owner->second.surface->retired) return "a plane ownership move";
    if (!surface->AnyStale() && surface->MirrorStorage() != nullptr) {
        if (const auto* owed = surface->MirrorStorage()->RefreshObligation()) return owed;
    }
    return nullptr;
}

bool DepthTargetOverlaps(const DepthTarget& target, std::uint64_t address, std::uint64_t bytes) {
    if (bytes == 0u) return false;
    const auto limit = std::numeric_limits<std::uint64_t>::max();
    if (address > limit - bytes || target.firstSlice != 0u || target.lastSlice != 0u) return true;
    const auto dimensionLimit = std::numeric_limits<std::uint32_t>::max() - 1023u;
    if (target.extent.width == 0u || target.extent.height == 0u || target.extent.width > dimensionLimit || target.extent.height > dimensionLimit) return true;
    if (target.samples != 1u && target.samples != 2u && target.samples != 4u && target.samples != 8u) return true;
    if (target.samples != 1u && (target.extent.width > 16384u || target.extent.height > 16384u)) return true;
    if (static_cast<std::uint64_t>(target.extent.width) + 1023u > limit / (static_cast<std::uint64_t>(target.extent.height) + 1023u) / 4u / target.samples) return true;
    const auto overlaps = [&](std::uint64_t begin, std::uint64_t size) {
        return size != 0u && (begin > limit - size || (address < begin + size && begin < address + bytes));
    };
    const auto planeBytes = [&](std::uint32_t bytesPerTexel) { return target.samples == 1u ? DepthSliceBytes(target.extent, bytesPerTexel) : MultisampledDepthBytes_nid_no_patch(target.extent, bytesPerTexel, target.samples); };
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    return overlaps(target.address, planeBytes(d16 ? 2u : 4u)) || (target.stencilAddress != 0u && overlaps(target.stencilAddress, planeBytes(1u))) || (target.htileAddress != 0u && overlaps(target.htileAddress, HtileMetadataBytes(target.extent)));
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
    if (surface != nullptr && overwrittenStencil && surface->PublishedUnderStorage()) {
        std::unique_lock guardLock(depthGuardMutex());
        surface->active->store(false, std::memory_order_release);
        std::erase_if(depthGuards(), [&](const auto& range) { return range.active == surface->active; });
        anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
        surface = nullptr;
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
    if (surface->retired) surfacesSerial.fetch_add(1, std::memory_order_release);
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
    if (stencilWritten || created || stencilRebound) surface->MarkStencilStale(stencilWritten);
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
                    const auto stencilBytes = static_cast<std::size_t>(stencilEnd - stencilBegin);
                    if (GuestMemory::CommittedRanges(stencilBegin, stencilBytes).empty() ||
                        unmappedSince(stencilBegin, stencilEnd, surface->stencilMark) ||
                        stencilWrittenSinceBaseline(*surface, stencilBegin, stencilBytes)) {
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

}
}
