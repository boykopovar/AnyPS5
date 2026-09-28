#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <cstdint>
#include <cstring>
#include <climits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" std::int64_t APS5_VABI sysconf_nid_postfix(int name);

namespace {
enum class HardwareQuery { CpuCount, PageSize, PhysicalMemory, RealMemory };

int QueryHardware(const HardwareQuery query, void* oldValue, std::size_t* oldLength,
                  const void* newValue, std::size_t newLength) {
    const auto fail = [](int error) { *__error_nid_postfix() = error; return -1; };
    if ((oldValue != nullptr && oldLength == nullptr) || (newValue == nullptr && newLength != 0))
        return fail(22); // EINVAL.

    // Values with known guest layouts. Firmware-specific names must remain
    // unknown until a firmware profile supplies their actual values.
    std::uint64_t value = 0;
    std::size_t required = 0;
    if (query == HardwareQuery::CpuCount || query == HardwareQuery::PageSize) {
        const auto queried = sysconf_nid_postfix(query == HardwareQuery::CpuCount ? 58 : 47);
        if (queried < 1 || queried > INT_MAX) return fail(5); // EIO.
        value = static_cast<std::uint32_t>(queried);
        required = sizeof(std::uint32_t);
    } else {
        const auto pages = sysconf_nid_postfix(121);
        if (pages < 1 || static_cast<std::uint64_t>(pages) > UINT64_MAX / PS5_PAGE_SIZE)
            return fail(5);
        value = static_cast<std::uint64_t>(pages) * PS5_PAGE_SIZE;
        required = sizeof(std::uint64_t);
    }
    if (newValue != nullptr) return fail(1); // EPERM: these nodes are read-only.
    if (oldLength == nullptr) return fail(22);
    const auto supplied = *oldLength;
    *oldLength = required;
    if (oldValue != nullptr) {
        if (supplied < required) return fail(12); // ENOMEM.
        std::memcpy(oldValue, &value, required);
    }
    return 0;
}
}

extern "C" {
int APS5_VABI sysctlbyname_nid_postfix(const char* name, void* oldValue, std::size_t* oldLength,
                                      const void* newValue, std::size_t newLength) {
    if (!name || !*name) { *__error_nid_postfix() = 22; return -1; }
    if (std::strcmp(name, "hw.ncpu") == 0)
        return QueryHardware(HardwareQuery::CpuCount, oldValue, oldLength, newValue, newLength);
    if (std::strcmp(name, "hw.pagesize") == 0)
        return QueryHardware(HardwareQuery::PageSize, oldValue, oldLength, newValue, newLength);
    if (std::strcmp(name, "hw.physmem") == 0)
        return QueryHardware(HardwareQuery::PhysicalMemory, oldValue, oldLength, newValue, newLength);
    if (std::strcmp(name, "hw.realmem") == 0)
        return QueryHardware(HardwareQuery::RealMemory, oldValue, oldLength, newValue, newLength);
    *__error_nid_postfix() = 2; // ENOENT.
    return -1;
}

int APS5_VABI sysctl_nid_postfix(const int* mib, unsigned int count, void* oldValue,
                                 std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    if (!mib || count == 0 || count > 24) { *__error_nid_postfix() = 22; return -1; }
    if (count != 2 || mib[0] != 6) { *__error_nid_postfix() = 2; return -1; }
    HardwareQuery query;
    switch (mib[1]) {
        case 3: query = HardwareQuery::CpuCount; break;
        case 5: query = HardwareQuery::PhysicalMemory; break;
        case 7: query = HardwareQuery::PageSize; break;
        case 12: query = HardwareQuery::RealMemory; break;
        default: *__error_nid_postfix() = 2; return -1;
    }
    return QueryHardware(query, oldValue, oldLength, newValue, newLength);
}

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
            if (GlobalMemoryStatusEx(&memory)) result = memory.ullTotalPhys / PS5_PAGE_SIZE;
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
    if (result <= 0) { *__error_nid_postfix() = 5; return -1; }
    *__error_nid_postfix() = saved;
    return result;
}
}
