#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

constexpr int SentinelErrno = 42;
constexpr int ErrnoFault = 14;
constexpr int ErrnoSrch = 3;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s (errno=%d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

void AuditSchedRrGetInterval() {
    KernelTimespec tp{};
    *__error_nid_postfix() = SentinelErrno;
    int res = sched_rr_get_interval_nid_postfix(0, &tp);
    Require(res == 0, "sched_rr_get_interval must return success for pid 0");
    Require(tp.tv_sec == 0 && tp.tv_nsec == 94000000, "sched_rr_get_interval must report 94ms quantum");

    const int self = getpid_nid_postfix();
    tp = {};
    *__error_nid_postfix() = SentinelErrno;
    res = sched_rr_get_interval_nid_postfix(self, &tp);
    Require(res == 0, "sched_rr_get_interval must return success for self pid");
    Require(tp.tv_sec == 0 && tp.tv_nsec == 94000000, "sched_rr_get_interval must report 94ms quantum for self");

    constexpr std::array<int, 4> invalidPids{
        -1,
        -999,
        999999,
        std::numeric_limits<int>::max()
    };

    for (const int pid : invalidPids) {
        *__error_nid_postfix() = SentinelErrno;
        Require(sched_rr_get_interval_nid_postfix(pid, &tp) == -1, "sched_rr_get_interval with invalid pid must fail");
        Require(*__error_nid_postfix() == ErrnoSrch, "sched_rr_get_interval with invalid pid must set errno to ESRCH");
    }

    *__error_nid_postfix() = SentinelErrno;
    Require(sched_rr_get_interval_nid_postfix(-1, nullptr) == -1, "sched_rr_get_interval with invalid pid and null timespec must fail");
    Require(*__error_nid_postfix() == ErrnoSrch, "sched_rr_get_interval with invalid pid and null timespec must set errno to ESRCH");

    *__error_nid_postfix() = SentinelErrno;
    Require(sched_rr_get_interval_nid_postfix(0, nullptr) == -1, "sched_rr_get_interval with null timespec must fail");
    Require(*__error_nid_postfix() == ErrnoFault, "sched_rr_get_interval with null timespec must set errno to EFAULT");
}

} // namespace

int main() {
    AuditSchedRrGetInterval();
    return 0;
}
