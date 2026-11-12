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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "assertion failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

void CheckHwRealmemSysctl() {
    int mib[4] = {};
    std::size_t mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.realmem", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 6 && mib[1] == 12);

    mibLen = 1;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("hw.realmem", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 12);

    mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.realmem.", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 6 && mib[1] == 12);

    const std::int64_t pages = sysconf_nid_postfix(121);
    Require(pages > 0);
    const std::uint64_t expectedBytes = static_cast<std::uint64_t>(pages) * 16384ULL;

    *__error_nid_postfix() = 77;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("hw.realmem", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);

    std::uint64_t value = 0;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("hw.realmem", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == expectedBytes);
    Require(*__error_nid_postfix() == 77);

    value = 0;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("hw.realmem.", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == expectedBytes);

    std::array<std::uint8_t, 8> shortBuf{};
    shortBuf.fill(0xAA);
    length = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("hw.realmem", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 4);
    Require(shortBuf[4] == 0xAA && shortBuf[5] == 0xAA && shortBuf[6] == 0xAA && shortBuf[7] == 0xAA);

    shortBuf.fill(0xAA);
    length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("hw.realmem", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 0);
    Require(shortBuf[0] == 0xAA);

    std::array<std::uint8_t, 16> bigBuf{};
    bigBuf.fill(0xCC);
    length = bigBuf.size();
    *__error_nid_postfix() = 77;
    Require(sysctlbyname_nid_postfix("hw.realmem", bigBuf.data(), &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);
    std::uint64_t bigVal = 0;
    std::memcpy(&bigVal, bigBuf.data(), sizeof(bigVal));
    Require(bigVal == expectedBytes);
    for (std::size_t i = sizeof(std::uint64_t); i < bigBuf.size(); ++i) {
        Require(bigBuf[i] == 0xCC);
    }

    std::uint64_t newValue = 12345;
    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("hw.realmem", &value, &length, &newValue, sizeof(newValue)) == -1);
    Require(*__error_nid_postfix() == 1);

    *__error_nid_postfix() = 77;
    length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);

    value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == expectedBytes);
    Require(*__error_nid_postfix() == 77);

    shortBuf.fill(0xBB);
    length = 7;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 7);
    Require(shortBuf[7] == 0xBB);

    bigBuf.fill(0xDD);
    length = bigBuf.size();
    *__error_nid_postfix() = 77;
    Require(sysctl_nid_postfix(mib, 2, bigBuf.data(), &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);
    for (std::size_t i = sizeof(std::uint64_t); i < bigBuf.size(); ++i) {
        Require(bigBuf[i] == 0xDD);
    }

    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &newValue, sizeof(newValue)) == -1);
    Require(*__error_nid_postfix() == 1);

    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 1, &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 25, &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);
}

void CheckHwPhysmemSysctl() {
    int mib[4] = {};
    std::size_t mibLen = 4;
    Require(sysctlnametomib_nid_postfix("hw.physmem", mib, &mibLen) == 0);
    Require(mibLen == 2);
    Require(mib[0] == 6 && mib[1] == 5);

    const std::int64_t pages = sysconf_nid_postfix(121);
    Require(pages > 0);
    const std::uint64_t expectedBytes = static_cast<std::uint64_t>(pages) * 16384ULL;

    *__error_nid_postfix() = 77;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("hw.physmem", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);

    std::uint64_t value = 0;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("hw.physmem", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == expectedBytes);
    Require(*__error_nid_postfix() == 77);

    value = 0;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("hw.physmem.", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == expectedBytes);

    std::array<std::uint8_t, 8> shortBuf{};
    shortBuf.fill(0xAA);
    length = 6;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("hw.physmem", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 6);
    Require(shortBuf[6] == 0xAA && shortBuf[7] == 0xAA);

    std::array<std::uint8_t, 16> bigBuf{};
    bigBuf.fill(0xCC);
    length = bigBuf.size();
    *__error_nid_postfix() = 77;
    Require(sysctlbyname_nid_postfix("hw.physmem", bigBuf.data(), &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);
    std::uint64_t bigVal = 0;
    std::memcpy(&bigVal, bigBuf.data(), sizeof(bigVal));
    Require(bigVal == expectedBytes);
    for (std::size_t i = sizeof(std::uint64_t); i < bigBuf.size(); ++i) {
        Require(bigBuf[i] == 0xCC);
    }

    std::uint64_t newValue = 54321;
    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("hw.physmem", &value, &length, &newValue, sizeof(newValue)) == -1);
    Require(*__error_nid_postfix() == 1);

    *__error_nid_postfix() = 77;
    length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint64_t));
    Require(*__error_nid_postfix() == 77);

    value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == expectedBytes);
    Require(*__error_nid_postfix() == 77);

    shortBuf.fill(0xBB);
    length = 2;
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 2);
    Require(shortBuf[2] == 0xBB);

    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &newValue, sizeof(newValue)) == -1);
    Require(*__error_nid_postfix() == 1);
}

void CheckVmOvercommitSysctl() {
    int mib[4] = {};
    std::size_t mibLen = 4;
    *__error_nid_postfix() = 0;
    Require(sysctlnametomib_nid_postfix("vm.overcommit", mib, &mibLen) == -1);
    Require(*__error_nid_postfix() == 2);

    *__error_nid_postfix() = 77;
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("vm.overcommit", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(int));
    Require(*__error_nid_postfix() == 77);

    int value = -1;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("vm.overcommit", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == 0);
    Require(*__error_nid_postfix() == 77);

    value = -1;
    length = sizeof(value);
    Require(sysctlbyname_nid_postfix("vm.overcommit.", &value, &length, nullptr, 0) == 0);
    Require(length == sizeof(value));
    Require(value == 0);

    std::array<std::uint8_t, 4> shortBuf{};
    shortBuf.fill(0xAA);
    length = 2;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("vm.overcommit", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 2);
    Require(shortBuf[2] == 0xAA && shortBuf[3] == 0xAA);

    shortBuf.fill(0xAA);
    length = 0;
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("vm.overcommit", shortBuf.data(), &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(length == 0);
    Require(shortBuf[0] == 0xAA);

    std::array<std::uint8_t, 8> bigBuf{};
    bigBuf.fill(0xCC);
    length = bigBuf.size();
    *__error_nid_postfix() = 77;
    Require(sysctlbyname_nid_postfix("vm.overcommit", bigBuf.data(), &length, nullptr, 0) == 0);
    Require(length == sizeof(int));
    Require(*__error_nid_postfix() == 77);
    int bigVal = -1;
    std::memcpy(&bigVal, bigBuf.data(), sizeof(bigVal));
    Require(bigVal == 0);
    for (std::size_t i = sizeof(int); i < bigBuf.size(); ++i) {
        Require(bigBuf[i] == 0xCC);
    }

    int newValue = 1;
    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("vm.overcommit", &value, &length, &newValue, sizeof(newValue)) == -1);
    Require(*__error_nid_postfix() == 1);

    length = sizeof(value);
    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix(nullptr, &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("hw", &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sysctlbyname_nid_postfix("vm", &value, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);
}

} // namespace

int main() {
    CheckHwRealmemSysctl();
    CheckHwPhysmemSysctl();
    CheckVmOvercommitSysctl();
    return 0;
}
