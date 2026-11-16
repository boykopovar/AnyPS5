#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

constexpr int guestEfault = 14;
constexpr int guestEinval = 22;

constexpr int guestRlimitCpu = 0;
constexpr int guestRlimitFsize = 1;
constexpr int guestRlimitData = 2;
constexpr int guestRlimitStack = 3;
constexpr int guestRlimitCore = 4;
constexpr int guestRlimitRss = 5;
constexpr int guestRlimitMemlock = 6;
constexpr int guestRlimitNproc = 7;
constexpr int guestRlimitNofile = 8;
constexpr int guestRlimitSbsize = 9;
constexpr int guestRlimitVmem = 10;
constexpr int guestRlimitNpts = 11;
constexpr int guestRlimitSwap = 12;
constexpr int guestRlimitKqueues = 13;
constexpr int guestRlimitUmtxp = 14;
constexpr int guestRlimitCount = 15;

constexpr std::int64_t guestRlimitInfinity = std::numeric_limits<std::int64_t>::max();

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void AuditGetrlimitParameterBounds() {
    constexpr int invalidResources[] = {-100, -1, 15, 16, 100, -2147483648, 2147483647};
    const auto* unmappedConst = reinterpret_cast<const GuestResourceLimit*>(std::uintptr_t{0x10});
    auto* unmappedMut = reinterpret_cast<GuestResourceLimit*>(std::uintptr_t{0x10});
    GuestResourceLimit validLim{};

    for (const int resource : invalidResources) {
        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, &validLim) == -1);
        Require(*__error_nid_postfix() == guestEinval);

        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == guestEinval);

        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, unmappedMut) == -1);
        Require(*__error_nid_postfix() == guestEinval);
    }

    for (int resource = 0; resource < guestRlimitCount; ++resource) {
        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == guestEfault);

        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, unmappedMut) == -1);
        Require(*__error_nid_postfix() == guestEfault);
    }
}

void AuditGetrlimitValuesAndDefaults() {
    constexpr int infinityResources[] = {
        guestRlimitCpu, guestRlimitFsize, guestRlimitRss,
        guestRlimitMemlock, guestRlimitSbsize, guestRlimitVmem,
        guestRlimitNpts, guestRlimitSwap, guestRlimitKqueues, guestRlimitUmtxp
    };

    for (int resource = 0; resource < guestRlimitCount; ++resource) {
        *__error_nid_postfix() = 0x5a5a;
        GuestResourceLimit limit{-1, -1};
        Require(getrlimit_nid_postfix(resource, &limit) == 0);
        Require(*__error_nid_postfix() == 0x5a5a);
        Require(limit.rlim_cur >= 0);
        Require(limit.rlim_max >= 0);
        Require(limit.rlim_cur <= limit.rlim_max);
    }

    for (const int resource : infinityResources) {
        GuestResourceLimit limit{};
        Require(getrlimit_nid_postfix(resource, &limit) == 0);
        Require(limit.rlim_cur == guestRlimitInfinity);
        Require(limit.rlim_max == guestRlimitInfinity);
    }

    GuestResourceLimit stackLimit{};
    Require(getrlimit_nid_postfix(guestRlimitStack, &stackLimit) == 0);
    Require(stackLimit.rlim_cur == 16 * 1024 * 1024);
    Require(stackLimit.rlim_max == 16 * 1024 * 1024);
}

void AuditSetrlimitParameterBounds() {
    constexpr int testResources[] = {-100, -1, 0, 2, 3, 4, 7, 8, 14, 15, 100, -2147483648, 2147483647};
    const auto* unmappedConst = reinterpret_cast<const GuestResourceLimit*>(std::uintptr_t{0x10});

    for (const int resource : testResources) {
        *__error_nid_postfix() = 0;
        Require(setrlimit_nid_postfix(resource, nullptr) == -1);
        Require(*__error_nid_postfix() == guestEfault);

        *__error_nid_postfix() = 0;
        Require(setrlimit_nid_postfix(resource, unmappedConst) == -1);
        Require(*__error_nid_postfix() == guestEfault);
    }

    constexpr int invalidResources[] = {-100, -1, 15, 16, 100, -2147483648, 2147483647};
    const GuestResourceLimit validLimit{100, 100};
    for (const int resource : invalidResources) {
        *__error_nid_postfix() = 0;
        Require(setrlimit_nid_postfix(resource, &validLimit) == -1);
        Require(*__error_nid_postfix() == guestEinval);
    }
}

