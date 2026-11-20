#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::size_t StorageTexture::StoreDrawnLinear(const char* reason) {
    auto& pending = Pending();
    if (pending.drawnCount.load(std::memory_order_acquire) == 0 || Recorder::InCompletion()) return 0;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    {
        std::lock_guard lock(pending.mutex);
        for (const auto& entry : pending.drawn) {
            if (entry.texture->dirty) ranges.emplace_back(entry.texture->descriptor.baseAddress, static_cast<std::size_t>(entry.texture->guestBytes));
        }
    }
    std::size_t stored = 0;
    for (const auto& [address, bytes] : ranges) {
        if (FlushPending(address, bytes, nullptr, reason)) ++stored;
    }
    return stored;
}

std::size_t StorageTexture::StoreDrawnLinearTouching(const char* reason, const std::function<bool(std::uint64_t, std::uint64_t)>& touches, bool all) {
    auto& pending = Pending();
    if (pending.drawnCount.load(std::memory_order_acquire) == 0 || Recorder::InCompletion()) return 0;
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    {
        std::lock_guard lock(pending.mutex);
        for (const auto& entry : pending.drawn) {
            if (!entry.texture->dirty) continue;
            const auto begin = entry.texture->descriptor.baseAddress;
            const auto end = begin + entry.texture->guestBytes;
            if (!all && !touches(begin, end)) continue;
            ranges.emplace_back(begin, static_cast<std::size_t>(entry.texture->guestBytes));
        }
    }
    if (ranges.empty()) return 0;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::size_t stored = 0;
    for (const auto& [address, bytes] : ranges) {
        if (FlushPending(address, bytes, nullptr, reason)) ++stored;
    }
    return stored;
}

bool StorageTexture::AnyPending() {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    return pending.textures.begin() != pending.textures.end();
}

bool StorageTexture::StoreAtFlipRequested(const char* value) {
    if (value == nullptr) return false;
    if (std::strcmp(value, "1") == 0) return true;
    throw std::runtime_error(std::string("APS5_STORE_AT_FLIP=") + value + ": expected 1");
}

bool StorageTexture::StoreAtLabelRequested(const char* value) {
    if (value == nullptr) return false;
    if (std::strcmp(value, "1") == 0) return true;
    throw std::runtime_error(std::string("APS5_STORE_AT_LABEL=") + value + ": expected 1");
}

std::uint64_t StorageTexture::EightBitWrites() {
    return eightBitWrites().load(std::memory_order_acquire);
}

std::shared_ptr<StorageTexture> StorageTexture::FindPending(std::uint64_t address, std::uint64_t bytes) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    StorageTexture* containing = nullptr;
    for (auto* texture : pending.textures) {
        if (texture->descriptor.baseAddress != address || texture->guestBytes < bytes) continue;
        if (texture->guestBytes == bytes) return texture->weak_from_this().lock();
        // Containment, not equality: a descriptor of a chain's first mips (its own guestBytes are
        // shorter) is served by the chain's image; CanCopyFrom then checks the geometry.
        if (containing == nullptr) containing = texture;
    }
    return containing != nullptr ? containing->weak_from_this().lock() : nullptr;
}

VkDevice StorageTexture::Device() const {
    return context.device;
}

std::shared_ptr<StorageTexture> StorageTexture::FindPendingSlice(std::uint64_t address, std::uint64_t bytes) {
    if (auto whole = FindPending(address, bytes)) return whole;
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    StorageTexture* found = nullptr;
    for (auto* texture : pending.textures) {
        const auto base = texture->descriptor.baseAddress;
        const auto layers = texture->geometry.imageLayers;
        if (texture->geometry.thick || layers <= 1 || bytes == 0 || address <= base || texture->guestBytes % layers != 0 || texture->guestBytes / layers != bytes) continue;
        if ((address - base) % bytes != 0 || (address - base) / bytes >= layers) continue;
        if (found != nullptr) {
            char message[256];
            std::snprintf(message, sizeof(message), "AGC graphics: 0x%llx+0x%llx is a slice of the pending array images 0x%llx and 0x%llx: which one holds its results is not modeled", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(found->descriptor.baseAddress), static_cast<unsigned long long>(base));
            throw std::runtime_error(message);
        }
        found = texture;
    }
    return found != nullptr ? found->weak_from_this().lock() : nullptr;
}

