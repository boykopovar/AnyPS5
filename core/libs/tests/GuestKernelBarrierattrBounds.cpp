#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int PosixEinval = 22;
constexpr int ProcessPrivate = 0;
constexpr int PthreadBarrierSerialThread = -1;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

void TestBarrierattrLifecycleAndDefaults() {
    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);
    pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestPsharedValidationAndForeignRejection() {
    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    const int foreignPsharedValues[] = {-1000, -50, -1, 1, 2, 42, 100, 9999};
    for (const int foreignValue : foreignPsharedValues) {
        Require(pthread_barrierattr_setpshared_nid_postfix(&attr, foreignValue) == PosixEinval);
        int pshared = -1;
        Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
        Require(pshared == ProcessPrivate);
    }

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestNullAndUninitializedAttributeRejection() {
    int pshared = -1;

    Require(pthread_barrierattr_init_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrierattr_destroy_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(nullptr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(nullptr, ProcessPrivate) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(nullptr, 1) == PosixEinval);

    PthreadBarrierattrPrivate* uninit = nullptr;
    Require(pthread_barrierattr_destroy_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&uninit, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&uninit, ProcessPrivate) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&uninit, 1) == PosixEinval);
}

void TestDestroyedAttributePrecedence() {
    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    int pshared = -1;
    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, 1) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, -1) == PosixEinval);
}

void TestNullOutputAndPrecedence() {
    Require(pthread_barrierattr_getpshared_nid_postfix(nullptr, nullptr) == PosixEinval);

    PthreadBarrierattrPrivate* uninit = nullptr;
    Require(pthread_barrierattr_getpshared_nid_postfix(&uninit, nullptr) == PosixEinval);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, nullptr) == PosixEinval);
}

void TestBarrierInitWithAttribute() {
    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 1) == 0);
    Require(barrier != nullptr);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PthreadBarrierSerialThread);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(barrier != nullptr);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PthreadBarrierSerialThread);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

} // namespace

int main() {
    TestBarrierattrLifecycleAndDefaults();
    TestPsharedValidationAndForeignRejection();
    TestNullAndUninitializedAttributeRejection();
    TestDestroyedAttributePrecedence();
    TestNullOutputAndPrecedence();
    TestBarrierInitWithAttribute();
    return 0;
}
