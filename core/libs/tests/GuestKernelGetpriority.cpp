#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int PrioProcess = 0;
constexpr int PrioPgrp = 1;
constexpr int PrioUser = 2;

constexpr int PrioMin = -20;
constexpr int PrioMax = 20;

constexpr int ErrnoSrch = 3;
constexpr int ErrnoAcces = 13;
constexpr int ErrnoInval = 22;

constexpr int SentinelErrno = 42;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s (errno=%d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

void AuditWhichParameterBoundaries() {
    constexpr std::array<int, 8> invalidWhichValues{
        -1,
        -2,
        -100,
        std::numeric_limits<int>::min(),
        3,
        4,
        100,
        std::numeric_limits<int>::max()
    };

    for (const int which : invalidWhichValues) {
        *__error_nid_postfix() = SentinelErrno;
        Require(getpriority_nid_postfix(which, 0) == -1, "getpriority invalid which must return -1");
        Require(*__error_nid_postfix() == ErrnoInval, "getpriority invalid which must set errno to EINVAL");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(which, 0, 0) == -1, "setpriority invalid which must return -1");
        Require(*__error_nid_postfix() == ErrnoInval, "setpriority invalid which must set errno to EINVAL");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(which, -1, -5) == -1, "setpriority invalid which must return -1 regardless of who or nice");
        Require(*__error_nid_postfix() == ErrnoInval, "setpriority invalid which must prioritize EINVAL over ESRCH and EACCES");
    }

    for (const int which : {PrioProcess, PrioPgrp, PrioUser}) {
        *__error_nid_postfix() = SentinelErrno;
        Require(getpriority_nid_postfix(which, 0) == 0, "getpriority valid which with who=0 must return 0");
        Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must leave errno unmodified");
    }
}

