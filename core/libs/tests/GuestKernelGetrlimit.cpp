#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

constexpr int GuestRlimitCpu = 0;
constexpr int GuestRlimitFsize = 1;
constexpr int GuestRlimitData = 2;
constexpr int GuestRlimitStack = 3;
constexpr int GuestRlimitCore = 4;
constexpr int GuestRlimitRss = 5;
constexpr int GuestRlimitMemlock = 6;
constexpr int GuestRlimitNproc = 7;
constexpr int GuestRlimitNofile = 8;
constexpr int GuestRlimitSbsize = 9;
constexpr int GuestRlimitVmem = 10;
constexpr int GuestRlimitNpts = 11;
constexpr int GuestRlimitSwap = 12;
constexpr int GuestRlimitKqueues = 13;
constexpr int GuestRlimitUmtxp = 14;
constexpr int GuestRlimitCount = 15;

constexpr std::int64_t GuestRlimitInfinity = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t GuestStackLimit = 0x200000;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

bool Fails(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void AuditInvalidResourceIds() {
    GuestResourceLimit limit{};
    const int invalidResources[] = {
        -1000, -100, -2, -1,
        GuestRlimitCount, 16, 999, 1000, 65535,
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::min()
    };

    for (const int resource : invalidResources) {
        *__error_nid_postfix() = 0;
        Require(Fails(getrlimit_nid_postfix(resource, &limit), GuestEinval));

        *__error_nid_postfix() = 0;
        Require(Fails(getrlimit_nid_postfix(resource, nullptr), GuestEinval));

        *__error_nid_postfix() = 0;
        Require(Fails(setrlimit_nid_postfix(resource, &limit), GuestEinval));

        *__error_nid_postfix() = 0;
        Require(Fails(setrlimit_nid_postfix(resource, nullptr), GuestEfault));
    }
}

void AuditNullPointerHandling() {
    const int validResources[] = {
        GuestRlimitCpu,
        GuestRlimitFsize,
        GuestRlimitData,
        GuestRlimitStack,
        GuestRlimitCore,
        GuestRlimitRss,
        GuestRlimitMemlock,
        GuestRlimitNproc,
        GuestRlimitNofile,
        GuestRlimitSbsize,
        GuestRlimitVmem,
        GuestRlimitNpts,
        GuestRlimitSwap,
        GuestRlimitKqueues,
        GuestRlimitUmtxp
    };

    for (const int resource : validResources) {
        *__error_nid_postfix() = 0;
        Require(Fails(getrlimit_nid_postfix(resource, nullptr), GuestEfault));

        *__error_nid_postfix() = 0;
        Require(Fails(setrlimit_nid_postfix(resource, nullptr), GuestEfault));

        auto* const invalidLimit = reinterpret_cast<GuestResourceLimit*>(static_cast<std::uintptr_t>(1));
        *__error_nid_postfix() = 0;
        Require(Fails(getrlimit_nid_postfix(resource, invalidLimit), GuestEfault));

        const auto* const constInvalidLimit = reinterpret_cast<const GuestResourceLimit*>(static_cast<std::uintptr_t>(1));
        *__error_nid_postfix() = 0;
        Require(Fails(setrlimit_nid_postfix(resource, constInvalidLimit), GuestEfault));
    }
}

void AuditResourceInvariants() {
    for (int resource = 0; resource < GuestRlimitCount; ++resource) {
        GuestResourceLimit limit{};
        *__error_nid_postfix() = 0;
        Require(getrlimit_nid_postfix(resource, &limit) == 0);
        Require(limit.rlim_cur >= 0);
        Require(limit.rlim_max >= 0);
        Require(limit.rlim_cur <= limit.rlim_max);
    }
}

void AuditStackLimit() {
    GuestResourceLimit stack{};
    Require(getrlimit_nid_postfix(GuestRlimitStack, &stack) == 0);
    Require(stack.rlim_cur == GuestStackLimit);
    Require(stack.rlim_max == GuestStackLimit);
    Require(stack.rlim_cur <= stack.rlim_max);

    Require(setrlimit_nid_postfix(GuestRlimitStack, &stack) == 0);

    const GuestResourceLimit raisedHard{GuestStackLimit, GuestStackLimit * 2};
    Require(setrlimit_nid_postfix(GuestRlimitStack, &raisedHard) == 0);

    GuestResourceLimit afterRaised{};
    Require(getrlimit_nid_postfix(GuestRlimitStack, &afterRaised) == 0);
    Require(afterRaised.rlim_cur == GuestStackLimit);
    Require(afterRaised.rlim_max == GuestStackLimit);

    const GuestResourceLimit loweredSoft{GuestStackLimit / 2, GuestStackLimit};
    bool loweredThrew = false;
    try {
        setrlimit_nid_postfix(GuestRlimitStack, &loweredSoft);
    } catch (const std::runtime_error&) {
        loweredThrew = true;
    }
    Require(loweredThrew);
}

void AuditDataLimit() {
    GuestResourceLimit data{};
    Require(getrlimit_nid_postfix(GuestRlimitData, &data) == 0);
    Require(data.rlim_cur > 0);
    Require(data.rlim_max >= data.rlim_cur);
    Require(data.rlim_cur <= data.rlim_max);

    Require(setrlimit_nid_postfix(GuestRlimitData, &data) == 0);

    if (data.rlim_max != GuestRlimitInfinity) {
        const GuestResourceLimit raisedHard{data.rlim_cur, data.rlim_max + 1};
        Require(setrlimit_nid_postfix(GuestRlimitData, &raisedHard) == 0);
    }

    const GuestResourceLimit modifiedSoft{data.rlim_cur > 1 ? data.rlim_cur - 1 : 1, data.rlim_max};
    if (modifiedSoft.rlim_cur != data.rlim_cur) {
        bool softThrew = false;
        try {
            setrlimit_nid_postfix(GuestRlimitData, &modifiedSoft);
        } catch (const std::runtime_error&) {
            softThrew = true;
        }
        Require(softThrew);
    }
}

void AuditCoreLimit() {
    GuestResourceLimit originalCore{};
    Require(getrlimit_nid_postfix(GuestRlimitCore, &originalCore) == 0);
    Require(originalCore.rlim_cur <= originalCore.rlim_max);

    const GuestResourceLimit zeroCore{0, originalCore.rlim_max};
    Require(setrlimit_nid_postfix(GuestRlimitCore, &zeroCore) == 0);

    GuestResourceLimit queryZero{};
    Require(getrlimit_nid_postfix(GuestRlimitCore, &queryZero) == 0);
    Require(queryZero.rlim_cur == 0);
    Require(queryZero.rlim_max == originalCore.rlim_max);
    Require(queryZero.rlim_cur <= queryZero.rlim_max);

    const GuestResourceLimit pageCore{4096, originalCore.rlim_max};
    Require(setrlimit_nid_postfix(GuestRlimitCore, &pageCore) == 0);

    GuestResourceLimit queryPage{};
    Require(getrlimit_nid_postfix(GuestRlimitCore, &queryPage) == 0);
    Require(queryPage.rlim_cur == 4096);
    Require(queryPage.rlim_max == originalCore.rlim_max);
    Require(queryPage.rlim_cur <= queryPage.rlim_max);

    Require(setrlimit_nid_postfix(GuestRlimitCore, &originalCore) == 0);

    GuestResourceLimit restoredCore{};
    Require(getrlimit_nid_postfix(GuestRlimitCore, &restoredCore) == 0);
    Require(restoredCore.rlim_cur == originalCore.rlim_cur);
    Require(restoredCore.rlim_max == originalCore.rlim_max);
    Require(restoredCore.rlim_cur <= restoredCore.rlim_max);
}

void AuditOpenFileLimit() {
    GuestResourceLimit originalNofile{};
    Require(getrlimit_nid_postfix(GuestRlimitNofile, &originalNofile) == 0);
    Require(originalNofile.rlim_cur > 0);
    Require(originalNofile.rlim_cur <= originalNofile.rlim_max);

    const std::int64_t targetCur = originalNofile.rlim_max > 1 ? originalNofile.rlim_max - 1 : 1;
    const GuestResourceLimit loweredNofile{targetCur, originalNofile.rlim_max};
    Require(setrlimit_nid_postfix(GuestRlimitNofile, &loweredNofile) == 0);

    GuestResourceLimit queriedNofile{};
    Require(getrlimit_nid_postfix(GuestRlimitNofile, &queriedNofile) == 0);
    Require(queriedNofile.rlim_cur == loweredNofile.rlim_cur);
    Require(queriedNofile.rlim_max == loweredNofile.rlim_max);
    Require(queriedNofile.rlim_cur <= queriedNofile.rlim_max);

    const GuestResourceLimit oversizedCur{originalNofile.rlim_max + 100, originalNofile.rlim_max};
    Require(setrlimit_nid_postfix(GuestRlimitNofile, &oversizedCur) == 0);

    GuestResourceLimit clampedNofile{};
    Require(getrlimit_nid_postfix(GuestRlimitNofile, &clampedNofile) == 0);
    Require(clampedNofile.rlim_cur == originalNofile.rlim_max);
    Require(clampedNofile.rlim_max == originalNofile.rlim_max);
    Require(clampedNofile.rlim_cur <= clampedNofile.rlim_max);

    Require(setrlimit_nid_postfix(GuestRlimitNofile, &loweredNofile) == 0);

    const GuestResourceLimit negativeCur{-1, originalNofile.rlim_max};
    Require(setrlimit_nid_postfix(GuestRlimitNofile, &negativeCur) == 0);

    GuestResourceLimit negClampedNofile{};
    Require(getrlimit_nid_postfix(GuestRlimitNofile, &negClampedNofile) == 0);
    Require(negClampedNofile.rlim_cur == originalNofile.rlim_max);
    Require(negClampedNofile.rlim_max == originalNofile.rlim_max);
    Require(negClampedNofile.rlim_cur <= negClampedNofile.rlim_max);

    Require(setrlimit_nid_postfix(GuestRlimitNofile, &originalNofile) == 0);

    GuestResourceLimit restoredNofile{};
    Require(getrlimit_nid_postfix(GuestRlimitNofile, &restoredNofile) == 0);
    Require(restoredNofile.rlim_cur == originalNofile.rlim_cur);
    Require(restoredNofile.rlim_max == originalNofile.rlim_max);
    Require(restoredNofile.rlim_cur <= restoredNofile.rlim_max);

    if (originalNofile.rlim_max != GuestRlimitInfinity) {
        const GuestResourceLimit raisedMax{originalNofile.rlim_cur, originalNofile.rlim_max + 1};
        bool raisedThrew = false;
        try {
            setrlimit_nid_postfix(GuestRlimitNofile, &raisedMax);
        } catch (const std::runtime_error&) {
            raisedThrew = true;
        }
        Require(raisedThrew);
    }
}

} // namespace

int main() {
    AuditInvalidResourceIds();
    AuditNullPointerHandling();
    AuditResourceInvariants();
    AuditStackLimit();
    AuditDataLimit();
    AuditCoreLimit();
    AuditOpenFileLimit();
    std::puts("guest_kernel_getrlimit passed");
    return 0;
}
