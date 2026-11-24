#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEnoent = 2;
constexpr int PosixEnomem = 12;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEnametoolong = 63;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

void SetError(int error) {
    *__error_nid_postfix() = error;
}

int NameToOid(std::string_view name, int* oid, std::size_t* length) {
    const int mib[] = {0, 3};
    return sysctl_nid_postfix(mib, 2, oid, length, name.data(), name.size());
}

void RequireSameOid(const char* name, const int* mib, std::size_t size) {
    int oid[4] = {-1, -1, -1, -1};
    std::size_t length = sizeof(oid);
    Require(NameToOid(name, oid, &length) == 0);
    Require(length == size * sizeof(int));
    Require(std::memcmp(oid, mib, sizeof(oid)) == 0);
}

void TestSysctlByNameNullAndFaultBounds() {
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page, 3, 0) == 0 && memory != nullptr);
    auto* readOnlyLength = static_cast<std::size_t*>(memory);
    *readOnlyLength = sizeof(int);
    Require(sceKernelMprotect(memory, page, 1) == 0);

    void* pages = nullptr;
    Require(sceKernelMapFlexibleMemory(&pages, page * 2, 3, 0) == 0 && pages != nullptr);
    auto* guard = static_cast<char*>(pages) + page;
    Require(sceKernelMprotect(guard, page, 0) == 0);
    char* crossing = guard - 4;
    std::memcpy(crossing, "kern", 4);

    int val = 0;
    std::size_t len = sizeof(val);

    SetError(0);
    Require(sysctlbyname_nid_postfix(nullptr, &val, &len, nullptr, 0) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlbyname_nid_postfix(reinterpret_cast<const char*>(0x10), &val, &len, nullptr, 0) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlbyname_nid_postfix(crossing, &val, &len, nullptr, 0) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlbyname_nid_postfix("hw.ncpu", reinterpret_cast<void*>(0x10), &len, nullptr, 0) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &val, reinterpret_cast<std::size_t*>(0x10), nullptr, 0) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &val, readOnlyLength, nullptr, 0) == -1);
    Require(Error() == PosixEfault);
}

void TestSysctlByNameLengthBounds() {
    int val = 0;
    std::size_t len = sizeof(val);

    std::string tooLong(1024, 'x');
    SetError(0);
    Require(sysctlbyname_nid_postfix(tooLong.c_str(), &val, &len, nullptr, 0) == -1);
    Require(Error() == PosixEnametoolong);

    std::string wayTooLong(2048, 'y');
    SetError(0);
    Require(sysctlbyname_nid_postfix(wayTooLong.c_str(), &val, &len, nullptr, 0) == -1);
    Require(Error() == PosixEnametoolong);

    std::string maxValidUnknown(1023, 'z');
    SetError(0);
    Require(sysctlbyname_nid_postfix(maxValidUnknown.c_str(), &val, &len, nullptr, 0) == -1);
    Require(Error() == PosixEnoent);
}

void TestSysctlByNameTopLevelAndMalformedBounds() {
    int val = 0;
    std::size_t len = sizeof(val);

    const char* topNodes[] = {
        "sysctl", "kern", "vm", "vfs", "net", "debug", "hw", "machdep", "user", "p1003_1b",
        "sysctl.", "kern.", "vm.", "vfs.", "net.", "debug.", "hw.", "machdep.", "user.", "p1003_1b."
    };
    for (const auto* node : topNodes) {
        SetError(0);
        val = 0x5a5a;
        len = sizeof(val);
        Require(sysctlbyname_nid_postfix(node, &val, &len, nullptr, 0) == -1);
        Require(Error() == PosixEinval);
        Require(val == 0x5a5a);
    }

    const char* malformedNames[] = {
        "", ".", "..", "...", "kern..", "..kern", ".hw", "hw..pagesize", "kern...version", " kern", "kern "
    };
    for (const auto* badName : malformedNames) {
        SetError(0);
        val = 0;
        len = sizeof(val);
        Require(sysctlbyname_nid_postfix(badName, &val, &len, nullptr, 0) == -1);
        Require(Error() == PosixEnoent);
    }

    const char* absentNames[] = {
        "machdep.cacheline_size",
        "hw.cacheline_size",
        "hw.usermem",
        "kern.sched.name",
        "kern.smp.maxcpus",
        "kern.stackprot",
        "kern.timecounter.hardware",
        "kern.nonexistent_node",
        "hw.not_a_metric",
        "user.unknown_field"
    };
    for (const auto* absent : absentNames) {
        SetError(0);
        val = 0;
        len = sizeof(val);
        Require(sysctlbyname_nid_postfix(absent, &val, &len, nullptr, 0) == -1);
        Require(Error() == PosixEnoent);
    }
}

