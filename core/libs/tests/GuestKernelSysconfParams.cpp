#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSysconfPositiveParameters() {
    *__error_nid_postfix() = 42;
    const auto nprocConf = sysconf_nid_postfix(57);
    Require(nprocConf > 0);
    Require(*__error_nid_postfix() == 42);

    const auto nprocOnln = sysconf_nid_postfix(58);
    Require(nprocOnln > 0);
    Require(nprocOnln <= nprocConf);
    Require(*__error_nid_postfix() == 42);

    const auto physPages = sysconf_nid_postfix(121);
    Require(physPages > 0);
    Require(*__error_nid_postfix() == 42);

    const auto openMax = sysconf_nid_postfix(5);
    Require(openMax > 0);
    Require(*__error_nid_postfix() == 42);

    const auto pageSize = sysconf_nid_postfix(47);
    Require(pageSize == 16384);
    Require(*__error_nid_postfix() == 42);

    const auto clkTck = sysconf_nid_postfix(3);
    Require(clkTck == 128);
    Require(*__error_nid_postfix() == 42);

    Require(sysconf_nid_postfix(1) == 262144);
    Require(sysconf_nid_postfix(2) > 0);
    Require(sysconf_nid_postfix(4) == 1023);
    Require(sysconf_nid_postfix(26) > 0);
    Require(sysconf_nid_postfix(56) == 1024);
    Require(sysconf_nid_postfix(122) == 8);
    Require(*__error_nid_postfix() == 42);
}

void TestSysconfInvalidOptions() {
    constexpr std::array<int, 10> invalidOptions{{
        -2147483647 - 1,
        -100,
        -50,
        -2,
        -1,
        0,
        92,
        98,
        9999,
        2147483647
    }};

    for (const auto opt : invalidOptions) {
        *__error_nid_postfix() = 0;
        Require(sysconf_nid_postfix(opt) == -1);
        Require(*__error_nid_postfix() == 22);
    }
}

void TestSysconfOption27() {
    *__error_nid_postfix() = 0;
    Require(sysconf_nid_postfix(27) == -1);
    Require(*__error_nid_postfix() == 2);
}

void TestSysconfUnimplementedPreserveErrno() {
    constexpr std::array<int, 33> preserved{{
        7, 19, 21, 23, 24, 30, 34, 40, 46, 59, 60, 61, 62, 63, 64,
        67, 81, 90, 91, 94, 97, 102, 103, 104, 106, 108, 109, 110,
        111, 112, 113, 114, 115
    }};

    for (const auto opt : preserved) {
        *__error_nid_postfix() = 55;
        Require(sysconf_nid_postfix(opt) == -1);
        Require(*__error_nid_postfix() == 55);
    }
}

} // namespace

int main() {
    TestSysconfPositiveParameters();
    TestSysconfInvalidOptions();
    TestSysconfOption27();
    TestSysconfUnimplementedPreserveErrno();
    return 0;
}
