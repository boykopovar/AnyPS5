#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int SentinelErrno = 42;
constexpr int ErrnoInval = 22;
constexpr int ErrnoSrch = 3;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s (errno=%d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

void AuditSchedGetSetParam() {
    struct sched_param param;
    *__error_nid_postfix() = SentinelErrno;
    int res = sched_getparam_nid_postfix(0, &param);
    Require(res == 0 || res == -1, "sched_getparam must return valid status");

    *__error_nid_postfix() = SentinelErrno;
    param.sched_priority = 10;
    int setRes = sched_setparam_nid_postfix(0, &param);
    Require(setRes == 0 || setRes == -1, "sched_setparam must return valid status");

    constexpr std::array<int, 4> invalidPids{
        -1,
        -999,
        999999,
        std::numeric_limits<int>::max()
    };

    for (const int pid : invalidPids) {
        *__error_nid_postfix() = SentinelErrno;
        Require(sched_getparam_nid_postfix(pid, &param) == -1, "sched_getparam with invalid pid must fail");
        Require(*__error_nid_postfix() == ErrnoSrch, "sched_getparam with invalid pid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(sched_setparam_nid_postfix(pid, &param) == -1, "sched_setparam with invalid pid must fail");
        Require(*__error_nid_postfix() == ErrnoSrch, "sched_setparam with invalid pid must set errno to ESRCH");
    }

    *__error_nid_postfix() = SentinelErrno;
    Require(sched_getparam_nid_postfix(0, nullptr) == -1, "sched_getparam with null param must fail");
    Require(*__error_nid_postfix() == ErrnoInval, "sched_getparam with null param must set errno to EINVAL");

    *__error_nid_postfix() = SentinelErrno;
    Require(sched_setparam_nid_postfix(0, nullptr) == -1, "sched_setparam with null param must fail");
    Require(*__error_nid_postfix() == ErrnoInval, "sched_setparam with null param must set errno to EINVAL");
}

} // namespace

int main() {
    AuditSchedGetSetParam();
    return 0;
}
