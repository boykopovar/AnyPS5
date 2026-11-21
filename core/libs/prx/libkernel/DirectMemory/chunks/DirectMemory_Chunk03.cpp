#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <memory>
#include <new>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#elif defined(__EMSCRIPTEN__)
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"

static constexpr int PROT_NONE = 0;
static constexpr int PROT_READ = 1;
static constexpr int PROT_WRITE = 2;
static constexpr int PROT_EXEC = 4;
static constexpr int MAP_PRIVATE = 0x02;
static constexpr int MAP_ANONYMOUS = 0x20;
static constexpr int MAP_FIXED = 0x10;
static void* const MAP_FAILED = reinterpret_cast<void*>(-1);

bool DirectMemoryAliased(std::uintptr_t address, std::size_t bytes) {
    const auto end = address + bytes;
    std::lock_guard lock(g_directLock);
    auto it = g_directMappings.upper_bound(address);
    if (it != g_directMappings.begin()) --it;
    for (; it != g_directMappings.end() && it->first < end; ++it) {
        if (it->second.end <= address) continue;
        const auto first = std::max<std::uintptr_t>(address, it->first);
        const auto last = std::min<std::uintptr_t>(end, it->second.end);
        const auto phys = it->second.phys + (first - it->first);
        if (MappingsSharingPhys(phys, phys + (last - first)).size() > 1) return true;
    }
    return false;
}

std::optional<DirectMemoryView> FirstPhysicalView(std::uint64_t first, std::uint64_t last) {
    std::lock_guard lock(g_directLock);
    auto span = g_physicalSpans.lower_bound(first);
    if (span != g_physicalSpans.begin() && std::prev(span)->second.end > first) --span;
    if (span == g_physicalSpans.end() || span->first >= last) return std::nullopt;
    auto mapping = g_directMappings.find(span->second.bases.front());
    if (mapping == g_directMappings.end()) throw std::logic_error("a physical span lists a direct mapping that does not exist");
    const auto clip = [&](const auto& entry, std::uintptr_t& viewStart, std::uintptr_t& viewEnd) {
        const auto entryLast = entry.second.phys + (entry.second.end - entry.first);
        viewStart = entry.first + (std::max(first, entry.second.phys) - entry.second.phys);
        viewEnd = entry.first + (std::min(last, entryLast) - entry.second.phys);
    };
    std::uintptr_t viewStart = 0;
    std::uintptr_t viewEnd = 0;
    clip(*mapping, viewStart, viewEnd);
    for (auto next = std::next(mapping); viewEnd == mapping->second.end && next != g_directMappings.end() && next->first == viewEnd; mapping = next++) {
        if (next->second.phys < first || next->second.phys >= last) break;
        std::uintptr_t nextStart = 0;
        clip(*next, nextStart, viewEnd);
    }
    return DirectMemoryView{viewStart, viewEnd - viewStart};
}

void UnmapDirectMemoryRange(int64_t start, size_t len) {
    const auto first = static_cast<std::uint64_t>(start);
    const auto last = first + len;
    GuestAllocations::Mutation mutation;
    for (auto view = FirstPhysicalView(first, last); view;) {
        Trace("unmap released view %p+0x%zx", reinterpret_cast<void*>(view->address), view->bytes);
        UnmapRegistered(mutation, reinterpret_cast<void*>(view->address), view->bytes);
        const auto unmapped = *view;
        view = FirstPhysicalView(first, last);
        if (view && view->address == unmapped.address && view->bytes == unmapped.bytes) throw std::runtime_error("released direct memory view survived its unmap");
    }
}

void DirectMemoryPauseBeforeRetypeForTest(void (*pause)()) {
    g_retypePause.store(pause, std::memory_order_release);
}

bool DirectMemoryReleaseClaimed(std::uint64_t first, std::uint64_t last, std::uint64_t* claimEnd) {
    std::lock_guard lock(g_directLock);
    return ReleasingLocked(first, last, claimEnd);
}

bool DirectMemoryTryAddReleaseClaim(std::uint64_t first, std::uint64_t last) {
    std::lock_guard lock(g_directLock);
    if (MapHeldLocked(first, last)) return false;
    g_releaseClaims.emplace(first, last);
    return true;
}

void DirectMemoryRemoveReleaseClaim(std::uint64_t first) {
    std::lock_guard lock(g_directLock);
    g_releaseClaims.erase(first);
}
