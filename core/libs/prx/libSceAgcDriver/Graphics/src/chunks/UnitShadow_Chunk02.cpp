#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>

namespace AgcDriver::Graphics {
namespace {
std::uint64_t UnitShadowBudget(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported) {
    constexpr std::uint64_t Ceiling = std::uint64_t{1024} << 20u;
    constexpr std::uint64_t Floor = std::uint64_t{128} << 20u;
    const auto heap = LargestDeviceLocalHeap(memory);
    if (reported == nullptr || !heap || reported->heapBudget[*heap] == 0) return Ceiling;
    return std::clamp<std::uint64_t>(reported->heapBudget[*heap] / 8u, Floor, Ceiling);
}

bool UnitShadowEnabled() {
    static const bool enabled = std::getenv("APS5_NO_UNIT_SHADOW") == nullptr && GuestMemory::WriteWatched();
    return enabled;
}

bool ShadowVerify() {
    static const bool verify = std::getenv("APS5_SHADOW_VERIFY") != nullptr;
    return verify;
}

void NoteShadowSeed(std::uint64_t begin, std::uint64_t end) {
    Stats().seeds.fetch_add(1, std::memory_order_relaxed);
    Stats().seedBytes.fetch_add(end - begin, std::memory_order_relaxed);
    if (traceSeedsLeft.load(std::memory_order_relaxed) > 0 && traceSeedsLeft.fetch_sub(1, std::memory_order_relaxed) > 0) std::fprintf(stderr, "[shadow] seed 0x%llx+0x%llx from the import\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin));
}

PublishReason PublishReasonFor(const char* flushReason) {
    const auto is = [&](const char* name) { return flushReason != nullptr && std::strcmp(flushReason, name) == 0; };
    if (is("memory access")) return GuestMemory::CurrentReadSite() == GuestMemory::ReadSite::Scanout ? PublishReason::Scanout : PublishReason::Hook;
    if (is("imported buffer region") || is("copied buffer region") || is("sampled texture")) return PublishReason::Region;
    if (is("indirect dispatch arguments") || is("indirect draw arguments")) return PublishReason::Indirect;
    if (is("buffer copy source")) return PublishReason::CopySource;
    if (is("buffer copy destination")) return PublishReason::CopyDestination;
    if (is("buffer fill")) return PublishReason::Fill;
    if (is("packet store")) return PublishReason::Label;
    if (is("buffer copy count") || is("dispatch-cache entry")) return PublishReason::Validate;
    if (is("storage image creation") || is("storage refresh")) return PublishReason::Upload;
    if (is("address-based heaps")) return PublishReason::Address;
    return PublishReason::Other;
}

ShadowSlab::ShadowSlab(const Context& context, VkBuffer buffer, VkDeviceMemory memory, std::uint64_t firstUnit, std::uint32_t units) : context(context), buffer(buffer), memory(memory), firstUnit(firstUnit), units(units) {}

ShadowSlab::~ShadowSlab() {
    if (buffer != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
    if (memory != VK_NULL_HANDLE) MemoryAccounting::Free(context, memory);
}

ShadowSlabPin::ShadowSlabPin(std::shared_ptr<ShadowSlab> slab) : slab(std::move(slab)) {
    this->slab->pins.fetch_add(1, std::memory_order_relaxed);
}

ShadowSlabPin::~ShadowSlabPin() {
    slab->pins.fetch_sub(1, std::memory_order_relaxed);
}

void SetPublishSelectedHook(void (*hook)(std::uint64_t begin, std::uint64_t end)) {
    publishSelectedHook.store(hook, std::memory_order_release);
}

PublishOnCpuScope::PublishOnCpuScope() : previous(std::exchange(publishOnCpu(), true)) {}

PublishOnCpuScope::~PublishOnCpuScope() {
    publishOnCpu() = previous;
}

std::size_t PublishShadow(std::uint64_t address, std::size_t bytes, PublishScope scope, PublishReason reason, bool waited) {
    if (!UnitShadowEnabled() || scope == PublishScope::None || bytes == 0) return 0;
    if (!waited && Recorder::InCompletion() && GuestMemory::CurrentReadSite() == GuestMemory::ReadSite::Store) {
        Stats().skippedInCompletion.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
    std::vector<std::shared_ptr<UnitShadow>> shadows;
    {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        shadows = overlapping(registry, address, end);
    }
    std::size_t units = 0;
    for (const auto& shadow : shadows) units += publishRange(shadow, address, end, scope, reason);
    return units;
}

namespace {

bool anyShadowedLocked(const Shadows& registry, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return false;
    const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
    return anyOverlapping(registry, address, end, [&](const std::shared_ptr<UnitShadow>& shadow) {
        if (shadow->liveUnits == 0) return false;
        const auto begin = std::max(address, shadow->importBase);
        const auto stop = std::min(end, shadow->ImportEnd());
        for (auto unit = shadow->UnitOf(begin); unit <= shadow->UnitOf(stop - 1); ++unit) {
            if (shadow->Live(unit)) return true;
        }
        return false;
    });
}

std::shared_ptr<const LiveRanges> currentLiveRanges() {
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    const auto serial = liveSerial.load(std::memory_order_relaxed);
    if (auto published = publishedLive.load(std::memory_order_acquire); published != nullptr && published->serial == serial) return published;
    auto built = std::make_shared<LiveRanges>();
    built->serial = serial;
    auto& ranges = built->ranges;
    for (const auto& [base, shadow] : registry.byBase) {
        if (shadow->liveUnits == 0) continue;
        for (std::uint64_t unit = 0; unit < shadow->Units(); ++unit) {
            if (!shadow->Live(unit)) continue;
            const auto [begin, end] = shadow->UnitRange(unit);
            if (end <= begin) continue;
            if (!ranges.empty() && ranges.back().second >= begin) ranges.back().second = std::max(ranges.back().second, end);
            else ranges.emplace_back(begin, end);
        }
    }
    std::shared_ptr<const LiveRanges> result = std::move(built);
    publishedLive.store(result, std::memory_order_release);
    return result;
}

}

bool LiveUnitsClear(std::uint64_t address, std::size_t bytes) {
    if (!UnitShadowEnabled() || bytes == 0) return true;
    auto& copy = ThreadScratch<std::shared_ptr<const LiveRanges>, LiveRangesTag>();
    const auto serial = liveSerial.load(std::memory_order_acquire);
    if (copy == nullptr || copy->serial != serial) {
        auto published = publishedLive.load(std::memory_order_acquire);
        copy = published != nullptr && published->serial == serial ? std::move(published) : currentLiveRanges();
    }
    const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
    const auto it = std::upper_bound(copy->ranges.begin(), copy->ranges.end(), address, [](std::uint64_t value, const std::pair<std::uint64_t, std::uint64_t>& range) { return value < range.second; });
    return it == copy->ranges.end() || it->first >= end;
}

bool AnyPublishInProgress() {
    return publishesInProgress.load(std::memory_order_seq_cst) != 0;
}

bool AnyShadowedOverlaps(std::uint64_t address, std::size_t bytes) {
    if (!UnitShadowEnabled() || bytes == 0) return false;
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    return anyShadowedLocked(registry, address, bytes);
}

bool AnyShadowedOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    if (!UnitShadowEnabled() || ranges.empty()) return false;
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    for (const auto& [begin, end] : ranges) {
        if (end > begin && anyShadowedLocked(registry, begin, static_cast<std::size_t>(end - begin))) return true;
    }
    return false;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> LiveShadowedRanges(std::uint64_t address, std::size_t bytes, PublishScope scope) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    if (!UnitShadowEnabled() || scope == PublishScope::None || bytes == 0) return ranges;
    const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    for (const auto& shadow : overlapping(registry, address, end)) {
        if (shadow->liveUnits == 0) continue;
        const auto begin = std::max(address, shadow->importBase);
        const auto stop = std::min(end, shadow->ImportEnd());
        if (stop <= begin) continue;
        for (auto unit = shadow->UnitOf(begin); unit <= shadow->UnitOf(stop - 1); ++unit) {
            if (!shadow->Live(unit)) continue;
            const auto [unitBegin, unitEnd] = shadow->UnitRange(unit);
            if (unitEnd <= unitBegin || (scope == PublishScope::PartialUnits && address <= unitBegin && end >= unitEnd)) continue;
            if (!ranges.empty() && ranges.back().second == unitBegin) ranges.back().second = unitEnd;
            else ranges.emplace_back(unitBegin, unitEnd);
        }
    }
    if (ranges.size() > 8) ranges.assign(1, {ranges.front().first, ranges.back().second});
    return ranges;
}

std::uint64_t SlabBoundary(const HostImport& import, std::uint64_t address) {
    const auto base = import.base & ~(UnitBytes - 1);
    const auto unit = (address - base) / UnitBytes;
    return base + (unit / SlabUnits() + 1) * SlabUnits() * UnitBytes;
}

VkDeviceSize SlabOffset(const HostImport& import, const ShadowSlab& slab, std::uint64_t address) {
    const auto base = import.base & ~(UnitBytes - 1);
    return (address - base) - slab.firstUnit * UnitBytes;
}

std::optional<ShadowDestination> ShadowDestinationFor(const Context& context, const HostImport& import, std::uint64_t begin, std::uint64_t end) {
    if (!UnitShadowEnabled() || end <= begin || begin < import.base || end > import.base + import.bytes) return std::nullopt;
    if (!GuestMemory::Watched(begin, static_cast<std::size_t>(end - begin))) return std::nullopt;
    auto& registry = Registry();
    std::shared_ptr<UnitShadow> shadow;
    std::size_t slabIndex = 0;
    std::uint64_t first = 0, last = 0;
    bool missing = false;
    {
        std::lock_guard lock(registry.mutex);
        shadow = findOrCreate(registry, context, import);
        first = shadow->UnitOf(begin);
        last = shadow->UnitOf(end - 1);
        slabIndex = static_cast<std::size_t>(first / SlabUnits());
        if (last / SlabUnits() != slabIndex) return std::nullopt;
        missing = shadow->slabs[slabIndex] == nullptr;
        if (missing && shadow->slabFailedUntil[slabIndex] > Recorder::Presents()) return std::nullopt;
    }
    if (missing) {
        const auto firstUnit = static_cast<std::uint64_t>(slabIndex) * SlabUnits();
        const auto units = static_cast<std::uint32_t>(std::min<std::uint64_t>(SlabUnits(), shadow->Units() - firstUnit));
        const auto needed = static_cast<std::uint64_t>(units) * UnitBytes;
        for (;;) {
            {
                std::lock_guard lock(registry.mutex);
                if (registry.liveBytes + needed <= BudgetBytes(context)) break;
            }
            if (!evictOne(registry)) {
                Stats().refused.fetch_add(1, std::memory_order_relaxed);
                Stats().refusedBytes.fetch_add(end - begin, std::memory_order_relaxed);
                return std::nullopt;
            }
        }
        auto slab = makeSlab(context, firstUnit, units);
        std::lock_guard lock(registry.mutex);
        if (slab == nullptr) {
            shadow->slabFailedUntil[slabIndex] = Recorder::Presents() + 1;
            Stats().refused.fetch_add(1, std::memory_order_relaxed);
            Stats().refusedBytes.fetch_add(end - begin, std::memory_order_relaxed);
            return std::nullopt;
        }
        if (shadow->slabs[slabIndex] == nullptr) {
            shadow->slabs[slabIndex] = slab;
            registry.liveBytes += SlabBytes(*slab);
            registry.peakBytes = std::max(registry.peakBytes, registry.liveBytes);
            ++registry.liveSlabs;
        }
    }
    for (auto unit = first; unit <= last; unit = last) {
        const auto [unitBegin, unitEnd] = shadow->UnitRange(unit);
        if (!(begin <= unitBegin && end >= unitEnd) && unitEnd > unitBegin) GuestMemory::CollectWritesUncached(unitBegin, static_cast<std::size_t>(unitEnd - unitBegin));
        if (unit == last) break;
    }
    std::lock_guard lock(registry.mutex);
    const auto& slab = shadow->slabs[slabIndex];
    if (slab == nullptr) return std::nullopt;
    slab->lastUse = Recorder::Presents();
    ShadowDestination destination{slab->buffer, SlabOffset(import, *slab, begin), slab, {}, std::make_shared<ShadowSlabPin>(slab)};
    std::vector<std::uint8_t> changed, cpu;
    bool queried = false;
    for (auto unit = first; unit <= last; ++unit) {
        const auto [unitBegin, unitEnd] = shadow->UnitRange(unit);
        if (begin <= unitBegin && end >= unitEnd) continue;
        if (!queried) {
            changedUnits(*shadow, first, last, changed, cpu);
            queried = true;
        }
        const auto k = static_cast<std::size_t>(unit - first);
        if (shadow->generation[unit] != 0 && changed[k] == 0) continue;
        destination.seedUnits.emplace_back(unitBegin, unitEnd);
    }
    return destination;
}

std::vector<ShadowRun> ShadowSources(const Context& context, const HostImport& import, std::uint64_t surfaceBase, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint64_t, std::uint64_t>> tailBlocks, bool countReads) {
    std::vector<ShadowRun> result;
    if (runs.empty()) return result;
    const auto importPiece = [&](std::uint64_t begin, std::uint64_t end) {
        result.push_back({begin, end, import.buffer, HostImportOffset(import, surfaceBase + begin), false, nullptr});
        if (countReads) Stats().detiledImportBytes.fetch_add(end - begin, std::memory_order_relaxed);
    };
    std::shared_ptr<UnitShadow> shadow;
    if (UnitShadowEnabled()) {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        shadow = find(registry, import);
        if (shadow != nullptr && (shadow->context.device != context.device || (shadow->liveUnits == 0 && std::none_of(shadow->generation.begin(), shadow->generation.end(), [](std::uint64_t generation) { return generation != 0; })))) shadow = nullptr;
    }
    if (shadow == nullptr) {
        for (const auto& [begin, end] : runs) importPiece(begin, end);
        if (countReads) Stats().detiledImport.fetch_add(1, std::memory_order_relaxed);
        return result;
    }
    const auto first = shadow->UnitOf(surfaceBase + runs.front().first);
    const auto last = shadow->UnitOf(surfaceBase + runs.back().second - 1);
    std::vector<std::uint8_t> fresh(static_cast<std::size_t>(last - first + 1), 0);
    std::vector<std::shared_ptr<ShadowSlab>> slabs(shadow->slabs.size());
    {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        std::vector<std::uint8_t> changed, cpu;
        changedUnits(*shadow, first, last, changed, cpu);
        for (auto unit = first; unit <= last; ++unit) {
            const auto k = static_cast<std::size_t>(unit - first);
            const auto& slab = shadow->slabs[static_cast<std::size_t>(unit / SlabUnits())];
            fresh[k] = shadow->generation[unit] != 0 && changed[k] == 0 && slab != nullptr ? 1 : 0;
        }
        slabs = shadow->slabs;
    }
    for (const auto& [tailBegin, tailEnd] : tailBlocks) {
        if (tailEnd <= tailBegin) continue;
        const auto tailFirst = shadow->UnitOf(surfaceBase + tailBegin);
        const auto tailLast = shadow->UnitOf(surfaceBase + tailEnd - 1);
        if (tailFirst < first || tailLast > last) continue;
        bool anyFresh = false, anyImport = false;
        for (auto unit = tailFirst; unit <= tailLast; ++unit) (fresh[static_cast<std::size_t>(unit - first)] != 0 ? anyFresh : anyImport) = true;
        if (!anyFresh || !anyImport) continue;
        PublishShadow(surfaceBase + tailBegin, static_cast<std::size_t>(tailEnd - tailBegin), PublishScope::Whole, PublishReason::TailMip);
        Stats().tailPublishes.fetch_add(1, std::memory_order_relaxed);
        for (auto unit = tailFirst; unit <= tailLast; ++unit) fresh[static_cast<std::size_t>(unit - first)] = 0;
    }
    std::uint64_t shadowBytes = 0, importBytes = 0;
    const auto present = Recorder::Presents();
    for (const auto& [runBegin, runEnd] : runs) {
        if (runEnd <= runBegin) continue;
        const auto guestBegin = surfaceBase + runBegin;
        const auto guestEnd = surfaceBase + runEnd;
        for (auto unit = shadow->UnitOf(guestBegin); unit <= shadow->UnitOf(guestEnd - 1); ++unit) {
            const auto pieceBegin = std::max(guestBegin, shadow->UnitBegin(unit)) - surfaceBase;
            const auto pieceEnd = std::min(guestEnd, shadow->UnitBegin(unit + 1)) - surfaceBase;
            if (pieceEnd <= pieceBegin) continue;
            const bool isFresh = fresh[static_cast<std::size_t>(unit - first)] != 0;
            const auto& slab = isFresh ? slabs[static_cast<std::size_t>(unit / SlabUnits())] : nullptr;
            if (!result.empty() && result.back().end == pieceBegin && result.back().shadow == isFresh && result.back().slab == slab) {
                result.back().end = pieceEnd;
            } else if (isFresh) {
                slab->lastUse = present;
                result.push_back({pieceBegin, pieceEnd, slab->buffer, SlabOffset(import, *slab, surfaceBase + pieceBegin), true, slab});
            } else {
                result.push_back({pieceBegin, pieceEnd, import.buffer, HostImportOffset(import, surfaceBase + pieceBegin), false, nullptr});
            }
            (isFresh ? shadowBytes : importBytes) += pieceEnd - pieceBegin;
        }
    }
    if (!countReads) shadowBytes = importBytes = 0;
    if (shadowBytes != 0) {
        Stats().detiledShadow.fetch_add(1, std::memory_order_relaxed);
        Stats().detiledShadowBytes.fetch_add(shadowBytes, std::memory_order_relaxed);
    }
    if (importBytes != 0) {
        Stats().detiledImport.fetch_add(1, std::memory_order_relaxed);
        Stats().detiledImportBytes.fetch_add(importBytes, std::memory_order_relaxed);
    }
    if (ShadowVerify()) {
        std::vector<std::uint8_t> changed, cpu;
        std::lock_guard lock(Registry().mutex);
        changedUnits(*shadow, first, last, changed, cpu);
        for (const auto& run : result) {
            if (!run.shadow) continue;
            for (auto unit = shadow->UnitOf(surfaceBase + run.begin); unit <= shadow->UnitOf(surfaceBase + run.end - 1); ++unit) {
                Stats().verified.fetch_add(1, std::memory_order_relaxed);
                if (changed[static_cast<std::size_t>(unit - first)] != 0) Stats().mismatches.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    return result;
}

void MarkShadowed(const HostImport& import, std::span<const ShadowedRange> ranges, std::uint64_t generation) {
    if (!UnitShadowEnabled() || ranges.empty() || generation == 0) return;
    auto& registry = Registry();
    std::uint64_t bytes = 0;
    std::uint64_t lost = 0;
    {
        std::lock_guard lock(registry.mutex);
        const auto shadow = find(registry, import);
        if (shadow == nullptr) return;
        const auto present = Recorder::Presents();
        bool turnedLive = false;
        for (const auto& [begin, end, slab] : ranges) {
            if (end <= begin) continue;
            bytes += end - begin;
            for (auto unit = shadow->UnitOf(begin); unit <= shadow->UnitOf(end - 1); ++unit) {
                const auto& current = shadow->slabs[static_cast<std::size_t>(unit / SlabUnits())];
                if (current == nullptr || current != slab) {
                    ++lost;
                    continue;
                }
                if (!shadow->Live(unit)) {
                    ++shadow->liveUnits;
                    turnedLive = true;
                }
                shadow->generation[unit] = generation;
                shadow->published[unit] = 0;
                current->lastUse = present;
            }
        }
        if (turnedLive) liveChanged();
    }
    if (lost != 0) {
        Stats().slabLost.fetch_add(lost, std::memory_order_relaxed);
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[shadow] %llu retiled units of import 0x%llx lost their slab before they were marked\n", static_cast<unsigned long long>(lost), static_cast<unsigned long long>(import.base));
    }
    Stats().retiled.fetch_add(1, std::memory_order_relaxed);
    Stats().retiledBytes.fetch_add(bytes, std::memory_order_relaxed);
    StorageTexture::BumpPendingSerial();
}

}
}
