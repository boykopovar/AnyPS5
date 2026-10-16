#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/System/include/HostCpu.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI getpid_nid_postfix();
extern "C" std::uint64_t APS5_VABI sceKernelGetTscFrequency(void);

struct SceKernelSystemSwVersion {
    std::uint64_t size;
    char versionString[0x1C];
    std::uint32_t version;
};

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

bool IsLeafMib(const int* name) {
    return (name[0] == 6 && (name[1] == 2 || name[1] == 3 || name[1] == 5 || name[1] == 7)) || (name[0] == 1 && name[1] == 37);
}

}

extern "C" {
// Guest long is 64 bits, including when the Windows host long is 32 bits.
std::int64_t APS5_VABI sysconf_nid_postfix(int name) {
    const int saved = *__error_nid_postfix();
    std::int64_t result = -1;
    switch (name) {
        case 3: // _SC_CLK_TCK
            result = 128;
            break;
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
    if (nameLength == 2 && name[0] == 6 && name[1] == 7) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        constexpr int pagesize = PS5_PAGE_SIZE;
        return CopyOut(&pagesize, sizeof(pagesize), oldValue, oldLength);
    }
    if (nameLength == 2 && name[0] == 6 && name[1] == 3) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        const std::int64_t processors = sysconf_nid_postfix(58);
        if (processors <= 0) return -1;
        const int value = static_cast<int>(processors);
        return CopyOut(&value, sizeof(value), oldValue, oldLength);
    }
    if (nameLength == 2 && name[0] == 6 && name[1] == 2) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static const std::string model = HostCpuModel();
        if (model.empty()) throw std::runtime_error(std::string(__func__) + ": the host CPU has no CPUID brand string for hw.model");
        return CopyOut(model.c_str(), model.size() + 1, oldValue, oldLength);
    }
    if (nameLength == 2 && name[0] == 6 && (name[1] == 5 || name[1] == 12)) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        const std::int64_t pages = sysconf_nid_postfix(121);
        if (pages <= 0) return -1;
        const std::uint64_t physmem = static_cast<std::uint64_t>(pages) * PS5_PAGE_SIZE;
        return CopyOut(&physmem, sizeof(physmem), oldValue, oldLength);
    }
    if (nameLength >= 3 && name[0] == 1 && name[1] == 14 && name[2] == 12) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (nameLength != 4) {
            *__error_nid_postfix() = 22;
            return -1;
        }
        const int pid = name[3];
        if (pid != -1 && pid != getpid_nid_postfix()) {
            *__error_nid_postfix() = 3;
            return -1;
        }
        static constexpr char path[] = "/app0/eboot.bin";
        return CopyOut(path, sizeof(path), oldValue, oldLength);
    }
    if (nameLength == 2 && name[0] == 1 && name[1] == 37) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (oldLength == nullptr) return 0;
        const std::size_t count = std::min(*oldLength, static_cast<std::size_t>(256));
        if (oldValue != nullptr) {
            static std::random_device device;
            auto* bytes = static_cast<std::uint8_t*>(oldValue);
            for (std::size_t i = 0; i < count; ++i) bytes[i] = static_cast<std::uint8_t>(device());
        }
        *oldLength = count;
        return 0;
    }
    if (nameLength > 2 && IsLeafMib(name)) {
        *__error_nid_postfix() = 20;
        return -1;
    }
    std::string mib;
    for (std::uint32_t i = 0; i < nameLength; ++i) mib += (i == 0 ? "" : ".") + std::to_string(name[i]);
    throw std::runtime_error(std::string(__func__) + ": unsupported MIB " + mib);
}

int APS5_VABI sysctlbyname_nid_postfix(const char* name, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    if (name == nullptr) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    if (std::strcmp(name, "machdep.cacheline_size") == 0) {
        *__error_nid_postfix() = 2;
        return -1;
    }
    if (std::strcmp(name, "kern.proc.pathname") == 0) {
        static constexpr int mib[] = {1, 14, 12};
        return sysctl_nid_postfix(mib, 3, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "kern.arandom") == 0) {
        static constexpr int mib[] = {1, 37};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "kern.rng_pseudo") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (oldValue == nullptr || oldLength == nullptr) throw std::runtime_error(std::string(__func__) + ": kern.rng_pseudo without an output buffer");
        static std::random_device device;
        auto* bytes = static_cast<std::uint8_t*>(oldValue);
        for (std::size_t i = 0; i < *oldLength; ++i) bytes[i] = static_cast<std::uint8_t>(device());
        return 0;
    }
    if (std::strcmp(name, "vm.overcommit") == 0) {
        if (newValue != nullptr) throw std::runtime_error(std::string(__func__) + ": setting vm.overcommit");
        static constexpr int overcommit = 0;
        return CopyOut(&overcommit, sizeof(overcommit), oldValue, oldLength);
    }
    if (std::strcmp(name, "kern.sdk_version") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static constexpr std::uint32_t sdkVersion = 0x01000000;
        return CopyOut(&sdkVersion, sizeof(sdkVersion), oldValue, oldLength);
    }
    if (std::strcmp(name, "machdep.tsc_freq") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        const std::uint64_t frequency = sceKernelGetTscFrequency();
        return CopyOut(&frequency, sizeof(frequency), oldValue, oldLength);
    }
    if (std::strcmp(name, "hw.pagesize") == 0) {
        static constexpr int mib[] = {6, 7};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "hw.ncpu") == 0) {
        static constexpr int mib[] = {6, 3};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "hw.model") == 0) {
        static constexpr int mib[] = {6, 2};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "hw.physmem") == 0) {
        static constexpr int mib[] = {6, 5};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "hw.realmem") == 0) {
        static constexpr int mib[] = {6, 12};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported name " + name);
}

int APS5_VABI sceKernelGetSystemSwVersion_nid_postfix(SceKernelSystemSwVersion* version) {
    if (version == nullptr) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    std::memset(version->versionString, 0, sizeof(version->versionString));
    std::memcpy(version->versionString, "01.000.000", sizeof("01.000.000"));
    version->version = 0x01000000;
    return 0;
}

int APS5_VABI sysctlnametomib_nid_postfix(const char* name, int* mib, std::size_t* size) {
    struct KnownName {
        const char* name;
        int mib[3];
        std::size_t length;
    };
    static constexpr KnownName known[] = {
        {"hw.model", {6, 2}, 2},
        {"hw.ncpu", {6, 3}, 2},
        {"hw.pagesize", {6, 7}, 2},
        {"hw.physmem", {6, 5}, 2},
        {"hw.realmem", {6, 12}, 2},
        {"kern.arandom", {1, 37}, 2},
        {"kern.proc.pathname", {1, 14, 12}, 3},
    };
    constexpr std::size_t MaximumPath = 1024;
    if (name == nullptr || size == nullptr) throw std::invalid_argument(std::string(__func__) + ": null name or size");
    const std::size_t nameLength = strnlen(name, MaximumPath);
    if (nameLength == MaximumPath) {
        *__error_nid_postfix() = 63;
        return -1;
    }
    if (nameLength == 0 || std::strcmp(name, "machdep.cacheline_size") == 0) {
        *__error_nid_postfix() = 2;
        return -1;
    }
    for (const auto& entry : known) {
        if (std::strcmp(name, entry.name) != 0) continue;
        std::size_t bytes = *size * sizeof(int);
        const int result = CopyOut(entry.mib, entry.length * sizeof(int), mib, &bytes);
        *size = bytes / sizeof(int);
        return result;
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported name " + name);
}
}
