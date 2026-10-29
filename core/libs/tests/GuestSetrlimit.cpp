#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_RLIMIT_DATA = 2;
static constexpr int GUEST_RLIMIT_STACK = 3;
static constexpr int GUEST_RLIMIT_CORE = 4;
static constexpr int GUEST_RLIMIT_RSS = 5;
static constexpr int GUEST_RLIMIT_MEMLOCK = 6;
static constexpr int GUEST_RLIMIT_NPROC = 7;
static constexpr int GUEST_RLIMIT_NOFILE = 8;
static constexpr int GUEST_RLIM_NLIMITS = 15;
static constexpr std::int64_t GUEST_RLIM_INFINITY = std::numeric_limits<std::int64_t>::max();

static void Require(bool value) { if (!value) std::abort(); }

static bool Throws(int resource, const GuestResourceLimit& limit) {
    try {
        setrlimit_nid_postfix(resource, &limit);

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void RequireLimit(int resource, std::int64_t cur, std::int64_t max) {
    GuestResourceLimit limit{};
    Require(getrlimit_nid_postfix(resource, &limit) == 0);
    Require(limit.rlim_cur == cur && limit.rlim_max == max);
}

static void RequireSoftLimitRoundTrip(int resource, std::int64_t lowered) {
    const GuestResourceLimit lowerSoft{lowered, GUEST_RLIM_INFINITY};
    Require(setrlimit_nid_postfix(resource, &lowerSoft) == 0);
    RequireLimit(resource, lowered, GUEST_RLIM_INFINITY);
    const GuestResourceLimit infinity{GUEST_RLIM_INFINITY, GUEST_RLIM_INFINITY};
    Require(setrlimit_nid_postfix(resource, &infinity) == 0);
    RequireLimit(resource, GUEST_RLIM_INFINITY, GUEST_RLIM_INFINITY);
}

static void RequireHardRaiseKeepsLimit(int resource, std::int64_t raisedMax) {
    GuestResourceLimit before{};
    Require(getrlimit_nid_postfix(resource, &before) == 0);
    Require(before.rlim_max < raisedMax);
    const GuestResourceLimit raised{before.rlim_cur, raisedMax};
    *__error_nid_postfix() = 13;
    Require(setrlimit_nid_postfix(resource, &raised) == 0 && *__error_nid_postfix() == 13);
    RequireLimit(resource, before.rlim_cur, before.rlim_max);
}

static void RequireError(int resource, const GuestResourceLimit* limit, int error) {
    *__error_nid_postfix() = 0;
    Require(setrlimit_nid_postfix(resource, limit) == -1);
    Require(*__error_nid_postfix() == error);
}

int main() {
    const GuestResourceLimit infinity{GUEST_RLIM_INFINITY, GUEST_RLIM_INFINITY};
    RequireError(-1, nullptr, GUEST_EFAULT);
    RequireError(GUEST_RLIM_NLIMITS, nullptr, GUEST_EFAULT);
    RequireError(-1, &infinity, GUEST_EINVAL);
    RequireError(GUEST_RLIM_NLIMITS, &infinity, GUEST_EINVAL);

    static constexpr int unchanged[] = {GUEST_RLIMIT_STACK, GUEST_RLIMIT_RSS, GUEST_RLIMIT_MEMLOCK};
    for (int resource : unchanged) {
        GuestResourceLimit before{};
        Require(getrlimit_nid_postfix(resource, &before) == 0);
        *__error_nid_postfix() = 13;
        Require(setrlimit_nid_postfix(resource, &before) == 0 && *__error_nid_postfix() == 13);
        GuestResourceLimit after{};
        Require(getrlimit_nid_postfix(resource, &after) == 0 && after.rlim_cur == before.rlim_cur && after.rlim_max == before.rlim_max);
    }
    GuestResourceLimit processes{};
    Require(getrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &processes) == 0 && processes.rlim_cur == 1604 && processes.rlim_max == 1604);
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &processes) == 0);
    Require(getrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &processes) == 0 && processes.rlim_cur == 1443 && processes.rlim_max == 1443);
    const GuestResourceLimit noProcesses{0, 0};
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &noProcesses) == 0);
    Require(getrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &processes) == 0 && processes.rlim_cur == 1 && processes.rlim_max == 1);
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &infinity) == 0);
    Require(getrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &processes) == 0 && processes.rlim_cur == 1443 && processes.rlim_max == 1443);
    Require(Throws(GUEST_RLIMIT_STACK, GuestResourceLimit{0x100000, 0x200000}));

    GuestResourceLimit current{};
    Require(getrlimit_nid_postfix(GUEST_RLIMIT_DATA, &current) == 0);
    *__error_nid_postfix() = 13;
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_DATA, &current) == 0);
    Require(*__error_nid_postfix() == 13);

    if (current.rlim_cur > 0) {
        const GuestResourceLimit changed{current.rlim_cur - 1, current.rlim_max};
        Require(Throws(GUEST_RLIMIT_DATA, changed));
    }
    const GuestResourceLimit softAboveHard{GUEST_RLIM_INFINITY, current.rlim_max};
    if (current.rlim_cur == current.rlim_max)
        Require(setrlimit_nid_postfix(GUEST_RLIMIT_DATA, &softAboveHard) == 0);
    else
        Require(Throws(GUEST_RLIMIT_DATA, softAboveHard));

    const GuestResourceLimit negative{-1, -2};
    if (current.rlim_cur == GUEST_RLIM_INFINITY && current.rlim_max == GUEST_RLIM_INFINITY)
        Require(setrlimit_nid_postfix(GUEST_RLIMIT_DATA, &negative) == 0);
    else
        Require(Throws(GUEST_RLIMIT_DATA, negative));

    Require(Throws(GUEST_RLIMIT_NOFILE, infinity));

    RequireSoftLimitRoundTrip(GUEST_RLIMIT_CORE, 0);
    RequireSoftLimitRoundTrip(GUEST_RLIMIT_CORE, 4096);
    RequireHardRaiseKeepsLimit(GUEST_RLIMIT_STACK, 0x200001);
    RequireHardRaiseKeepsLimit(GUEST_RLIMIT_STACK, GUEST_RLIM_INFINITY);
    if (current.rlim_max != GUEST_RLIM_INFINITY)
        RequireHardRaiseKeepsLimit(GUEST_RLIMIT_DATA, current.rlim_max + 1);
    return 0;
}
