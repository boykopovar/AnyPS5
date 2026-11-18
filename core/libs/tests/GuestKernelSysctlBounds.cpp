#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSysctlNameLengthAndPointerBounds() {
    int mib[2] = {1, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 0, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 1, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    int largeMib[25] = {};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(largeMib, 25, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(nullptr, 2, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 14);
}

void TestSysctlTopLevelAndLeafBounds() {
    int badMib[2] = {-1, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(badMib, 2, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);

    int badMib2[2] = {10, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(badMib2, 2, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);

    int badMib3[2] = {250, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(badMib3, 2, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);

    int leafTooLong1[3] = {6, 7, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(leafTooLong1, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 20);

    int leafTooLong2[3] = {1, 1, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(leafTooLong2, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 20);

    int leafTooLong3[3] = {1, 3, 1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(leafTooLong3, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 20);

    const int unansweredMibs[][2] = {{1, 6}, {1, 7}, {1, 12}, {1, 27}, {1, 32}, {6, 4}, {6, 11}};
    for (const auto& entry : unansweredMibs) {
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(entry, 2, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 2);
    }

    const int stackprotMib[2] = {1, 2147483151};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(stackprotMib, 2, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);

    const int schedNameMib[3] = {1, 2147483182, 2147483181};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(schedNameMib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);
}

void TestSysctlReadOnlyWriteBounds() {
    int dummy = 42;
    const int writeMibs[][2] = {
        {1, 1}, {1, 2}, {1, 4}, {1, 10}, {6, 1}, {6, 2},
        {1, 3}, {1, 8}, {1, 18}, {1, 24}, {6, 7}, {6, 3},
        {6, 5}, {6, 12}, {2, 2}, {1, 21}, {1, 37}
    };
    for (const auto& entry : writeMibs) {
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(entry, 2, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
        Require(*__error_nid_postfix() == 1);
    }
}

void TestSysctlQueryAndTruncationBounds() {
    int mib[2] = {1, 1};
    std::size_t size = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &size, nullptr, 0) == 0);
    Require(size == std::strlen("FreeBSD") + 1);

    char shortBuf[8] = {};
    size = 3;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, shortBuf, &size, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(size == 3);
    Require(std::memcmp(shortBuf, "Fre", 3) == 0);

    char exactBuf[16] = {};
    size = sizeof(exactBuf);
    Require(sysctl_nid_postfix(mib, 2, exactBuf, &size, nullptr, 0) == 0);
    Require(size == std::strlen("FreeBSD") + 1);
    Require(std::strcmp(exactBuf, "FreeBSD") == 0);

    mib[0] = 1;
    mib[1] = 3;
    std::int32_t osrev = 0;
    size = sizeof(osrev);
    Require(sysctl_nid_postfix(mib, 2, &osrev, &size, nullptr, 0) == 0);
    Require(osrev == 199506);
    Require(size == sizeof(osrev));

    mib[0] = 1;
    mib[1] = 8;
    std::int32_t argmax = 0;
    size = sizeof(argmax);
    Require(sysctl_nid_postfix(mib, 2, &argmax, &size, nullptr, 0) == 0);
    Require(argmax == 262144);

    mib[0] = 1;
    mib[1] = 18;
    std::int32_t ngroups = 0;
    size = sizeof(ngroups);
    Require(sysctl_nid_postfix(mib, 2, &ngroups, &size, nullptr, 0) == 0);
    Require(ngroups == 1023);

    mib[0] = 1;
    mib[1] = 24;
    std::int32_t osreldate = 0;
    size = sizeof(osreldate);
    Require(sysctl_nid_postfix(mib, 2, &osreldate, &size, nullptr, 0) == 0);
    Require(osreldate == 900000);

    mib[0] = 6;
    mib[1] = 7;
    int pagesize = 0;
    size = sizeof(pagesize);
    Require(sysctl_nid_postfix(mib, 2, &pagesize, &size, nullptr, 0) == 0);
    Require(pagesize == 16384);
    Require(size == sizeof(pagesize));

    mib[0] = 6;
    mib[1] = 1;
    char machine[16] = {};
    size = sizeof(machine);
    Require(sysctl_nid_postfix(mib, 2, machine, &size, nullptr, 0) == 0);
    Require(std::strcmp(machine, "amd64") == 0);

    mib[0] = 1;
    mib[1] = 37;
    std::array<std::uint8_t, 32> randBuf{};
    size = randBuf.size();
    Require(sysctl_nid_postfix(mib, 2, randBuf.data(), &size, nullptr, 0) == 0);
    Require(size == 32);
    Require(sysctl_nid_postfix(mib, 2, nullptr, nullptr, nullptr, 0) == 0);
}

void TestSysctlProcBounds() {
    int procPathMib[4] = {1, 14, 12, -1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(procPathMib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    procPathMib[3] = 999999;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(procPathMib, 4, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 3);

    procPathMib[3] = -1;
    char pathBuf[64] = {};
    std::size_t pathLen = sizeof(pathBuf);
    Require(sysctl_nid_postfix(procPathMib, 4, pathBuf, &pathLen, nullptr, 0) == 0);
    Require(std::strcmp(pathBuf, "/app0/eboot.bin") == 0);

    procPathMib[3] = getpid_nid_postfix();
    pathLen = sizeof(pathBuf);
    Require(sysctl_nid_postfix(procPathMib, 4, pathBuf, &pathLen, nullptr, 0) == 0);
    Require(std::strcmp(pathBuf, "/app0/eboot.bin") == 0);

    int val = 0;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(procPathMib, 4, nullptr, nullptr, &val, sizeof(val)) == -1);
    Require(*__error_nid_postfix() == 1);

    int procProcMib[5] = {1, 14, 8, 0, 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(procProcMib, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    std::size_t procSize = 0;
    Require(sysctl_nid_postfix(procProcMib, 3, nullptr, &procSize, nullptr, 0) == 0);
    Require(procSize == 1088);

    int procPidMib[4] = {1, 14, 1, getpid_nid_postfix()};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(procPidMib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    procPidMib[3] = 999999;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(procPidMib, 4, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 3);

    procPidMib[3] = getpid_nid_postfix();
    procSize = 0;
    Require(sysctl_nid_postfix(procPidMib, 4, nullptr, &procSize, nullptr, 0) == 0);
    Require(procSize == 1088);
}

void TestSysctlByNameBounds() {
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix(nullptr, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 14);

    const char* topLevels[] = {
        "sysctl", "kern", "vm", "vfs", "net",
        "debug", "hw", "machdep", "user", "p1003_1b"
    };
    for (const char* node : topLevels) {
        *__error_nid_postfix() = 0;
        Require(sysctlbyname_nid_postfix(node, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 22);
    }

    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.", nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    const char* unanswered[] = {
        "hw.byteorder", "hw.cacheline_size", "hw.floatingpoint",
        "hw.machine_arch", "hw.usermem", "kern.bootfile",
        "kern.clockrate", "kern.cp_time", "kern.ident",
        "kern.maxfiles", "kern.maxfilesperproc", "kern.maxproc",
        "kern.ps_strings", "kern.sched.name", "kern.smp.maxcpus",
        "kern.stackprot", "kern.timecounter.hardware", "machdep.cacheline_size"
    };
    for (const char* name : unanswered) {
        *__error_nid_postfix() = 0;
        Require(sysctlbyname_nid_postfix(name, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == 2);
    }

    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("some.random.sysctl.node", nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);

    int dummyVal = 1;
    const char* roNames[] = {
        "kern.ostype", "kern.osrevision", "kern.sdk_version",
        "machdep.upd_version", "machdep.curr_manumode",
        "machdep.rcmgr_intdev", "machdep.rcmgr_intdev_for_internal_libc",
        "machdep.tsc_freq", "kern.rng_pseudo"
    };
    for (const char* name : roNames) {
        *__error_nid_postfix() = 0;
        Require(sysctlbyname_nid_postfix(name, nullptr, nullptr, &dummyVal, sizeof(dummyVal)) == -1);
        Require(*__error_nid_postfix() == 1);
    }

    std::uint32_t sdk = 0;
    std::size_t sdkLen = sizeof(sdk);
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &sdk, &sdkLen, nullptr, 0) == 0);
    Require(sdk == 0x01000000);
    Require(sdkLen == sizeof(sdk));

    std::uint32_t upd = 999;
    std::size_t updLen = sizeof(upd);
    Require(sysctlbyname_nid_postfix("machdep.upd_version", &upd, &updLen, nullptr, 0) == 0);
    Require(upd == 0);

    int manu = 999;
    std::size_t manuLen = sizeof(manu);
    Require(sysctlbyname_nid_postfix("machdep.curr_manumode", &manu, &manuLen, nullptr, 0) == 0);
    Require(manu == 0);

    int rcmgr = 999;
    std::size_t rcmgrLen = sizeof(rcmgr);
    Require(sysctlbyname_nid_postfix("machdep.rcmgr_intdev", &rcmgr, &rcmgrLen, nullptr, 0) == 0);
    Require(rcmgr == 0);

    int rcmgrLibc = 999;
    std::size_t rcmgrLibcLen = sizeof(rcmgrLibc);
    Require(sysctlbyname_nid_postfix("machdep.rcmgr_intdev_for_internal_libc", &rcmgrLibc, &rcmgrLibcLen, nullptr, 0) == 0);
    Require(rcmgrLibc == 0);

    std::uint64_t tsc = 0;
    std::size_t tscLen = sizeof(tsc);
    Require(sysctlbyname_nid_postfix("machdep.tsc_freq", &tsc, &tscLen, nullptr, 0) == 0);
    Require(tsc > 0);

    char shortSdk[2] = {};
    std::size_t shortLen = 2;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", shortSdk, &shortLen, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(shortLen == 2);

    sdk = 0;
    sdkLen = sizeof(sdk);
    Require(sysctlbyname_nid_postfix("kern.sdk_version.", &sdk, &sdkLen, nullptr, 0) == 0);
    Require(sdk == 0x01000000);
}

void TestSysctlNameToMibBounds() {
    bool caughtNullName = false;
    try {
        std::size_t s = 4;
        int m[4] = {};
        sysctlnametomib_nid_postfix(nullptr, m, &s);
    } catch (const std::invalid_argument&) {
        caughtNullName = true;
    }
    Require(caughtNullName);

    bool caughtNullSize = false;
    try {
        int m[4] = {};
        sysctlnametomib_nid_postfix("kern.ostype", m, nullptr);
    } catch (const std::invalid_argument&) {
        caughtNullSize = true;
    }
    Require(caughtNullSize);

    int mib[4] = {};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 2);

    mibLen = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix(".", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 2);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern", mib, &mibLen) == 0);
    Require(mibLen == 1);
    Require(mib[0] == 1);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw", mib, &mibLen) == 0);
    Require(mibLen == 1);
    Require(mib[0] == 6);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("vm", mib, &mibLen) == 0);
    Require(mibLen == 1);
    Require(mib[0] == 2);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.pagesize", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 6 && mib[1] == 7);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.ostype", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 1 && mib[1] == 1);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mib, &mibLen) == 0);
    Require(mibLen == 3);
    Require(mib[0] == 1 && mib[1] == 14 && mib[2] == 12);

    mibLen = 1;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("hw.pagesize", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(mibLen == 1);
    Require(mib[0] == 6);

    mibLen = 0;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("hw.pagesize", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(mibLen == 0);

    const char* absentNames[] = {
        "hw.cacheline_size", "hw.usermem", "machdep.cacheline_size", "unknown.sysctl.node"
    };
    for (const char* name : absentNames) {
        mibLen = 4;
        *__error_nid_postfix() = 0;
        Require(sysctlnametomib_nid_postfix(name, mib, &mibLen) == -1);
        Require(*__error_nid_postfix() == 2);
    }
}

} // namespace

int main() {
    TestSysctlNameLengthAndPointerBounds();
    TestSysctlTopLevelAndLeafBounds();
    TestSysctlReadOnlyWriteBounds();
    TestSysctlQueryAndTruncationBounds();
    TestSysctlProcBounds();
    TestSysctlByNameBounds();
    TestSysctlNameToMibBounds();
    return 0;
}
