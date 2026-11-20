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
namespace {
void RetireShadow(VkDevice device, const HostImport& import, const std::function<bool(std::uint64_t, std::uint64_t)>& registered) {
    if (!UnitShadowEnabled()) return;
    auto& registry = Registry();
    std::shared_ptr<UnitShadow> shadow;
    std::vector<std::uint8_t> selected;
    bool any = false;
    std::uint64_t dropped = 0;
    {
        std::lock_guard lock(registry.mutex);
        shadow = find(registry, import);
        if (shadow == nullptr) return;
        // The memory the title took back holds its bytes now: only a unit still inside a readable
        // registered range receives its results.
        selected.assign(static_cast<std::size_t>(shadow->Units()), 0);
        for (std::uint64_t unit = 0; unit < shadow->Units(); ++unit) {
            if (!shadow->Live(unit)) continue;
            const auto [begin, end] = shadow->UnitRange(unit);
            if (end > begin && registered(begin, end)) {
                selected[static_cast<std::size_t>(unit)] = 1;
                any = true;
            } else {
                shadow->generation[unit] = 0;
                ++dropped;
            }
        }
        shadow->Recount();
        if (dropped != 0) liveChanged();
    }
    if (dropped != 0) Stats().droppedOnRetire.fetch_add(dropped, std::memory_order_relaxed);
    std::size_t published = 0;
    if (any) {
        if (GuestMemory::GpuMutex().HeldByThisThread() && shadow->context.device == device) {
            published = publishUnits(shadow, 0, shadow->Units() - 1, selected, PublishReason::Retire);
        } else {
            Stats().lostOnRetire.fetch_add(shadow->liveUnits, std::memory_order_relaxed);
            static std::atomic<int> reports{0};
            if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[shadow] import 0x%llx+0x%llx retired outside the device lock: %u shadowed units lost\n", static_cast<unsigned long long>(import.base), static_cast<unsigned long long>(import.bytes), shadow->liveUnits);
        }
    }
    std::lock_guard lock(registry.mutex);
    const auto found = registry.byBase.find(import.base);
    if (found == registry.byBase.end() || found->second != shadow) return;
    for (const auto& slab : shadow->slabs) {
        if (slab == nullptr) continue;
        registry.liveBytes -= SlabBytes(*slab);
        --registry.liveSlabs;
    }
    ++registry.retired;
    if (TraceEnabled()) std::fprintf(stderr, "[shadow] retire import 0x%llx+0x%llx: %zu units published, %llu dropped (memory no longer registered)\n", static_cast<unsigned long long>(import.base), static_cast<unsigned long long>(import.bytes), published, static_cast<unsigned long long>(dropped));
    registry.byBase.erase(found);
    liveChanged();
}

void PublishAllShadows(const Context& context, PublishReason reason) {
    if (!UnitShadowEnabled()) return;
    std::vector<std::shared_ptr<UnitShadow>> shadows;
    {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        for (const auto& [base, shadow] : registry.byBase) {
            if (shadow->context.device == context.device) shadows.push_back(shadow);
        }
    }
    for (const auto& shadow : shadows) publishRange(shadow, shadow->importBase, shadow->ImportEnd(), PublishScope::Whole, reason);
}

void DestroyShadows(VkDevice device) {
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    for (auto it = registry.byBase.begin(); it != registry.byBase.end();) {
        if (it->second->context.device != device) {
            ++it;
            continue;
        }
        for (const auto& slab : it->second->slabs) {
            if (slab == nullptr) continue;
            registry.liveBytes -= SlabBytes(*slab);
            --registry.liveSlabs;
        }
        it = registry.byBase.erase(it);
        liveChanged();
    }
}

std::string ShadowReport() {
    auto& statistics = Stats();
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    const auto mib = [](unsigned long long bytes) { return bytes / 1048576.0; };
    char text[256];
    std::string line;
    if (!UnitShadowEnabled()) return "; shadow: off";
    std::snprintf(text, sizeof(text), "; shadow: retiled %llu/%.1f (refused %llu/%.1f to import), seeds %llu/%.1f, detiled from shadow %llu/%.1f, from import %llu/%.1f, tail publishes %llu, published", take(statistics.retiled), mib(take(statistics.retiledBytes)), take(statistics.refused), mib(take(statistics.refusedBytes)), take(statistics.seeds), mib(take(statistics.seedBytes)), take(statistics.detiledShadow), mib(take(statistics.detiledShadowBytes)), take(statistics.detiledImport), mib(take(statistics.detiledImportBytes)), take(statistics.tailPublishes));
    line += text;
    unsigned long long publishedTotal = 0, publishedBytesTotal = 0;
    std::string reasons;
    for (std::size_t i = 0; i < ReasonCount; ++i) {
        const auto count = take(statistics.published[i]);
        const auto bytes = take(statistics.publishedBytes[i]);
        publishedTotal += count;
        publishedBytesTotal += bytes;
        if (count == 0) continue;
        std::snprintf(text, sizeof(text), " %s %llu/%.1f", ReasonNames[i], count, mib(bytes));
        reasons += text;
    }
    const auto cpuDropped = take(statistics.staleDroppedCpu), driverDropped = take(statistics.staleDroppedDriver);
    unsigned long long liveSlabs = 0, liveBytes = 0, peakBytes = 0, retired = 0;
    {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        liveSlabs = registry.liveSlabs;
        liveBytes = registry.liveBytes;
        peakBytes = registry.peakBytes;
        retired = registry.retired;
    }
    std::snprintf(text, sizeof(text), " %llu/%.1f by reason {%s }, stale-dropped %llu (cpu %llu, driver %llu), skipped-in-completion %llu, slabs %llu live %.1f MiB / peak %.1f MiB, evictions %llu, slab lost %llu, retired %llu, dropped on retire %llu, lost on retire %llu", publishedTotal, mib(publishedBytesTotal), reasons.c_str(), cpuDropped + driverDropped, cpuDropped, driverDropped, take(statistics.skippedInCompletion), liveSlabs, mib(liveBytes), mib(peakBytes), take(statistics.evictions), take(statistics.slabLost), retired, take(statistics.droppedOnRetire), take(statistics.lostOnRetire));
    line += text;
    if (ShadowVerify()) {
        std::snprintf(text, sizeof(text), ", verified %llu, mismatches %llu", take(statistics.verified), take(statistics.mismatches));
        line += text;
    }
    return line;
}

}
}
}
