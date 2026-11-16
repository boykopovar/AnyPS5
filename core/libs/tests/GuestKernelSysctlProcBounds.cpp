#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEnoent = 2;
constexpr int PosixEsrch = 3;
constexpr int PosixEnomem = 12;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

struct TestKinfoProc {
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
static_assert(offsetof(TestKinfoProc, pid) == 72 && offsetof(TestKinfoProc, parentPid) == 76 && offsetof(TestKinfoProc, flag) == 368 && offsetof(TestKinfoProc, command) == 447 && sizeof(TestKinfoProc) == 1088);

struct TestVmEntry {
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
static_assert(offsetof(TestVmEntry, start) == 8 && offsetof(TestVmEntry, protection) == 56 && offsetof(TestVmEntry, path) == 136 && sizeof(TestVmEntry) == 144);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool Untouched(const unsigned char* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0xaa) {
            return false;
        }
    }
    return true;
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template <typename TCall>
bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void AuditProcPathname() {
    const int selfPid = getpid_nid_postfix();
    const int mibSelf[] = {1, 14, 12, selfPid};
    const int mibNegativeOne[] = {1, 14, 12, -1};
    const int mibForeign[] = {1, 14, 12, selfPid + 10000};
    const int mibInvalidPid[] = {1, 14, 12, -2};

    std::size_t length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, nullptr, &length, nullptr, 0) == 0);
    Require(length == 16);

    length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibNegativeOne, 4, nullptr, &length, nullptr, 0) == 0);
    Require(length == 16);

    char path[32];
    std::memset(path, 0xaa, sizeof(path));
    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, path, &length, nullptr, 0) == 0);
    Require(length == 16);
    Require(std::strcmp(path, "/app0/eboot.bin") == 0);

    std::memset(path, 0xaa, sizeof(path));
    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibNegativeOne, 4, path, &length, nullptr, 0) == 0);
    Require(length == 16);
    Require(std::strcmp(path, "/app0/eboot.bin") == 0);

    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibForeign, 4, path, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibInvalidPid, 4, path, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    char shortBuf[10];
    std::memset(shortBuf, 0xaa, sizeof(shortBuf));
    length = sizeof(shortBuf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, shortBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(length == sizeof(shortBuf));
    Require(std::memcmp(shortBuf, "/app0/eboo", sizeof(shortBuf)) == 0);

    int writeVal = 1;
    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, path, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    const int mibShort[] = {1, 14, 12};
    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibShort, 3, path, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const int mibLong[] = {1, 14, 12, selfPid, 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibLong, 5, path, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    length = sizeof(path);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.pathname", path, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int mibOut[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mibOut, &mibLen) == 0);
    Require(mibLen == 3 && mibOut[0] == 1 && mibOut[1] == 14 && mibOut[2] == 12);

    mibLen = 0;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", nullptr, &mibLen) == 0);
    Require(mibLen == 3);

    mibLen = 2;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.proc.pathname", mibOut, &mibLen) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(mibLen == 2);
}

void AuditProcProc() {
    const int selfPid = getpid_nid_postfix();
    const int parentPid = getppid_nid_postfix();
    const int mib3[] = {1, 14, 8};
    const int mib4[] = {1, 14, 8, 0};
    const int mib5[] = {1, 14, 8, 0, 0};

    std::size_t length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib3, 3, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(TestKinfoProc));

    TestKinfoProc proc{};
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib3, 3, &proc, &length, nullptr, 0) == 0);
    Require(length == sizeof(proc));
    Require(proc.structSize == sizeof(TestKinfoProc));
    Require(proc.pid == selfPid);
    Require(proc.parentPid == parentPid);
    Require(std::strcmp(proc.command, "eboot.bin") == 0);

    std::memset(&proc, 0, sizeof(proc));
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib4, 4, &proc, &length, nullptr, 0) == 0);
    Require(length == sizeof(proc));
    Require(proc.pid == selfPid);

    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib5, 5, &proc, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int writeVal = 1;
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib3, 3, &proc, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    unsigned char shortProc[512];
    length = sizeof(shortProc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib3, 3, shortProc, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(length == sizeof(shortProc));

    length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.proc", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(TestKinfoProc));

    std::memset(&proc, 0, sizeof(proc));
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.proc", &proc, &length, nullptr, 0) == 0);
    Require(length == sizeof(proc));
    Require(proc.pid == selfPid);
    Require(std::strcmp(proc.command, "eboot.bin") == 0);

    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.proc.proc", &proc, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    int mibOut[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.proc.proc", mibOut, &mibLen) == 0);
    Require(mibLen == 3 && mibOut[0] == 1 && mibOut[1] == 14 && mibOut[2] == 8);
}

