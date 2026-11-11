#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

struct BootTimeval {
    std::int64_t sec;
    std::int64_t usec;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "assertion failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

void CheckSysconfLimits() {
    Require(sysconf_nid_postfix(1) == 262144);
    Require(sysconf_nid_postfix(3) == 128);
    Require(sysconf_nid_postfix(4) == 1023);
    Require(sysconf_nid_postfix(6) == 1);
    Require(sysconf_nid_postfix(8) == 200112);
    Require(sysconf_nid_postfix(9) == 99);
    Require(sysconf_nid_postfix(10) == 2048);
    Require(sysconf_nid_postfix(11) == 99);
    Require(sysconf_nid_postfix(12) == 1000);
    Require(sysconf_nid_postfix(13) == 0);
    Require(sysconf_nid_postfix(14) == 32);
    Require(sysconf_nid_postfix(15) == 2048);
    Require(sysconf_nid_postfix(16) == 255);
    Require(sysconf_nid_postfix(17) == 199212);
    Require(sysconf_nid_postfix(18) == 200112);
    Require(sysconf_nid_postfix(20) == 1);
    Require(sysconf_nid_postfix(22) == 200112);
    Require(sysconf_nid_postfix(25) == 200112);
    Require(sysconf_nid_postfix(47) == 16384);
    Require(sysconf_nid_postfix(56) == 1024);
    Require(sysconf_nid_postfix(57) > 0);
    Require(sysconf_nid_postfix(58) > 0);
    Require(sysconf_nid_postfix(72) == 255);
    Require(sysconf_nid_postfix(73) == 33);
    Require(sysconf_nid_postfix(121) > 0);
    Require(sysconf_nid_postfix(122) == 8);

    const int preservedNames[] = {
        7, 19, 21, 23, 24, 30, 34, 40, 46, 59, 60, 61, 62, 63, 64,
        67, 81, 90, 91, 94, 97, 102, 103, 104, 106, 108, 109, 110,
        111, 112, 113, 114, 115
    };
    for (int name : preservedNames) {
        *__error_nid_postfix() = 77;
        Require(sysconf_nid_postfix(name) == -1);
        Require(*__error_nid_postfix() == 77);
    }

    const int invalidNames[] = {-1, -50, -2147483647 - 1, 0, 9999, 2147483647};
    for (int name : invalidNames) {
        *__error_nid_postfix() = 0;
        Require(sysconf_nid_postfix(name) == -1);
        Require(*__error_nid_postfix() == 22);
    }

    *__error_nid_postfix() = 0;
    Require(sysconf_nid_postfix(27) == -1);
    Require(*__error_nid_postfix() == 2);
}

void CheckSdkVersionSysctl() {
    *__error_nid_postfix() = 77;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint32_t));
    Require(*__error_nid_postfix() == 77);

    std::uint32_t version = 0;
    length = sizeof(version);
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &version, &length, nullptr, 0) == 0);
    Require(version == 0x01000000);
    Require(length == sizeof(version));
    Require(*__error_nid_postfix() == 77);

    length = sizeof(version);
    Require(sysctlbyname_nid_postfix("kern.sdk_version.", &version, &length, nullptr, 0) == 0);
    Require(version == 0x01000000);
    Require(length == sizeof(version));

    std::array<std::uint8_t, 4> shortBuf{};
    shortBuf.fill(0xAA);
    length = 2;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 2);
    Require(shortBuf[2] == 0xAA && shortBuf[3] == 0xAA);

    shortBuf.fill(0xAA);
    length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 0);
    Require(shortBuf[0] == 0xAA && shortBuf[1] == 0xAA);

    std::array<std::uint8_t, 8> bigBuf{};
    bigBuf.fill(0xCC);
    length = bigBuf.size();
    *__error_nid_postfix() = 77;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", bigBuf.data(), &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint32_t));
    Require(*__error_nid_postfix() == 77);
    Require(bigBuf[4] == 0xCC && bigBuf[5] == 0xCC && bigBuf[6] == 0xCC && bigBuf[7] == 0xCC);

    std::uint32_t newVersion = 0x02000000;
    length = sizeof(version);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &version, &length, &newVersion, sizeof(newVersion)) == -1);
    Require(*__error_nid_postfix() == 1);

    length = sizeof(version);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix(nullptr, &version, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 14);
}

void CheckBootTimeSysctl() {
    int mib[4] = {};
    std::size_t mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.boottime", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 1 && mib[1] == 21);

    mibLen = 1;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("kern.boottime", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 12);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("kern.boottime.", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 1 && mib[1] == 21);

    *__error_nid_postfix() = 77;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(BootTimeval));
    Require(*__error_nid_postfix() == 77);

    BootTimeval bootTime{};
    length = sizeof(bootTime);
    Require(sysctl_nid_postfix(mib, 2, &bootTime, &length, nullptr, 0) == 0);
    Require(length == sizeof(BootTimeval));
    Require(bootTime.sec > 0);
    Require(bootTime.usec >= 0 && bootTime.usec < 1000000);
    Require(*__error_nid_postfix() == 77);

    BootTimeval namedBootTime{};
    length = sizeof(namedBootTime);
    Require(sysctlbyname_nid_postfix("kern.boottime", &namedBootTime, &length, nullptr, 0) == 0);
    Require(length == sizeof(BootTimeval));
    Require(namedBootTime.sec == bootTime.sec);
    Require(namedBootTime.usec == bootTime.usec);

    std::array<std::uint8_t, sizeof(BootTimeval)> truncated{};
    truncated.fill(0xBB);
    length = 8;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, truncated.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 8);
    for (std::size_t i = 8; i < truncated.size(); ++i) {
        Require(truncated[i] == 0xBB);
    }

    std::array<std::uint8_t, sizeof(BootTimeval) * 2> oversized{};
    oversized.fill(0xDD);
    length = oversized.size();
    *__error_nid_postfix() = 77;
    Require(sysctl_nid_postfix(mib, 2, oversized.data(), &length, nullptr, 0) == 0);
    Require(length == sizeof(BootTimeval));
    Require(*__error_nid_postfix() == 77);
    for (std::size_t i = sizeof(BootTimeval); i < oversized.size(); ++i) {
        Require(oversized[i] == 0xDD);
    }

    BootTimeval dummy{123, 456};
    length = sizeof(bootTime);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, &bootTime, &length, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == 1);

    length = sizeof(bootTime);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("kern.boottime", &bootTime, &length, &dummy, sizeof(dummy)) == -1);
    Require(*__error_nid_postfix() == 1);

    length = sizeof(bootTime);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 1, &bootTime, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 0, &bootTime, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 25, &bootTime, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    const int leafChildMib[] = {1, 21, 0};
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(leafChildMib, 3, &bootTime, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 20);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(nullptr, 2, &bootTime, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 14);
}

} // namespace

int main() {
    CheckSysconfLimits();
    CheckSdkVersionSysctl();
    CheckBootTimeSysctl();
    return 0;
}
