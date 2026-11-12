#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_CPUTOPOLOGY_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_CPUTOPOLOGY_HPP

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <fstream>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <unistd.h>
#endif

// Hybrid CPU layout (Intel P/E cores) from the OS: which logical processors belong to the lowest and
// the highest efficiency class. Non-hybrid machines report no layout and every pin is a no-op unless
// a mask is given explicitly. Header-only so libkernel and the AGC driver share it without a link.
namespace CpuTopology {

struct Layout {
    std::uint64_t process = 0;
    std::uint64_t efficient = 0;
    std::uint64_t performant = 0;
    bool hybrid = false;
};

#ifdef __linux__
inline std::uint64_t MaskFromCpuList(const char* path) {
    std::ifstream file(path);
    std::string text;
    if (!std::getline(file, text)) return 0;
    std::uint64_t mask = 0;
    const char* cursor = text.c_str();
    while (*cursor != '\0') {
        char* end = nullptr;
        const unsigned long first = std::strtoul(cursor, &end, 10);
        if (end == cursor) return 0;
        unsigned long last = first;
        cursor = end;
        if (*cursor == '-') {
            last = std::strtoul(cursor + 1, &end, 10);
            if (end == cursor + 1 || last < first) return 0;
            cursor = end;
        }
        for (unsigned long cpu = first; cpu <= last && cpu < 64; ++cpu) mask |= std::uint64_t{1} << cpu;
        if (*cursor == ',') ++cursor;
        else if (*cursor != '\0') return 0;
    }
    return mask;
}
#endif

inline Layout LayoutFromClasses(const std::uint64_t* byClass, const bool* seen, std::size_t count) {
    Layout result{};
    int lowest = -1;
    int highest = -1;
    for (std::size_t cls = 0; cls < count; ++cls) {
        if (!seen[cls]) continue;
        if (lowest < 0) lowest = static_cast<int>(cls);
        highest = static_cast<int>(cls);
    }
    if (lowest < 0 || lowest == highest) return result;
    for (int cls = 0; cls < highest; ++cls) {
        if (seen[cls]) result.efficient |= byClass[cls];
    }
    result.performant = byClass[highest];
    result.hybrid = result.efficient != 0 && result.performant != 0;
    return result;
}

struct CpuSet {
    std::uint32_t id = 0;
    std::uint16_t group = 0;
    std::uint8_t logical = 0;
    std::uint8_t core = 0;
    std::uint8_t efficiencyClass = 0;
    std::uint8_t lastLevelCache = 0;
    bool parked = false;
};

struct CoreReservation {
    CpuSet chosen;
    std::vector<std::uint32_t> others;
};

inline std::optional<CoreReservation> ChooseReservedCore(const std::vector<CpuSet>& sets, const std::vector<CpuSet>& reserved, std::optional<unsigned> logical) {
    constexpr std::size_t minimumCores = 6;
    const auto sameCore = [](const CpuSet& left, const CpuSet& right) { return left.group == right.group && left.core == right.core; };
    const auto held = [&](const CpuSet& set) {
        for (const auto& other : reserved) {
            if (sameCore(other, set)) return true;
        }
        return false;
    };
    const auto parked = [&](const CpuSet& set) {
        for (const auto& other : sets) {
            if (sameCore(other, set) && other.parked) return true;
        }
        return false;
    };
    const CpuSet* chosen = nullptr;
    if (logical.has_value()) {
        for (const auto& set : sets) {
            if (set.group == 0 && set.logical == *logical) chosen = &set;
        }
    } else {
        std::uint8_t highest = 0;
        for (const auto& set : sets) {
            if (set.efficiencyClass > highest) highest = set.efficiencyClass;
        }
        const CpuSet* home = nullptr;
        for (const auto& set : sets) {
            if (set.efficiencyClass != highest || set.parked) continue;
            if (home == nullptr || set.group < home->group || (set.group == home->group && set.logical < home->logical)) home = &set;
        }
        if (home == nullptr) return std::nullopt;
        const auto candidate = [&](const CpuSet& set) {
            return set.efficiencyClass == highest && set.group == home->group && set.lastLevelCache == home->lastLevelCache;
        };
        std::vector<std::uint8_t> cores;
        for (const auto& set : sets) {
            if (!candidate(set)) continue;
            bool counted = false;
            for (const auto core : cores) counted = counted || core == set.core;
            if (!counted) cores.push_back(set.core);
        }
        if (cores.size() < minimumCores) return std::nullopt;
        for (const auto& set : sets) {
            if (!candidate(set) || held(set) || parked(set)) continue;
            if (chosen == nullptr || set.core > chosen->core || (set.core == chosen->core && set.logical < chosen->logical)) chosen = &set;
        }
    }
    if (chosen == nullptr || held(*chosen)) return std::nullopt;
    CoreReservation result{*chosen, {}};
    for (const auto& set : sets) {
        if (!sameCore(set, *chosen) && !held(set)) result.others.push_back(set.id);
    }
    if (result.others.empty()) return std::nullopt;
    return result;
}

inline std::vector<std::uint32_t> PipelineCacheSet(const std::vector<CpuSet>& sets, const std::vector<CpuSet>& reserved) {
    if (reserved.empty()) return {};
    const auto& home = reserved.front();
    bool split = false;
    for (const auto& set : sets) split = split || set.group != home.group || set.lastLevelCache != home.lastLevelCache;
    if (!split) return {};
    std::vector<std::uint32_t> ids;
    for (const auto& set : sets) {
        bool held = false;
        for (const auto& other : reserved) held = held || (other.group == set.group && other.core == set.core);
        if (!held && set.group == home.group && set.lastLevelCache == home.lastLevelCache) ids.push_back(set.id);
    }
    return ids;
}

#ifdef __linux__
inline std::optional<unsigned long> ReadSysfsNumber(const std::string& path) {
    std::ifstream file(path);
    unsigned long value = 0;
    if (file >> value) return value;
    return std::nullopt;
}

inline std::vector<CpuSet> SysfsCpuSets(const std::string& root, std::uint64_t process, std::uint64_t efficient) {
    std::vector<CpuSet> sets;
    std::vector<std::pair<unsigned long, unsigned long>> cores;
    std::vector<std::pair<unsigned long, unsigned long>> caches;
    const auto indexOf = [](auto& seen, const auto& key) {
        std::size_t index = 0;
        while (index < seen.size() && seen[index] != key) ++index;
        if (index == seen.size()) seen.push_back(key);
        return index;
    };
    for (unsigned cpu = 0; cpu < 64; ++cpu) {
        if (((process >> cpu) & 1u) == 0) continue;
        const auto base = root + "/cpu" + std::to_string(cpu);
        const auto core = ReadSysfsNumber(base + "/topology/core_id");
        const auto package = ReadSysfsNumber(base + "/topology/physical_package_id");
        if (!core.has_value() || !package.has_value()) return {};
        const auto coreIndex = indexOf(cores, std::pair{*package, *core});
        unsigned long level = 0;
        unsigned long cacheId = 0;
        for (unsigned index = 0; index < 8; ++index) {
            const auto cacheBase = base + "/cache/index" + std::to_string(index);
            const auto cacheLevel = ReadSysfsNumber(cacheBase + "/level");
            const auto id = ReadSysfsNumber(cacheBase + "/id");
            if (!cacheLevel.has_value() || !id.has_value() || *cacheLevel < level) continue;
            level = *cacheLevel;
            cacheId = *id;
        }
        const auto cacheIndex = indexOf(caches, std::pair{*package, cacheId});
        if (coreIndex > 0xffu || cacheIndex > 0xffu) return {};
        const bool efficientCore = ((efficient >> cpu) & 1u) != 0;
        sets.push_back({cpu, 0, static_cast<std::uint8_t>(cpu), static_cast<std::uint8_t>(coreIndex), static_cast<std::uint8_t>(efficientCore ? 0u : 1u), static_cast<std::uint8_t>(cacheIndex), false});
    }
    return sets;
}
#endif

inline const Layout& Get() {
    static const Layout layout = [] {
        Layout result{};
#ifdef _WIN32
        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) result.process = static_cast<std::uint64_t>(processMask);
        DWORD length = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) return result;
        std::vector<unsigned char> buffer(length);
        if (!GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &length)) return result;
        std::uint64_t byClass[256] = {};
        bool seen[256] = {};
        for (DWORD offset = 0; offset + 8 <= length;) {
            const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
            if (entry->Size == 0 || offset + entry->Size > length) break;
            if (entry->Relationship == RelationProcessorCore && entry->Processor.GroupCount >= 1 && entry->Processor.GroupMask[0].Group == 0) {
                const unsigned cls = entry->Processor.EfficiencyClass;
                byClass[cls] |= static_cast<std::uint64_t>(entry->Processor.GroupMask[0].Mask);
                seen[cls] = true;
            }
            offset += entry->Size;
        }
        const auto classified = LayoutFromClasses(byClass, seen, 256);
        result.efficient = classified.efficient;
        result.performant = classified.performant;
        result.hybrid = classified.hybrid;
