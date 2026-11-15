#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEnoent = 2;
constexpr int PosixEnomem = 12;
constexpr int PosixEfault = 14;
constexpr int PosixEnotdir = 20;
constexpr int PosixEinval = 22;

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

void AuditHardwareMemoryNodes() {
    const std::int64_t physPages = sysconf_nid_postfix(121);
    Require(physPages > 0);
    const std::uint64_t expectedPhysmem = static_cast<std::uint64_t>(physPages) * 0x4000ULL;

    struct HardwareMemNode {
        const char* name;
        const char* nameWithDot;
        int mib[2];
    };
    const HardwareMemNode nodes[] = {
        {"hw.physmem", "hw.physmem.", {6, 5}},
        {"hw.realmem", "hw.realmem.", {6, 12}},
    };

    for (const auto& node : nodes) {
        int resolvedMib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
        std::size_t mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(node.name, resolvedMib, &mibLen) == 0);
        Require(mibLen == 2);
        Require(resolvedMib[0] == node.mib[0] && resolvedMib[1] == node.mib[1]);
        Require(*__error_nid_postfix() == 13);

        mibLen = 4;
        Require(sysctlnametomib_nid_postfix(node.nameWithDot, resolvedMib, &mibLen) == 0);
        Require(mibLen == 2);
        Require(resolvedMib[0] == node.mib[0] && resolvedMib[1] == node.mib[1]);

        mibLen = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(node.name, nullptr, &mibLen) == 0);
        Require(mibLen == 2);
        Require(*__error_nid_postfix() == 13);

        mibLen = 1;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(node.name, resolvedMib, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(mibLen == 1 && resolvedMib[0] == 6);

        std::size_t len = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, nullptr, &len, nullptr, 0) == 0);
        Require(len == sizeof(std::uint64_t));
        Require(*__error_nid_postfix() == 13);

        std::uint64_t val = 0;
        len = sizeof(val);
        Require(sysctlbyname_nid_postfix(node.name, &val, &len, nullptr, 0) == 0);
        Require(len == sizeof(val) && val == expectedPhysmem);
        Require(*__error_nid_postfix() == 13);

        val = 0;
        len = sizeof(val);
        Require(sysctlbyname_nid_postfix(node.nameWithDot, &val, &len, nullptr, 0) == 0);
        Require(len == sizeof(val) && val == expectedPhysmem);

        unsigned char bigBuf[32];
        std::memset(bigBuf, 0xaa, sizeof(bigBuf));
        len = sizeof(bigBuf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, bigBuf, &len, nullptr, 0) == 0);
        Require(len == sizeof(std::uint64_t));
        std::uint64_t readBig = 0;
        std::memcpy(&readBig, bigBuf, sizeof(readBig));
        Require(readBig == expectedPhysmem);
        Require(Untouched(bigBuf + sizeof(std::uint64_t), sizeof(bigBuf) - sizeof(std::uint64_t)));

        unsigned char shortBuf[8];
        std::memset(shortBuf, 0xaa, sizeof(shortBuf));
        len = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, shortBuf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(len == 4 && Untouched(shortBuf + 4, 4));
        Require(std::memcmp(shortBuf, &expectedPhysmem, 4) == 0);

        len = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, shortBuf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(len == 0);

        std::uint64_t writeVal = 0x1234;
        len = sizeof(val);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, &val, &len, &writeVal, sizeof(writeVal)) == -1);
        Require(*__error_nid_postfix() == PosixEperm);

        len = 0;
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, nullptr, &len, nullptr, 0) == 0);
        Require(len == sizeof(std::uint64_t));
        Require(*__error_nid_postfix() == 13);

        val = 0;
        len = sizeof(val);
        Require(sysctl_nid_postfix(node.mib, 2, &val, &len, nullptr, 0) == 0);
        Require(len == sizeof(val) && val == expectedPhysmem);
        Require(*__error_nid_postfix() == 13);

        std::memset(bigBuf, 0xaa, sizeof(bigBuf));
        len = sizeof(bigBuf);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, bigBuf, &len, nullptr, 0) == 0);
        Require(len == sizeof(std::uint64_t));
        readBig = 0;
        std::memcpy(&readBig, bigBuf, sizeof(readBig));
        Require(readBig == expectedPhysmem);
        Require(Untouched(bigBuf + sizeof(std::uint64_t), sizeof(bigBuf) - sizeof(std::uint64_t)));

        std::memset(shortBuf, 0xaa, sizeof(shortBuf));
        len = 4;
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, shortBuf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(len == 4 && Untouched(shortBuf + 4, 4));

        len = sizeof(val);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, &val, &len, &writeVal, sizeof(writeVal)) == -1);
        Require(*__error_nid_postfix() == PosixEperm);

        len = sizeof(val);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 1, &val, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        int longMib[25] = {};
        longMib[0] = node.mib[0];
        longMib[1] = node.mib[1];
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(longMib, 25, &val, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(nullptr, 2, &val, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEfault);
    }

    int leafChildMib[] = {6, 5, 1};
    std::uint64_t val = 0;
    std::size_t len = sizeof(val);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(leafChildMib, 3, &val, &len, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnotdir);

    Require(Throws([&] {
        int unhandledChild[] = {6, 12, 1};
        std::size_t childLen = sizeof(val);
        sysctl_nid_postfix(unhandledChild, 3, &val, &childLen, nullptr, 0);
    }));

    Require(Throws([&] {
        int mib[4];
        std::size_t size = 4;
        sysctlnametomib_nid_postfix(nullptr, mib, &size);
    }));

    Require(Throws([&] {
        int mib[4];
        sysctlnametomib_nid_postfix("hw.physmem", mib, nullptr);
    }));
}

