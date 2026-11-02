#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SceEinval = static_cast<int>(0x80020016u);
static constexpr int GuestEinval = 22;
static constexpr KernelClockid Monotonic = 4;

static void Require(bool value) { if (!value) std::abort(); }

static void CheckClock(const PthreadCondattr* attr, KernelClockid expected) {
    const auto handle = *attr;
    KernelClockid output[] = {0x12345678, -1, 0x23456789};
    *__error_nid_postfix() = 123;
    Require(pthread_condattr_getclock_nid_postfix(attr, &output[1]) == 0);
    Require(output[0] == 0x12345678 && output[1] == expected && output[2] == 0x23456789);
    Require(*attr == handle && *__error_nid_postfix() == 123);
}

static void CheckClockRefused(const PthreadCondattr* attr, KernelClockid* clockId) {
    bool threw = false;
    try {
        pthread_condattr_getclock_nid_postfix(attr, clockId);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    PthreadCondattr unset = nullptr;
    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(nullptr, &clockId) == GuestEinval && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(&unset, &clockId) == GuestEinval && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, nullptr) == GuestEinval);
    Require(pthread_condattr_getclock_nid_postfix(&unset, nullptr) == GuestEinval);
    int sharing = -1;
    Require(scePthreadCondattrSetclock(nullptr, Monotonic) == SceEinval);
    Require(scePthreadCondattrSetclock(&unset, Monotonic) == SceEinval);
    Require(scePthreadCondattrDestroy(nullptr) == SceEinval);
    Require(scePthreadCondattrDestroy(&unset) == SceEinval);
    Require(pthread_condattr_setclock_nid_postfix(&unset, Monotonic) == GuestEinval);
    Require(pthread_condattr_setpshared_nid_postfix(nullptr, 0) == GuestEinval);
    Require(pthread_condattr_setpshared_nid_postfix(&unset, 0) == GuestEinval);
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, &sharing) == GuestEinval && sharing == -1);
    Require(pthread_condattr_getpshared_nid_postfix(&unset, &sharing) == GuestEinval && sharing == -1);
    Require(scePthreadCondattrSetpshared(&unset, 0) == SceEinval);
    Require(scePthreadCondattrGetpshared(nullptr, &sharing) == SceEinval && sharing == -1);

    PthreadCondattr attr = nullptr;
    Require(scePthreadCondattrInit(&attr) == SCE_OK);
    CheckClock(&attr, 0);
    CheckClockRefused(&attr, nullptr);
    CheckClock(&attr, 0);
    PthreadCondattr other = nullptr;
    Require(pthread_condattr_init_nid_postfix(&other) == 0);
    for (const KernelClockid clock : {0, 1, 2, Monotonic}) {
        Require(scePthreadCondattrSetclock(&attr, clock) == SCE_OK);
        CheckClock(&attr, clock);
        CheckClock(&other, 0);
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == 0);
        CheckClock(&attr, clock);
    }
    Require(pthread_condattr_setclock_nid_postfix(&other, Monotonic) == 0);
    CheckClock(&other, Monotonic);
    CheckClock(&attr, Monotonic);
    Require(pthread_condattr_destroy_nid_postfix(&other) == 0 && other == nullptr);
    Require(pthread_condattr_getclock_nid_postfix(&other, &clockId) == GuestEinval && clockId == -1);
    Require(scePthreadCondattrSetpshared(&attr, 1) == SceEinval);
    Require(scePthreadCondattrSetpshared(&attr, 0) == SCE_OK);
    Require(scePthreadCondattrGetpshared(&attr, &sharing) == SCE_OK && sharing == 0);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    for (const KernelClockid clock : {-1, 3, 5, 12, 13, 14, 15}) {
        Require(scePthreadCondattrSetclock(&attr, clock) == SceEinval);
        CheckClock(&attr, Monotonic);
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == GuestEinval);
        CheckClock(&attr, Monotonic);
    }
    for (const int invalid : {1, 2, -1}) {
        Require(pthread_condattr_setpshared_nid_postfix(&attr, invalid) == GuestEinval);
        sharing = -1;
        Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    }
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == 0);
    sharing = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    bool threw = false;
    try {
        pthread_condattr_getpshared_nid_postfix(&attr, nullptr);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(scePthreadCondattrDestroy(&attr) == SCE_OK && attr == nullptr);
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == GuestEinval && clockId == -1);
    Require(scePthreadCondattrDestroy(&attr) == SceEinval);
    sharing = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == GuestEinval && sharing == -1);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == GuestEinval);
}
