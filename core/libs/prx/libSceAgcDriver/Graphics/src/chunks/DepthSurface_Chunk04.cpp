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
    if (mirrorBytes == 0) mirrorBytes = SurfaceGuestBytes(mirrorResource(target));
    return mirrorBytes;
}

std::uint64_t DepthSurface::StencilMirrorBytes() const {
    if (stencilMirrorBytes == 0) stencilMirrorBytes = SurfaceGuestBytes(stencilResource(target));
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

bool stencilWrittenSinceBaseline(const DepthSurface& surface, std::uint64_t begin, std::size_t bytes) {
    if (GuestMemory::Watched(begin, bytes)) {
        GuestMemory::CollectWritesUncached(begin, bytes);
        return surface.stencilGeneration != 0u && GuestMemory::WrittenSince(begin, bytes, surface.stencilGeneration);
    }
    return surface.stencilBaseline.size() == bytes && !GuestMemory::EqualsCommittedUnsynced(begin, surface.stencilBaseline);
}

void storeStaleStencil(DepthSurface& surface) {
    if (!surface.stencilStale || surface.syncing || !surface.StencilMirrorable()) return;
    const auto [begin, end] = staleStencilRange(surface);
    const auto bytes = static_cast<std::size_t>(end - begin);
    if (GuestMemory::CommittedRanges(begin, bytes).empty()) return;
    if (unmappedSince(begin, end, surface.stencilMark) || stencilWrittenSinceBaseline(surface, begin, bytes)) {
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
    GuestMemory::CollectWritesUncached(surface.target.address, static_cast<std::size_t>(planeBytes));
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

}