void AuditProcPid() {
    const int selfPid = getpid_nid_postfix();
    const int mibSelf[] = {1, 14, 1, selfPid};
    const int mibForeign[] = {1, 14, 1, selfPid + 5000};
    const int mibNegativeOne[] = {1, 14, 1, -1};

    std::size_t length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(TestKinfoProc));

    TestKinfoProc proc{};
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, &proc, &length, nullptr, 0) == 0);
    Require(length == sizeof(proc));
    Require(proc.structSize == sizeof(TestKinfoProc));
    Require(proc.pid == selfPid);

    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibForeign, 4, &proc, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibNegativeOne, 4, &proc, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    const int mibShort[] = {1, 14, 1};
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibShort, 3, &proc, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const int mibLong[] = {1, 14, 1, selfPid, 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibLong, 5, &proc, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int writeVal = 1;
    length = sizeof(proc);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, &proc, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);
}

void AuditProcEnv() {
    const int selfPid = getpid_nid_postfix();
    const int mibSelf[] = {1, 14, 35, selfPid};
    const int mibForeign[] = {1, 14, 35, selfPid + 1000};
    const int mibNegativeOne[] = {1, 14, 35, -1};

    std::size_t length = 64;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, nullptr, &length, nullptr, 0) == 0);
    Require(length == 0);

    unsigned char buf[64];
    std::memset(buf, 0xaa, sizeof(buf));
    length = sizeof(buf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, buf, &length, nullptr, 0) == 0);
    Require(length == 0);
    Require(Untouched(buf, sizeof(buf)));

    length = sizeof(buf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibForeign, 4, buf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    length = sizeof(buf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibNegativeOne, 4, buf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    const int mibShort[] = {1, 14, 35};
    length = sizeof(buf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibShort, 3, buf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int writeVal = 1;
    length = sizeof(buf);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, buf, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);
}

void AuditProcVmmap() {
    const int selfPid = getpid_nid_postfix();
    const int mibSelf[] = {1, 14, 32, selfPid};
    const int mibForeign[] = {1, 14, 32, selfPid + 3000};
    const int mibNegativeOne[] = {1, 14, 32, -1};

    std::size_t length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, nullptr, &length, nullptr, 0) == 0);
    Require(length % sizeof(TestVmEntry) == 0);

    if (length > 0) {
        std::vector<TestVmEntry> entries(length / sizeof(TestVmEntry));
        *__error_nid_postfix() = 0;
        Require(sysctl_nid_postfix(mibSelf, 4, entries.data(), &length, nullptr, 0) == 0);
        for (const auto& entry : entries) {
            Require(entry.structSize == sizeof(TestVmEntry));
            Require(entry.type == 1);
            Require(entry.start < entry.end);
        }
    }

    length = 100;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibForeign, 4, nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    length = 100;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibNegativeOne, 4, nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    const int mibShort[] = {1, 14, 32};
    length = 100;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibShort, 3, nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int writeVal = 1;
    length = 100;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, nullptr, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    unsigned char shortVm[50];
    length = sizeof(shortVm);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mibSelf, 4, shortVm, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
}

void AuditProcBoundsAndErrors() {
    const int unsupportedMib[] = {1, 14, 9999};
    Require(Throws([&] {
        std::size_t len = 64;
        sysctl_nid_postfix(unsupportedMib, 3, nullptr, &len, nullptr, 0);
    }));

    const int shortMib[] = {1};
    std::size_t length = 64;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(shortMib, 1, nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int longMib[25] = {1, 14};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(longMib, 25, nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(nullptr, 4, nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    Require(ThrowsInvalidArgument([&] {
        int mib[4];
        std::size_t size = 4;
        sysctlnametomib_nid_postfix(nullptr, mib, &size);
    }));

    Require(ThrowsInvalidArgument([&] {
        int mib[4];
        sysctlnametomib_nid_postfix("kern.proc.pathname", mib, nullptr);
    }));
}

} // namespace

int main() {
    AuditProcPathname();
    AuditProcProc();
    AuditProcPid();
    AuditProcEnv();
    AuditProcVmmap();
    AuditProcBoundsAndErrors();
    return 0;
}
