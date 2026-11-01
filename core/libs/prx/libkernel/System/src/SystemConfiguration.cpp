#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

int Fault() {
    *__error_nid_postfix() = 14;
    return -1;
}

int CopyOut(const void* value, std::size_t size, void* oldValue, std::size_t* oldLength) {
    if (oldValue == nullptr) {
        if (oldLength != nullptr && !GuestCopyOut_nid_no_patch(oldLength, &size, sizeof(size))) return Fault();
        return 0;
    }
    std::size_t capacity = 0;
    if (oldLength != nullptr && !GuestCopyIn_nid_no_patch(&capacity, oldLength, sizeof(capacity))) return Fault();
    const std::size_t copied = std::min(capacity, size);
    if (!GuestCopyOut_nid_no_patch(oldValue, value, copied)) return Fault();
    if (oldLength != nullptr && !GuestCopyOut_nid_no_patch(oldLength, &copied, sizeof(copied))) return Fault();
    if (copied < size) {
        *__error_nid_postfix() = 12;
        return -1;
    }
    return 0;
}

struct SystemString {
    int mib[2];
    const char* name;
    const char* value;
};

constexpr SystemString SystemStrings[] = {
    {{1, 1}, "kern.ostype", "FreeBSD"},
    {{1, 2}, "kern.osrelease", "0.0-prototype"},
    {{1, 4}, "kern.version", "FreeBSD 11.4-RELEASE\n"},
    {{1, 10}, "kern.hostname", ""},
    {{6, 2}, "hw.model", "100-000001020                                  "},
    {{6, 1}, "hw.machine", "amd64"},
};

struct SystemInteger {
    int mib[2];
    const char* name;
    std::int32_t value;
};

constexpr SystemInteger SystemIntegers[] = {
    {{1, 3}, "kern.osrevision", 199506},
    {{1, 8}, "kern.argmax", 262144},
    {{1, 18}, "kern.ngroups", 1023},
    {{1, 24}, "kern.osreldate", 900000},
};

const SystemString* FindSystemString(const int* name) {
    for (const auto& entry : SystemStrings) {
        if (name[0] == entry.mib[0] && name[1] == entry.mib[1]) return &entry;
    }
    return nullptr;
}

const SystemInteger* FindSystemInteger(const int* name) {
    for (const auto& entry : SystemIntegers) {
        if (name[0] == entry.mib[0] && name[1] == entry.mib[1]) return &entry;
    }
    return nullptr;
}

bool IsLeafMib(const int* name) {
    return FindSystemInteger(name) != nullptr || (name[0] == 6 && (name[1] == 2 || name[1] == 3 || name[1] == 5 || name[1] == 7)) || (name[0] == 1 && (name[1] == 21 || name[1] == 37 || name[1] == 50)) || (name[0] == 2 && name[1] == 2);
}

KernelTimeval BootTime() {
    static const KernelTimeval bootTime = [] {
        KernelTimespec now{};
        KernelTimespec uptime{};
        if (clock_gettime_nid_postfix(0, &now) != 0 || clock_gettime_nid_postfix(5, &uptime) != 0) throw std::runtime_error("kern.boottime: reading CLOCK_REALTIME or CLOCK_UPTIME failed");
        const std::int64_t nanos = (now.tv_sec - uptime.tv_sec) * 1000000000LL + now.tv_nsec - uptime.tv_nsec;
        return KernelTimeval{nanos / 1000000000LL, nanos % 1000000000LL / 1000LL};
    }();
    return bootTime;
}

struct GuestLoadAverage {
    std::uint32_t ldavg[3];
    std::int64_t fscale;
};
static_assert(sizeof(GuestLoadAverage) == 24);

std::uint32_t ScaledLoad(double average, std::int64_t fscale) {
    const auto scaled = average * static_cast<double>(fscale) + 0.5;
    if (!(scaled > 0.0)) return 0;
    if (scaled >= static_cast<double>(std::numeric_limits<std::uint32_t>::max())) return std::numeric_limits<std::uint32_t>::max();
    return static_cast<std::uint32_t>(scaled);
}

