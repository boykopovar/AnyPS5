#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int RLimitCpu = 0;
constexpr int RLimitFsize = 1;
constexpr int RLimitData = 2;
constexpr int RLimitStack = 3;
constexpr int RLimitCore = 4;
constexpr int RLimitNofile = 8;
constexpr int RLimitAs = 15;

constexpr int SentinelErrno = 42;
constexpr int ErrnoInval = 22;
constexpr int ErrnoSrch = 3;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s (errno=%d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

void AuditResourceTypes() {
    constexpr std::array<int, 7> resources{
        RLimitCpu,
        RLimitFsize,
        RLimitData,
        RLimitStack,
        RLimitCore,
        RLimitNofile,
        RLimitAs
    };

    struct rlimit oldLim;
    struct rlimit newLim;
    newLim.rlim_cur = 1024 * 1024;
    newLim.rlim_max = 2048 * 1024;

    for (const int resource : resources) {
        *__error_nid_postfix() = SentinelErrno;
        int res = prlimit_nid_postfix(0, resource, &newLim, &oldLim);
        Require(res == 0 || res == -1, "prlimit must return valid status for supported resource");
    }

    constexpr std::array<int, 6> invalidResources{
        -1,
        -10,
        16,
        100,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    };

    for (const int resource : invalidResources) {
        *__error_nid_postfix() = SentinelErrno;
        Require(prlimit_nid_postfix(0, resource, nullptr, &oldLim) == -1, "prlimit with invalid resource must fail");
        Require(*__error_nid_postfix() == ErrnoInval, "prlimit with invalid resource must set errno to EINVAL");
    }
}

void AuditPidParameterValidation() {
    constexpr std::array<int, 5> invalidPids{
        -1,
        -999,
        999999,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    };

    struct rlimit oldLim;
    for (const int pid : invalidPids) {
        *__error_nid_postfix() = SentinelErrno;
        Require(prlimit_nid_postfix(pid, RLimitNofile, nullptr, &oldLim) == -1, "prlimit with invalid pid must fail");
        Require(*__error_nid_postfix() == ErrnoSrch, "prlimit with invalid pid must set errno to ESRCH");
    }

    *__error_nid_postfix() = SentinelErrno;
    Require(prlimit_nid_postfix(0, RLimitNofile, nullptr, &oldLim) == 0, "prlimit with pid=0 must succeed");
    Require(*__error_nid_postfix() == SentinelErrno, "prlimit success must preserve errno");
}

} // namespace

int main() {
    AuditResourceTypes();
    AuditPidParameterValidation();
    return 0;
}