bool PendingStorageOverlaps(std::uint64_t address, std::size_t bytes, const StorageTexture* except) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    if (!pending.textures.MayOverlap(address, bytes)) return false;
    for (const auto* texture : pending.textures) {
        // A free function (declared in ShaderResources.hpp): the overlap is computed from the public
        // surface description rather than the private helper.
        const auto begin = texture->Descriptor().baseAddress;
        if (texture != except && address < begin + texture->GuestBytes() && begin < address + bytes) return true;
    }
    return false;
}

std::uint64_t StorageTexture::StampKeepingNewerResults(std::uint64_t address, std::size_t bytes, std::uint64_t recorded, const std::function<std::uint64_t()>& stamp) {
    struct Candidate {
        std::shared_ptr<StorageTexture> texture;
        std::uint32_t unit;
        bool drawn;
    };
    struct Kept {
        std::shared_ptr<StorageTexture> texture;
        std::uint32_t unit;
        std::uint64_t from;
        std::uint64_t to;
        std::uint64_t since;
        bool whole;
        bool drawn;
    };
    std::vector<Candidate> candidates;
    if (recorded != 0 && bytes != 0) {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto consider = [&](StorageTexture* texture) {
            if ((!texture->blockUnits && !texture->drawnListed) || !texture->overlaps(address, bytes)) return;
            auto alive = texture->weak_from_this().lock();
            if (alive == nullptr) return;
            for (std::uint32_t unit = 0; unit < texture->trackedLayers; ++unit) {
                const auto begin = texture->layerBegin(unit);
                const auto unitBytes = texture->layerBytes(unit);
                if (!texture->layerPending[unit] || begin >= address + bytes || address >= begin + unitBytes) continue;
                if (texture->layerGeneration[unit] >= recorded) candidates.push_back({alive, unit, texture->drawnListed});
            }
        };
        for (auto* texture : pending.textures) consider(texture);
        for (auto* texture : pending.flushing) consider(texture);
    }
    constexpr std::uint64_t block = 65536;
    std::vector<Kept> kept;
    for (const auto& [texture, unit, drawn] : candidates) {
        const auto begin = texture->layerBegin(unit);
        const auto end = begin + texture->layerBytes(unit);
        GuestMemory::CollectWritesUncached(begin, static_cast<std::size_t>(end - begin));
        if (GuestMemory::UnchangedSince(begin, static_cast<std::size_t>(end - begin), texture->layerGeneration[unit])) {
            kept.push_back({texture, unit, begin, end, texture->layerGeneration[unit], true, drawn});
            continue;
        }
        if (!drawn) continue;
        const auto first = std::max(begin, address) / block;
        const auto last = (std::min(end, address + bytes) - 1) / block;
        const auto from = std::max(begin, first * block);
        const auto to = std::min(end, (last + 1) * block);
        std::vector<std::uint64_t> generations(static_cast<std::size_t>(last - first + 1));
        for (std::size_t k = 0; k < generations.size(); ++k) generations[k] = texture->unitBlockGeneration(unit, (first + k) * block);
        std::vector<std::uint8_t> changed(generations.size());
        if (!GuestMemory::ChangedBlocks(from, static_cast<std::size_t>(to - from), generations, changed)) continue;
        for (std::size_t k = 0; k < generations.size(); ++k) {
            const auto at = (first + k) * block;
            const auto blockFrom = std::max(at, begin);
            const auto blockTo = std::min(at + block, end);
            const bool edge = blockFrom != at || blockTo != at + block;
            if (changed[k] == GuestMemory::BlockWritten && (!edge || !GuestMemory::UnchangedSince(blockFrom, static_cast<std::size_t>(blockTo - blockFrom), generations[k]))) continue;
            if (!kept.empty() && !kept.back().whole && kept.back().texture == texture && kept.back().unit == unit && kept.back().since == generations[k] && kept.back().to == blockFrom) kept.back().to = blockTo;
            else kept.push_back({texture, unit, blockFrom, blockTo, generations[k], false, true});
        }
    }
    const auto generation = stamp();
    if (generation == 0 || kept.empty()) return generation;
    std::vector<Kept> confirmed;
    for (const auto& entry : kept) {
        if ((entry.whole && !entry.drawn) || GuestMemory::UnchangedSinceCollected(entry.from, static_cast<std::size_t>(entry.to - entry.from), entry.since)) {
            confirmed.push_back(entry);
            continue;
        }
        if (entry.from / block == (entry.to - 1) / block) continue;
        const auto last = (std::min(entry.to, address + bytes) - 1) / block;
        for (auto at = std::max(entry.from, address) / block * block; at <= last * block; at += block) {
            const auto from = std::max(at, entry.from);
            const auto to = std::min(at + block, entry.to);
            if (GuestMemory::UnchangedSinceCollected(from, static_cast<std::size_t>(to - from), entry.since)) confirmed.push_back({entry.texture, entry.unit, from, to, entry.since, false, true});
        }
    }
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    for (const auto& entry : confirmed) {
        auto& texture = *entry.texture;
        if (!texture.layerPending[entry.unit]) continue;
        if (entry.whole) {
            texture.layerGeneration[entry.unit] = generation;
            texture.refreshGeneration();
            continue;
        }
        const auto spanBegin = texture.descriptor.baseAddress / block;
        if (texture.completionKept.empty()) texture.completionKept.assign(static_cast<std::size_t>((texture.descriptor.baseAddress + texture.guestBytes - 1) / block - spanBegin + 1), 0);
        for (auto at = entry.from / block; at <= (entry.to - 1) / block; ++at) texture.completionKept[static_cast<std::size_t>(at - spanBegin)] = generation;
    }
    return generation;
}

