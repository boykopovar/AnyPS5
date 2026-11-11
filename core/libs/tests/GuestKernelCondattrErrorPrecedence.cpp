#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestEfault = 14;
constexpr int SceEinval = static_cast<int>(0x80020016u);
constexpr int SceEfault = static_cast<int>(0x8002000eu);
constexpr KernelClockid ClockRealtime = 0;
constexpr KernelClockid ClockVirtual = 1;
constexpr KernelClockid ClockProf = 2;
constexpr KernelClockid ClockMonotonic = 4;
constexpr std::size_t PageSize = 0x4000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestNullAttributePointer() {
    Require(pthread_condattr_init_nid_postfix(nullptr) == GuestEinval);
    Require(scePthreadCondattrInit(nullptr) == SceEfault);

    Require(pthread_condattr_destroy_nid_postfix(nullptr) == GuestEinval);
    Require(scePthreadCondattrDestroy(nullptr) == SceEinval);

    Require(pthread_condattr_setclock_nid_postfix(nullptr, ClockRealtime) == GuestEinval);
    Require(scePthreadCondattrSetclock(nullptr, ClockRealtime) == SceEinval);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(nullptr, &clockId) == GuestEinval);
    Require(clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, nullptr) == GuestEinval);

    Require(pthread_condattr_setpshared_nid_postfix(nullptr, 0) == GuestEinval);
    Require(scePthreadCondattrSetpshared(nullptr, 0) == SceEinval);

    int pshared = -1;
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, &pshared) == GuestEinval);
    Require(pshared == -1);
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, nullptr) == GuestEinval);

    Require(scePthreadCondattrGetpshared(nullptr, &pshared) == SceEinval);
    Require(pshared == -1);
    Require(scePthreadCondattrGetpshared(nullptr, nullptr) == SceEinval);
}

void TestNullAttributeHandle() {
    PthreadCondattr unset = nullptr;

    Require(pthread_condattr_destroy_nid_postfix(&unset) == GuestEinval);
    Require(scePthreadCondattrDestroy(&unset) == SceEinval);

    Require(pthread_condattr_setclock_nid_postfix(&unset, ClockRealtime) == GuestEinval);
    Require(scePthreadCondattrSetclock(&unset, ClockRealtime) == SceEinval);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&unset, &clockId) == GuestEinval);
    Require(clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(&unset, nullptr) == GuestEinval);

    Require(pthread_condattr_setpshared_nid_postfix(&unset, 0) == GuestEinval);
    Require(scePthreadCondattrSetpshared(&unset, 0) == SceEinval);

    int pshared = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&unset, &pshared) == GuestEinval);
    Require(pshared == -1);
    Require(pthread_condattr_getpshared_nid_postfix(&unset, nullptr) == GuestEinval);

    Require(scePthreadCondattrGetpshared(&unset, &pshared) == SceEinval);
    Require(pshared == -1);
    Require(scePthreadCondattrGetpshared(&unset, nullptr) == SceEinval);
}