#elif defined(__linux__)
        cpu_set_t set;
        CPU_ZERO(&set);
        if (sched_getaffinity(0, sizeof(set), &set) == 0) {
            for (unsigned cpu = 0; cpu < 64; ++cpu)
                if (CPU_ISSET(cpu, &set)) result.process |= std::uint64_t{1} << cpu;
        }
        result.performant = MaskFromCpuList("/sys/devices/cpu_core/cpus");
        result.efficient = MaskFromCpuList("/sys/devices/cpu_atom/cpus");
        result.hybrid = result.efficient != 0 && result.performant != 0 && (result.efficient & result.performant) == 0;
#endif
        return result;
    }();
    return layout;
}

// A hex mask from the environment, 0 when unset or empty.
inline std::uint64_t MaskFromEnvironment(const char* name) {
    const char* text = std::getenv(name);
    if (text == nullptr || *text == '\0') return 0;
    return std::strtoull(text, nullptr, 16);
}

inline bool Trace() {
    static const bool trace = std::getenv("APS5_TRACE_AFFINITY") != nullptr;
    return trace;
}

inline unsigned long ThreadId(void* handle) {
#ifdef _WIN32
    return handle != nullptr ? GetThreadId(static_cast<HANDLE>(handle)) : GetCurrentThreadId();
#elif defined(__linux__)
    return handle == nullptr ? static_cast<unsigned long>(gettid()) : 0;
#else
    (void)handle;
    return 0;
#endif
}