bool StorageTexture::AnyPendingOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    if (ranges.empty()) return false;
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    const auto overlapsAny = [&](const StorageTexture* texture) {
        for (const auto& [begin, end] : ranges) {
            if (end > begin && texture->overlaps(begin, static_cast<std::size_t>(end - begin))) return true;
        }
        return false;
    };
    if (std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.second > range.first && pending.textures.MayOverlap(range.first, static_cast<std::size_t>(range.second - range.first)); })) {
        for (const auto* texture : pending.textures) {
            if (overlapsAny(texture)) return true;
        }
    }
    for (const auto* texture : pending.flushing) {
        if (overlapsAny(texture)) return true;
    }
    return false;
}

bool StorageTexture::AnyPendingUnitsOverlap(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    for (const auto& [begin, end] : ranges) {
        if (end <= begin) continue;
        const auto bytes = static_cast<std::size_t>(end - begin);
        for (const auto* texture : pending.flushing) {
            if (texture->overlaps(begin, bytes)) return true;
        }
        if (!pending.textures.MayOverlap(begin, bytes)) continue;
        for (const auto* texture : pending.textures) {
            if (!texture->overlaps(begin, bytes)) continue;
            for (std::uint32_t layer = 0; layer < texture->trackedLayers; ++layer) {
                const auto unit = texture->layerBegin(layer);
                if (texture->layerPending[layer] && unit < end && begin < unit + texture->layerBytes(layer)) return true;
            }
        }
    }
    return false;
}

bool StorageTexture::AnyLiveOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    for (const auto& range : ranges) {
        if (range.second <= range.first) continue;
        const auto bytes = static_cast<std::size_t>(range.second - range.first);
        for (const auto* texture : live.textures) {
            if (texture->overlapsLive(range.first, bytes)) return true;
        }
    }
    return false;
}

