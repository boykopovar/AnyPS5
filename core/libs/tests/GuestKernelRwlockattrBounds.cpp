#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr int ProcessPrivate = 0;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestRwlockattrLifecycleAndDefaults() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    pshared = -1;
    Require(scePthreadRwlockattrGetpshared(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);
    Require(scePthreadRwlockattrSetpshared(&attr, ProcessPrivate) == 0);

    pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(reinterpret_cast<std::uintptr_t>(attr) == 1);
}

void TestPsharedValidationAndForeignRejection() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    const int foreignPsharedValues[] = {-1000, -50, -1, 1, 2, 42, 100, 9999};
    for (const int foreignValue : foreignPsharedValues) {
        Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, foreignValue) == PosixEinval);
        Require(scePthreadRwlockattrSetpshared(&attr, foreignValue) == SceKernelErrorEinval);

        int pshared = -1;
        Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
        Require(pshared == ProcessPrivate);
    }

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

void TestPsharedPrecedenceOverAttribute() {
    const int foreignPsharedValues[] = {-500, -1, 1, 2, 42, 100};
    for (const int foreignValue : foreignPsharedValues) {
        Require(pthread_rwlockattr_setpshared_nid_postfix(nullptr, foreignValue) == PosixEinval);
        Require(scePthreadRwlockattrSetpshared(nullptr, foreignValue) == SceKernelErrorEinval);

        PthreadRwlockattr uninit = nullptr;
        Require(pthread_rwlockattr_setpshared_nid_postfix(&uninit, foreignValue) == PosixEinval);
        Require(scePthreadRwlockattrSetpshared(&uninit, foreignValue) == SceKernelErrorEinval);
    }

    Require(Throws([&] { pthread_rwlockattr_setpshared_nid_postfix(nullptr, ProcessPrivate); }));
    Require(Throws([&] { scePthreadRwlockattrSetpshared(nullptr, ProcessPrivate); }));

    PthreadRwlockattr uninit = nullptr;
    Require(Throws([&] { pthread_rwlockattr_setpshared_nid_postfix(&uninit, ProcessPrivate); }));
    Require(Throws([&] { scePthreadRwlockattrSetpshared(&uninit, ProcessPrivate); }));
}

void TestNullAndUninitializedAttributeHandling() {
    Require(Throws([&] { pthread_rwlockattr_init_nid_postfix(nullptr); }));
    Require(Throws([&] { pthread_rwlockattr_destroy_nid_postfix(nullptr); }));

    int pshared = -1;
    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(nullptr, &pshared); }));
    Require(Throws([&] { scePthreadRwlockattrGetpshared(nullptr, &pshared); }));

    PthreadRwlockattr uninit = nullptr;
    Require(Throws([&] { pthread_rwlockattr_destroy_nid_postfix(&uninit); }));
    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(&uninit, &pshared); }));
    Require(Throws([&] { scePthreadRwlockattrGetpshared(&uninit, &pshared); }));
}

void TestDestroyedAttributePrecedence() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(reinterpret_cast<std::uintptr_t>(attr) == 1);

    Require(Throws([&] { pthread_rwlockattr_destroy_nid_postfix(&attr); }));

    int pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    pshared = -1;
    Require(scePthreadRwlockattrGetpshared(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, 1) == PosixEinval);
    Require(scePthreadRwlockattrSetpshared(&attr, 1) == SceKernelErrorEinval);
    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);
    Require(scePthreadRwlockattrSetpshared(&attr, ProcessPrivate) == 0);

    int type = -1;
    Require(Throws([&] { pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type); }));
    Require(Throws([&] { scePthreadRwlockattrGettype(&attr, &type); }));
}

void TestFaultPointerPrecedence() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadRwlockattrGetpshared(&attr, nullptr) == SceKernelErrorEfault);

    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(nullptr, nullptr); }));
    Require(Throws([&] { scePthreadRwlockattrGetpshared(nullptr, nullptr); }));

    PthreadRwlockattr uninit = nullptr;
    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(&uninit, nullptr); }));
    Require(Throws([&] { scePthreadRwlockattrGetpshared(&uninit, nullptr); }));

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);

    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadRwlockattrGetpshared(&attr, nullptr) == SceKernelErrorEfault);
}

void TestRwlockInitWithAttribute() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, &attr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

} // namespace

int main() {
    TestRwlockattrLifecycleAndDefaults();
    TestPsharedValidationAndForeignRejection();
    TestPsharedPrecedenceOverAttribute();
    TestNullAndUninitializedAttributeHandling();
    TestDestroyedAttributePrecedence();
    TestFaultPointerPrecedence();
    TestRwlockInitWithAttribute();
    return 0;
}