void TestSysctlByNameValidNodesAndDotEquivalence() {
    int ncpu = 0;
    std::size_t len = sizeof(ncpu);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &ncpu, &len, nullptr, 0) == 0);
    Require(ncpu > 0);
    Require(len == sizeof(int));

    int ncpuDot = 0;
    len = sizeof(ncpuDot);
    Require(sysctlbyname_nid_postfix("hw.ncpu.", &ncpuDot, &len, nullptr, 0) == 0);
    Require(ncpuDot == ncpu);
    Require(len == sizeof(int));

    int pageSize = 0;
    len = sizeof(pageSize);
    Require(sysctlbyname_nid_postfix("hw.pagesize", &pageSize, &len, nullptr, 0) == 0);
    Require(pageSize == 16384);
    Require(len == sizeof(int));

    int pageSizeDot = 0;
    len = sizeof(pageSizeDot);
    Require(sysctlbyname_nid_postfix("hw.pagesize.", &pageSizeDot, &len, nullptr, 0) == 0);
    Require(pageSizeDot == 16384);

    char ostype[32] = {};
    len = sizeof(ostype);
    Require(sysctlbyname_nid_postfix("kern.ostype", ostype, &len, nullptr, 0) == 0);
    Require(std::strcmp(ostype, "FreeBSD") == 0);
    Require(len == std::strlen("FreeBSD") + 1);

    char ostypeDot[32] = {};
    len = sizeof(ostypeDot);
    Require(sysctlbyname_nid_postfix("kern.ostype.", ostypeDot, &len, nullptr, 0) == 0);
    Require(std::strcmp(ostypeDot, "FreeBSD") == 0);

    char osrelease[32] = {};
    len = sizeof(osrelease);
    Require(sysctlbyname_nid_postfix("kern.osrelease", osrelease, &len, nullptr, 0) == 0);
    Require(std::strcmp(osrelease, "0.0-prototype") == 0);

    std::int32_t osrev = 0;
    len = sizeof(osrev);
    Require(sysctlbyname_nid_postfix("kern.osrevision", &osrev, &len, nullptr, 0) == 0);
    Require(osrev == 199506);

    std::int32_t argmax = 0;
    len = sizeof(argmax);
    Require(sysctlbyname_nid_postfix("kern.argmax", &argmax, &len, nullptr, 0) == 0);
    Require(argmax == 262144);

    std::int32_t ngroups = 0;
    len = sizeof(ngroups);
    Require(sysctlbyname_nid_postfix("kern.ngroups", &ngroups, &len, nullptr, 0) == 0);
    Require(ngroups == 1023);

    std::int32_t osreldate = 0;
    len = sizeof(osreldate);
    Require(sysctlbyname_nid_postfix("kern.osreldate", &osreldate, &len, nullptr, 0) == 0);
    Require(osreldate == 900000);

    char machine[32] = {};
    len = sizeof(machine);
    Require(sysctlbyname_nid_postfix("hw.machine", machine, &len, nullptr, 0) == 0);
    Require(std::strcmp(machine, "amd64") == 0);

    char model[64] = {};
    len = sizeof(model);
    Require(sysctlbyname_nid_postfix("hw.model", model, &len, nullptr, 0) == 0);
    Require(len > 1);

    std::uint8_t randomBytes[16] = {};
    len = sizeof(randomBytes);
    Require(sysctlbyname_nid_postfix("kern.arandom", randomBytes, &len, nullptr, 0) == 0);
    Require(len == sizeof(randomBytes));

    std::uint8_t rngBytes[64] = {};
    len = sizeof(rngBytes);
    Require(sysctlbyname_nid_postfix("kern.rng_pseudo", rngBytes, &len, nullptr, 0) == 0);
    Require(len == sizeof(rngBytes));
}