std::uint64_t StorageTexture::PendingSerial() {
    return pendingSerial.load(std::memory_order_acquire);
}

bool StorageTexture::PendingClearOnThread(std::uint64_t address, std::size_t bytes) {
    auto& pending = Pending();
    return pending.textures.ClearOnThread(address, bytes) && pending.flushingCount.load(std::memory_order_acquire) == 0;
}

void StorageTexture::NoteProved() const {
    provedPresent.store(Recorder::Presents(), std::memory_order_relaxed);
}

std::vector<std::shared_ptr<StorageTexture>> StorageTexture::overlappingPending(std::uint64_t address, std::size_t bytes) {
    // FlushPending's listing; the images stay alive across the caller's scan as there.
    std::vector<std::shared_ptr<StorageTexture>> overlapping;
    auto& pending = Pending();
    if (pending.textures.SurelyClear(address, bytes)) return overlapping;
    std::lock_guard lock(pending.mutex);
    if (!pending.textures.MayOverlap(address, bytes)) return overlapping;
    const auto* exempt = Refreshing::Get();
    for (auto* texture : pending.textures) {
        if (texture == exempt || !texture->overlaps(address, bytes) || !texture->pendingUnitInside(address, bytes)) continue;
        if (auto alive = texture->weak_from_this().lock()) overlapping.push_back(std::move(alive));
    }
    return overlapping;
}

bool StorageTexture::blocksKept(std::span<const std::shared_ptr<StorageTexture>> images, std::uint64_t address, std::size_t bytes) {
    // The stamps are read outside the registry mutex (the tracker lock is never taken under it);
    // per pending layer, as the keep decision of writeBackLayers reads them.
    constexpr std::uint64_t block = 65536;
    const auto accessEnd = address + bytes;
    for (const auto& texture : images) {
        for (std::uint32_t layer = 0; layer < texture->trackedLayers; ++layer) {
            if (!texture->layerPending[layer]) continue;
            const auto begin = std::max(address, texture->layerBegin(layer));
            const auto end = std::min(accessEnd, texture->layerBegin(layer) + texture->layerBytes(layer));
            if (end <= begin) continue;
            if (texture->layerGeneration[layer] == 0) return false;
            for (auto at = begin & ~(block - 1); at < end; at += block) {
                const auto from = std::max(at, begin);
                const auto to = std::min(at + block, end);
                if (!GuestMemory::WrittenSince(from, static_cast<std::size_t>(to - from), texture->layerGeneration[layer])) return false;
            }
        }
    }
    return true;
}

void StorageTexture::ClassifyAccess(std::uint64_t address, std::size_t bytes, AccessClassification& out) {
    out = {};
    const auto overlapping = overlappingPending(address, bytes);
    if (overlapping.empty()) return;
    out.images = overlapping.size();
    const auto present = Recorder::Presents();
    for (const auto& texture : overlapping) {
        if (texture->provedPresent.load(std::memory_order_relaxed) + DeadImagePresents <= present) ++out.dead;
        else ++out.live;
    }
    if (bytes > ClassifyLimit) return;
    out.checked = true;
    GuestMemory::CollectWritesUncached(address, bytes);
    out.allCpuWritten = blocksKept(overlapping, address, bytes);
}