void AuditWhoTargetResolution() {
    const int selfPid = getpid_nid_postfix();
    const int selfUid = getuid_nid_postfix();
    Require(selfPid > 0, "calling process pid must be greater than zero");

    *__error_nid_postfix() = SentinelErrno;
    Require(getpriority_nid_postfix(PrioProcess, 0) == 0, "getpriority PRIO_PROCESS who=0 must return 0");
    Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must preserve errno");
    Require(getpriority_nid_postfix(PrioProcess, selfPid) == 0, "getpriority PRIO_PROCESS who=selfPid must return 0");
    Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must preserve errno");

    *__error_nid_postfix() = SentinelErrno;
    Require(getpriority_nid_postfix(PrioPgrp, 0) == 0, "getpriority PRIO_PGRP who=0 must return 0");
    Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must preserve errno");
    Require(getpriority_nid_postfix(PrioPgrp, selfPid) == 0, "getpriority PRIO_PGRP who=selfPid must return 0");
    Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must preserve errno");

    *__error_nid_postfix() = SentinelErrno;
    Require(getpriority_nid_postfix(PrioUser, 0) == 0, "getpriority PRIO_USER who=0 must return 0");
    Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must preserve errno");
    Require(getpriority_nid_postfix(PrioUser, selfUid) == 0, "getpriority PRIO_USER who=selfUid must return 0");
    Require(*__error_nid_postfix() == SentinelErrno, "getpriority success must preserve errno");

    const std::array<int, 5> invalidPids{-1, -100, selfPid + 1, selfPid + 1000, 999999};
    for (const int badPid : invalidPids) {
        *__error_nid_postfix() = SentinelErrno;
        Require(getpriority_nid_postfix(PrioProcess, badPid) == -1, "getpriority PRIO_PROCESS badPid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "getpriority PRIO_PROCESS badPid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(PrioProcess, badPid, 0) == -1, "setpriority PRIO_PROCESS badPid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "setpriority PRIO_PROCESS badPid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(PrioProcess, badPid, -5) == -1, "setpriority PRIO_PROCESS badPid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "setpriority PRIO_PROCESS badPid must prioritize ESRCH over EACCES");

        *__error_nid_postfix() = SentinelErrno;
        Require(getpriority_nid_postfix(PrioPgrp, badPid) == -1, "getpriority PRIO_PGRP badPid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "getpriority PRIO_PGRP badPid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(PrioPgrp, badPid, 0) == -1, "setpriority PRIO_PGRP badPid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "setpriority PRIO_PGRP badPid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(PrioPgrp, badPid, -10) == -1, "setpriority PRIO_PGRP badPid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "setpriority PRIO_PGRP badPid must prioritize ESRCH over EACCES");
    }

    const std::array<int, 6> invalidUids{-1, -100, selfUid + 1, selfUid + 1000, 65534, 999999};
    for (const int badUid : invalidUids) {
        *__error_nid_postfix() = SentinelErrno;
        Require(getpriority_nid_postfix(PrioUser, badUid) == -1, "getpriority PRIO_USER badUid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "getpriority PRIO_USER badUid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(PrioUser, badUid, 0) == -1, "setpriority PRIO_USER badUid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "setpriority PRIO_USER badUid must set errno to ESRCH");

        *__error_nid_postfix() = SentinelErrno;
        Require(setpriority_nid_postfix(PrioUser, badUid, -20) == -1, "setpriority PRIO_USER badUid must return -1");
        Require(*__error_nid_postfix() == ErrnoSrch, "setpriority PRIO_USER badUid must prioritize ESRCH over EACCES");
    }
}

void AuditPriorityRangeAndNiceLimits() {
    const int selfPid = getpid_nid_postfix();
    const int selfUid = getuid_nid_postfix();

    constexpr std::array<int, 10> negativeNiceValues{
        PrioMin,
        -19,
        -15,
        -10,
        -5,
        -2,
        -1,
        -21,
        -99,
        std::numeric_limits<int>::min()
    };

    for (const int nice : negativeNiceValues) {
        for (const int who : {0, selfPid}) {
            *__error_nid_postfix() = SentinelErrno;
            Require(setpriority_nid_postfix(PrioProcess, who, nice) == -1, "setpriority PRIO_PROCESS negative nice must fail");
            Require(*__error_nid_postfix() == ErrnoAcces, "setpriority PRIO_PROCESS negative nice must set errno to EACCES");

            *__error_nid_postfix() = SentinelErrno;
            Require(setpriority_nid_postfix(PrioPgrp, who, nice) == -1, "setpriority PRIO_PGRP negative nice must fail");
            Require(*__error_nid_postfix() == ErrnoAcces, "setpriority PRIO_PGRP negative nice must set errno to EACCES");
        }

        for (const int who : {0, selfUid}) {
            *__error_nid_postfix() = SentinelErrno;
            Require(setpriority_nid_postfix(PrioUser, who, nice) == -1, "setpriority PRIO_USER negative nice must fail");
            Require(*__error_nid_postfix() == ErrnoAcces, "setpriority PRIO_USER negative nice must set errno to EACCES");
        }
    }

    Require(getpriority_nid_postfix(PrioProcess, 0) == 0, "getpriority must remain 0 after failed setpriority");
    Require(getpriority_nid_postfix(PrioPgrp, 0) == 0, "getpriority must remain 0 after failed setpriority");
    Require(getpriority_nid_postfix(PrioUser, 0) == 0, "getpriority must remain 0 after failed setpriority");

    constexpr std::array<int, 10> nonNegativeNiceValues{
        0,
        1,
        5,
        10,
        15,
        19,
        PrioMax,
        21,
        99,
        std::numeric_limits<int>::max()
    };

    for (const int nice : nonNegativeNiceValues) {
        for (const int who : {0, selfPid}) {
            *__error_nid_postfix() = SentinelErrno;
            Require(setpriority_nid_postfix(PrioProcess, who, nice) == 0, "setpriority PRIO_PROCESS non-negative nice must succeed");
            Require(*__error_nid_postfix() == SentinelErrno, "setpriority success must preserve errno");
            Require(getpriority_nid_postfix(PrioProcess, who) == 0, "getpriority must return 0");
        }
    }

    for (const int which : {PrioPgrp, PrioUser}) {
        for (const int nice : {0, 5, PrioMax}) {
            bool caught = false;
            try {
                setpriority_nid_postfix(which, 0, nice);
            } catch (const std::runtime_error&) {
                caught = true;
            }
            Require(caught, "setpriority with non-negative nice on non-process target must throw runtime_error");
        }
    }
}

void AuditErrorPrecedenceAndErrnoConvention() {
    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(-1, 999999, -20) == -1, "invalid which must fail");
    Require(*__error_nid_postfix() == ErrnoInval, "invalid which must take precedence over invalid who and negative nice");

    *__error_nid_postfix() = SentinelErrno;
    Require(getpriority_nid_postfix(-1, 999999) == -1, "invalid which must fail");
    Require(*__error_nid_postfix() == ErrnoInval, "invalid which must take precedence over invalid who");

    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(PrioProcess, 999999, -20) == -1, "invalid who must fail");
    Require(*__error_nid_postfix() == ErrnoSrch, "invalid who must take precedence over negative nice");

    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(PrioPgrp, 999999, -20) == -1, "invalid who must fail");
    Require(*__error_nid_postfix() == ErrnoSrch, "invalid who must take precedence over negative nice");

    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(PrioUser, 999999, -20) == -1, "invalid who must fail");
    Require(*__error_nid_postfix() == ErrnoSrch, "invalid who must take precedence over negative nice");

    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(PrioProcess, 0, -1) == -1, "negative nice must fail");
    Require(*__error_nid_postfix() == ErrnoAcces, "negative nice must return EACCES");

    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(PrioPgrp, 0, -1) == -1, "negative nice must fail");
    Require(*__error_nid_postfix() == ErrnoAcces, "negative nice must return EACCES");

    *__error_nid_postfix() = SentinelErrno;
    Require(setpriority_nid_postfix(PrioUser, 0, -1) == -1, "negative nice must fail");
    Require(*__error_nid_postfix() == ErrnoAcces, "negative nice must return EACCES");
}

} // namespace

int main() {
    AuditWhichParameterBoundaries();
    AuditWhoTargetResolution();
    AuditPriorityRangeAndNiceLimits();
    AuditErrorPrecedenceAndErrnoConvention();
    return 0;
}