GuestLoadAverage HostLoadAverage() {
    GuestLoadAverage load;
    std::memset(&load, 0, sizeof(load));
    load.fscale = 1 << 11;
#ifndef _WIN32
    double averages[3]{};
    if (getloadavg(averages, 3) == 3) {
        for (int index = 0; index < 3; ++index) load.ldavg[index] = ScaledLoad(averages[index], load.fscale);
    }
#endif
    return load;
}

constexpr std::size_t MaximumSysctlName = 1024;

int TopLevelNode(std::string_view name) {
    static constexpr std::string_view nodes[] = {"sysctl", "kern", "vm", "vfs", "net", "debug", "hw", "machdep", "user", "p1003_1b"};
    for (std::size_t i = 0; i < std::size(nodes); ++i)
        if (name == nodes[i]) return static_cast<int>(i);
    return -1;
}

std::string WithoutTrailingDot(const char* name) {
    std::string_view view(name);
    if (!view.empty() && view.back() == '.') view.remove_suffix(1);
    return std::string(view);
}

bool IsMeasuredAbsentMib(const int* name, std::uint32_t nameLength) {
    static constexpr struct {
        int mib[3];
        std::uint32_t length;
    } absent[] = {
        {{1, 2147482816, 2147482814}, 3},
        {{1, 2147483151}, 2},
        {{1, 2147483182, 2147483181}, 3},
        {{1, 2147482838, 2147482834}, 3},
    };
    for (const auto& entry : absent) {
        if (nameLength == entry.length && std::memcmp(name, entry.mib, entry.length * sizeof(int)) == 0) return true;
    }
    return false;
}
constexpr std::int32_t VmEntryDefaultType = 1;

struct GuestVmEntry {
    std::int32_t structSize;
    std::int32_t type;
    std::uint64_t start;
    std::uint64_t end;
    std::uint64_t offset;
    std::uint64_t vnodeFileId;
    std::uint32_t vnodeFsid;
    std::int32_t flags;
    std::int32_t resident;
    std::int32_t privateResident;
    std::int32_t protection;
    std::int32_t referenceCount;
    std::int32_t shadowCount;
    std::int32_t vnodeType;
    std::uint64_t vnodeSize;
    std::uint32_t vnodeDevice;
    std::uint16_t vnodeMode;
    std::uint16_t status;
    std::int32_t spare[12];
    char path[8];
};
static_assert(offsetof(GuestVmEntry, start) == 8 && offsetof(GuestVmEntry, protection) == 56 && offsetof(GuestVmEntry, path) == 136 && sizeof(GuestVmEntry) == 144);

struct GuestKinfoProc {
    std::int32_t structSize;
    std::int32_t layout;
    unsigned char reserved0[64];
    std::int32_t pid;
    std::int32_t parentPid;
    unsigned char reserved1[288];
    std::int64_t flag;
    unsigned char reserved2[71];
    char command[20];
    unsigned char reserved3[621];
};
static_assert(offsetof(GuestKinfoProc, pid) == 72 && offsetof(GuestKinfoProc, parentPid) == 76 && offsetof(GuestKinfoProc, flag) == 368 && offsetof(GuestKinfoProc, command) == 447 && sizeof(GuestKinfoProc) == 1088);

GuestKinfoProc GuestProcessRecord() {
    GuestKinfoProc process{};
    process.structSize = sizeof(process);
    process.pid = getpid_nid_postfix();
    process.parentPid = getppid_nid_postfix();
    std::memcpy(process.command, "eboot.bin", sizeof("eboot.bin"));
    return process;
}

std::vector<GuestVmEntry> GuestVmEntries() {
    std::vector<GuestVmEntry> entries;
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    for (std::uintptr_t cursor = 0; GuestArena::GuestArenaNextUsed_nid_no_patch(cursor, &start, &end); cursor = end) {
        GuestVmEntry entry{};
        entry.structSize = sizeof(entry);
        entry.type = VmEntryDefaultType;
        entry.start = start;
        entry.end = end;
        VirtualQueryInfo info{};
        if (sceKernelVirtualQuery(reinterpret_cast<const void*>(start), 0, &info, sizeof(info)) == 0) entry.protection = info.protection & 7;
        entries.push_back(entry);
    }
    return entries;
}