void AuditVmOvercommit() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("vm.overcommit", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(mibLen == 4 && mib[0] == 0x5a);

    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("vm.overcommit", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(int));
    Require(*__error_nid_postfix() == 13);

    int value = -1;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("vm.overcommit", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value) && value == 0);
    Require(*__error_nid_postfix() == 13);

    value = -1;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("vm.overcommit.", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value) && value == 0);

    unsigned char bigBuf[16];
    std::memset(bigBuf, 0xaa, sizeof(bigBuf));
    length = sizeof(bigBuf);
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm.overcommit", bigBuf, &length, nullptr, 0) == 0);
    Require(length == sizeof(int));
    int readBig = -1;
    std::memcpy(&readBig, bigBuf, sizeof(readBig));
    Require(readBig == 0);
    Require(Untouched(bigBuf + sizeof(int), sizeof(bigBuf) - sizeof(int)));

    unsigned char shortBuf[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm.overcommit", shortBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(length == 2 && shortBuf[2] == 0xaa && shortBuf[3] == 0xaa);

    length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm.overcommit", shortBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(length == 0);

    int writeVal = 1;
    length = sizeof(value);
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm.overcommit", &value, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm.overcommit", nullptr, nullptr, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    length = sizeof(value);
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm", &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("vm.", &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("vm", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 2);
}

