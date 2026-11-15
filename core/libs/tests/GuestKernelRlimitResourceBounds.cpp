#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int RlimitCpu = 0;
constexpr int RlimitFsize = 1;
constexpr int RlimitData = 2;
constexpr int RlimitStack = 3;
constexpr int RlimitCore = 4;
constexpr int RlimitRss = 5;
constexpr int RlimitMemlock = 6;
constexpr int RlimitNproc = 7;
constexpr int RlimitNofile = 8;
constexpr int RlimitSbsize = 9;
constexpr int RlimitVmem = 10;
constexpr int RlimitNpts = 11;
constexpr int RlimitSwap = 12;
constexpr int RlimitKqueues = 13;
constexpr int RlimitUmtxp = 14;
constexpr int RlimNlimits = 15;
constexpr std::int64_t RlimInfinity = std::numeric_limits<std::int64_t>::max();

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestGetrlimitResourceBoundsAndPrecedence() {
    GuestResourceLimit limit{};

    const int invalidResources[] = {-1, -10, -100, RlimNlimits, RlimNlimits + 1, 100};
    for (int resource : invalidResources) {
        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, &limit) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
    }

    for (int resource = 0; resource < RlimNlimits; ++resource) {
        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEfault);
    }

    for (int resource = 0; resource < RlimNlimits; ++resource) {
        GuestResourceLimit resLimit{-1, -1};
        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, &resLimit) == 0);
        Require(*__error_nid_postfix() == 0);
        Require(resLimit.rlim_cur >= 0);
        Require(resLimit.rlim_max >= 0);
        Require(resLimit.rlim_cur <= resLimit.rlim_max);
    }

    GuestResourceLimit stackLimit{};
    Require(getrlimit_nid_postfix(RlimitStack, &stackLimit) == 0);
    Require(stackLimit.rlim_cur == 0x200000);
    Require(stackLimit.rlim_max == 0x200000);

    GuestResourceLimit nofileLimit{};
    Require(getrlimit_nid_postfix(RlimitNofile, &nofileLimit) == 0);
    Require(nofileLimit.rlim_cur == nofileLimit.rlim_max);
    Require(nofileLimit.rlim_cur > 0);
}

void TestSetrlimitResourceBoundsAndPrecedence() {
    const GuestResourceLimit validInfinity{RlimInfinity, RlimInfinity};

    const int invalidResources[] = {-1, -10, -100, RlimNlimits, RlimNlimits + 1, 100};
    for (int resource : invalidResources) {
        *__error_nid_postfix() = 0;
        Require(setrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEfault);

        *__error_nid_postfix() = 0;
        Require(setrlimit_nid_postfix(resource, &validInfinity) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
    }

    for (int resource = 0; resource < RlimNlimits; ++resource) {
        *__error_nid_postfix() = 0;
        Require(setrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEfault);
    }

    GuestResourceLimit coreBefore{};
    Require(getrlimit_nid_postfix(RlimitCore, &coreBefore) == 0);

    const GuestResourceLimit lowerCore{0, coreBefore.rlim_max};
    Require(setrlimit_nid_postfix(RlimitCore, &lowerCore) == 0);
    GuestResourceLimit coreCurrent{};
    Require(getrlimit_nid_postfix(RlimitCore, &coreCurrent) == 0);
    Require(coreCurrent.rlim_cur == 0);
    Require(coreCurrent.rlim_max == coreBefore.rlim_max);

    Require(setrlimit_nid_postfix(RlimitCore, &coreBefore) == 0);
    Require(getrlimit_nid_postfix(RlimitCore, &coreCurrent) == 0);
    Require(coreCurrent.rlim_cur == coreBefore.rlim_cur);
    Require(coreCurrent.rlim_max == coreBefore.rlim_max);

    bool threw = false;
    try {
        const GuestResourceLimit invalidStack{0x100000, 0x100000};
        setrlimit_nid_postfix(RlimitStack, &invalidStack);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

} // namespace

int main() {
    TestGetrlimitResourceBoundsAndPrecedence();
    TestSetrlimitResourceBoundsAndPrecedence();
    return 0;
}