void AuditSetrlimitNofileRules() {
    GuestResourceLimit originalNofile{};
    Require(getrlimit_nid_postfix(guestRlimitNofile, &originalNofile) == 0);

    const GuestResourceLimit loweredNofile{originalNofile.rlim_cur / 2, originalNofile.rlim_max};
    *__error_nid_postfix() = 0x5a5a;
    Require(setrlimit_nid_postfix(guestRlimitNofile, &loweredNofile) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);

    GuestResourceLimit readBackNofile{};
    Require(getrlimit_nid_postfix(guestRlimitNofile, &readBackNofile) == 0);
    Require(readBackNofile.rlim_cur == loweredNofile.rlim_cur);
    Require(readBackNofile.rlim_max == loweredNofile.rlim_max);

    Require(setrlimit_nid_postfix(guestRlimitNofile, &originalNofile) == 0);
}

void AuditSetrlimitNprocRules() {
    const GuestResourceLimit zeroProc{0, 0};
    Require(setrlimit_nid_postfix(guestRlimitNproc, &zeroProc) == 0);

    GuestResourceLimit clampedMin{};
    Require(getrlimit_nid_postfix(guestRlimitNproc, &clampedMin) == 0);
    Require(clampedMin.rlim_cur == 1);
    Require(clampedMin.rlim_max == 1);

    const GuestResourceLimit infProc{guestRlimitInfinity, guestRlimitInfinity};
    Require(setrlimit_nid_postfix(guestRlimitNproc, &infProc) == 0);

    GuestResourceLimit clampedMax{};
    Require(getrlimit_nid_postfix(guestRlimitNproc, &clampedMax) == 0);
    Require(clampedMax.rlim_cur == 1443);
    Require(clampedMax.rlim_max == 1443);
}

void AuditSetrlimitCoreRules() {
    GuestResourceLimit originalCore{};
    Require(getrlimit_nid_postfix(guestRlimitCore, &originalCore) == 0);

    const GuestResourceLimit newCoreSoft{4096, originalCore.rlim_max};
    *__error_nid_postfix() = 0x5a5a;
    Require(setrlimit_nid_postfix(guestRlimitCore, &newCoreSoft) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);

    GuestResourceLimit readBackCore{};
    Require(getrlimit_nid_postfix(guestRlimitCore, &readBackCore) == 0);
    Require(readBackCore.rlim_cur == 4096);
    Require(readBackCore.rlim_max == originalCore.rlim_max);

    Require(setrlimit_nid_postfix(guestRlimitCore, &originalCore) == 0);
}

void AuditSetrlimitDataStackRules() {
    GuestResourceLimit dataLimit{};
    Require(getrlimit_nid_postfix(guestRlimitData, &dataLimit) == 0);
    *__error_nid_postfix() = 0x5a5a;
    Require(setrlimit_nid_postfix(guestRlimitData, &dataLimit) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);

    GuestResourceLimit stackLimit{};
    Require(getrlimit_nid_postfix(guestRlimitStack, &stackLimit) == 0);
    *__error_nid_postfix() = 0x5a5a;
    Require(setrlimit_nid_postfix(guestRlimitStack, &stackLimit) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);

    if (dataLimit.rlim_max < guestRlimitInfinity) {
        const GuestResourceLimit raisedData{dataLimit.rlim_cur, dataLimit.rlim_max + 1};
        Require(setrlimit_nid_postfix(guestRlimitData, &raisedData) == 0);
    }
}

void AuditSetrlimitOtherResourcesRules() {
    constexpr int otherResources[] = {
        guestRlimitCpu, guestRlimitFsize, guestRlimitRss,
        guestRlimitMemlock, guestRlimitSbsize, guestRlimitVmem,
        guestRlimitNpts, guestRlimitSwap, guestRlimitKqueues, guestRlimitUmtxp
    };

    for (const int resource : otherResources) {
        GuestResourceLimit current{};
        Require(getrlimit_nid_postfix(resource, &current) == 0);
        *__error_nid_postfix() = 0x5a5a;
        Require(setrlimit_nid_postfix(resource, &current) == 0);
        Require(*__error_nid_postfix() == 0x5a5a);
    }
}

} // namespace

int main() {
    AuditGetrlimitParameterBounds();
    AuditGetrlimitValuesAndDefaults();
    AuditSetrlimitParameterBounds();
    AuditSetrlimitNofileRules();
    AuditSetrlimitNprocRules();
    AuditSetrlimitCoreRules();
    AuditSetrlimitDataStackRules();
    AuditSetrlimitOtherResourcesRules();
    return 0;
}
