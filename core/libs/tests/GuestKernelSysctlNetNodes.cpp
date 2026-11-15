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

void AuditNetSysctlbyname() {
    const char* const netNodes[] = {
        "net.inet.tcp.recvspace",
        "net.inet.tcp.sendspace",
        "net.inet.ip.ttl",
        "net.inet.ip.forwarding",
    };

    for (const char* name : netNodes) {
        std::size_t length = 64;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, nullptr, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == 64);

        unsigned char buf[64];
        std::memset(buf, 0xaa, sizeof(buf));
        length = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buf) && Untouched(buf, sizeof(buf)));

        length = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == 0);

        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buf, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(Untouched(buf, sizeof(buf)));

        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        int writeVal = 65536;
        length = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(name, buf, &length, &writeVal, sizeof(writeVal)) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        std::string dotted = std::string(name) + ".";
        length = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(dotted.c_str(), buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
        std::size_t mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(name, mib, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(mibLen == 4 && mib[0] == 0x5a && mib[1] == 0x5a);

        mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(dotted.c_str(), mib, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        mibLen = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(name, nullptr, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(mibLen == 0);
    }

    const char* const invalidSubnodes[] = {
        "net.inet.tcp.recvspace.0",
        "net.inet.tcp.sendspace.child",
        "net.inet.ip.ttl.sub",
        "net.inet.ip.forwarding.extra",
        "net.unknown_node",
    };
    for (const char* subnode : invalidSubnodes) {
        unsigned char buf[32];
        std::memset(buf, 0xaa, sizeof(buf));
        std::size_t length = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(subnode, buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buf) && Untouched(buf, sizeof(buf)));

        int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
        std::size_t mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(subnode, mib, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
    }
}

void AuditNetTopLevelAndMibBounds() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("net", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 4);

    mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("net.", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 4);

    std::size_t length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("net", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("net.", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const int netMib[] = {4, 2, 6, 1};
    unsigned char mibBuf[32];
    Require(Throws([&] {
        std::size_t len = sizeof(mibBuf);
        sysctl_nid_postfix(netMib, 4, mibBuf, &len, nullptr, 0);
    }));

    const int absentTopLevelMib[] = {201, 1};
    length = sizeof(mibBuf);
    std::memset(mibBuf, 0xaa, sizeof(mibBuf));
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(absentTopLevelMib, 2, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(length == sizeof(mibBuf) && Untouched(mibBuf, sizeof(mibBuf)));

    Require(Throws([&] {
        int writeVal = 1;
        std::size_t len = sizeof(mibBuf);
        sysctl_nid_postfix(absentTopLevelMib, 2, mibBuf, &len, &writeVal, sizeof(writeVal));
    }));

    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(netMib, 1, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int longMib[25] = {4};
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(longMib, 25, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(nullptr, 2, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    Require(Throws([&] {
        int outMib[4];
        std::size_t size = 4;
        sysctlnametomib_nid_postfix(nullptr, outMib, &size);
    }));

    Require(Throws([&] {
        int outMib[4];
        sysctlnametomib_nid_postfix("net.inet.tcp.recvspace", outMib, nullptr);
    }));
}

} // namespace

int main() {
    AuditNetSysctlbyname();
    AuditNetTopLevelAndMibBounds();
    return 0;
}
