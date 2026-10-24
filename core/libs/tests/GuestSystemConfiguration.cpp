#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <vector>
struct SceKernelSystemSwVersion {
    std::uint64_t size;
    char versionString[0x1C];
    std::uint32_t version;
};
static_assert(offsetof(SceKernelSystemSwVersion, size) == 0x00);
static_assert(offsetof(SceKernelSystemSwVersion, versionString) == 0x08);
static_assert(offsetof(SceKernelSystemSwVersion, version) == 0x24);
static_assert(sizeof(SceKernelSystemSwVersion) == 0x28);
extern "C" {
std::int64_t APS5_VABI sysconf_nid_postfix(int);
int APS5_VABI getpagesize_nid_postfix();
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sysctl_nid_postfix(const int*, std::uint32_t, void*, std::size_t*, const void*, std::size_t);
int APS5_VABI sysctlbyname_nid_postfix(const char*, void*, std::size_t*, const void*, std::size_t);
int APS5_VABI sysctlnametomib_nid_postfix(const char*, int*, std::size_t*);
int APS5_VABI sceKernelGetSystemSwVersion_nid_postfix(SceKernelSystemSwVersion*);
int APS5_VABI sceKernelGetSystemExVersion(char*, char*);
int APS5_VABI sysKernelGetLowerLimitSysexVersion_nid_postfix(char*);
int APS5_VABI sysKernelGetLowerLimitUpdVersion_nid_postfix(std::uint32_t*);
int APS5_VABI getpid_nid_postfix();
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t);
int APS5_VABI munmap_nid_postfix(void*, std::size_t);
std::uint64_t APS5_VABI sceKernelGetTscFrequency(void);
struct GuestRlimitTest {
    std::int64_t rlim_cur;
    std::int64_t rlim_max;
};
int APS5_VABI getrlimit_nid_postfix(int resource, GuestRlimitTest* limit);
extern char** environ_nid_postfix;
}
static void Require(bool value) { if (!value) std::abort(); }
static void CheckProcessorCountSysctl() {
    const int mib[] = {6, 3};
    const int processors = static_cast<int>(sysconf_nid_postfix(58));
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == sizeof(int));
    int value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0 && value == processors && length == sizeof(int));
    value = 0;
    length = 16;
    Require(sysctlbyname_nid_postfix("hw.ncpu", &value, &length, nullptr, 0) == 0 && value == processors && length == sizeof(int));
    Require(*__error_nid_postfix() == 13);
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("hw.ncpu", bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && bytes[2] == 0xaa && bytes[3] == 0xaa);
    Require(sysctl_nid_postfix(mib, 2, bytes, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctl_nid_postfix(mib, 1, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    Require(sysctl_nid_postfix(mib, 25, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    Require(sysctl_nid_postfix(nullptr, 2, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(sysctlbyname_nid_postfix(nullptr, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    bool threw = false;
    const int unknown[] = {6, 99};
    try {
        sysctl_nid_postfix(unknown, 2, &value, &length, nullptr, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
static void CheckPageSizeSysctl() {
    const int mib[] = {6, 7};
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == sizeof(int));
    int value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0 && value == 0x4000 && length == sizeof(int));
    value = 0;
    length = 16;
    Require(sysctlbyname_nid_postfix("hw.pagesize", &value, &length, nullptr, 0) == 0 && value == 0x4000 && length == sizeof(int));
    Require(*__error_nid_postfix() == 13);
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("hw.pagesize", bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && bytes[2] == 0xaa && bytes[3] == 0xaa);
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctlbyname_nid_postfix("hw.pagesize", &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
}
static void CheckPhysicalMemorySysctl(int number, const char* name) {
    const int mib[] = {6, number};
    const std::uint64_t expected = static_cast<std::uint64_t>(sysconf_nid_postfix(121)) * 0x4000;
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == sizeof(std::uint64_t));
    std::uint64_t value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0 && value == expected && length == sizeof(std::uint64_t));
    value = 0;
    length = 16;
    Require(sysctlbyname_nid_postfix(name, &value, &length, nullptr, 0) == 0 && value == expected && length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 13);
    unsigned char bytes[8] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    length = 4;
    Require(sysctlbyname_nid_postfix(name, bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 4 && bytes[4] == 0xaa);
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctlbyname_nid_postfix(name, &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
}
#if defined(__x86_64__) || defined(__i386__)
static void CheckModelSysctl() {
    const int mib[] = {6, 2};
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length > 1);
    const std::size_t size = length;
    char model[128] = {};
    length = sizeof(model);
    Require(sysctlbyname_nid_postfix("hw.model", model, &length, nullptr, 0) == 0 && length == size);
    Require(std::strlen(model) + 1 == size && model[0] != ' ');
    Require(*__error_nid_postfix() == 13);
    char truncated[4] = {};
    length = sizeof(truncated);
    Require(sysctl_nid_postfix(mib, 2, truncated, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == sizeof(truncated) && std::memcmp(truncated, model, sizeof(truncated)) == 0);
    length = sizeof(model);
    Require(sysctlbyname_nid_postfix("hw.model", model, &length, model, size) == -1 && *__error_nid_postfix() == 1);
}
#endif
static void CheckTscFrequencySysctl() {
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("machdep.tsc_freq", nullptr, &length, nullptr, 0) == 0 && length == sizeof(std::uint64_t));
    std::uint64_t value = 0;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("machdep.tsc_freq", &value, &length, nullptr, 0) == 0);
    Require(value == sceKernelGetTscFrequency() && value != 0 && length == sizeof(value));
    Require(*__error_nid_postfix() == 13);
    std::uint32_t truncated = 0;
    length = sizeof(truncated);
    Require(sysctlbyname_nid_postfix("machdep.tsc_freq", &truncated, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("machdep.tsc_freq", &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
}
static bool Untouched(const unsigned char* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xaa) return false;
    return true;
}
template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
static void CheckUnansweredSysctl() {
    static constexpr const char* names[] = {
        "hw.byteorder",
        "hw.cacheline_size",
        "hw.machine_arch",
        "hw.usermem",
        "kern.clockrate",
        "kern.maxfiles",
        "kern.maxfilesperproc",
        "kern.maxproc",
        "kern.ps_strings",
        "kern.sched.name",
        "kern.smp.maxcpus",
        "kern.stackprot",
        "kern.timecounter.hardware",
        "machdep.cacheline_size",
    };
    unsigned char buffer[64];
    for (const char* name : names) {
        std::memset(buffer, 0xaa, sizeof(buffer));
        std::size_t length = sizeof(buffer);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 2);
        Require(length == sizeof(buffer) && Untouched(buffer, sizeof(buffer)));
        length = 77;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 2 && length == 77);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buffer, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 2);
        Require(Untouched(buffer, sizeof(buffer)));
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, nullptr, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 2);
        if (std::strcmp(name, "machdep.cacheline_size") == 0) continue;
        length = sizeof(buffer);
        Require(Throws([&] { sysctlbyname_nid_postfix(name, buffer, &length, buffer, sizeof(buffer)); }));
    }
    struct Numbered {
        const char* name;
        int mib[2];
    };
    static constexpr Numbered numbered[] = {
        {"hw.byteorder", {6, 4}},
        {"hw.machine_arch", {6, 11}},
        {"kern.clockrate", {1, 12}},
        {"kern.maxfiles", {1, 7}},
        {"kern.maxfilesperproc", {1, 27}},
        {"kern.maxproc", {1, 6}},
        {"kern.ps_strings", {1, 32}},
    };
    for (const auto& entry : numbered) {
        int mib[4] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
        std::size_t size = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(entry.name, mib, &size) == 0 && size == 2);
        Require(mib[0] == entry.mib[0] && mib[1] == entry.mib[1] && mib[2] == 0x5a5a5a5a);
        Require(*__error_nid_postfix() == 13);
        std::memset(buffer, 0xaa, sizeof(buffer));
        std::size_t length = sizeof(buffer);
        Require(sysctl_nid_postfix(mib, 2, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 2);
        Require(length == sizeof(buffer) && Untouched(buffer, sizeof(buffer)));
        length = 77;
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 2 && length == 77);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(mib, 2, buffer, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 2 && Untouched(buffer, sizeof(buffer)));
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(mib, 2, nullptr, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 2);
        length = sizeof(buffer);
        Require(Throws([&] { sysctl_nid_postfix(mib, 2, buffer, &length, buffer, sizeof(buffer)); }));
    }
    int mib[4] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::size_t size = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("kern.maxproc", nullptr, &size) == 0 && size == 2 && *__error_nid_postfix() == 13);
    size = 1;
    Require(sysctlnametomib_nid_postfix("kern.maxproc", mib, &size) == -1 && *__error_nid_postfix() == 12);
    Require(size == 1 && mib[0] == 1 && mib[1] == 0x5a5a5a5a);
    mib[0] = 0x5a5a5a5a;
    size = 0;
    Require(sysctlnametomib_nid_postfix("kern.maxproc", mib, &size) == -1 && *__error_nid_postfix() == 12);
    Require(size == 0 && mib[0] == 0x5a5a5a5a);
    static constexpr const char* unresolved[] = {"hw.cacheline_size", "hw.usermem", "machdep.cacheline_size"};
    for (const char* name : unresolved) {
        int untouched[4] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
        size = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(name, untouched, &size) == -1 && *__error_nid_postfix() == 2);
        Require(size == 4 && untouched[0] == 0x5a5a5a5a && untouched[1] == 0x5a5a5a5a && untouched[2] == 0x5a5a5a5a);
    }
}
static void CheckUnknownSysctlName() {
    static constexpr const char* name = "kern.anyps5_no_such_name";
    unsigned char buffer[16];
    std::memset(buffer, 0xaa, sizeof(buffer));
    std::size_t length = sizeof(buffer);
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix(name, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 2);
    Require(length == sizeof(buffer) && Untouched(buffer, sizeof(buffer)));

    length = 77;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix(name, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 2 && length == 77);

    int mib[4] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::size_t mibLength = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix(name, mib, &mibLength) == -1 && *__error_nid_postfix() == 2);
    Require(mibLength == 4 && mib[0] == 0x5a5a5a5a && mib[1] == 0x5a5a5a5a && mib[2] == 0x5a5a5a5a && mib[3] == 0x5a5a5a5a);

    mibLength = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix(name, nullptr, &mibLength) == -1 && *__error_nid_postfix() == 2 && mibLength == 0);
}
static void CheckPathnameSysctl() {
    *__error_nid_postfix() = 13;
    char buffer[32] = {};
    std::size_t length = sizeof(buffer);
    Require(sysctlbyname_nid_postfix("kern.proc.pathname", buffer, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    const int mibTooShort[] = {1, 14, 12};
    Require(sysctl_nid_postfix(mibTooShort, 3, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);

    const int mibTooLong[] = {1, 14, 12, -1, 0};
    Require(sysctl_nid_postfix(mibTooLong, 5, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);

    Require(sysctl_nid_postfix(mibTooShort, 3, buffer, &length, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctl_nid_postfix(mibTooLong, 5, buffer, &length, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctlbyname_nid_postfix("kern.proc.pathname", buffer, &length, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == 1);

    const int mibSelf[] = {1, 14, 12, -1};
    length = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, nullptr, &length, nullptr, 0) == 0 && length == sizeof("/app0/eboot.bin"));

    std::memset(buffer, 0xaa, sizeof(buffer));
    length = sizeof(buffer);
    Require(sysctl_nid_postfix(mibSelf, 4, buffer, &length, nullptr, 0) == 0);
    Require(length == sizeof("/app0/eboot.bin"));
    Require(std::strcmp(buffer, "/app0/eboot.bin") == 0);

    const int callerPid = getpid_nid_postfix();
    const int mibPid[] = {1, 14, 12, callerPid};
    std::memset(buffer, 0xaa, sizeof(buffer));
    length = sizeof(buffer);
    Require(sysctl_nid_postfix(mibPid, 4, buffer, &length, nullptr, 0) == 0);
    Require(length == sizeof("/app0/eboot.bin"));
    Require(std::strcmp(buffer, "/app0/eboot.bin") == 0);

    const int mibForeign[] = {1, 14, 12, callerPid + 1000};
    Require(sysctl_nid_postfix(mibForeign, 4, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 3);

    length = 5;
    std::memset(buffer, 0xaa, sizeof(buffer));
    Require(sysctl_nid_postfix(mibSelf, 4, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 5 && buffer[4] == '0' && buffer[5] == static_cast<char>(0xaa));

    length = sizeof(buffer);
    Require(sysctl_nid_postfix(mibSelf, 4, buffer, &length, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == 1);
}
struct SystemString {
    int mib[2];
    const char* name;
    const char* value;
};
static const SystemString SystemStrings[] = {
    {{1, 1}, "kern.ostype", "FreeBSD"},
    {{1, 2}, "kern.osrelease", "11.4-RELEASE"},
    {{1, 4}, "kern.version", "FreeBSD 11.4-RELEASE\n"},
    {{1, 10}, "kern.hostname", "AnyPS5"},
    {{6, 1}, "hw.machine", "amd64"},
};
static void CheckSystemStringSysctl() {
    for (const auto& entry : SystemStrings) {
        const std::size_t size = std::strlen(entry.value) + 1;
        *__error_nid_postfix() = 13;
        std::size_t length = 0;
        Require(sysctl_nid_postfix(entry.mib, 2, nullptr, &length, nullptr, 0) == 0 && length == size);
        char buffer[32];
        std::memset(buffer, 0xaa, sizeof(buffer));
        length = sizeof(buffer);
        Require(sysctl_nid_postfix(entry.mib, 2, buffer, &length, nullptr, 0) == 0 && length == size);
        Require(std::strcmp(buffer, entry.value) == 0 && buffer[size] == static_cast<char>(0xaa));
        Require(*__error_nid_postfix() == 13);
        std::memset(buffer, 0xaa, sizeof(buffer));
        length = 2;
        Require(sysctl_nid_postfix(entry.mib, 2, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
        Require(length == 2 && std::memcmp(buffer, entry.value, 2) == 0 && buffer[2] == static_cast<char>(0xaa));
        length = sizeof(buffer);
        Require(sysctl_nid_postfix(entry.mib, 2, buffer, &length, "Prospero", sizeof("Prospero")) == -1 && *__error_nid_postfix() == 1);
        const int deeper[] = {entry.mib[0], entry.mib[1], 0};
        length = sizeof(buffer);
        Require(sysctl_nid_postfix(deeper, 3, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 20);
        Require(length == sizeof(buffer));
    }
}
static void CheckSystemStringNames() {
    for (const auto& entry : SystemStrings) {
        *__error_nid_postfix() = 13;
        char buffer[32];
        std::memset(buffer, 0xaa, sizeof(buffer));
        std::size_t length = sizeof(buffer);
        Require(sysctlbyname_nid_postfix(entry.name, buffer, &length, nullptr, 0) == 0 && length == std::strlen(entry.value) + 1);
        Require(std::strcmp(buffer, entry.value) == 0 && *__error_nid_postfix() == 13);
        Require(sysctlbyname_nid_postfix(entry.name, buffer, &length, "Prospero", sizeof("Prospero")) == -1 && *__error_nid_postfix() == 1);
        int mib[3] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
        std::size_t size = 3;
        Require(sysctlnametomib_nid_postfix(entry.name, mib, &size) == 0 && size == 2);
        Require(mib[0] == entry.mib[0] && mib[1] == entry.mib[1] && mib[2] == 0x5a5a5a5a);
    }
}
static void CheckOsReleaseDateSysctl() {
    const int mib[] = {1, 24};
    constexpr std::int32_t expected = 1104001;
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == sizeof(expected));
    std::int32_t value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0 && value == expected && length == sizeof(value));
    Require(*__error_nid_postfix() == 13);

    value = 0;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("kern.osreldate", &value, &length, nullptr, 0) == 0 && value == expected && length == sizeof(value));
    Require(*__error_nid_postfix() == 13);

    int namedMib[3] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::size_t mibLength = 3;
    Require(sysctlnametomib_nid_postfix("kern.osreldate", namedMib, &mibLength) == 0 && mibLength == 2);
    Require(namedMib[0] == mib[0] && namedMib[1] == mib[1] && namedMib[2] == 0x5a5a5a5a);
    value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(namedMib, static_cast<std::uint32_t>(mibLength), &value, &length, nullptr, 0) == 0 && value == expected);

    unsigned char truncated[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("kern.osreldate", truncated, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && truncated[0] == static_cast<unsigned char>(expected) && truncated[1] == static_cast<unsigned char>(expected >> 8) && truncated[2] == 0xaa && truncated[3] == 0xaa);

    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctlbyname_nid_postfix("kern.osreldate", &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);

    const int deeper[] = {1, 24, 0};
    length = sizeof(value);
    Require(sysctl_nid_postfix(deeper, 3, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 20);
}
struct BootTimeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};
static_assert(sizeof(BootTimeval) == 16);
struct BootTimespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};
extern "C" int APS5_VABI clock_gettime_nid_postfix(int, BootTimespec*);
static void CheckBootTimeSysctl() {
    const int mib[] = {1, 21};
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == sizeof(BootTimeval));
    length = 0;
    Require(sysctlbyname_nid_postfix("kern.boottime", nullptr, &length, nullptr, 0) == 0 && length == sizeof(BootTimeval));
    BootTimeval first{};
    length = sizeof(first);
    Require(sysctl_nid_postfix(mib, 2, &first, &length, nullptr, 0) == 0 && length == sizeof(first));
    Require(first.tv_sec > 0 && first.tv_sec <= static_cast<std::int64_t>(std::time(nullptr)));
    Require(first.tv_usec >= 0 && first.tv_usec < 1000000);
    BootTimespec now{};
    BootTimespec uptime{};
    Require(clock_gettime_nid_postfix(5, &uptime) == 0 && clock_gettime_nid_postfix(0, &now) == 0);
    const std::int64_t drift = (first.tv_sec + uptime.tv_sec - now.tv_sec) * 1000000 + first.tv_usec + uptime.tv_nsec / 1000 - now.tv_nsec / 1000;
    Require(drift > -1000000 && drift < 1000000);
    BootTimeval second{};
    length = sizeof(second);
    Require(sysctl_nid_postfix(mib, 2, &second, &length, nullptr, 0) == 0 && length == sizeof(second));
    Require(second.tv_sec == first.tv_sec && second.tv_usec == first.tv_usec);
    BootTimeval named{};
    length = 32;
    Require(sysctlbyname_nid_postfix("kern.boottime", &named, &length, nullptr, 0) == 0 && length == sizeof(named));
    Require(named.tv_sec == first.tv_sec && named.tv_usec == first.tv_usec);
    Require(*__error_nid_postfix() == 13);

    int namedMib[3] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::size_t mibLength = 3;
    Require(sysctlnametomib_nid_postfix("kern.boottime", namedMib, &mibLength) == 0 && mibLength == 2);
    Require(namedMib[0] == 1 && namedMib[1] == 21 && namedMib[2] == 0x5a5a5a5a);

    unsigned char truncated[16];
    std::memset(truncated, 0xaa, sizeof(truncated));
    length = 8;
    Require(sysctl_nid_postfix(mib, 2, truncated, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 8 && std::memcmp(truncated, &first.tv_sec, 8) == 0 && truncated[8] == 0xaa);
    std::memset(truncated, 0xaa, sizeof(truncated));
    length = 8;
    Require(sysctlbyname_nid_postfix("kern.boottime", truncated, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 8 && truncated[8] == 0xaa);

    length = sizeof(first);
    Require(sysctl_nid_postfix(mib, 2, &first, &length, &first, sizeof(first)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctlbyname_nid_postfix("kern.boottime", &first, &length, &first, sizeof(first)) == -1 && *__error_nid_postfix() == 1);
}
static void CheckLeafWithTrailingComponent() {
    const int leaves[][2] = {{6, 3}, {6, 2}, {6, 5}, {6, 7}, {1, 21}, {1, 24}, {1, 37}, {1, 50}};
    for (const auto& leaf : leaves) {
        const int mib[] = {leaf[0], leaf[1], 0};
        char buffer[8] = {};
        std::size_t length = sizeof(buffer);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(mib, 3, buffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 20);
        Require(sysctl_nid_postfix(mib, 3, buffer, &length, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == 20);
        Require(length == sizeof(buffer));
    }
}
static void CheckMeasuredAbsentSysctlMibs() {
    static constexpr struct {
        int mib[3];
        std::uint32_t length;
    } cases[] = {
        {{1, 2147482816, 2147482814}, 3},
        {{1, 2147483151}, 2},
        {{1, 2147483182, 2147483181}, 3},
        {{1, 2147482838, 2147482834}, 3},
    };
    for (const auto& entry : cases) {
        unsigned char buffer[64];
        std::memset(buffer, 0xaa, sizeof(buffer));
        std::size_t length = sizeof(buffer);
        *__error_nid_postfix() = 77;
        Require(sysctl_nid_postfix(entry.mib, entry.length, buffer, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 2 && length == sizeof(buffer));
        for (const auto byte : buffer) Require(byte == 0xaa);

        length = 77;
        *__error_nid_postfix() = 77;
        Require(sysctl_nid_postfix(entry.mib, entry.length, nullptr, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 2 && length == 77);

        std::memset(buffer, 0xaa, sizeof(buffer));
        *__error_nid_postfix() = 77;
        Require(sysctl_nid_postfix(entry.mib, entry.length, buffer, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 2);
        for (const auto byte : buffer) Require(byte == 0xaa);

        *__error_nid_postfix() = 77;
        Require(sysctl_nid_postfix(entry.mib, entry.length, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 2);

        bool threw = false;
        try {
            sysctl_nid_postfix(entry.mib, entry.length, buffer, &length, buffer, sizeof(buffer));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
}
static void CheckVersionLimits() {
    unsigned char first[4];
    unsigned char second[4];
    std::memset(first, 0xaa, sizeof(first));
    std::memset(second, 0xaa, sizeof(second));
    *__error_nid_postfix() = 13;
    Require(sceKernelGetSystemExVersion(reinterpret_cast<char*>(first + 1), reinterpret_cast<char*>(second + 1)) == 0 && *__error_nid_postfix() == 13);
    Require(first[0] == 0xaa && first[1] == 0 && first[2] == 0xaa && second[0] == 0xaa && second[1] == 0 && second[2] == 0xaa);
    std::memset(first, 0xaa, sizeof(first));
    Require(sceKernelGetSystemExVersion(reinterpret_cast<char*>(first), nullptr) == 22 && first[0] == 0xaa);
    Require(sceKernelGetSystemExVersion(nullptr, reinterpret_cast<char*>(first)) == 22 && first[0] == 0xaa);
    Require(sceKernelGetSystemExVersion(nullptr, nullptr) == 22 && *__error_nid_postfix() == 13);

    std::memset(first, 0xaa, sizeof(first));
    Require(sysKernelGetLowerLimitSysexVersion_nid_postfix(reinterpret_cast<char*>(first + 1)) == 0);
    Require(first[0] == 0xaa && first[1] == 0 && first[2] == 0xaa);
    Require(sysKernelGetLowerLimitSysexVersion_nid_postfix(nullptr) == SCE_KERNEL_ERROR_EINVAL);

    struct {
        std::uint32_t version;
        std::uint32_t after;
    } update{0xaaaaaaaau, 0xaaaaaaaau};
    Require(sysKernelGetLowerLimitUpdVersion_nid_postfix(&update.version) == 0 && update.version == 0 && update.after == 0xaaaaaaaau);
    Require(sysKernelGetLowerLimitUpdVersion_nid_postfix(nullptr) == SCE_KERNEL_ERROR_EINVAL && *__error_nid_postfix() == 13);
}
static void CheckSystemSwVersion() {
    *__error_nid_postfix() = 13;
    Require(sceKernelGetSystemSwVersion_nid_postfix(nullptr) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 13);
    const std::uint64_t sizes[] = {0, 24, 39, 40, 41, 48, 64};
    for (const std::uint64_t size : sizes) {
        struct {
            std::uint64_t prefix;
            SceKernelSystemSwVersion version;
            unsigned char suffix[88];
        } storage;
        std::memset(&storage, 0xaa, sizeof(storage));
        storage.version.size = size;
        *__error_nid_postfix() = 13;
        Require(sceKernelGetSystemSwVersion_nid_postfix(&storage.version) == 0 && *__error_nid_postfix() == 13);
        Require(storage.prefix == 0xaaaaaaaaaaaaaaaaull && storage.version.size == size);
        Require(std::strcmp(storage.version.versionString, "01.000.000") == 0);
        for (std::size_t i = sizeof("01.000.000"); i < sizeof(storage.version.versionString); ++i) {
            Require(storage.version.versionString[i] == 0);
        }
        Require(storage.version.version == 0x01000000);
        for (unsigned char byte : storage.suffix) Require(byte == 0xaa);
    }
}
static void CheckPseudoRandomSysctl() {
    unsigned char first[128];
    unsigned char second[128];
    const auto untouchedFrom = [&first](std::size_t from) {
        for (std::size_t i = from; i < sizeof(first); ++i)
            if (first[i] != 0xaa) return false;
        return true;
    };
    *__error_nid_postfix() = 13;
    std::memset(first, 0xaa, sizeof(first));
    std::memset(second, 0xaa, sizeof(second));
    std::size_t length = 64;
    Require(sysctlbyname_nid_postfix("kern.rng_pseudo", first, &length, nullptr, 0) == 0 && length == 64 && untouchedFrom(64) && !untouchedFrom(0));
    length = 64;
    Require(sysctlbyname_nid_postfix("kern.rng_pseudo", second, &length, nullptr, 0) == 0 && length == 64);
    Require(std::memcmp(first, second, 64) != 0);
    Require(*__error_nid_postfix() == 13);
    static constexpr std::size_t larger[] = {65, 128};
    for (std::size_t asked : larger) {
        std::memset(first, 0xaa, sizeof(first));
        length = asked;
        Require(sysctlbyname_nid_postfix("kern.rng_pseudo", first, &length, nullptr, 0) == 0 && length == 64 && untouchedFrom(64) && !untouchedFrom(0));
    }
    Require(*__error_nid_postfix() == 13);
    static constexpr std::size_t smaller[] = {0, 1, 63};
    for (std::size_t asked : smaller) {
        std::memset(first, 0xaa, sizeof(first));
        length = asked;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix("kern.rng_pseudo", first, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
        Require(length == asked && untouchedFrom(asked));
        if (asked == 63) Require(!untouchedFrom(0));
    }
    static constexpr std::size_t presets[] = {0, 1, 64, 100, 4096};
    for (std::size_t preset : presets) {
        length = preset;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix("kern.rng_pseudo", nullptr, &length, nullptr, 0) == 0 && length == 64 && *__error_nid_postfix() == 13);
    }
    std::memset(first, 0xaa, sizeof(first));
    Require(sysctlbyname_nid_postfix("kern.rng_pseudo", first, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 12 && untouchedFrom(0));
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("kern.rng_pseudo", nullptr, nullptr, nullptr, 0) == 0 && *__error_nid_postfix() == 13);
    length = 64;
    Require(sysctlbyname_nid_postfix("kern.rng_pseudo", first, &length, first, 64) == -1 && *__error_nid_postfix() == 1);
}
static void CheckOvercommitSysctl() {
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("vm.overcommit", nullptr, &length, nullptr, 0) == 0 && length == sizeof(int));
    int value = -1;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("vm.overcommit", &value, &length, nullptr, 0) == 0 && value == 0 && length == sizeof(int));
    Require(*__error_nid_postfix() == 13);
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("vm.overcommit", bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && bytes[2] == 0xaa && bytes[3] == 0xaa);
    bool threw = false;
    length = sizeof(value);
    try {
        sysctlbyname_nid_postfix("vm.overcommit", &value, &length, &value, sizeof(value));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
static void CheckSdkVersionSysctl() {
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", nullptr, &length, nullptr, 0) == 0 && length == sizeof(std::uint32_t));
    std::uint32_t version = 0;
    length = sizeof(version);
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &version, &length, nullptr, 0) == 0 && version == 0x01000000 && length == sizeof(std::uint32_t));
    Require(*__error_nid_postfix() == 13);
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && bytes[2] == 0xaa && bytes[3] == 0xaa);
    length = sizeof(version);
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &version, &length, &version, sizeof(version)) == -1 && *__error_nid_postfix() == 1);
}
static void CheckArandomSysctl() {
    const int mib[] = {1, 37};
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == 0);
    length = 64;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == 64);
    length = 300;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == 256);
    Require(sysctl_nid_postfix(mib, 2, nullptr, nullptr, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 13);
    unsigned char first[64] = {};
    unsigned char second[64] = {};
    length = sizeof(first);
    Require(sysctl_nid_postfix(mib, 2, first, &length, nullptr, 0) == 0 && length == sizeof(first));
    length = sizeof(second);
    Require(sysctl_nid_postfix(mib, 2, second, &length, nullptr, 0) == 0 && length == sizeof(second));
    Require(std::memcmp(first, second, sizeof(first)) != 0);
    Require(*__error_nid_postfix() == 13);
    unsigned char named[64] = {};
    length = sizeof(named);
    Require(sysctlbyname_nid_postfix("kern.arandom", named, &length, nullptr, 0) == 0 && length == sizeof(named));
    Require(*__error_nid_postfix() == 13);
    length = sizeof(first);
    Require(sysctl_nid_postfix(mib, 2, first, &length, first, sizeof(first)) == -1 && *__error_nid_postfix() == 1);
    *__error_nid_postfix() = 13;
    unsigned char big[300] = {};
    length = sizeof(big);
    Require(sysctl_nid_postfix(mib, 2, big, &length, nullptr, 0) == 0 && length == 256);
    unsigned char small[16] = {};
    length = sizeof(small);
    Require(sysctl_nid_postfix(mib, 2, small, &length, nullptr, 0) == 0 && length == sizeof(small));
    Require(sysctl_nid_postfix(mib, 2, first, nullptr, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 13);
}
struct VmRange {
    std::uint64_t start;
    std::uint64_t end;
    std::int32_t protection;
};
static std::vector<VmRange> ReadVmmap(const int* mib) {
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, &length, nullptr, 0) == 0 && length != 0 && length % 8 == 0);
    std::vector<unsigned char> buffer(length * 4 / 3);
    length = buffer.size();
    Require(sysctl_nid_postfix(mib, 4, buffer.data(), &length, nullptr, 0) == 0 && length != 0 && length <= buffer.size());
    std::vector<VmRange> ranges;
    for (std::size_t at = 0; at < length;) {
        std::int32_t size = 0;
        std::memcpy(&size, buffer.data() + at, sizeof(size));
        Require(size > 136 && size % 8 == 0 && at + static_cast<std::size_t>(size) <= length);
        Require(std::memchr(buffer.data() + at + 136, 0, static_cast<std::size_t>(size) - 136) != nullptr);
        VmRange range{};
        std::memcpy(&range.start, buffer.data() + at + 8, sizeof(range.start));
        std::memcpy(&range.end, buffer.data() + at + 16, sizeof(range.end));
        std::memcpy(&range.protection, buffer.data() + at + 56, sizeof(range.protection));
        Require(range.start < range.end && (ranges.empty() || ranges.back().end <= range.start));
        ranges.push_back(range);
        at += static_cast<std::size_t>(size);
    }
    return ranges;
}
static void CheckVmmapSysctl() {
    const int self = getpid_nid_postfix();
    const int mib[] = {1, 14, 32, self};
    constexpr std::size_t Bytes = 0x10000;
    *__error_nid_postfix() = 13;
    void* mapped = mmap_nid_postfix(nullptr, Bytes, 3, 0x1002, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mapped));
    auto ranges = ReadVmmap(mib);
    const auto covering = std::find_if(ranges.begin(), ranges.end(), [&](const VmRange& range) { return range.start <= address && address + Bytes <= range.end; });
    Require(covering != ranges.end() && (covering->protection & 3) == 3);
    Require(munmap_nid_postfix(mapped, Bytes) == 0);
    ranges = ReadVmmap(mib);
    Require(std::none_of(ranges.begin(), ranges.end(), [&](const VmRange& range) { return range.start < address + Bytes && address < range.end; }));
    Require(*__error_nid_postfix() == 13);

    unsigned char truncated[32];
    std::memset(truncated, 0xaa, sizeof(truncated));
    std::size_t length = 16;
    Require(sysctl_nid_postfix(mib, 4, truncated, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 16 && truncated[16] == 0xaa);
    length = sizeof(truncated);
    Require(sysctl_nid_postfix(mib, 4, truncated, &length, truncated, sizeof(truncated)) == -1 && *__error_nid_postfix() == 1);
    const int foreign[] = {1, 14, 32, self + 1000};
    Require(sysctl_nid_postfix(foreign, 4, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 3);
    const int current[] = {1, 14, 32, -1};
    Require(sysctl_nid_postfix(current, 4, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 3);
    Require(sysctl_nid_postfix(mib, 3, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    const int tooLong[] = {1, 14, 32, self, 0};
    Require(sysctl_nid_postfix(tooLong, 5, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
}
static void CheckEnvSysctl() {
    const int self = getpid_nid_postfix();
    const int mib[] = {1, 14, 35, self};
    *__error_nid_postfix() = 13;
    std::size_t length = 7;
    Require(sysctl_nid_postfix(mib, 4, nullptr, &length, nullptr, 0) == 0 && length == 0);
    unsigned char buffer[16];
    std::memset(buffer, 0xaa, sizeof(buffer));
    length = sizeof(buffer);
    Require(sysctl_nid_postfix(mib, 4, buffer, &length, nullptr, 0) == 0 && length == 0 && buffer[0] == 0xaa);
    Require(*__error_nid_postfix() == 13);
    length = sizeof(buffer);
    Require(sysctl_nid_postfix(mib, 4, buffer, &length, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == 1);
    const int foreign[] = {1, 14, 35, self + 1000};
    Require(sysctl_nid_postfix(foreign, 4, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 3);
    const int current[] = {1, 14, 35, -1};
    Require(sysctl_nid_postfix(current, 4, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 3);
    Require(sysctl_nid_postfix(mib, 3, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    const int tooLong[] = {1, 14, 35, self, 0};
    Require(sysctl_nid_postfix(tooLong, 5, nullptr, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
}
static void CheckPseudoRandomMibSysctl() {
    const int mib[] = {1, 50};
    *__error_nid_postfix() = 13;
    std::size_t length = 7;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == 64);
    Require(*__error_nid_postfix() == 13);
    Require(sysctl_nid_postfix(mib, 2, nullptr, nullptr, nullptr, 0) == 0);
    unsigned char bytes[80];
    std::memset(bytes, 0xaa, sizeof(bytes));
    length = 64;
    Require(sysctl_nid_postfix(mib, 2, bytes, &length, nullptr, 0) == 0 && length == 64);
    bool changed = false;
    for (std::size_t i = 0; i < 64; ++i) changed = changed || bytes[i] != 0xaa;
    Require(changed && bytes[64] == 0xaa && bytes[79] == 0xaa);
    unsigned char other[64] = {};
    length = sizeof(other);
    Require(sysctl_nid_postfix(mib, 2, other, &length, nullptr, 0) == 0 && length == sizeof(other));
    Require(std::memcmp(bytes, other, sizeof(other)) != 0);

    unsigned char shortBuffer[64];
    std::memset(shortBuffer, 0xaa, sizeof(shortBuffer));
    length = 63;
    Require(sysctl_nid_postfix(mib, 2, shortBuffer, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    bool shortChanged = false;
    for (std::size_t i = 0; i < 63; ++i) shortChanged = shortChanged || shortBuffer[i] != 0xaa;
    Require(length == 63 && shortChanged && shortBuffer[63] == 0xaa);
    std::memset(shortBuffer, 0xaa, sizeof(shortBuffer));
    Require(sysctl_nid_postfix(mib, 2, shortBuffer, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    for (unsigned char byte : shortBuffer) Require(byte == 0xaa);
    int value = 0;
    length = sizeof(value);
    bool threw = false;
    try {
        sysctl_nid_postfix(mib, 2, &value, &length, &value, sizeof(value));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
static void CheckNameToMib() {
    struct Expected {
        const char* name;
        int mib[3];
        std::size_t length;
    };
    const Expected expected[] = {
        {"hw.model", {6, 2}, 2},
        {"hw.ncpu", {6, 3}, 2},
        {"hw.pagesize", {6, 7}, 2},
        {"hw.physmem", {6, 5}, 2},
        {"hw.realmem", {6, 12}, 2},
        {"kern.arandom", {1, 37}, 2},
        {"kern.boottime", {1, 21}, 2},
        {"kern.rng_pseudo", {1, 50}, 2},
        {"kern.proc.pathname", {1, 14, 12}, 3},
    };
    *__error_nid_postfix() = 13;
    for (const auto& entry : expected) {
        std::size_t size = 0;
        Require(sysctlnametomib_nid_postfix(entry.name, nullptr, &size) == 0 && size == entry.length);
        int mib[5] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
        size = 5;
        Require(sysctlnametomib_nid_postfix(entry.name, mib, &size) == 0 && size == entry.length);
        Require(std::memcmp(mib, entry.mib, entry.length * sizeof(int)) == 0 && mib[entry.length] == 0x5a5a5a5a);
    }
    Require(*__error_nid_postfix() == 13);

    int mib[4] = {};
    std::size_t size = 4;
    Require(sysctlnametomib_nid_postfix("hw.ncpu", mib, &size) == 0 && size == 2);
    int processors = 0;
    std::size_t length = sizeof(processors);
    Require(sysctl_nid_postfix(mib, static_cast<std::uint32_t>(size), &processors, &length, nullptr, 0) == 0);
    Require(processors == static_cast<int>(sysconf_nid_postfix(58)));
    size = 4;
    Require(sysctlnametomib_nid_postfix("hw.realmem", mib, &size) == 0 && size == 2);
    std::uint64_t realmem = 0;
    length = sizeof(realmem);
    Require(sysctl_nid_postfix(mib, static_cast<std::uint32_t>(size), &realmem, &length, nullptr, 0) == 0);
    std::uint64_t namedRealmem = 0;
    length = sizeof(namedRealmem);
    Require(sysctlbyname_nid_postfix("hw.realmem", &namedRealmem, &length, nullptr, 0) == 0 && realmem == namedRealmem);
    size = 4;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mib, &size) == 0 && size == 3);
    mib[3] = -1;
    char path[32] = {};
    length = sizeof(path);
    Require(sysctl_nid_postfix(mib, 4, path, &length, nullptr, 0) == 0 && std::strcmp(path, "/app0/eboot.bin") == 0);

    int truncated[3] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    size = 1;
    Require(sysctlnametomib_nid_postfix("hw.ncpu", truncated, &size) == -1 && *__error_nid_postfix() == 12);
    Require(size == 1 && truncated[0] == 6 && truncated[1] == 0x5a5a5a5a);
    size = 0;
    Require(sysctlnametomib_nid_postfix("hw.ncpu", truncated, &size) == -1 && *__error_nid_postfix() == 12);
    Require(size == 0 && truncated[0] == 6);

    int rngMib[3] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    size = 3;
    Require(sysctlnametomib_nid_postfix("kern.rng_pseudo", rngMib, &size) == 0 && size == 2);
    Require(rngMib[0] == 1 && rngMib[1] == 50 && rngMib[2] == 0x5a5a5a5a);
    size = 2;
    Require(sysctl_nid_postfix(rngMib, static_cast<std::uint32_t>(size), nullptr, &length, nullptr, 0) == 0 && length == 64);
    rngMib[0] = rngMib[1] = rngMib[2] = 0x5a5a5a5a;
    size = 1;
    Require(sysctlnametomib_nid_postfix("kern.rng_pseudo", rngMib, &size) == -1 && *__error_nid_postfix() == 12);
    Require(size == 1 && rngMib[0] == 1 && rngMib[1] == 0x5a5a5a5a);

    size = 4;
    Require(sysctlnametomib_nid_postfix("", mib, &size) == -1 && *__error_nid_postfix() == 2);
    Require(sysctlnametomib_nid_postfix("machdep.cacheline_size", mib, &size) == -1 && *__error_nid_postfix() == 2);
    char tooLong[1025];
    std::memset(tooLong, 'a', sizeof(tooLong) - 1);
    tooLong[sizeof(tooLong) - 1] = 0;
    int tooLongMib[4] = {0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::size_t tooLongMibSize = 4;
    Require(sysctlnametomib_nid_postfix(tooLong, tooLongMib, &tooLongMibSize) == -1 && *__error_nid_postfix() == 63);
    Require(tooLongMibSize == 4 && tooLongMib[0] == 0x5a5a5a5a && tooLongMib[1] == 0x5a5a5a5a && tooLongMib[2] == 0x5a5a5a5a && tooLongMib[3] == 0x5a5a5a5a);
    tooLong[1023] = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix(tooLong, tooLongMib, &tooLongMibSize) == -1 && *__error_nid_postfix() == 2 && tooLongMibSize == 4);
    Require(tooLongMib[0] == 0x5a5a5a5a && tooLongMib[1] == 0x5a5a5a5a && tooLongMib[2] == 0x5a5a5a5a && tooLongMib[3] == 0x5a5a5a5a);
    bool threw = false;
    try {
        sysctlnametomib_nid_postfix(nullptr, mib, &size);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        sysctlnametomib_nid_postfix("hw.ncpu", mib, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("kern.anyps5_no_such_name", mib, &size) == -1 && *__error_nid_postfix() == 2);
}
int main() {
    CheckProcessorCountSysctl();
    CheckPageSizeSysctl();
    CheckPhysicalMemorySysctl(5, "hw.physmem");
    CheckPhysicalMemorySysctl(12, "hw.realmem");
#if defined(__x86_64__) || defined(__i386__)
    CheckModelSysctl();
#endif
    CheckTscFrequencySysctl();
    CheckUnansweredSysctl();
    CheckUnknownSysctlName();
    CheckPathnameSysctl();
    CheckSystemStringSysctl();
    CheckSystemStringNames();
    CheckOsReleaseDateSysctl();
    CheckBootTimeSysctl();
    CheckLeafWithTrailingComponent();
    CheckMeasuredAbsentSysctlMibs();
    CheckSystemSwVersion();
    CheckVersionLimits();
    CheckPseudoRandomSysctl();
    CheckSdkVersionSysctl();
    CheckOvercommitSysctl();
    CheckArandomSysctl();
    CheckVmmapSysctl();
    CheckEnvSysctl();
    CheckPseudoRandomMibSysctl();
    CheckNameToMib();
    *__error_nid_postfix() = 13;
    Require(sysconf_nid_postfix(1) == 262144);
    Require(sysconf_nid_postfix(3) == 128);
    GuestRlimitTest nofileLimit{};
    Require(getrlimit_nid_postfix(8, &nofileLimit) == 0);
    Require(sysconf_nid_postfix(5) == nofileLimit.rlim_cur);
    Require(sysconf_nid_postfix(47) == 0x4000);
    Require(sysconf_nid_postfix(56) == 1024);
    Require(getpagesize_nid_postfix() == sysconf_nid_postfix(47));
    Require(sysconf_nid_postfix(57) > 0);
    Require(sysconf_nid_postfix(58) > 0);
    Require(sysconf_nid_postfix(121) > 0);
    Require(*__error_nid_postfix() == 13);
    Require(sysconf_nid_postfix(-1) == -1); // verifies full-width signed return
    Require(*__error_nid_postfix() == 22);
    Require(sysconf_nid_postfix(0x7fffffff) == -1);
    Require(environ_nid_postfix && !environ_nid_postfix[0]);
}