bool IsUnansweredMib(const int* name, std::uint32_t nameLength) {
    if (nameLength != 2) return false;
    return (name[0] == 1 && (name[1] == 6 || name[1] == 7 || name[1] == 12 || name[1] == 27 || name[1] == 32)) || (name[0] == 6 && (name[1] == 4 || name[1] == 11));
}

struct SysconfConstant {
    int name;
    std::int64_t value;
};

constexpr SysconfConstant kSysconfConstants[] = {
    {6, 1}, {7, -1}, {8, 200112}, {9, 99}, {10, 2048}, {11, 99}, {12, 1000}, {13, 0}, {14, 32},
    {15, 2048}, {16, 255}, {17, 199212}, {18, 200112}, {19, -1}, {20, 1}, {21, -1}, {22, 200112},
    {23, -1}, {24, -1}, {25, 200112}, {28, 200112}, {29, 200112}, {30, -1}, {31, 200112}, {32, 200112},
    {33, 200112}, {34, -1}, {35, 200112}, {36, 200112}, {37, 200112}, {38, 200112}, {39, 200112},
    {40, -1}, {41, 200112}, {42, 16}, {43, 1024}, {44, 0}, {45, 2147483647}, {46, -1}, {48, 62},
    {49, 30}, {50, 2147483647}, {51, 128}, {52, 32}, {59, -1}, {60, -1}, {61, -1}, {62, -1}, {63, -1},
    {64, -1}, {65, 200112}, {66, 200112}, {67, -1}, {68, 200112}, {74, 200112}, {75, 64}, {76, 200112},
    {77, 1}, {78, 1}, {79, 200112}, {80, 200112}, {81, -1}, {82, 200112}, {83, 200112}, {84, 200112},
    {85, 4}, {86, 256}, {87, 200112}, {88, 200112}, {89, 200112}, {90, -1}, {91, -1}, {93, 16384},
    {94, -1}, {95, 200112}, {96, 200112}, {97, -1}, {101, 255}, {102, -1}, {103, -1}, {104, -1},
    {105, 1}, {106, -1}, {107, 64}, {108, -1}, {109, -1}, {110, -1}, {111, -1}, {112, -1}, {113, -1},
    {114, -1}, {115, -1}, {118, 200112}, {119, 200112}, {120, 32}, {122, 8},
};

const SysconfConstant* FindSysconfConstant(int name) {
    const auto* end = std::end(kSysconfConstants);
    const auto* entry = std::lower_bound(std::begin(kSysconfConstants), end, name, [](const SysconfConstant& constant, int key) { return constant.name < key; });
    return entry != end && entry->name == name ? entry : nullptr;
}

bool IsUnansweredName(const char* name) {
    static constexpr const char* unanswered[] = {
        "hw.byteorder",
        "hw.cacheline_size",
        "hw.floatingpoint",
        "hw.machine_arch",
        "hw.usermem",
        "kern.bootfile",
        "kern.clockrate",
        "kern.cp_time",
        "kern.ident",
        "kern.maxfiles",
        "kern.maxfilesperproc",
        "kern.maxproc",
        "kern.ps_strings",
        "kern.sched.name",
        "kern.smp.maxcpus",
        "kern.stackprot",
        "kern.timecounter.hardware",
    };
    return std::any_of(std::begin(unanswered), std::end(unanswered), [name](const char* entry) { return std::strcmp(name, entry) == 0; });
}

}

