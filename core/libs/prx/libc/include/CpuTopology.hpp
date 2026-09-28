#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_CPUTOPOLOGY_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_CPUTOPOLOGY_HPP

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
#include <windows.h>
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
        int lowest = -1;
        int highest = -1;
        for (int cls = 0; cls < 256; ++cls) {
            if (!seen[cls]) continue;
            if (lowest < 0) lowest = cls;
            highest = cls;
        }
        if (lowest < 0 || lowest == highest) return result;
        result.efficient = byClass[lowest];
        result.performant = byClass[highest];
        result.hybrid = result.efficient != 0 && result.performant != 0;
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
inline void PinHelperThread(const char* role) {
    static const std::uint64_t mask = MaskFromEnvironment("APS5_HELPER_AFFINITY_MASK");
    if (mask != 0) PinTraced(role, nullptr, mask);
}

}

#endif
