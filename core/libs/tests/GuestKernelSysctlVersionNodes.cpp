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

struct VersionNodeDescriptor {
    const char* name;
    const char* nameWithDot;
    int mib[2];
    const char* expectedValue;
};

void AuditKernelVersionNodes() {
    const VersionNodeDescriptor nodes[] = {
        {"kern.ostype", "kern.ostype.", {1, 1}, "FreeBSD"},
        {"kern.osrelease", "kern.osrelease.", {1, 2}, "0.0-prototype"},
        {"kern.version", "kern.version.", {1, 4}, "FreeBSD 11.4-RELEASE\n"},
        {"kern.hostname", "kern.hostname.", {1, 10}, ""},
    };

    for (const auto& node : nodes) {
        const std::size_t expectedSize = std::strlen(node.expectedValue) + 1;

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
        Require(mibLen == 1 && resolvedMib[0] == node.mib[0]);

        std::size_t len = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, nullptr, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(*__error_nid_postfix() == 13);

        char buf[64] = {};
        len = sizeof(buf);
        Require(sysctlbyname_nid_postfix(node.name, buf, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(std::strcmp(buf, node.expectedValue) == 0);
        Require(*__error_nid_postfix() == 13);

        std::memset(buf, 0, sizeof(buf));
        len = sizeof(buf);
        Require(sysctlbyname_nid_postfix(node.nameWithDot, buf, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(std::strcmp(buf, node.expectedValue) == 0);

        unsigned char bigBuf[128];
        std::memset(bigBuf, 0xaa, sizeof(bigBuf));
        len = sizeof(bigBuf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, bigBuf, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(std::memcmp(bigBuf, node.expectedValue, expectedSize) == 0);
        Require(Untouched(bigBuf + expectedSize, sizeof(bigBuf) - expectedSize));

        if (expectedSize > 1) {
            unsigned char shortBuf[32];
            std::memset(shortBuf, 0xaa, sizeof(shortBuf));
            const std::size_t halfLen = expectedSize / 2;
            len = halfLen;
            *__error_nid_postfix() = 13;
            Require(sysctlbyname_nid_postfix(node.name, shortBuf, &len, nullptr, 0) == -1);
            Require(*__error_nid_postfix() == PosixEnomem);
            Require(len == halfLen);
            Require(std::memcmp(shortBuf, node.expectedValue, halfLen) == 0);
            Require(Untouched(shortBuf + halfLen, sizeof(shortBuf) - halfLen));
        }

        unsigned char zeroBuf[16];
        std::memset(zeroBuf, 0xaa, sizeof(zeroBuf));
        len = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, zeroBuf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(len == 0 && Untouched(zeroBuf, sizeof(zeroBuf)));

        char dummyValue[] = "replacement";
        len = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, buf, &len, dummyValue, sizeof(dummyValue)) == -1);
        Require(*__error_nid_postfix() == PosixEperm);

        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(node.name, nullptr, nullptr, dummyValue, sizeof(dummyValue)) == -1);
        Require(*__error_nid_postfix() == PosixEperm);

        len = 0;
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, nullptr, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(*__error_nid_postfix() == 13);

        std::memset(buf, 0, sizeof(buf));
        len = sizeof(buf);
        Require(sysctl_nid_postfix(node.mib, 2, buf, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(std::strcmp(buf, node.expectedValue) == 0);
        Require(*__error_nid_postfix() == 13);

        std::memset(bigBuf, 0xaa, sizeof(bigBuf));
        len = sizeof(bigBuf);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, bigBuf, &len, nullptr, 0) == 0);
        Require(len == expectedSize);
        Require(std::memcmp(bigBuf, node.expectedValue, expectedSize) == 0);
        Require(Untouched(bigBuf + expectedSize, sizeof(bigBuf) - expectedSize));

        if (expectedSize > 1) {
            unsigned char shortBuf[32];
            std::memset(shortBuf, 0xaa, sizeof(shortBuf));
            const std::size_t halfLen = expectedSize / 2;
            len = halfLen;
            *__error_nid_postfix() = 13;
            Require(sysctl_nid_postfix(node.mib, 2, shortBuf, &len, nullptr, 0) == -1);
            Require(*__error_nid_postfix() == PosixEnomem);
            Require(len == halfLen);
            Require(std::memcmp(shortBuf, node.expectedValue, halfLen) == 0);
            Require(Untouched(shortBuf + halfLen, sizeof(shortBuf) - halfLen));
        }

        len = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, buf, &len, dummyValue, sizeof(dummyValue)) == -1);
        Require(*__error_nid_postfix() == PosixEperm);

        len = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 2, buf, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);

        int leafChildMib[] = {node.mib[0], node.mib[1], 1};
        len = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(leafChildMib, 3, buf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnotdir);

        len = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(node.mib, 1, buf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        int longMib[25] = {};
        longMib[0] = node.mib[0];
        longMib[1] = node.mib[1];
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(longMib, 25, buf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(nullptr, 2, buf, &len, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEfault);
    }

    std::size_t len = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix(nullptr, nullptr, &len, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    Require(Throws([&] {
        int mib[4];
        std::size_t size = 4;
        sysctlnametomib_nid_postfix(nullptr, mib, &size);
    }));

    Require(Throws([&] {
        int mib[4];
        sysctlnametomib_nid_postfix("kern.ostype", mib, nullptr);
    }));
}

void AuditTopLevelKernAndHwMachine() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("kern", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 1);

    std::size_t length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("kern", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("kern.", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("hw.machine", mib, &mibLen) == 0);
    Require(mibLen == 2 && mib[0] == 6 && mib[1] == 1);

    length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("hw.machine", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof("amd64"));

    char machineBuf[32] = {};
    length = sizeof(machineBuf);
    Require(sysctlbyname_nid_postfix("hw.machine", machineBuf, &length, nullptr, 0) == 0);
    Require(length == sizeof("amd64") && std::strcmp(machineBuf, "amd64") == 0);

    const int mibMachine[] = {6, 1};
    std::memset(machineBuf, 0, sizeof(machineBuf));
    length = sizeof(machineBuf);
    Require(sysctl_nid_postfix(mibMachine, 2, machineBuf, &length, nullptr, 0) == 0);
    Require(length == sizeof("amd64") && std::strcmp(machineBuf, "amd64") == 0);

    int leafChild[] = {6, 1, 1};
    length = sizeof(machineBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(leafChild, 3, machineBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnotdir);
}

} // namespace

int main() {
    AuditKernelVersionNodes();
    AuditTopLevelKernAndHwMachine();
    return 0;
}