void TestSysctlByNameOutputBufferBounds() {
    std::size_t queryLen = 0;
    Require(sysctlbyname_nid_postfix("kern.ostype", nullptr, &queryLen, nullptr, 0) == 0);
    Require(queryLen == std::strlen("FreeBSD") + 1);

    char buf[16] = {};
    SetError(0);
    Require(sysctlbyname_nid_postfix("kern.ostype", buf, nullptr, nullptr, 0) == -1);
    Require(Error() == PosixEnomem);

    char truncated[4] = {};
    std::size_t truncLen = 3;
    SetError(0);
    Require(sysctlbyname_nid_postfix("kern.ostype", truncated, &truncLen, nullptr, 0) == -1);
    Require(Error() == PosixEnomem);
    Require(truncLen == 3);
    Require(std::memcmp(truncated, "Fre", 3) == 0);

    truncLen = 0;
    SetError(0);
    Require(sysctlbyname_nid_postfix("kern.ostype", truncated, &truncLen, nullptr, 0) == -1);
    Require(Error() == PosixEnomem);
    Require(truncLen == 0);
}

void TestSysctlByNameReadOnlyWriteBounds() {
    int dummy = 123;
    const char* readOnlyNames[] = {
        "hw.ncpu", "hw.pagesize", "kern.ostype", "kern.osrelease", "kern.version", "kern.rng_pseudo", "hw.machine"
    };
    for (const auto* name : readOnlyNames) {
        SetError(0);
        Require(sysctlbyname_nid_postfix(name, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
        Require(Error() == PosixEperm);
    }

    bool caughtUnanswered = false;
    try {
        sysctlbyname_nid_postfix("kern.sched.name", nullptr, nullptr, &dummy, sizeof(dummy));
    } catch (const std::runtime_error&) {
        caughtUnanswered = true;
    }
    Require(caughtUnanswered);
}

void TestSysctlNameToMibNullAndFaultBounds() {
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page, 3, 0) == 0 && memory != nullptr);
    auto* readOnlySize = static_cast<std::size_t*>(memory);
    *readOnlySize = 2;
    Require(sceKernelMprotect(memory, page, 1) == 0);

    void* pages = nullptr;
    Require(sceKernelMapFlexibleMemory(&pages, page * 2, 3, 0) == 0 && pages != nullptr);
    auto* guard = static_cast<char*>(pages) + page;
    Require(sceKernelMprotect(guard, page, 0) == 0);
    char* crossing = guard - 4;
    std::memcpy(crossing, "kern", 4);

    int mib[4] = {};
    std::size_t size = 4;

    bool caughtNullName = false;
    try {
        sysctlnametomib_nid_postfix(nullptr, mib, &size);
    } catch (const std::invalid_argument&) {
        caughtNullName = true;
    }
    Require(caughtNullName);

    bool caughtNullSize = false;
    try {
        sysctlnametomib_nid_postfix("hw.ncpu", mib, nullptr);
    } catch (const std::invalid_argument&) {
        caughtNullSize = true;
    }
    Require(caughtNullSize);

    SetError(0);
    Require(sysctlnametomib_nid_postfix(reinterpret_cast<const char*>(0x10), mib, &size) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlnametomib_nid_postfix(crossing, mib, &size) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlnametomib_nid_postfix("hw.ncpu", reinterpret_cast<int*>(0x10), &size) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlnametomib_nid_postfix("hw.ncpu", mib, reinterpret_cast<std::size_t*>(0x10)) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(sysctlnametomib_nid_postfix("hw.ncpu", mib, readOnlySize) == -1);
    Require(Error() == PosixEfault);
}

void TestSysctlNameToMibLengthAndMalformedBounds() {
    int mib[4] = {};
    std::size_t size = 4;

    std::string tooLong(1024, 'a');
    SetError(0);
    size = 4;
    Require(sysctlnametomib_nid_postfix(tooLong.c_str(), mib, &size) == -1);
    Require(Error() == PosixEnametoolong);

    std::string maxUnknown(1023, 'b');
    SetError(0);
    size = 4;
    Require(sysctlnametomib_nid_postfix(maxUnknown.c_str(), mib, &size) == -1);
    Require(Error() == PosixEnoent);

    const char* malformed[] = {
        "", ".", "..", "kern..", "hw..ncpu", "hw.cacheline_size", "hw.usermem", "machdep.cacheline_size", "unknown.node"
    };
    for (const auto* name : malformed) {
        SetError(0);
        size = 4;
        Require(sysctlnametomib_nid_postfix(name, mib, &size) == -1);
        Require(Error() == PosixEnoent);

        std::size_t length = sizeof(mib);
        SetError(0);
        Require(NameToOid(name, mib, &length) == -1);
        Require(Error() == PosixEnoent);
    }
}

void TestSysctlNameToMibTopLevelAndKnownNodes() {
    struct TopNodeExpected {
        const char* name;
        int expectedNode;
    };
    const TopNodeExpected topNodes[] = {
        {"sysctl", 0}, {"kern", 1}, {"vm", 2}, {"vfs", 3}, {"net", 4},
        {"debug", 5}, {"hw", 6}, {"machdep", 7}, {"user", 8}, {"p1003_1b", 9},
        {"sysctl.", 0}, {"kern.", 1}, {"vm.", 2}, {"vfs.", 3}, {"net.", 4},
        {"debug.", 5}, {"hw.", 6}, {"machdep.", 7}, {"user.", 8}, {"p1003_1b.", 9}
    };
    for (const auto& item : topNodes) {
        int mib[4] = {-1, -1, -1, -1};
        std::size_t size = 4;
        Require(sysctlnametomib_nid_postfix(item.name, mib, &size) == 0);
        Require(size == 1);
        Require(mib[0] == item.expectedNode);
        Require(mib[1] == -1);
        RequireSameOid(item.name, mib, size);
    }

    struct KnownExpected {
        const char* name;
        int mib[3];
        std::size_t length;
    };
    const KnownExpected knownNodes[] = {
        {"hw.byteorder", {6, 4, 0}, 2},
        {"hw.machine_arch", {6, 11, 0}, 2},
        {"hw.model", {6, 2, 0}, 2},
        {"hw.ncpu", {6, 3, 0}, 2},
        {"hw.ncpu.", {6, 3, 0}, 2},
        {"hw.pagesize", {6, 7, 0}, 2},
        {"hw.pagesize.", {6, 7, 0}, 2},
        {"hw.physmem", {6, 5, 0}, 2},
        {"hw.realmem", {6, 12, 0}, 2},
        {"kern.arandom", {1, 37, 0}, 2},
        {"kern.boottime", {1, 21, 0}, 2},
        {"kern.clockrate", {1, 12, 0}, 2},
        {"kern.maxfiles", {1, 7, 0}, 2},
        {"kern.maxfilesperproc", {1, 27, 0}, 2},
        {"kern.maxproc", {1, 6, 0}, 2},
        {"kern.proc.pathname", {1, 14, 12}, 3},
        {"kern.proc.proc", {1, 14, 8}, 3},
        {"kern.ps_strings", {1, 32, 0}, 2},
        {"kern.rng_pseudo", {1, 50, 0}, 2},
        {"vm.loadavg", {2, 2, 0}, 2},
        {"kern.ostype", {1, 1, 0}, 2},
        {"kern.osrelease", {1, 2, 0}, 2},
        {"kern.version", {1, 4, 0}, 2},
        {"kern.hostname", {1, 10, 0}, 2},
        {"hw.machine", {6, 1, 0}, 2},
        {"kern.osrevision", {1, 3, 0}, 2},
        {"kern.argmax", {1, 8, 0}, 2},
        {"kern.ngroups", {1, 18, 0}, 2},
        {"kern.osreldate", {1, 24, 0}, 2},
        {"kern.sched.name", {1, 2147483182, 2147483181}, 3},
        {"kern.smp.maxcpus", {1, 2147482816, 2147482814}, 3},
        {"kern.stackprot", {1, 2147483151, 0}, 2},
        {"kern.timecounter.hardware", {1, 2147482838, 2147482834}, 3}
    };
    for (const auto& item : knownNodes) {
        int mib[4] = {-1, -1, -1, -1};
        std::size_t size = 4;
        Require(sysctlnametomib_nid_postfix(item.name, mib, &size) == 0);
        Require(size == item.length);
        for (std::size_t i = 0; i < item.length; ++i) {
            Require(mib[i] == item.mib[i]);
        }
        Require(mib[item.length] == -1);
        RequireSameOid(item.name, mib, size);
    }
}

void TestSysctlNameToOid() {
    int oid[4] = {-1, -1, -1, -1};
    std::size_t length = sizeof(oid);
    Require(NameToOid("kern.ostype", oid, &length) == 0);
    Require(length == 2 * sizeof(int));
    Require(oid[0] == 1 && oid[1] == 1 && oid[2] == -1);

    const char embedded[] = "kern.ostype\0hw.ncpu";
    std::fill(std::begin(oid), std::end(oid), -1);
    length = sizeof(oid);
    Require(NameToOid(std::string_view(embedded, sizeof(embedded) - 1), oid, &length) == 0);
    Require(length == 2 * sizeof(int));
    Require(oid[0] == 1 && oid[1] == 1 && oid[2] == -1);

    length = 0;
    Require(NameToOid("kern.proc.pathname", nullptr, &length) == 0);
    Require(length == 3 * sizeof(int));

    Require(NameToOid("kern.proc.pathname", nullptr, nullptr) == 0);

    std::fill(std::begin(oid), std::end(oid), 0x5a);
    length = 2 * sizeof(int);
    SetError(0);
    Require(NameToOid("kern.proc.pathname", oid, &length) == -1);
    Require(Error() == PosixEnomem);
    Require(length == 2 * sizeof(int));
    Require(oid[0] == 1 && oid[1] == 14 && oid[2] == 0x5a);

    length = 0;
    SetError(0);
    Require(NameToOid("hw.ncpu", oid, &length) == -1);
    Require(Error() == PosixEnomem);
    Require(length == 0);

    SetError(0);
    Require(NameToOid("hw.ncpu", oid, nullptr) == -1);
    Require(Error() == PosixEnomem);

    const int mib[] = {0, 3};
    length = sizeof(oid);
    SetError(0);
    Require(sysctl_nid_postfix(mib, 2, oid, &length, nullptr, 11) == -1);
    Require(Error() == PosixEnoent);

    SetError(0);
    Require(sysctl_nid_postfix(mib, 2, oid, &length, "kern.ostype", 0) == -1);
    Require(Error() == PosixEnoent);

    std::fill(std::begin(oid), std::end(oid), -1);
    length = sizeof(oid);
    Require(NameToOid("kern.ostype.", oid, &length) == 0);
    Require(length == 2 * sizeof(int));
    Require(oid[0] == 1 && oid[1] == 1 && oid[2] == -1);

    const char* unknown[] = {"kern.anyps5.x", "kern.ostype..", "kern.ostype.x", "hw.cacheline_size"};
    for (const auto* name : unknown) {
        length = sizeof(oid);
        SetError(0);
        Require(NameToOid(name, oid, &length) == -1);
        Require(Error() == PosixEnoent);
    }

    const std::string tooLong(1024, 'x');
    length = sizeof(oid);
    SetError(0);
    Require(NameToOid(tooLong, oid, &length) == -1);
    Require(Error() == PosixEnametoolong);

    const std::string longest(1023, 'x');
    length = sizeof(oid);
    SetError(0);
    Require(NameToOid(longest, oid, &length) == -1);
    Require(Error() == PosixEnoent);

    length = sizeof(oid);
    SetError(0);
    Require(sysctl_nid_postfix(mib, 2, oid, &length, reinterpret_cast<const void*>(0x10), 11) == -1);
    Require(Error() == PosixEfault);
}

void TestSysctlNameToMibCapacityBounds() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t size = 0;

    SetError(0);
    Require(sysctlnametomib_nid_postfix("hw.ncpu", mib, &size) == -1);
    Require(Error() == PosixEnomem);
    Require(size == 0);

    size = 1;
    SetError(0);
    Require(sysctlnametomib_nid_postfix("hw.ncpu", mib, &size) == -1);
    Require(Error() == PosixEnomem);
    Require(size == 1);
    Require(mib[0] == 6);
    Require(mib[1] == 0x5a);

    size = 2;
    SetError(0);
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mib, &size) == -1);
    Require(Error() == PosixEnomem);
    Require(size == 2);
    Require(mib[0] == 1);
    Require(mib[1] == 14);
    Require(mib[2] == 0x5a);

    size = 4;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mib, &size) == 0);
    Require(size == 3);
    Require(mib[0] == 1);
    Require(mib[1] == 14);
    Require(mib[2] == 12);
    Require(mib[3] == 0x5a);
}