void TestUnmappedAndUnwritablePointers() {
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, PageSize * 2, 3, 0) == 0 && memory != nullptr);
    auto* base = static_cast<char*>(memory);
    Require(sceKernelMprotect(base + PageSize, PageSize, 0) == 0);
    void* inaccessible = base + PageSize;

    auto* badAttr = static_cast<PthreadCondattr*>(inaccessible);
    auto* badClockOut = static_cast<KernelClockid*>(inaccessible);
    auto* badSharedOut = static_cast<int*>(inaccessible);

    Require(pthread_condattr_init_nid_postfix(badAttr) == GuestEfault);
    Require(scePthreadCondattrInit(badAttr) == SceEfault);

    Require(pthread_condattr_destroy_nid_postfix(badAttr) == GuestEfault);
    Require(scePthreadCondattrDestroy(badAttr) == SceEfault);

    Require(pthread_condattr_setclock_nid_postfix(badAttr, ClockMonotonic) == GuestEfault);
    Require(scePthreadCondattrSetclock(badAttr, ClockMonotonic) == SceEfault);

    KernelClockid dummyClock = 0;
    Require(pthread_condattr_getclock_nid_postfix(badAttr, &dummyClock) == GuestEfault);
    Require(pthread_condattr_getclock_nid_postfix(badAttr, nullptr) == GuestEfault);
    Require(pthread_condattr_getclock_nid_postfix(badAttr, badClockOut) == GuestEfault);

    Require(pthread_condattr_setpshared_nid_postfix(badAttr, 0) == GuestEfault);
    Require(scePthreadCondattrSetpshared(badAttr, 0) == SceEfault);

    int dummyShared = 0;
    Require(pthread_condattr_getpshared_nid_postfix(badAttr, &dummyShared) == GuestEfault);
    Require(pthread_condattr_getpshared_nid_postfix(badAttr, nullptr) == GuestEfault);
    Require(pthread_condattr_getpshared_nid_postfix(badAttr, badSharedOut) == GuestEfault);

    Require(scePthreadCondattrGetpshared(badAttr, &dummyShared) == SceEfault);
    Require(scePthreadCondattrGetpshared(badAttr, nullptr) == SceEfault);
    Require(scePthreadCondattrGetpshared(badAttr, badSharedOut) == SceEfault);

    PthreadCondattr unset = nullptr;
    Require(pthread_condattr_getclock_nid_postfix(nullptr, badClockOut) == GuestEinval);
    Require(pthread_condattr_getclock_nid_postfix(&unset, badClockOut) == GuestEinval);
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, badSharedOut) == GuestEinval);
    Require(pthread_condattr_getpshared_nid_postfix(&unset, badSharedOut) == GuestEinval);
    Require(scePthreadCondattrGetpshared(nullptr, badSharedOut) == SceEinval);
    Require(scePthreadCondattrGetpshared(&unset, badSharedOut) == SceEinval);

    PthreadCondattr validAttr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&validAttr) == 0 && validAttr != nullptr);

    Require(pthread_condattr_getclock_nid_postfix(&validAttr, nullptr) == GuestEfault);
    Require(pthread_condattr_getclock_nid_postfix(&validAttr, badClockOut) == GuestEfault);
    Require(pthread_condattr_getpshared_nid_postfix(&validAttr, nullptr) == GuestEfault);
    Require(pthread_condattr_getpshared_nid_postfix(&validAttr, badSharedOut) == GuestEfault);

    Require(scePthreadCondattrGetpshared(&validAttr, nullptr) == SceEfault);
    Require(scePthreadCondattrGetpshared(&validAttr, badSharedOut) == SceEfault);

    Require(pthread_condattr_destroy_nid_postfix(&validAttr) == 0 && validAttr == nullptr);

    Require(sceKernelMprotect(memory, PageSize * 2, 3) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory), PageSize * 2) == 0);
}

void TestClockBoundsAndValues() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0 && attr != nullptr);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
    Require(clockId == ClockRealtime);

    for (KernelClockid validClock : {ClockRealtime, ClockVirtual, ClockProf, ClockMonotonic}) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, validClock) == 0);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
        Require(clockId == validClock);

        Require(scePthreadCondattrSetclock(&attr, validClock) == 0);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
        Require(clockId == validClock);
    }

    for (KernelClockid invalidClock : {-1, 3, 5, 6, 7, 8, 9, 10, 100}) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, invalidClock) == GuestEinval);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
        Require(clockId == ClockMonotonic);

        Require(scePthreadCondattrSetclock(&attr, invalidClock) == SceEinval);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
        Require(clockId == ClockMonotonic);
    }

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0 && attr == nullptr);
}

void TestPsharedBoundsAndValues() {
    PthreadCondattr attr = nullptr;
    Require(scePthreadCondattrInit(&attr) == 0 && attr != nullptr);

    int pshared = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == 0);
    pshared = -1;
    Require(scePthreadCondattrGetpshared(&attr, &pshared) == 0 && pshared == 0);

    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == 0);
    pshared = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == 0);

    Require(scePthreadCondattrSetpshared(&attr, 0) == 0);
    pshared = -1;
    Require(scePthreadCondattrGetpshared(&attr, &pshared) == 0 && pshared == 0);

    for (int invalidShared : {-1, 1, 2, 42, 100, 0x7fffffff}) {
        Require(pthread_condattr_setpshared_nid_postfix(&attr, invalidShared) == GuestEinval);
        pshared = -1;
        Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == 0);

        Require(scePthreadCondattrSetpshared(&attr, invalidShared) == SceEinval);
        pshared = -1;
        Require(scePthreadCondattrGetpshared(&attr, &pshared) == 0 && pshared == 0);
    }

    Require(scePthreadCondattrDestroy(&attr) == 0 && attr == nullptr);
}

void TestDestroyLifecycle() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0 && attr != nullptr);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0 && attr == nullptr);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == GuestEinval);

    Require(scePthreadCondattrInit(&attr) == 0 && attr != nullptr);
    Require(scePthreadCondattrDestroy(&attr) == 0 && attr == nullptr);
    Require(scePthreadCondattrDestroy(&attr) == SceEinval);
}

} // namespace

int main() {
    TestNullAttributePointer();
    TestNullAttributeHandle();
    TestUnmappedAndUnwritablePointers();
    TestClockBoundsAndValues();
    TestPsharedBoundsAndValues();
    TestDestroyLifecycle();
    return 0;
}