extern "C" {
// Guest long is 64 bits, including when the Windows host long is 32 bits.
std::int64_t APS5_VABI sysconf_nid_postfix(int name) {
    const int saved = *__error_nid_postfix();
    std::int64_t result = -1;
    switch (name) {
        case 1: // _SC_ARG_MAX
            result = 262144;
            break;
        case 2: {
            GuestResourceLimit limit{};
            if (getrlimit_nid_postfix(7, &limit) == 0) {
                result = limit.rlim_cur;
            }
            break;
        }
        case 3: // _SC_CLK_TCK
            result = 128;
            break;
        case 4:
            result = 1023;
            break;
        case 5: { // _SC_OPEN_MAX
            GuestResourceLimit limit{};
            if (getrlimit_nid_postfix(8, &limit) == 0) {
                result = limit.rlim_cur;
            }
            break;
        }
        case 26: {
            GuestResourceLimit limit{};
            if (getrlimit_nid_postfix(8, &limit) == 0) {
                result = std::min<std::int64_t>(limit.rlim_cur, 32767);
            }
            break;
        }
        case 47: // _SC_PAGESIZE
            result = PS5_PAGE_SIZE;
            break;
        case 56: // _SC_IOV_MAX
            result = 1024;
            break;
        case 57: // _SC_NPROCESSORS_CONF
        case 58: // _SC_NPROCESSORS_ONLN
#ifdef _WIN32
            result = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
            result = ::sysconf(name == 57 ? _SC_NPROCESSORS_CONF : _SC_NPROCESSORS_ONLN);
#endif
            break;
        case 72:
            result = 255;
            break;
        case 73:
            result = 33;
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
        case 27:
            *__error_nid_postfix() = 2;
            return -1;
        default: {
            const SysconfConstant* constant = FindSysconfConstant(name);
            if (!constant) {
                *__error_nid_postfix() = 22;
                return -1;
            }
            *__error_nid_postfix() = saved;
            return constant->value;
        }
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
    int guestName[24];
    if (!GuestCopyIn_nid_no_patch(guestName, name, nameLength * sizeof(int))) return Fault();
    name = guestName;
    std::size_t capacity = 0;
    if (oldLength != nullptr && !GuestCopyIn_nid_no_patch(&capacity, oldLength, sizeof(capacity))) return Fault();
    if (IsMeasuredAbsentMib(name, nameLength)) {
        if (newValue != nullptr) throw std::runtime_error(std::string(__func__) + ": writing an unmeasured absent MIB");
        *__error_nid_postfix() = 2;
        return -1;
    }
    if (IsUnansweredMib(name, nameLength)) {
        if (newValue != nullptr) throw std::runtime_error(std::string(__func__) + ": writing MIB " + std::to_string(name[0]) + "." + std::to_string(name[1]));
        *__error_nid_postfix() = 2;
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
    if (nameLength == 2 && name[0] == 2 && name[1] == 2) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        const GuestLoadAverage load = HostLoadAverage();
        return CopyOut(&load, sizeof(load), oldValue, oldLength);
    }
    if (nameLength == 2) {
        if (const auto* entry = FindSystemString(name)) {
            if (newValue != nullptr) {
                *__error_nid_postfix() = 1;
                return -1;
            }
            return CopyOut(entry->value, std::strlen(entry->value) + 1, oldValue, oldLength);
        }
        if (const auto* entry = FindSystemInteger(name)) {
            if (newValue != nullptr) {
                *__error_nid_postfix() = 1;
                return -1;
            }
            return CopyOut(&entry->value, sizeof(entry->value), oldValue, oldLength);
        }
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
    if (nameLength >= 3 && name[0] == 1 && name[1] == 14 && name[2] == 8) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (nameLength > 4) {
            *__error_nid_postfix() = 22;
            return -1;
        }
        const GuestKinfoProc process = GuestProcessRecord();
        return CopyOut(&process, sizeof(process), oldValue, oldLength);
    }
    if (nameLength >= 3 && name[0] == 1 && name[1] == 14 && name[2] == 1) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (nameLength != 4) {
            *__error_nid_postfix() = 22;
            return -1;
        }
        if (name[3] != getpid_nid_postfix()) {
            *__error_nid_postfix() = 3;
            return -1;
        }
        const GuestKinfoProc process = GuestProcessRecord();
        return CopyOut(&process, sizeof(process), oldValue, oldLength);
    }
    if (nameLength >= 3 && name[0] == 1 && name[1] == 14 && name[2] == 35) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (nameLength != 4) {
            *__error_nid_postfix() = 22;
            return -1;
        }
        if (name[3] != getpid_nid_postfix()) {
            *__error_nid_postfix() = 3;
            return -1;
        }
        return CopyOut(nullptr, 0, oldValue, oldLength);
    }
    if (nameLength >= 3 && name[0] == 1 && name[1] == 14 && name[2] == 32) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (nameLength != 4) {
            *__error_nid_postfix() = 22;
            return -1;
        }
        if (name[3] != getpid_nid_postfix()) {
            *__error_nid_postfix() = 3;
            return -1;
        }
        const auto entries = GuestVmEntries();
        return CopyOut(entries.data(), entries.size() * sizeof(GuestVmEntry), oldValue, oldLength);
    }
    if (nameLength == 2 && name[0] == 1 && name[1] == 21) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        const KernelTimeval bootTime = BootTime();
        return CopyOut(&bootTime, sizeof(bootTime), oldValue, oldLength);
    }
    if (nameLength == 2 && name[0] == 1 && name[1] == 37) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        if (oldLength == nullptr) return 0;
        const std::size_t count = std::min(capacity, static_cast<std::size_t>(256));
        if (oldValue != nullptr) {
            static std::random_device device;
            std::array<std::uint8_t, 256> bytes{};
            for (std::size_t i = 0; i < count; ++i) bytes[i] = static_cast<std::uint8_t>(device());
            if (!GuestCopyOut_nid_no_patch(oldValue, bytes.data(), count)) return Fault();
        }
        if (!GuestCopyOut_nid_no_patch(oldLength, &count, sizeof(count))) return Fault();
        return 0;
    }
    if (nameLength == 2 && name[0] == 1 && name[1] == 50) {
        if (newValue != nullptr) throw std::runtime_error(std::string(__func__) + ": writing kern.rng_pseudo is unverified");
        std::array<std::uint8_t, 64> bytes{};
        if (oldValue != nullptr && oldLength != nullptr) {
            static std::random_device device;
            for (auto& byte : bytes) byte = static_cast<std::uint8_t>(device());
        }
        return CopyOut(bytes.data(), bytes.size(), oldValue, oldLength);
    }
    if (nameLength > 2 && (IsLeafMib(name) || FindSystemString(name) != nullptr)) {
        *__error_nid_postfix() = 20;
        return -1;
    }
    if (name[0] < 0 || (name[0] >= 10 && name[0] < 256)) {
        if (newValue != nullptr) throw std::runtime_error(std::string(__func__) + ": writing MIB under absent top-level node " + std::to_string(name[0]));
        *__error_nid_postfix() = 2;
        return -1;
    }
    std::string mib;
    for (std::uint32_t i = 0; i < nameLength; ++i) mib += (i == 0 ? "" : ".") + std::to_string(name[i]);
    throw std::runtime_error(std::string(__func__) + ": unsupported MIB " + mib);
}

