#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SCE_OK = 0;
constexpr int POSIX_EINVAL = 22;
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
constexpr KernelClockid CLOCK_REALTIME_ID = 0;
constexpr KernelClockid CLOCK_VIRTUAL_ID = 1;
constexpr KernelClockid CLOCK_PROF_ID = 2;
constexpr KernelClockid CLOCK_MONOTONIC_ID = 4;

constexpr std::array<KernelClockid, 4> VALID_CLOCKS = {
    CLOCK_REALTIME_ID,
    CLOCK_VIRTUAL_ID,
    CLOCK_PROF_ID,
    CLOCK_MONOTONIC_ID
};

constexpr std::array<KernelClockid, 24> INVALID_CLOCKS = {
    -2147483647 - 1,
    -99999,
    -10000,
    -1000,
    -100,
    -50,
    -10,
    -4,
    -3,
    -2,
    -1,
    3,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    2147483647
};

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestCondattrSetclockNullAndUnset() {
    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(nullptr, &clockId) == POSIX_EINVAL && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);

    PthreadCondattr unset = nullptr;
    Require(pthread_condattr_getclock_nid_postfix(&unset, &clockId) == POSIX_EINVAL && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(&unset, nullptr) == POSIX_EINVAL);

    for (const KernelClockid validClock : VALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(nullptr, validClock) == POSIX_EINVAL);
        Require(pthread_condattr_setclock_nid_postfix(&unset, validClock) == POSIX_EINVAL);
        Require(scePthreadCondattrSetclock(nullptr, validClock) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadCondattrSetclock(&unset, validClock) == SCE_KERNEL_ERROR_EINVAL);
    }

    for (const KernelClockid invalidClock : INVALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(nullptr, invalidClock) == POSIX_EINVAL);
        Require(pthread_condattr_setclock_nid_postfix(&unset, invalidClock) == POSIX_EINVAL);
        Require(scePthreadCondattrSetclock(nullptr, invalidClock) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadCondattrSetclock(&unset, invalidClock) == SCE_KERNEL_ERROR_EINVAL);
    }
}

void TestCondattrSetclockInvalidClockIds() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == SCE_OK);
    Require(attr != nullptr);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == CLOCK_REALTIME_ID);

    for (const KernelClockid invalidClock : INVALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, invalidClock) == POSIX_EINVAL);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == CLOCK_REALTIME_ID);

        Require(scePthreadCondattrSetclock(&attr, invalidClock) == SCE_KERNEL_ERROR_EINVAL);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == CLOCK_REALTIME_ID);
    }

    Require(pthread_condattr_setclock_nid_postfix(&attr, CLOCK_MONOTONIC_ID) == SCE_OK);
    clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == CLOCK_MONOTONIC_ID);

    for (const KernelClockid invalidClock : INVALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, invalidClock) == POSIX_EINVAL);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == CLOCK_MONOTONIC_ID);

        Require(scePthreadCondattrSetclock(&attr, invalidClock) == SCE_KERNEL_ERROR_EINVAL);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == CLOCK_MONOTONIC_ID);
    }

    bool threw = false;
    try {
        pthread_condattr_getclock_nid_postfix(&attr, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    Require(pthread_condattr_destroy_nid_postfix(&attr) == SCE_OK);
    Require(attr == nullptr);
}

void TestCondattrSetclockValidTransitions() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == SCE_OK);
    Require(attr != nullptr);

    for (const KernelClockid clock : VALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == SCE_OK);
        KernelClockid clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == clock);

        Require(pthread_condattr_setclock_nid_postfix(&attr, -1) == POSIX_EINVAL);
        Require(pthread_condattr_setclock_nid_postfix(&attr, 9999) == POSIX_EINVAL);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == clock);

        Require(scePthreadCondattrSetclock(&attr, clock) == SCE_OK);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == clock);

        Require(scePthreadCondattrSetclock(&attr, -1) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadCondattrSetclock(&attr, 9999) == SCE_KERNEL_ERROR_EINVAL);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == SCE_OK && clockId == clock);
    }

    Require(pthread_condattr_destroy_nid_postfix(&attr) == SCE_OK);
    Require(attr == nullptr);
}

void TestCondattrSetclockDestroyed() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == SCE_OK);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == SCE_OK);
    Require(attr == nullptr);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == POSIX_EINVAL && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(&attr, nullptr) == POSIX_EINVAL);

    for (const KernelClockid validClock : VALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, validClock) == POSIX_EINVAL);
        Require(scePthreadCondattrSetclock(&attr, validClock) == SCE_KERNEL_ERROR_EINVAL);
    }

    for (const KernelClockid invalidClock : INVALID_CLOCKS) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, invalidClock) == POSIX_EINVAL);
        Require(scePthreadCondattrSetclock(&attr, invalidClock) == SCE_KERNEL_ERROR_EINVAL);
    }
}

}

int main() {
    TestCondattrSetclockNullAndUnset();
    TestCondattrSetclockInvalidClockIds();
    TestCondattrSetclockValidTransitions();
    TestCondattrSetclockDestroyed();
    return 0;
}
