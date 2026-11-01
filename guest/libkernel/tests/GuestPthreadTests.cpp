#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEdeadlk = 11;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEperm = static_cast<int>(0x80020001u);
constexpr int SceKernelErrorEdeadlk = static_cast<int>(0x8002000bu);
constexpr int SceKernelErrorEbusy = static_cast<int>(0x80020010u);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCallable>
static bool CatchesException(TCallable&& callable) {
    try {
        callable();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void TestErrorcheckMutexTrylockAndDeadlock() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 1) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexTrylock(&mutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixEdeadlk);
    Require(scePthreadMutexLock(&mutex) == SceKernelErrorEdeadlk);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, nullptr) == PosixEdeadlk);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEbusy);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexUnlock(&mutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestDefaultErrorcheckMutexTrylock() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexTrylock(&mutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixEdeadlk);
    Require(scePthreadMutexLock(&mutex) == SceKernelErrorEdeadlk);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

static void TestRecursiveMutexTrylockBounds() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 2) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

    Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
    Require(scePthreadMutexTrylock(&mutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEbusy);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexUnlock(&mutex) == 0);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
    Require(scePthreadMutexUnlock(&mutex) == SceKernelErrorEperm);

    std::atomic<bool> workerLocked{false};
    std::atomic<bool> workerRelease{false};
    std::thread worker([&] {
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
        workerLocked.store(true);
        while (!workerRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });

    while (!workerLocked.load()) {
        std::this_thread::yield();
    }

    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexTrylock(&mutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
    Require(scePthreadMutexUnlock(&mutex) == SceKernelErrorEperm);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEbusy);

    workerRelease.store(true);
    worker.join();

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestErrorcheckMutexOtherThreadContention() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 1) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

    std::atomic<bool> workerLocked{false};
    std::atomic<bool> workerRelease{false};
    std::thread worker([&] {
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
        workerLocked.store(true);
        while (!workerRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });

    while (!workerLocked.load()) {
        std::this_thread::yield();
    }

    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexTrylock(&mutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
    Require(scePthreadMutexUnlock(&mutex) == SceKernelErrorEperm);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEbusy);

    workerRelease.store(true);
    worker.join();

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestInvalidAndDestroyedMutexTrylockBounds() {
    Require(CatchesException([] { pthread_mutex_trylock_nid_postfix(nullptr); }));
    Require(CatchesException([] { scePthreadMutexTrylock(nullptr); }));
    Require(CatchesException([] { pthread_mutex_lock_nid_postfix(nullptr); }));
    Require(CatchesException([] { scePthreadMutexLock(nullptr); }));
    Require(CatchesException([] { pthread_mutex_unlock_nid_postfix(nullptr); }));
    Require(CatchesException([] { scePthreadMutexUnlock(nullptr); }));
    Require(CatchesException([] { pthread_mutex_destroy_nid_postfix(nullptr); }));
    Require(CatchesException([] { scePthreadMutexDestroy(nullptr); }));

    PthreadMutex destroyed = nullptr;
    Require(scePthreadMutexInit(&destroyed, nullptr, nullptr) == 0);
    Require(scePthreadMutexDestroy(&destroyed) == 0);

    Require(pthread_mutex_trylock_nid_postfix(&destroyed) == PosixEinval);
    Require(scePthreadMutexTrylock(&destroyed) == SceKernelErrorEinval);
    Require(pthread_mutex_lock_nid_postfix(&destroyed) == PosixEinval);
    Require(scePthreadMutexLock(&destroyed) == SceKernelErrorEinval);
    Require(pthread_mutex_unlock_nid_postfix(&destroyed) == PosixEinval);
    Require(scePthreadMutexUnlock(&destroyed) == SceKernelErrorEinval);
    Require(pthread_mutex_destroy_nid_postfix(&destroyed) == PosixEinval);
    Require(scePthreadMutexDestroy(&destroyed) == SceKernelErrorEinval);

    PthreadMutex destroyedSentinel = reinterpret_cast<PthreadMutex>(std::uintptr_t{2});
    Require(pthread_mutex_trylock_nid_postfix(&destroyedSentinel) == PosixEinval);
    Require(scePthreadMutexTrylock(&destroyedSentinel) == SceKernelErrorEinval);
    Require(pthread_mutex_lock_nid_postfix(&destroyedSentinel) == PosixEinval);
    Require(scePthreadMutexLock(&destroyedSentinel) == SceKernelErrorEinval);

    PthreadMutexattr unsetAttr = nullptr;
    Require(pthread_mutexattr_settype_nid_postfix(&unsetAttr, 1) == PosixEinval);
    Require(scePthreadMutexattrSettype(&unsetAttr, 1) == SceKernelErrorEinval);
    Require(pthread_mutexattr_settype_nid_postfix(nullptr, 1) == PosixEinval);
    Require(scePthreadMutexattrSettype(nullptr, 1) == SceKernelErrorEinval);

    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    for (const int badType : {-999, -100, -1, 0, 5, 6, 100}) {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, badType) == PosixEinval);
        Require(scePthreadMutexattrSettype(&attr, badType) == SceKernelErrorEinval);
    }
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestStaticAdaptiveMutexTrylockBounds() {
    PthreadMutex adaptive = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(pthread_mutex_trylock_nid_postfix(&adaptive) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&adaptive) == PosixEbusy);
    Require(scePthreadMutexTrylock(&adaptive) == SceKernelErrorEbusy);
    Require(pthread_mutex_lock_nid_postfix(&adaptive) == PosixEdeadlk);
    Require(scePthreadMutexLock(&adaptive) == SceKernelErrorEdeadlk);
    Require(pthread_mutex_unlock_nid_postfix(&adaptive) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&adaptive) == 0);
}

}

int main() {
    TestErrorcheckMutexTrylockAndDeadlock();
    TestDefaultErrorcheckMutexTrylock();
    TestRecursiveMutexTrylockBounds();
    TestErrorcheckMutexOtherThreadContention();
    TestInvalidAndDestroyedMutexTrylockBounds();
    TestStaticAdaptiveMutexTrylockBounds();
    return 0;
}
