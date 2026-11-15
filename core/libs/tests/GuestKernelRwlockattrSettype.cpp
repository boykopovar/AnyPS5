#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEbusy = 16;
constexpr int GuestEinval = 22;

constexpr int SceOk = 0;
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);
constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

void TestValidTypes() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int type = -1;
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 0);

    for (const int validType : {1, 2}) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, validType) == 0);

        type = -1;
        Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == validType);

        int sceType = -1;
        Require(scePthreadRwlockattrGettype(&attr, &sceType) == SceOk && sceType == validType);

        PthreadRwlock rwlock = nullptr;
        Require(pthread_rwlock_init_nid_postfix(&rwlock, &attr) == 0);
        Require(rwlock != nullptr);

        Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);
        Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GuestEbusy);
        Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GuestEbusy);
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

        Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
        Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);
        Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GuestEbusy);
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

        Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    }

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

void TestInvalidTypes() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 1) == 0);

    const int invalidTypes[] = {0, 3, 4, 10, 100, -1, -50, static_cast<int>(0x80000000u), 0x7fffffff};
    for (const int badType : invalidTypes) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, badType) == GuestEinval);

        int currentType = -1;
        Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &currentType) == 0 && currentType == 1);
    }

    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 2) == 0);
    for (const int badType : invalidTypes) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, badType) == GuestEinval);

        int currentType = -1;
        Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &currentType) == 0 && currentType == 2);
    }

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

void TestNullAndDestroyedAttr() {
    const int typesToTest[] = {0, 1, 2, 3, -1};
    for (const int t : typesToTest) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(nullptr, t) == GuestEinval);
    }

    PthreadRwlockattr uninit = nullptr;
    for (const int t : typesToTest) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&uninit, t) == GuestEinval);
    }

    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    for (const int t : typesToTest) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, t) == GuestEinval);
    }
}

void TestFaultyPointers() {
    for (const std::uintptr_t address : {std::uintptr_t{0x10}, std::uintptr_t{0x1000}, ~std::uintptr_t{0xfff}}) {
        auto* badAttr = reinterpret_cast<PthreadRwlockattr*>(address);
        Require(pthread_rwlockattr_settype_np_nid_postfix(badAttr, 1) == GuestEfault);
        Require(pthread_rwlockattr_settype_np_nid_postfix(badAttr, 2) == GuestEfault);
        Require(pthread_rwlockattr_settype_np_nid_postfix(badAttr, 0) == GuestEfault);
    }

    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, nullptr) == GuestEfault);
    for (const std::uintptr_t address : {std::uintptr_t{0x10}, std::uintptr_t{0x1000}, ~std::uintptr_t{0xfff}}) {
        auto* badOut = reinterpret_cast<int*>(address);
        Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, badOut) == GuestEfault);
    }
    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

void TestSceEquivalence() {
    PthreadRwlockattr attr = nullptr;
    Require(scePthreadRwlockattrSettype(nullptr, 1) == SceKernelErrorEinval);
    Require(scePthreadRwlockattrSettype(nullptr, 0) == SceKernelErrorEinval);

    PthreadRwlockattr uninit = nullptr;
    Require(scePthreadRwlockattrSettype(&uninit, 1) == SceKernelErrorEinval);

    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(scePthreadRwlockattrSettype(&attr, 1) == SceOk);
    Require(scePthreadRwlockattrSettype(&attr, 2) == SceOk);
    Require(scePthreadRwlockattrSettype(&attr, 0) == SceKernelErrorEinval);
    Require(scePthreadRwlockattrSettype(&attr, 3) == SceKernelErrorEinval);
    Require(scePthreadRwlockattrSettype(&attr, -1) == SceKernelErrorEinval);

    for (const std::uintptr_t address : {std::uintptr_t{0x10}, std::uintptr_t{0x1000}, ~std::uintptr_t{0xfff}}) {
        auto* badAttr = reinterpret_cast<PthreadRwlockattr*>(address);
        Require(scePthreadRwlockattrSettype(badAttr, 1) == SceKernelErrorEfault);
    }

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(scePthreadRwlockattrSettype(&attr, 1) == SceKernelErrorEinval);
}

} // namespace

int main() {
    TestValidTypes();
    TestInvalidTypes();
    TestNullAndDestroyedAttr();
    TestFaultyPointers();
    TestSceEquivalence();
    return 0;
}