// Restricts the thread (`nullptr` is the calling thread) to `mask` clipped to the process affinity;
// returns the mask applied, 0 when nothing was.
inline std::uint64_t Pin(void* handle, std::uint64_t mask) {
#ifdef _WIN32
    const auto& layout = Get();
    if (layout.process != 0) mask &= layout.process;
    if (mask == 0) return 0;
    HANDLE thread = handle != nullptr ? static_cast<HANDLE>(handle) : GetCurrentThread();
    return SetThreadAffinityMask(thread, static_cast<DWORD_PTR>(mask)) != 0 ? mask : 0;
#elif defined(__linux__)
    const auto& layout = Get();
    if (layout.process != 0) mask &= layout.process;
    if (mask == 0 || handle != nullptr) return 0;
    cpu_set_t set;
    CPU_ZERO(&set);
    for (unsigned cpu = 0; cpu < 64; ++cpu)
        if ((mask >> cpu) & 1u) CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0 ? mask : 0;
#else
    (void)handle;
    (void)mask;
    return 0;
#endif
}

// Pin plus the APS5_TRACE_AFFINITY=1 line naming the thread.
inline std::uint64_t PinTraced(const char* role, void* handle, std::uint64_t mask) {
    const auto applied = Pin(handle, mask);
    if (Trace()) std::fprintf(stderr, "[affinity] %s tid=%lu mask=0x%llx%s\n", role, ThreadId(handle), static_cast<unsigned long long>(mask), applied != 0 ? "" : " (not applied)");
    return applied;
}

// Helper threads (deferred release, mirror refresh) are pinned only when APS5_HELPER_AFFINITY_MASK
// names their cores.
inline std::atomic<std::uint64_t>& UnreservedMask() {
    static std::atomic<std::uint64_t> mask{0};
    return mask;
}

inline void PinHelperThread(const char* role) {
#ifdef __linux__
    if (const auto unreserved = UnreservedMask().load(std::memory_order_acquire); unreserved != 0) Pin(nullptr, unreserved);
#endif
    static const std::uint64_t mask = MaskFromEnvironment("APS5_HELPER_AFFINITY_MASK");
    if (mask != 0) PinTraced(role, nullptr, mask);
}

}

#endif