bool StorageTexture::AccessKeptByCpu(std::uint64_t address, std::size_t bytes, std::size_t* images, std::size_t* evicted) {
    if (evicted != nullptr) *evicted = 0;
    const auto overlapping = overlappingPending(address, bytes);
    if (images != nullptr) *images = overlapping.size();
    if (overlapping.empty() || bytes > ClassifyLimit) return false;
    // The keep decision of a store sees every CPU write to the range up to now (an uncached
    // collect, as writeBackLayers makes it: a memo hit could leave the store's own faults for the
    // next walk); so must this one.
    GuestMemory::CollectWritesUncached(address, bytes);
    // A neighbour's write-back stamps the 64 KiB block its surface shares with one of these images
    // and then advances that image's layer generation past the stamp (advanceAdjacent), both under
    // the GpuMutex; read between the two, the stamp would pass for a CPU write and the access
    // would read the image's stale bytes in the block. Under the mutex the layer state is what a
    // store there would see, and the stamps only grow.
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    if (std::any_of(overlapping.begin(), overlapping.end(), [](const std::shared_ptr<StorageTexture>& texture) { return texture->drawnArea().drawn; })) return false;
    if (!blocksKept(overlapping, address, bytes)) return false;
    const auto site = static_cast<std::uint8_t>(GuestMemory::CurrentReadSite());
    std::size_t dropped = 0;
    for (const auto& texture : overlapping) {
        if (texture->evictStale(address, bytes, dropped)) continue;
        texture->hookSkips.fetch_add(1, std::memory_order_relaxed);
        texture->hookSkipSite.store(site, std::memory_order_relaxed);
        for (std::uint32_t layer = 0; layer < texture->trackedLayers; ++layer) {
            if (!texture->layerPending[layer] || address >= texture->layerBegin(layer) + texture->layerBytes(layer) || texture->layerBegin(layer) >= address + bytes) continue;
            texture->hookSkipLayer = layer;
            texture->hookSkipVersion = texture->version;
            texture->hookSkipGeneration = texture->layerGeneration[layer];
            break;
        }
    }
    if (evicted != nullptr) *evicted = dropped;
    return true;
}

bool StorageTexture::evictStale(std::uint64_t address, std::size_t bytes, std::size_t& dropped) {
    static const bool staleEvict = std::getenv("APS5_STALE_EVICT") != nullptr;
    constexpr std::uint64_t block = 65536;
    // writeBack's selection for the access: the pending layers it overlaps, every one under a
    // clear code (see there), or every one of a dead image (H3).
    const bool whole = storesWhole();
    const bool all = whole || (staleEvict && provedPresent.load(std::memory_order_relaxed) + DeadImagePresents <= Recorder::Presents());
    const auto accessEnd = address + bytes;
    const auto selected = [&](std::uint32_t layer) {
        return layerPending[layer] && (all || (address < layerBegin(layer) + layerBytes(layer) && layerBegin(layer) < accessEnd));
    };
    std::uint64_t first = descriptor.baseAddress + guestBytes;
    std::uint64_t last = descriptor.baseAddress;
    bool beyond = false;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected(layer)) continue;
        if (layerGeneration[layer] == 0) return false;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        first = std::min(first, begin);
        last = std::max(last, end);
        if (begin < (address & ~(block - 1)) || end > ((accessEnd + block - 1) & ~(block - 1))) beyond = true;
    }
    if (last <= first) return false;
    if (beyond) {
        // Blocks outside the access's own were not collected by this call: the walk of the layers'
        // span (a write-back's own) runs once per present per image.
        const auto present = Recorder::Presents();
        if (staleCheckPresent == present) return false;
        staleCheckPresent = present;
        GuestMemory::CollectWritesUncached(first, static_cast<std::size_t>(last - first));
    }
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected(layer)) continue;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        for (auto at = begin & ~(block - 1); at < end; at += block) {
            const auto from = std::max(at, begin);
            if (!GuestMemory::WrittenSince(from, static_cast<std::size_t>(std::min(at + block, end) - from), layerGeneration[layer])) return false;
        }
    }
    // As the empty write-back leaves the layers: no results pending, the generation unchanged (the
    // stamps make their next Refresh re-upload), `original` no longer vouching for the bytes.
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected(layer)) continue;
        layerPending[layer] = false;
        layerOriginal[layer] = false;
        cancelStoredSave(layer);
    }
    releaseUnheldOriginal();
    if (!anyLayerPending()) {
        ++dropped;
        staleEvicted.fetch_add(1, std::memory_order_relaxed);
    }
    reconcilePending();
    return true;
}

}
}
}
}
}
}
}
}
}
}
}
}
}
}
