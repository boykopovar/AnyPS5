#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

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

template <typename TCall>
bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void AuditSecurityAndJailSysctlbyname() {
    const char* const securityNodes[] = {
        "security.bsd.see_other_uids",
        "security.bsd.see_other_gids",
        "security.jail.jailed",
        "security.jail.mount_allowed",
        "security.bsd.suser_enabled",
        "security.bsd.unprivileged_proc_debug",
        "security.bsd.unprivileged_read_msgbuf",
        "security.jail.enforce_statfs",
        "security.jail.allow_raw_sockets",
        "security.jail.chflags_allowed",
    };

    for (const char* name : securityNodes) {
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

        int writeVal = 1;
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
        "security.bsd.see_other_uids.0",
        "security.bsd.see_other_gids.child",
        "security.jail.jailed.active",
        "security.jail.mount_allowed.deep",
        "security.unknown_node",
        "security.bsd.unknown_leaf",
        "security.jail.param.unknown",
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

void AuditSecurityTopLevelAndMibBounds() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("security", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);

    std::size_t length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("security", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("security.", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);

    const int absentTopLevelMib[] = {10, 1, 1};
    unsigned char mibBuf[32];
    std::memset(mibBuf, 0xaa, sizeof(mibBuf));
    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(absentTopLevelMib, 3, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(length == sizeof(mibBuf) && Untouched(mibBuf, sizeof(mibBuf)));

    Require(Throws([&] {
        int writeVal = 1;
        std::size_t len = sizeof(mibBuf);
        sysctl_nid_postfix(absentTopLevelMib, 3, mibBuf, &len, &writeVal, sizeof(writeVal));
    }));

    const int absentHighTopLevelMib[] = {205, 1};
    std::memset(mibBuf, 0xaa, sizeof(mibBuf));
    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(absentHighTopLevelMib, 2, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(length == sizeof(mibBuf) && Untouched(mibBuf, sizeof(mibBuf)));

    Require(Throws([&] {
        int writeVal = 1;
        std::size_t len = sizeof(mibBuf);
        sysctl_nid_postfix(absentHighTopLevelMib, 2, mibBuf, &len, &writeVal, sizeof(writeVal));
    }));

    const int unknownKernMib[] = {1, 9999};
    Require(Throws([&] {
        std::size_t len = sizeof(mibBuf);
        sysctl_nid_postfix(unknownKernMib, 2, mibBuf, &len, nullptr, 0);
    }));

    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(absentTopLevelMib, 1, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(absentTopLevelMib, 0, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int longMib[25] = {10};
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(longMib, 25, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(nullptr, 2, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    Require(ThrowsInvalidArgument([&] {
        int outMib[4];
        std::size_t size = 4;
        sysctlnametomib_nid_postfix(nullptr, outMib, &size);
    }));

    Require(ThrowsInvalidArgument([&] {
        int outMib[4];
        sysctlnametomib_nid_postfix("security.jail.jailed", outMib, nullptr);
    }));

    Require(issetugid_nid_postfix() == 0);
    Require(getuid_nid_postfix() >= 0);
    Require(geteuid_nid_postfix() >= 0);
    Require(getgid_nid_postfix() >= 0);
    Require(getegid_nid_postfix() >= 0);
}

} // namespace

int main() {
    AuditSecurityAndJailSysctlbyname();
    AuditSecurityTopLevelAndMibBounds();
    return 0;
}
