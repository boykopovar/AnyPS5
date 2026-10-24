#include "SceTypes.hpp"
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrSetclock(PthreadCondattr* attr, KernelClockid clockId);
int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clockId);
int APS5_VABI pthread_condattr_setpshared_nid_postfix(PthreadCondattr* attr, int pshared);
int APS5_VABI pthread_condattr_getpshared_nid_postfix(const PthreadCondattr* attr, int* pshared);
int APS5_VABI scePthreadCondattrSetpshared(PthreadCondattr* attr, int pshared);
int APS5_VABI scePthreadCondattrGetpshared(const PthreadCondattr* attr, int* pshared);
}

static constexpr int SCE_OK = 0;
static constexpr int SceEinval = static_cast<int>(0x80020016u);
static constexpr int GuestEinval = 22;
static constexpr KernelClockid Monotonic = 4;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    PthreadCondattr unset = nullptr;
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
    for (const KernelClockid clock : {0, 1, 2, Monotonic, 5, 12}) {
        Require(scePthreadCondattrSetclock(&attr, clock) == SCE_OK);
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == 0);
    }
    Require(scePthreadCondattrSetpshared(&attr, 1) == SceEinval);
    Require(scePthreadCondattrSetpshared(&attr, 0) == SCE_OK);
    Require(scePthreadCondattrGetpshared(&attr, &sharing) == SCE_OK && sharing == 0);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
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
    Require(scePthreadCondattrDestroy(&attr) == SceEinval);
    sharing = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == GuestEinval && sharing == -1);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == GuestEinval);
}
