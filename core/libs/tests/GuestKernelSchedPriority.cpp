#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int SchedOther = 0;
constexpr int SchedFifo = 1;
constexpr int SchedRr = 2;

constexpr int SentinelErrno = 42;
constexpr int ErrnoInval = 22;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s (errno=%d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

void AuditSchedYield() {
    *__error_nid_postfix() = SentinelErrno;
    int res = sched_yield_nid_postfix();
    Require(res == 0, "sched_yield must return 0 on success");
    Require(*__error_nid_postfix() == SentinelErrno, "sched_yield success must preserve errno");
}

void AuditPriorityRanges() {
    constexpr std::array<int, 3> policies{
        SchedOther,
        SchedFifo,
        SchedRr
    };

    for (const int policy : policies) {
        *__error_nid_postfix() = SentinelErrno;
        int minPri = sched_get_priority_min_nid_postfix(policy);
        Require(minPri >= 0, "sched_get_priority_min must return a valid non-negative minimum priority");
        Require(*__error_nid_postfix() == SentinelErrno, "sched_get_priority_min success must preserve errno");

        *__error_nid_postfix() = SentinelErrno;
        int maxPri = sched_get_priority_max_nid_postfix(policy);
        Require(maxPri >= minPri, "sched_get_priority_max must return a priority >= min");
        Require(*__error_nid_postfix() == SentinelErrno, "sched_get_priority_max success must preserve errno");
    }

    constexpr std::array<int, 5> invalidPolicies{
        -1,
        -100,
        3,
        99,
        std::numeric_limits<int>::max()
    };

    for (const int policy : invalidPolicies) {
        *__error_nid_postfix() = SentinelErrno;
        Require(sched_get_priority_min_nid_postfix(policy) == -1, "sched_get_priority_min with invalid policy must return -1");
        Require(*__error_nid_postfix() == ErrnoInval, "sched_get_priority_min with invalid policy must set errno to EINVAL");

        *__error_nid_postfix() = SentinelErrno;
        Require(sched_get_priority_max_nid_postfix(policy) == -1, "sched_get_priority_max with invalid policy must return -1");
        Require(*__error_nid_postfix() == ErrnoInval, "sched_get_priority_max with invalid policy must set errno to EINVAL");
    }
}

} // namespace

int main() {
    AuditSchedYield();
    AuditPriorityRanges();
    return 0;
}
