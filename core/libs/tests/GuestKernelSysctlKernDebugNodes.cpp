#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>

namespace {

constexpr int PosixEnoent = 2;
constexpr int PosixEnomem = 12;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u (errno %d)\n", static_cast<unsigned>(location.line()), *__error_nid_postfix());
        std::fflush(stderr);
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

void AuditUnansweredKernNodes() {
    struct KernUnansweredNode {
        const char* name;
        int expectedMib[2];
    };

    const KernUnansweredNode unanswered[] = {
        {"kern.maxfiles", {1, 7}},
        {"kern.maxproc", {1, 6}},
    };

    for (const auto& entry : unanswered) {
        std::size_t length = 64;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(entry.name, nullptr, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == 64);

        unsigned char buf[64];
        std::memset(buf, 0xaa, sizeof(buf));
        length = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(entry.name, buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buf) && Untouched(buf, sizeof(buf)));

        length = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(entry.name, buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == 0);

        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(entry.name, buf, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(Untouched(buf, sizeof(buf)));

        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(entry.name, nullptr, nullptr, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        std::string dotted = std::string(entry.name) + ".";
        length = sizeof(buf);
        *__error_nid_postfix() = 13;
        Require(sysctlbyname_nid_postfix(dotted.c_str(), buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);

        int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
        std::size_t mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(entry.name, mib, &mibLen) == 0);
        Require(mibLen == 2 && mib[0] == entry.expectedMib[0] && mib[1] == entry.expectedMib[1]);

        mibLen = 4;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(dotted.c_str(), mib, &mibLen) == 0);
        Require(mibLen == 2 && mib[0] == entry.expectedMib[0] && mib[1] == entry.expectedMib[1]);

        mibLen = 1;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(entry.name, mib, &mibLen) == -1);
        Require(*__error_nid_postfix() == PosixEnomem);
        Require(mibLen == 1);

        mibLen = 0;
        *__error_nid_postfix() = 13;
        Require(sysctlnametomib_nid_postfix(entry.name, nullptr, &mibLen) == 0);
        Require(mibLen == 2);

        length = sizeof(buf);
        std::memset(buf, 0xaa, sizeof(buf));
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(entry.expectedMib, 2, buf, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == sizeof(buf) && Untouched(buf, sizeof(buf)));

        length = 64;
        *__error_nid_postfix() = 13;
        Require(sysctl_nid_postfix(entry.expectedMib, 2, nullptr, &length, nullptr, 0) == -1);
        Require(*__error_nid_postfix() == PosixEnoent);
        Require(length == 64);
    }

    const char* const invalidKernSubnodes[] = {
        "kern.maxfiles.child",
        "kern.maxproc.extra",
        "kern.maxfiles.0",
        "kern.maxproc.1",
    };
    for (const char* subnode : invalidKernSubnodes) {
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

void AuditDebugSizeofNodes() {
    const char* const debugNodes[] = {
        "debug.sizeof.kinfo_proc",
        "debug.sizeof.proc",
        "debug.sizeof.vmentry",
        "debug.sizeof.thread",
        "debug.sizeof.file",
    };

    for (const char* name : debugNodes) {
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

        int writeVal = 64;
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

    const char* const invalidDebugSubnodes[] = {
        "debug.sizeof.kinfo_proc.0",
        "debug.sizeof.proc.sub",
        "debug.sizeof.unknown",
        "debug.unknown_node",
    };
    for (const char* subnode : invalidDebugSubnodes) {
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

void AuditDebugTopLevelAndMibBounds() {
    int mib[4] = {0x5a, 0x5a, 0x5a, 0x5a};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("debug", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 5);

    mibLen = 4;
    *__error_nid_postfix() = 13;
    Require(sysctlnametomib_nid_postfix("debug.", mib, &mibLen) == 0);
    Require(mibLen == 1 && mib[0] == 5);

    std::size_t length = 0;
    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("debug", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctlbyname_nid_postfix("debug.", nullptr, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const int absentTopLevelMib[] = {15, 1};
    unsigned char mibBuf[32];
    std::memset(mibBuf, 0xaa, sizeof(mibBuf));
    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(absentTopLevelMib, 2, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEnoent);
    Require(length == sizeof(mibBuf) && Untouched(mibBuf, sizeof(mibBuf)));

    const int debugMib[] = {5, 1, 1};
    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(debugMib, 1, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    length = sizeof(mibBuf);
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(debugMib, 0, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    int longMib[25] = {5};
    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(longMib, 25, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 13;
    Require(sysctl_nid_postfix(nullptr, 2, mibBuf, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == PosixEfault);
}

} // namespace

int main() {
    AuditUnansweredKernNodes();
    AuditDebugSizeofNodes();
    AuditDebugTopLevelAndMibBounds();
    return 0;
}