int APS5_VABI sysctlbyname_nid_postfix(const char* name, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    std::string guestName;
    if (const int error = CopyGuestPath_nid_no_patch(name, guestName)) {
        *__error_nid_postfix() = error;
        return -1;
    }
    name = guestName.c_str();
    const std::string trimmed = WithoutTrailingDot(name);
    name = trimmed.c_str();
    if (TopLevelNode(trimmed) >= 0) {
        *__error_nid_postfix() = 22;
        return -1;
    }
    if (std::strcmp(name, "machdep.cacheline_size") == 0) {
        *__error_nid_postfix() = 2;
        return -1;
    }
    for (const auto& entry : SystemStrings) {
        if (std::strcmp(name, entry.name) == 0) return sysctl_nid_postfix(entry.mib, 2, oldValue, oldLength, newValue, newLength);
    }
    for (const auto& entry : SystemIntegers) {
        if (std::strcmp(name, entry.name) == 0) return sysctl_nid_postfix(entry.mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (IsUnansweredName(name)) {
        if (newValue != nullptr) throw std::runtime_error(std::string(__func__) + ": writing " + name);
        *__error_nid_postfix() = 2;
        return -1;
    }
    if (std::strcmp(name, "kern.proc.pathname") == 0) {
        static constexpr int mib[] = {1, 14, 12};
        return sysctl_nid_postfix(mib, 3, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "kern.boottime") == 0) {
        static constexpr int mib[] = {1, 21};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "vm.loadavg") == 0) {
        static constexpr int mib[] = {2, 2};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
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
        static std::random_device device;
        std::uint8_t block[64]{};
        if (oldValue != nullptr && oldLength != nullptr)
            for (auto& byte : block) byte = static_cast<std::uint8_t>(device());
        return CopyOut(block, sizeof(block), oldValue, oldLength);
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
    if (std::strcmp(name, "machdep.upd_version") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static constexpr std::uint32_t updVersion = 0;
        return CopyOut(&updVersion, sizeof(updVersion), oldValue, oldLength);
    }
    if (std::strcmp(name, "machdep.curr_manumode") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static constexpr int manumode = 0;
        return CopyOut(&manumode, sizeof(manumode), oldValue, oldLength);
    }
    if (std::strcmp(name, "machdep.rcmgr_intdev") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static constexpr int intdev = 0;
        return CopyOut(&intdev, sizeof(intdev), oldValue, oldLength);
    }
    if (std::strcmp(name, "machdep.rcmgr_intdev_for_internal_libc") == 0) {
        if (newValue != nullptr) {
            *__error_nid_postfix() = 1;
            return -1;
        }
        static constexpr int intdev = 0;
        return CopyOut(&intdev, sizeof(intdev), oldValue, oldLength);
    }
    if (std::strcmp(name, "hw.pagesize") == 0) {
        static constexpr int mib[] = {6, 7};
        return sysctl_nid_postfix(mib, 2, oldValue, oldLength, newValue, newLength);
    }
    if (std::strcmp(name, "hw.ncpu") == 0) {
        static constexpr int mib[] = {6, 3};
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
    *__error_nid_postfix() = 2;
    return -1;
}

int APS5_VABI sceKernelGetSystemSwVersion_nid_postfix(SceKernelSystemSwVersion* version) {
    if (version == nullptr) {
        return 0;
    }
    char versionString[sizeof(version->versionString)] = {};
    std::memcpy(versionString, "01.000.000", sizeof("01.000.000"));
    constexpr std::uint32_t packedVersion = 0x01000000;
    if (!GuestCopyOut_nid_no_patch(version->versionString, versionString, sizeof(versionString)) ||
        !GuestCopyOut_nid_no_patch(&version->version, &packedVersion, sizeof(packedVersion))) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

int APS5_VABI sceKernelGetSystemExVersion(char* first, char* second) {
    if (first == nullptr || second == nullptr) {
        return 22;
    }
    constexpr char zero = '\0';
    if (!GuestCopyOut_nid_no_patch(first, &zero, sizeof(zero)) ||
        !GuestCopyOut_nid_no_patch(second, &zero, sizeof(zero))) {
        return 14;
    }
    return 0;
}

int APS5_VABI sysKernelGetLowerLimitSysexVersion_nid_postfix(char* version) {
    if (version == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    constexpr char zero = '\0';
    if (!GuestCopyOut_nid_no_patch(version, &zero, sizeof(zero))) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

int APS5_VABI sysKernelGetLowerLimitUpdVersion_nid_postfix(std::uint32_t* version) {
    if (version == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    constexpr std::uint32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(version, &zero, sizeof(zero))) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

int APS5_VABI sysKernelGetUpdVersion_nid_postfix(std::uint32_t* version) {
    if (version == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    std::size_t length = sizeof(*version);
    return sysctlbyname_nid_postfix("machdep.upd_version", version, &length, nullptr, 0);
}

int APS5_VABI sysKernelGetManufacturingMode_nid_postfix(void) {
    int mode = 0;
    std::size_t length = sizeof(mode);
    if (sysctlbyname_nid_postfix("machdep.curr_manumode", &mode, &length, nullptr, 0) != 0) {
        return -1;
    }
    return mode;
}

int APS5_VABI sysKernelGetIntdevModeForInternalLibc_nid_postfix(void) {
    int mode = 0;
    std::size_t length = sizeof(mode);
    const int result = sysctlbyname_nid_postfix("machdep.rcmgr_intdev_for_internal_libc", &mode, &length, nullptr, 0);
    if (result != 0) {
        return result;
    }
    return mode;
}

int APS5_VABI sysKernelGetIntdevModeForRcmgr_nid_postfix(void) {
    int mode = 0;
    std::size_t length = sizeof(mode);
    const int result = sysctlbyname_nid_postfix("machdep.rcmgr_intdev", &mode, &length, nullptr, 0);
    if (result != 0) {
        return result;
    }
    return mode;
}

int APS5_VABI sysctlnametomib_nid_postfix(const char* name, int* mib, std::size_t* size) {
    struct KnownName {
        const char* name;
        int mib[3];
        std::size_t length;
    };
    static constexpr KnownName known[] = {
        {"hw.byteorder", {6, 4}, 2},
        {"hw.machine_arch", {6, 11}, 2},
        {"hw.model", {6, 2}, 2},
        {"hw.ncpu", {6, 3}, 2},
        {"hw.pagesize", {6, 7}, 2},
        {"hw.physmem", {6, 5}, 2},
        {"hw.realmem", {6, 12}, 2},
        {"kern.arandom", {1, 37}, 2},
        {"kern.boottime", {1, 21}, 2},
        {"kern.clockrate", {1, 12}, 2},
        {"kern.maxfiles", {1, 7}, 2},
        {"kern.maxfilesperproc", {1, 27}, 2},
        {"kern.maxproc", {1, 6}, 2},
        {"kern.proc.pathname", {1, 14, 12}, 3},
        {"kern.ps_strings", {1, 32}, 2},
        {"kern.rng_pseudo", {1, 50}, 2},
        {"vm.loadavg", {2, 2}, 2},
    };
    if (name == nullptr || size == nullptr) throw std::invalid_argument(std::string(__func__) + ": null name or size");
    std::string guestName;
    if (const int error = CopyGuestPath_nid_no_patch(name, guestName)) {
        *__error_nid_postfix() = error;
        return -1;
    }
    name = guestName.c_str();
    const std::string trimmed = WithoutTrailingDot(name);
    name = trimmed.c_str();
    if (trimmed.empty() || std::strcmp(name, "hw.cacheline_size") == 0 || std::strcmp(name, "hw.usermem") == 0 || std::strcmp(name, "machdep.cacheline_size") == 0) {
        *__error_nid_postfix() = 2;
        return -1;
    }
    std::size_t count = 0;
    if (!GuestCopyIn_nid_no_patch(&count, size, sizeof(count))) return Fault();
    const auto copyMib = [&](const int* found, std::size_t length) {
        std::size_t bytes = count * sizeof(int);
        const int result = CopyOut(found, length * sizeof(int), mib, &bytes);
        if (result != 0 && *__error_nid_postfix() != 12) return result;
        count = bytes / sizeof(int);
        if (!GuestCopyOut_nid_no_patch(size, &count, sizeof(count))) return Fault();
        return result;
    };
    if (const int node = TopLevelNode(trimmed); node >= 0) return copyMib(&node, 1);
    for (const auto& entry : known) {
        if (std::strcmp(name, entry.name) == 0) return copyMib(entry.mib, entry.length);
    }
    for (const auto& entry : SystemStrings) {
        if (std::strcmp(name, entry.name) == 0) return copyMib(entry.mib, 2);
    }
    for (const auto& entry : SystemIntegers) {
        if (std::strcmp(name, entry.name) == 0) return copyMib(entry.mib, 2);
    }
    *__error_nid_postfix() = 2;
    return -1;
}
}
