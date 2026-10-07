#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <cstdint>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

extern "C" {
// Guest long is 64 bits, including when the Windows host long is 32 bits.
std::int64_t APS5_VABI sysconf_nid_postfix(int name) {
    const int saved = *__error_nid_postfix();
    std::int64_t result = -1;
    switch (name) {
    case 47: // _SC_PAGESIZE
        result = PS5_PAGE_SIZE;
        break;
    case 57: // _SC_NPROCESSORS_CONF
    case 58: // _SC_NPROCESSORS_ONLN
#ifdef _WIN32
        result = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
        result = ::sysconf(name == 57 ? _SC_NPROCESSORS_CONF : _SC_NPROCESSORS_ONLN);
#endif
        break;
    case 121: { // _SC_PHYS_PAGES: report host physical capacity in guest pages
#ifdef _WIN32
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory))
            result = memory.ullTotalPhys / PS5_PAGE_SIZE;
#else
        const auto pages = ::sysconf(_SC_PHYS_PAGES);
        const auto pageSize = ::sysconf(_SC_PAGESIZE);
        if (pages > 0 && pageSize > 0)
            result = static_cast<std::uint64_t>(pages) * pageSize / PS5_PAGE_SIZE;
#endif
        break;
    }
    default:
        *__error_nid_postfix() = 22;
        return -1;
    }
    if (result <= 0) {
        *__error_nid_postfix() = 5;
        return -1;
    }
    *__error_nid_postfix() = saved;
    return result;
}
}
