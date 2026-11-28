#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EACCES = 13;
static constexpr int GUEST_EINVAL = 22;

static constexpr int PRIO_PROCESS = 0;
static constexpr int PRIO_PGRP = 1;
static constexpr int PRIO_USER = 2;

static void Require(bool value) {
    if (!value) std::abort();
}

static void TestWhichParameterBounds() {
    const int invalidWhich[] = {-1, 3, 4, 100, -100, INT32_MIN, INT32_MAX};

    for (const int which : invalidWhich) {
        *__error_nid_postfix() = GUEST_EACCES;
        Require(getpriority_nid_postfix(which, 0) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(getpriority_nid_postfix(which, 9999) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(which, 0, 0) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(which, 9999, -5) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(which, 0, 20) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
    }
}

static void TestWhoParameterBounds() {
    const int self = getpid_nid_postfix();
    const int uid = getuid_nid_postfix();

    *__error_nid_postfix() = GUEST_EACCES;
    Require(getpriority_nid_postfix(PRIO_PROCESS, 0) == 0);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(getpriority_nid_postfix(PRIO_PROCESS, self) == 0);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(getpriority_nid_postfix(PRIO_PGRP, 0) == 0);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(getpriority_nid_postfix(PRIO_PGRP, self) == 0);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(getpriority_nid_postfix(PRIO_USER, 0) == 0);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(getpriority_nid_postfix(PRIO_USER, uid) == 0);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    const int invalidProcessWho[] = {self + 1, self + 100, 9999, -1, -100, INT32_MIN, INT32_MAX};
    for (const int who : invalidProcessWho) {
        *__error_nid_postfix() = GUEST_EACCES;
        Require(getpriority_nid_postfix(PRIO_PROCESS, who) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_PROCESS, who, 0) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_PROCESS, who, -5) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);
    }

    const int invalidGroupWho[] = {self + 1, self + 100, 9999, -1, -100, INT32_MIN, INT32_MAX};
    for (const int who : invalidGroupWho) {
        *__error_nid_postfix() = GUEST_EACCES;
        Require(getpriority_nid_postfix(PRIO_PGRP, who) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_PGRP, who, 0) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_PGRP, who, -5) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);
    }

    const int invalidUserWho[] = {uid == 0 ? 1000 : 0, uid + 100, 9999, -1, -100, INT32_MIN, INT32_MAX};
    for (const int who : invalidUserWho) {
        if (who == 0 || who == uid) continue;
        *__error_nid_postfix() = GUEST_EACCES;
        Require(getpriority_nid_postfix(PRIO_USER, who) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_USER, who, 0) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_USER, who, -5) == -1);
        Require(*__error_nid_postfix() == GUEST_ESRCH);
    }
}

static void TestPriorityClampingAndNiceBounds() {
    const int self = getpid_nid_postfix();

    const int negativeNice[] = {-20, -19, -15, -10, -5, -2, -1, -25, -99, INT32_MIN};
    for (const int nice : negativeNice) {
        *__error_nid_postfix() = 0;
        Require(setpriority_nid_postfix(PRIO_PROCESS, 0, nice) == -1);
        Require(*__error_nid_postfix() == GUEST_EACCES);

        *__error_nid_postfix() = 0;
        Require(setpriority_nid_postfix(PRIO_PROCESS, self, nice) == -1);
        Require(*__error_nid_postfix() == GUEST_EACCES);

        *__error_nid_postfix() = 0;
        Require(setpriority_nid_postfix(PRIO_PGRP, 0, nice) == -1);
        Require(*__error_nid_postfix() == GUEST_EACCES);

        *__error_nid_postfix() = 0;
        Require(setpriority_nid_postfix(PRIO_USER, 0, nice) == -1);
        Require(*__error_nid_postfix() == GUEST_EACCES);
    }

    const int nonNegativeNice[] = {0, 1, 2, 5, 10, 15, 19, 20, 25, 99, INT32_MAX};
    for (const int nice : nonNegativeNice) {
        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_PROCESS, 0, nice) == 0);
        Require(*__error_nid_postfix() == GUEST_EACCES);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(setpriority_nid_postfix(PRIO_PROCESS, self, nice) == 0);
        Require(*__error_nid_postfix() == GUEST_EACCES);

        Require(getpriority_nid_postfix(PRIO_PROCESS, 0) == 0);
        Require(getpriority_nid_postfix(PRIO_PROCESS, self) == 0);
    }

    bool groupThrew = false;
    try {
        setpriority_nid_postfix(PRIO_PGRP, 0, 0);
    } catch (const std::runtime_error&) {
        groupThrew = true;
    }
    Require(groupThrew);

    bool userThrew = false;
    try {
        setpriority_nid_postfix(PRIO_USER, 0, 0);
    } catch (const std::runtime_error&) {
        userThrew = true;
    }
    Require(userThrew);
}

int main() {
    TestWhichParameterBounds();
    TestWhoParameterBounds();
    TestPriorityClampingAndNiceBounds();
    return 0;
}
