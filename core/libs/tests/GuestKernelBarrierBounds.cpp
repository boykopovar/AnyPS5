#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>

namespace {

constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr int BarrierSerialThread = -1;

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

struct WorkerContext {
    PthreadBarrierPrivate* barrier = nullptr;
    int waitResult = 0;
};

void* APS5_VABI WorkerThread(void* arg) {
    auto* ctx = static_cast<WorkerContext*>(arg);
    ctx->waitResult = pthread_barrier_wait_nid_postfix(&ctx->barrier);
    return nullptr;
}

void TestBarrierInitBounds() {
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 0) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, UINT_MAX) == PosixEinval);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, 0) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, UINT_MAX) == PosixEinval);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0) == PosixEinval);
    Require(barrier == nullptr);
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 0) == PosixEinval);
    Require(barrier == nullptr);

    const unsigned invalidCounts[] = {
        static_cast<unsigned>(INT_MAX) + 1u,
        static_cast<unsigned>(INT_MAX) + 2u,
        0xC0000000u,
        UINT_MAX - 1u,
        UINT_MAX
    };

    for (const unsigned invalidCount : invalidCounts) {
        Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, invalidCount) == PosixEinval);
        Require(barrier == nullptr);
        Require(pthread_barrier_init_nid_postfix(&barrier, &attr, invalidCount) == PosixEinval);
        Require(barrier == nullptr);
    }

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 2) == 0);
    Require(barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, static_cast<unsigned>(INT_MAX)) == 0);
    Require(barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestBarrierWaitAndDestroyLifecycle() {
    Require(pthread_barrier_wait_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierPrivate* uninitBarrier = nullptr;
    Require(pthread_barrier_wait_nid_postfix(&uninitBarrier) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(&uninitBarrier) == PosixEinval);

    PthreadBarrierPrivate* singleBarrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&singleBarrier, nullptr, 1) == 0);
    Require(singleBarrier != nullptr);

    Require(pthread_barrier_wait_nid_postfix(&singleBarrier) == BarrierSerialThread);
    Require(pthread_barrier_wait_nid_postfix(&singleBarrier) == BarrierSerialThread);
    Require(pthread_barrier_wait_nid_postfix(&singleBarrier) == BarrierSerialThread);

    Require(pthread_barrier_destroy_nid_postfix(&singleBarrier) == 0);
    Require(singleBarrier == nullptr);

    Require(pthread_barrier_wait_nid_postfix(&singleBarrier) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(&singleBarrier) == PosixEinval);

    PthreadBarrierPrivate* multiBarrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&multiBarrier, nullptr, 2) == 0);
    Require(multiBarrier != nullptr);

    WorkerContext workerCtx;
    workerCtx.barrier = multiBarrier;

    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, WorkerThread, &workerCtx, nullptr) == 0);

    bool arrived = false;
    while (!arrived) {
        {
            std::lock_guard lock(multiBarrier->mutex);
            arrived = (multiBarrier->arrived == 1);
        }
        if (!arrived) {
            scePthreadYield();
        }
    }

    Require(pthread_barrier_destroy_nid_postfix(&multiBarrier) == PosixEbusy);
    Require(multiBarrier != nullptr);

    const int mainWaitResult = pthread_barrier_wait_nid_postfix(&multiBarrier);
    Require(scePthreadJoin(worker, nullptr) == 0);

    const bool serialValid = (mainWaitResult == BarrierSerialThread && workerCtx.waitResult == 0) ||
                             (mainWaitResult == 0 && workerCtx.waitResult == BarrierSerialThread);
    Require(serialValid);

    workerCtx.waitResult = -999;
    Require(scePthreadCreate(&worker, nullptr, WorkerThread, &workerCtx, nullptr) == 0);
    const int mainWaitResult2 = pthread_barrier_wait_nid_postfix(&multiBarrier);
    Require(scePthreadJoin(worker, nullptr) == 0);

    const bool serialValid2 = (mainWaitResult2 == BarrierSerialThread && workerCtx.waitResult == 0) ||
                              (mainWaitResult2 == 0 && workerCtx.waitResult == BarrierSerialThread);
    Require(serialValid2);

    Require(pthread_barrier_destroy_nid_postfix(&multiBarrier) == 0);
    Require(multiBarrier == nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&multiBarrier) == PosixEinval);
    Require(pthread_barrier_wait_nid_postfix(&multiBarrier) == PosixEinval);
}

void TestBarrierAttributes() {
    Require(pthread_barrierattr_init_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrierattr_destroy_nid_postfix(nullptr) == PosixEinval);

    int pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(nullptr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(nullptr, 0) == PosixEinval);

    PthreadBarrierattrPrivate* uninitAttr = nullptr;
    Require(pthread_barrierattr_destroy_nid_postfix(&uninitAttr) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&uninitAttr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&uninitAttr, 0) == PosixEinval);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == 0);

    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, 0) == 0);
    pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == 0);

    const int invalidPshared[] = {-100, -1, 1, 2, 42, 9999};
    for (const int val : invalidPshared) {
        Require(pthread_barrierattr_setpshared_nid_postfix(&attr, val) == PosixEinval);
    }

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, 0) == PosixEinval);
}

void TestBarrierSetname() {
    Require(Throws([] { pthread_barrier_setname_np_nid_postfix(nullptr, "test"); }));

    PthreadBarrierPrivate* uninitBarrier = nullptr;
    Require(Throws([&] { pthread_barrier_setname_np_nid_postfix(&uninitBarrier, "test"); }));

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(barrier != nullptr);

    Require(pthread_barrier_setname_np_nid_postfix(&barrier, nullptr) == 0);
    Require(!barrier->named);

    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "barrier_initial") == 0);
    Require(barrier->named);
    Require(barrier->name == "barrier_initial");

    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "barrier_second") == PosixEagain);
    Require(barrier->name == "barrier_initial");

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(Throws([&] { pthread_barrier_setname_np_nid_postfix(&barrier, "destroyed"); }));
}

} // namespace

int main() {
    TestBarrierInitBounds();
    TestBarrierWaitAndDestroyLifecycle();
    TestBarrierAttributes();
    TestBarrierSetname();
    return 0;
}
