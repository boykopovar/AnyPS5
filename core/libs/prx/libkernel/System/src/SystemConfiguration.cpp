#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

int CopyOut(const void* value, std::size_t size, void* oldValue, std::size_t* oldLength) {
    if (oldValue == nullptr) {
        if (oldLength != nullptr) *oldLength = size;
        return 0;
    }
    const std::size_t copied = std::min(oldLength != nullptr ? *oldLength : 0, size);
    std::memcpy(oldValue, value, copied);
    if (oldLength != nullptr) *oldLength = copied;
    if (copied < size) {
        *__error_nid_postfix() = 12;
        return -1;
    }
    return 0;
}

}

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
int APS5_VABI sysctl_nid_postfix(const int* name, std::uint32_t nameLength, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    (void)newLength;
    if (nameLength < 2 || nameLength > 24) {
        *__error_nid_postfix() = 22;
        return -1;
    }
    if (name == nullptr) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    if (nameLength != 2 || name[0] != 6 || name[1] != 3) {
        std::string mib;
        for (std::uint32_t i = 0; i < nameLength; ++i) mib += (i == 0 ? "" : ".") + std::to_string(name[i]);
        throw std::runtime_error(std::string(__func__) + ": unsupported MIB " + mib);
    }
    if (newValue != nullptr) {
        *__error_nid_postfix() = 1;
        return -1;
    }
    const std::int64_t processors = sysconf_nid_postfix(58);
    if (processors <= 0) return -1;
    const int value = static_cast<int>(processors);
    return CopyOut(&value, sizeof(value), oldValue, oldLength);
}

int APS5_VABI sysctlbyname_nid_postfix(const char* name, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    if (name == nullptr) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    if (std::strcmp(name, "hw.ncpu") == 0) {
        static constexpr int mib[] = {6, 3};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "kern.sdk_version") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static constexpr std::uint32_t value = 0x01000000;
        return CopyOut(&value, sizeof(value), oldValue, oldLength);
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported name " + name);
}
}
