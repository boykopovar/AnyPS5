#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr int ProcessPrivate = 0;

constexpr KernelClockid ClockRealtime = 0;
constexpr KernelClockid ClockVirtual = 1;
constexpr KernelClockid ClockProf = 2;
constexpr KernelClockid ClockMonotonic = 4;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCondattrLifecycleAndDefaults() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
    Require(clockId == ClockRealtime);

    int pshared = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    pshared = -1;
    Require(scePthreadCondattrGetpshared(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestClockValidationAndBounds() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    const KernelClockid validClocks[] = {ClockRealtime, ClockVirtual, ClockProf, ClockMonotonic};
    for (const auto clock : validClocks) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == 0);
        KernelClockid readBack = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &readBack) == 0);
        Require(readBack == clock);
    }

    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockMonotonic) == 0);

    const KernelClockid invalidClocks[] = {-100, -1, 3, 5, 6, 7, 8, 9, 10, 13, 100, 9999};
    for (const auto clock : invalidClocks) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == PosixEinval);
        KernelClockid readBack = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &readBack) == 0);
        Require(readBack == ClockMonotonic);
    }

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestPsharedValidationAndForeignRejection() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_condattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);
    Require(scePthreadCondattrSetpshared(&attr, ProcessPrivate) == 0);

    const int foreignPsharedValues[] = {-1000, -50, -1, 1, 2, 42, 100, 9999};
    for (const int foreignValue : foreignPsharedValues) {
        Require(pthread_condattr_setpshared_nid_postfix(&attr, foreignValue) == PosixEinval);
        Require(scePthreadCondattrSetpshared(&attr, foreignValue) == SceKernelErrorEinval);

        int pshared = -1;
        Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0);
        Require(pshared == ProcessPrivate);
    }

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestNullAndUninitializedAttributeHandling() {
    KernelClockid clockId = -1;
    int pshared = -1;

    Require(pthread_condattr_init_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, &clockId) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(nullptr, ClockRealtime) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(nullptr, ClockMonotonic) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(nullptr, -1) == PosixEinval);
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, &pshared) == PosixEinval);
    Require(pthread_condattr_setpshared_nid_postfix(nullptr, ProcessPrivate) == PosixEinval);
    Require(scePthreadCondattrGetpshared(nullptr, &pshared) == SceKernelErrorEinval);
    Require(scePthreadCondattrSetpshared(nullptr, ProcessPrivate) == SceKernelErrorEinval);

    PthreadCondattr uninit = nullptr;
    Require(pthread_condattr_destroy_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_condattr_getclock_nid_postfix(&uninit, &clockId) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&uninit, ClockRealtime) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&uninit, ClockMonotonic) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&uninit, -1) == PosixEinval);
    Require(pthread_condattr_getpshared_nid_postfix(&uninit, &pshared) == PosixEinval);
    Require(pthread_condattr_setpshared_nid_postfix(&uninit, ProcessPrivate) == PosixEinval);
    Require(scePthreadCondattrGetpshared(&uninit, &pshared) == SceKernelErrorEinval);
    Require(scePthreadCondattrSetpshared(&uninit, ProcessPrivate) == SceKernelErrorEinval);
}

void TestDestroyedAttributePrecedence() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    KernelClockid clockId = -1;
    int pshared = -1;

    Require(pthread_condattr_destroy_nid_postfix(&attr) == PosixEinval);
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockRealtime) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockMonotonic) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&attr, -1) == PosixEinval);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == PosixEinval);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, ProcessPrivate) == PosixEinval);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 1) == PosixEinval);
    Require(scePthreadCondattrGetpshared(&attr, &pshared) == SceKernelErrorEinval);
    Require(scePthreadCondattrSetpshared(&attr, ProcessPrivate) == SceKernelErrorEinval);
}

void TestFaultPointerPrecedence() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_condattr_getclock_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadCondattrGetpshared(&attr, nullptr) == SceKernelErrorEfault);

    Require(pthread_condattr_getclock_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(scePthreadCondattrGetpshared(nullptr, nullptr) == SceKernelErrorEinval);

    PthreadCondattr uninit = nullptr;
    Require(pthread_condattr_getclock_nid_postfix(&uninit, nullptr) == PosixEinval);
    Require(pthread_condattr_getpshared_nid_postfix(&uninit, nullptr) == PosixEinval);
    Require(scePthreadCondattrGetpshared(&uninit, nullptr) == SceKernelErrorEinval);

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    Require(pthread_condattr_getclock_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(scePthreadCondattrGetpshared(&attr, nullptr) == SceKernelErrorEinval);
}

void TestCondInitWithAttribute() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockMonotonic) == 0);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, &attr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);

    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

} // namespace

int main() {
    TestCondattrLifecycleAndDefaults();
    TestClockValidationAndBounds();
    TestPsharedValidationAndForeignRejection();
    TestNullAndUninitializedAttributeHandling();
    TestDestroyedAttributePrecedence();
    TestFaultPointerPrecedence();
    TestCondInitWithAttribute();
    return 0;
}