void AuditProcessorCountEnumeration() {
    const std::int64_t confCores = sysconf_nid_postfix(57);
    const std::int64_t onlnCores = sysconf_nid_postfix(58);
    Require(confCores > 0 && onlnCores > 0);
    Require(onlnCores <= confCores);

    const int mibNcpu[] = {6, 3};
    int resolvedMib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("hw.ncpu", resolvedMib, &mibLen) == 0);
    Require(mibLen == 2 && resolvedMib[0] == 6 && resolvedMib[1] == 3);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.ncpu.", resolvedMib, &mibLen) == 0);
    Require(mibLen == 2 && resolvedMib[0] == 6 && resolvedMib[1] == 3);

    std::size_t length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("hw.ncpu", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(int));
    Require(*__error_nid_postfix() == 13);

    int ncpuByName = 0;
    length = sizeof(ncpuByName);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &ncpuByName, &length, nullptr, 0) == 0);
    Require(length == sizeof(int) && ncpuByName == static_cast<int>(onlnCores));

    ncpuByName = 0;
    length = sizeof(ncpuByName);
    Require(sysctlbyname_nid_postfix("hw.ncpu.", &ncpuByName, &length, nullptr, 0) == 0);
    Require(length == sizeof(int) && ncpuByName == static_cast<int>(onlnCores));

    length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(mibNcpu, 2, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(int));

    int ncpuByMib = 0;
    length = sizeof(ncpuByMib);
    Require(sysctl_nid_postfix(mibNcpu, 2, &ncpuByMib, &length, nullptr, 0) == 0);
    Require(length == sizeof(int) && ncpuByMib == static_cast<int>(onlnCores));

    unsigned char bigBuf[16];
    std::memset(bigBuf, 0xaa, sizeof(bigBuf));
    length = sizeof(bigBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(mibNcpu, 2, bigBuf, &length, nullptr, 0) == 0);
    Require(length == sizeof(int));
    int readBig = 0;
    std::memcpy(&readBig, bigBuf, sizeof(readBig));
    Require(readBig == static_cast<int>(onlnCores));
    Require(Untouched(bigBuf + sizeof(int), sizeof(bigBuf) - sizeof(int)));

    unsigned char shortBuf[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(mibNcpu, 2, shortBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnomem);
    Require(length == 2 && shortBuf[2] == 0xaa && shortBuf[3] == 0xaa);

    int writeVal = 8;
    length = sizeof(ncpuByMib);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(mibNcpu, 2, &ncpuByMib, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("hw.ncpu", &ncpuByMib, &length, &writeVal, sizeof(writeVal)) == -1);
    Require(*__error_nid_postfix() == PosixEperm);

    int leafChildMib[] = {6, 3, 1};
    *__error_nid_postfix() = 13;
    length = sizeof(ncpuByMib);
    Require(sysctl_nid_postfix(leafChildMib, 3, &ncpuByMib, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnotdir);

    int smpMib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t smpMibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("kern.smp.maxcpus", smpMib, &smpMibLen) == 0);
    Require(smpMibLen == 3);
    Require(smpMib[0] == 1 && smpMib[1] == 2147482816 && smpMib[2] == 2147482814);

    unsigned char smpBuf[16];
    std::memset(smpBuf, 0xaa, sizeof(smpBuf));
    length = sizeof(smpBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(smpMib, 3, smpBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(length == sizeof(smpBuf) && Untouched(smpBuf, sizeof(smpBuf)));

    length = sizeof(smpBuf);
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("kern.smp.maxcpus", smpBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(length == sizeof(smpBuf) && Untouched(smpBuf, sizeof(smpBuf)));

    Require(Throws([&] {
        sysctl_nid_postfix(smpMib, 3, smpBuf, &length, smpBuf, sizeof(smpBuf));
    }));

    Require(Throws([&] {
        sysctlbyname_nid_postfix("kern.smp.maxcpus", smpBuf, &length, smpBuf, sizeof(smpBuf));
    }));

    const KernelCpumask availMask = sceKernelGetAvailableCpumask();
    Require(availMask == 0x1FFF);

    const int currentCpu = sceKernelGetCurrentCpu();
    Require(currentCpu >= 0 && currentCpu < 16);
    Require((availMask & (1u << currentCpu)) != 0);

    const std::int64_t cpuFreq = sceKernelGetCpuFrequency();
    Require(cpuFreq > 0);
}

void AuditHardwareSystemNodes() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("hw", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 6);

    std::size_t length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("hw", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("hw.", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const int mibPagesize[] = {6, 7};
    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.pagesize", mib, &mibLen) == 0);
    Require(mibLen == 2 && mib[0] == 6 && mib[1] == 7);

    int pageSize = 0;
    length = sizeof(pageSize);
    Require(sysctlbyname_nid_postfix("hw.pagesize", &pageSize, &length, nullptr, 0) == 0);
    Require(length == sizeof(int) && pageSize == 0x4000);

    pageSize = 0;
    length = sizeof(pageSize);
    Require(sysctl_nid_postfix(mibPagesize, 2, &pageSize, &length, nullptr, 0) == 0);
    Require(length == sizeof(int) && pageSize == 0x4000);

    int leafChild[] = {6, 7, 1};
    length = sizeof(pageSize);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(leafChild, 3, &pageSize, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnotdir);

    const int mibModel[] = {6, 2};
    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.model", mib, &mibLen) == 0);
    Require(mibLen == 2 && mib[0] == 6 && mib[1] == 2);

    length = 0;
    Require(sysctl_nid_postfix(mibModel, 2, nullptr, &length, nullptr, 0) == 0);
    Require(length > 1);

    char modelBuf[128] = {};
    length = sizeof(modelBuf);
    Require(sysctlbyname_nid_postfix("hw.model", modelBuf, &length, nullptr, 0) == 0);
    Require(length > 1 && modelBuf[0] != '\0');

    int modelChild[] = {6, 2, 1};
    length = sizeof(modelBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(modelChild, 3, modelBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnotdir);

    struct UnansweredNumberedNode {
        const char* name;
        int mib[2];
    };
    const UnansweredNumberedNode numberedNodes[] = {
        {"hw.byteorder", {6, 4}},
        {"hw.machine_arch", {6, 11}},
    };
    for (const auto& entry : numberedNodes) {
        mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(entry.name, mib, &mibLen) == 0);
        Require(mibLen == 2 && mib[0] == entry.mib[0] && mib[1] == entry.mib[1]);

        unsigned char buffer[32];
        std::memset(buffer, 0xaa, sizeof(buffer));
        length = sizeof(buffer);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(entry.name, buffer, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buffer) && Untouched(buffer, sizeof(buffer)));

        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(entry.mib, 2, buffer, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buffer) && Untouched(buffer, sizeof(buffer)));
    }

    const char* const unnumberedNodes[] = {
        "hw.usermem",
        "hw.cacheline_size",
        "hw.floatingpoint",
    };
    for (const char* name : unnumberedNodes) {
        mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(name, mib, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        unsigned char buffer[32];
        std::memset(buffer, 0xaa, sizeof(buffer));
        length = sizeof(buffer);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buffer, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buffer) && Untouched(buffer, sizeof(buffer)));
    }
}

} // namespace

int main() {
    AuditHardwareMemoryNodes();
    AuditVmOvercommit();
    AuditProcessorCountEnumeration();
    AuditHardwareSystemNodes();
    return 0;
}
