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
    const bool bare = bareDepthTarget(target);
    const bool htile = !uniformPlaneTarget(target) && !bare;
    const bool publishable = bare ? publication.size() == 1u : target.format == VK_FORMAT_D32_SFLOAT_S8_UINT && publication.size() == (htile ? 3u : 2u);
    Require(publishable && hostFormat == target.format, "sample-preserving guest publication of native multisample depth is not implemented for this owner");
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
        refreshSampleShadow(true, HasStencil(hostFormat));
        if (auto* recorder = RecorderFor(context)) recorder->Sync();
    }
    std::array<std::vector<std::byte>, 3> result;
    for (std::size_t index = 0u; index < publication.size(); ++index) {
        if (htile && index == 1u) continue;
        const auto elementBytes = index == 0u ? 4u : 1u;
        const auto aspect = index == 0u ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
        const MultisampleTextureLayout layout(TextureTileMode::kZ64KBX, elementBytes, target.samples, target.extent.width, target.extent.height, 1u, target.pipeBankXor);
        Require(layout.Bytes() == publication[index].range.bytes, "native multisample depth publication layout differs from its guarded range");
        const auto linear = readBackSamples(aspect, layout.LinearBytes() / elementBytes);
        result[index] = publication[index].Expand();
        layout.TileHoisted(linear->Bytes().first(static_cast<std::size_t>(layout.LinearBytes())), result[index]);
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

}
}
