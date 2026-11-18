#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEnoent = 2;
constexpr int PosixEsrch = 3;
constexpr int PosixEnomem = 12;
constexpr int PosixEfault = 14;
constexpr int PosixEnotdir = 20;
constexpr int PosixEinval = 22;

struct GuestKinfoProc {
    std::int32_t structSize;
    std::int32_t layout;
    unsigned char reserved0[64];
    std::int32_t pid;
    std::int32_t parentPid;
    unsigned char reserved1[288];
    std::int64_t flag;
    unsigned char reserved2[18];
    char threadName[17];
    unsigned char reserved3[36];
    char command[20];
    unsigned char reserved4[35];
    char moreThreadName[4];
    unsigned char reserved5[90];
    std::int32_t threadCount;
    std::int32_t threadId;
    unsigned char reserved6[484];
};
static_assert(sizeof(GuestKinfoProc) == 1088);

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
static_assert(sizeof(GuestVmEntry) == 144);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSysctlProcNameAndLengthBounds() {
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(nullptr, 4, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    int mib[4] = {1, 14, 12, -1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 0, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 1, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnotdir);

    int largeMib[25] = {1, 14, 12, -1};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(largeMib, 25, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const int invalidLeaves[] = {-5, -1, 0, 2, 3, 4, 7, 9, 10, 11, 13, 16, 20, 30, 31, 33, 34, 36, 100, 999};
    for (const int leaf : invalidLeaves) {
        int badMib[4] = {1, 14, leaf, -1};
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(badMib, 4, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
    }
}

void TestSysctlProcPathnameBounds() {
    int mib[4] = {1, 14, 12, -1};

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int mib5[5] = {1, 14, 12, -1, 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int dummy = 123;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    const int foreignPids[] = {-10, -2, 0, 9999, 12345, 999999};
    for (const int pid : foreignPids) {
        int foreignMib[4] = {1, 14, 12, pid};
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(foreignMib, 4, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEsrch);
    }

    for (const int pid : {-1, getpid_nid_postfix()}) {
        int validMib[4] = {1, 14, 12, pid};

        std::size_t size = 0;
        Require(sysctl_nid_postfix(validMib, 4, nullptr, &size, nullptr, 0) == 0);
        Require(size == sizeof("/app0/eboot.bin"));

        char truncated[4] = {};
        std::size_t truncSize = sizeof(truncated);
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(validMib, 4, truncated, &truncSize, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(truncSize == sizeof(truncated));
        Require(std::memcmp(truncated, "/app", 4) == 0);

        char exact[16] = {};
        std::size_t exactSize = sizeof(exact);
        Require(sysctl_nid_postfix(validMib, 4, exact, &exactSize, nullptr, 0) == 0);
        Require(exactSize == sizeof("/app0/eboot.bin"));
        Require(std::strcmp(exact, "/app0/eboot.bin") == 0);

        char large[64] = {};
        std::size_t largeSize = sizeof(large);
        Require(sysctl_nid_postfix(validMib, 4, large, &largeSize, nullptr, 0) == 0);
        Require(largeSize == sizeof("/app0/eboot.bin"));
        Require(std::strcmp(large, "/app0/eboot.bin") == 0);
    }
}

void TestSysctlProcProcBounds() {
    int mib[4] = {1, 14, 8, 0};

    int mib5[5] = {1, 14, 8, 0, 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int dummy = 42;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    std::size_t probedSize = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, &probedSize, nullptr, 0) == 0);
    Require(probedSize == 6 * sizeof(GuestKinfoProc));

    probedSize = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, &probedSize, nullptr, 0) == 0);
    Require(probedSize == 6 * sizeof(GuestKinfoProc));

    char smallBuf[100] = {};
    std::size_t smallSize = sizeof(smallBuf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, smallBuf, &smallSize, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(smallSize == sizeof(smallBuf));

    GuestKinfoProc proc{};
    std::size_t procSize = sizeof(proc);
    Require(sysctl_nid_postfix(mib, 3, &proc, &procSize, nullptr, 0) == 0);
    Require(procSize == sizeof(GuestKinfoProc));
    Require(proc.structSize == sizeof(GuestKinfoProc));
    Require(proc.pid == getpid_nid_postfix());
    Require(proc.parentPid == getppid_nid_postfix());
    Require(std::strcmp(proc.command, "eboot.bin") == 0);
}

void TestSysctlProcPidBounds() {
    int mib[4] = {1, 14, 1, getpid_nid_postfix()};

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int mib5[5] = {1, 14, 1, getpid_nid_postfix(), 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int dummy = 1;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    const int invalidPids[] = {-1, 0, 9999, 12345, 999999};
    for (const int pid : invalidPids) {
        int badMib[4] = {1, 14, 1, pid};
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(badMib, 4, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEsrch);
    }

    std::size_t probedSize = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, &probedSize, nullptr, 0) == 0);
    Require(probedSize == sizeof(GuestKinfoProc));

    char smallBuf[64] = {};
    std::size_t smallSize = sizeof(smallBuf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, smallBuf, &smallSize, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(smallSize == sizeof(smallBuf));

    GuestKinfoProc proc{};
    std::size_t procSize = sizeof(proc);
    Require(sysctl_nid_postfix(mib, 4, &proc, &procSize, nullptr, 0) == 0);
    Require(procSize == sizeof(GuestKinfoProc));
    Require(proc.structSize == sizeof(GuestKinfoProc));
    Require(proc.pid == getpid_nid_postfix());
    Require(proc.parentPid == getppid_nid_postfix());
    Require(std::strcmp(proc.command, "eboot.bin") == 0);
}

void TestSysctlProcThreadsBounds() {
    int mib[4] = {1, 14, 17, getpid_nid_postfix()};

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int mib5[5] = {1, 14, 17, getpid_nid_postfix(), 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int dummy = 1;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    const int invalidPids[] = {-1, 0, 9999, 12345, 999999};
    for (const int pid : invalidPids) {
        int badMib[4] = {1, 14, 17, pid};
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(badMib, 4, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEsrch);
    }

    std::size_t probedSize = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, &probedSize, nullptr, 0) == 0);
    Require(probedSize >= sizeof(GuestKinfoProc));
    Require(probedSize % sizeof(GuestKinfoProc) == 0);

    const std::size_t threadCount = probedSize / sizeof(GuestKinfoProc);
    std::vector<GuestKinfoProc> threads(threadCount);
    std::size_t readSize = probedSize;
    Require(sysctl_nid_postfix(mib, 4, threads.data(), &readSize, nullptr, 0) == 0);
    Require(readSize == probedSize);
    for (const auto& t : threads) {
        Require(t.structSize == sizeof(GuestKinfoProc));
        Require(t.pid == getpid_nid_postfix());
        Require(t.threadCount == static_cast<std::int32_t>(threadCount));
        Require(std::strcmp(t.command, "eboot.bin") == 0);
    }
}

void TestSysctlProcVmMapBounds() {
    int mib[4] = {1, 14, 32, getpid_nid_postfix()};

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int mib5[5] = {1, 14, 32, getpid_nid_postfix(), 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int dummy = 1;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    const int invalidPids[] = {-1, 0, 9999, 12345, 999999};
    for (const int pid : invalidPids) {
        int badMib[4] = {1, 14, 32, pid};
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(badMib, 4, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEsrch);
    }

    std::size_t probedSize = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, &probedSize, nullptr, 0) == 0);
    if (probedSize > 0) {
        Require(probedSize % sizeof(GuestVmEntry) == 0);
        const std::size_t entryCount = probedSize / sizeof(GuestVmEntry);
        std::vector<GuestVmEntry> entries(entryCount);
        std::size_t readSize = probedSize;
        Require(sysctl_nid_postfix(mib, 4, entries.data(), &readSize, nullptr, 0) == 0);
        Require(readSize == probedSize);
        for (const auto& entry : entries) {
            Require(entry.structSize == sizeof(GuestVmEntry));
            Require(entry.type == 1);
            Require(entry.start <= entry.end);
        }
    }
}

void TestSysctlProcGroupsBounds() {
    int mib[4] = {1, 14, 35, getpid_nid_postfix()};

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 3, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int mib5[5] = {1, 14, 35, getpid_nid_postfix(), 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, nullptr, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int dummy = 1;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 4, nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    const int invalidPids[] = {-1, 0, 9999, 12345, 999999};
    for (const int pid : invalidPids) {
        int badMib[4] = {1, 14, 35, pid};
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(badMib, 4, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEsrch);
    }

    std::size_t size = 128;
    char buffer[128] = {};
    Require(sysctl_nid_postfix(mib, 4, buffer, &size, nullptr, 0) == 0);
    Require(size == 0);
}

void TestSysctlByNameAndMibMapping() {
    int dummy = 1;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.pathname", nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.proc", nullptr, nullptr, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    char path[64] = {};
    std::size_t pathLen = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.pathname", path, &pathLen, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    pathLen = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.pathname.", path, &pathLen, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    GuestKinfoProc proc{};
    std::size_t procLen = sizeof(proc);
    Require(sysctlbyname_nid_postfix("kern.proc.proc", &proc, &procLen, nullptr, 0) == 0);
    Require(procLen == sizeof(GuestKinfoProc));
    Require(proc.pid == getpid_nid_postfix());
    Require(std::strcmp(proc.command, "eboot.bin") == 0);

    procLen = sizeof(proc);
    Require(sysctlbyname_nid_postfix("kern.proc.proc.", &proc, &procLen, nullptr, 0) == 0);
    Require(procLen == sizeof(GuestKinfoProc));
    Require(proc.pid == getpid_nid_postfix());

    int mib[4] = {};
    std::size_t mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.proc", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 1 && mib[1] == 14);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mib, &mibLen) == 0);
    Require(mibLen == 3);
    Require(mib[0] == 1 && mib[1] == 14 && mib[2] == 12);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.proc.proc", mib, &mibLen) == 0);
    Require(mibLen == 3);
    Require(mib[0] == 1 && mib[1] == 14 && mib[2] == 8);

    mibLen = 2;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.proc.proc", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(mibLen == 2);

    mibLen = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.proc.unknown", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
}

} // namespace

int main() {
    TestSysctlProcNameAndLengthBounds();
    TestSysctlProcPathnameBounds();
    TestSysctlProcProcBounds();
    TestSysctlProcPidBounds();
    TestSysctlProcThreadsBounds();
    TestSysctlProcVmMapBounds();
    TestSysctlProcGroupsBounds();
    TestSysctlByNameAndMibMapping();
    return 0;
}