void TestSysctlNameAndMibRoundtrip() {
    const char* names[] = {
        "hw.ncpu", "hw.pagesize", "kern.ostype", "kern.osrelease", "kern.osrevision", "kern.argmax", "kern.ngroups", "kern.osreldate"
    };
    for (const auto* name : names) {
        int mib[4] = {};
        std::size_t mibSize = 4;
        Require(sysctlnametomib_nid_postfix(name, mib, &mibSize) == 0);
        Require(mibSize >= 2);

        char valByName[64] = {};
        std::size_t lenByName = sizeof(valByName);
        Require(sysctlbyname_nid_postfix(name, valByName, &lenByName, nullptr, 0) == 0);

        char valByMib[64] = {};
        std::size_t lenByMib = sizeof(valByMib);
        Require(sysctl_nid_postfix(mib, static_cast<std::uint32_t>(mibSize), valByMib, &lenByMib, nullptr, 0) == 0);

        Require(lenByName == lenByMib);
        Require(std::memcmp(valByName, valByMib, lenByName) == 0);
    }
}

} // namespace

int main() {
    TestSysctlByNameNullAndFaultBounds();
    TestSysctlByNameLengthBounds();
    TestSysctlByNameTopLevelAndMalformedBounds();
    TestSysctlByNameValidNodesAndDotEquivalence();
    TestSysctlByNameOutputBufferBounds();
    TestSysctlByNameReadOnlyWriteBounds();
    TestSysctlNameToMibNullAndFaultBounds();
    TestSysctlNameToMibLengthAndMalformedBounds();
    TestSysctlNameToMibTopLevelAndKnownNodes();
    TestSysctlNameToMibCapacityBounds();
    TestSysctlNameToOid();
    TestSysctlNameAndMibRoundtrip();
    return 0;
}
