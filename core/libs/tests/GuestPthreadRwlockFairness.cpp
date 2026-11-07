#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EAGAIN = 35;
static constexpr int GUEST_ETIMEDOUT = 60;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TFunc>
static bool Throws(TFunc&& func) {
    try {
        func();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void CheckInvalidPointerHandling() {
    Require(Throws([] { scePthreadRwlockInit(nullptr, nullptr, "null_init"); }));
    Require(Throws([] { scePthreadRwlockDestroy(nullptr); }));
    Require(Throws([] { scePthreadRwlockRdlock(nullptr); }));
    Require(Throws([] { scePthreadRwlockTryrdlock(nullptr); }));
    Require(Throws([] { scePthreadRwlockTimedrdlock(nullptr, 100); }));
    Require(Throws([] { scePthreadRwlockWrlock(nullptr); }));
    Require(Throws([] { scePthreadRwlockTrywrlock(nullptr); }));
    Require(Throws([] { scePthreadRwlockTimedwrlock(nullptr, 100); }));
    Require(Throws([] { scePthreadRwlockUnlock(nullptr); }));

    Require(Throws([] { pthread_rwlock_destroy_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_rdlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_tryrdlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_reltimedrdlock_np_nid_postfix(nullptr, 100); }));
    Require(Throws([] { pthread_rwlock_wrlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_trywrlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_reltimedwrlock_np_nid_postfix(nullptr, 100); }));
    Require(Throws([] { pthread_rwlock_unlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_setname_np_nid_postfix(nullptr, "null_rwlock"); }));

    const KernelTimespec abstime{100, 0};
    Require(Throws([&] { pthread_rwlock_timedrdlock_nid_postfix(nullptr, &abstime); }));
    Require(Throws([&] { pthread_rwlock_timedwrlock_nid_postfix(nullptr, &abstime); }));
    Require(Throws([] { pthread_rwlock_timedrdlock_nid_postfix(nullptr, nullptr); }));
    Require(Throws([] { pthread_rwlock_timedwrlock_nid_postfix(nullptr, nullptr); }));

    PthreadRwlock destroyed = nullptr;
    Require(scePthreadRwlockInit(&destroyed, nullptr, "destroyed") == 0);
    Require(destroyed != nullptr);
    Require(scePthreadRwlockDestroy(&destroyed) == 0);

    Require(scePthreadRwlockDestroy(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockRdlock(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockTryrdlock(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockTimedrdlock(&destroyed, 100) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockWrlock(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockTrywrlock(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockTimedwrlock(&destroyed, 100) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockUnlock(&destroyed) == SCE_KERNEL_ERROR_EINVAL);

    Require(pthread_rwlock_destroy_nid_postfix(&destroyed) == GUEST_EINVAL);
    Require(pthread_rwlock_rdlock_nid_postfix(&destroyed) == GUEST_EINVAL);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&destroyed) == GUEST_EINVAL);
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&destroyed, 100) == GUEST_EINVAL);
    Require(pthread_rwlock_wrlock_nid_postfix(&destroyed) == GUEST_EINVAL);
    Require(pthread_rwlock_trywrlock_nid_postfix(&destroyed) == GUEST_EINVAL);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&destroyed, 100) == GUEST_EINVAL);
    Require(pthread_rwlock_unlock_nid_postfix(&destroyed) == GUEST_EINVAL);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&destroyed, &abstime) == GUEST_EINVAL);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&destroyed, &abstime) == GUEST_EINVAL);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&destroyed, nullptr) == GUEST_EINVAL);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&destroyed, nullptr) == GUEST_EINVAL);
    Require(Throws([&] { pthread_rwlock_setname_np_nid_postfix(&destroyed, "destroyed_name"); }));

    PthreadRwlock zeroLock = nullptr;
    Require(Throws([&] { pthread_rwlock_setname_np_nid_postfix(&zeroLock, "zero_name"); }));
    Require(scePthreadRwlockUnlock(&zeroLock) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_rwlock_unlock_nid_postfix(&zeroLock) == GUEST_EPERM);
    Require(scePthreadRwlockDestroy(&zeroLock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&zeroLock) == 0);

    PthreadRwlock nameLock = nullptr;
    Require(scePthreadRwlockInit(&nameLock, nullptr, "init_name") == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&nameLock, nullptr) == GUEST_EINVAL);
    Require(pthread_rwlock_setname_np_nid_postfix(&nameLock, "new_name") == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&nameLock, "second_name") == GUEST_EAGAIN);
    Require(scePthreadRwlockDestroy(&nameLock) == 0);
}

static void CheckReaderAcquisitionWithQueuedWriter() {
    PthreadRwlock rwlock = nullptr;
    Require(scePthreadRwlockInit(&rwlock, nullptr, "writer_pref") == 0);

    Require(scePthreadRwlockRdlock(&rwlock) == 0);

    std::atomic<bool> writerStarted{false};
    std::atomic<bool> writerAcquired{false};

    std::thread writer([&] {
        writerStarted.store(true, std::memory_order_release);
        Require(scePthreadRwlockWrlock(&rwlock) == 0);
        writerAcquired.store(true, std::memory_order_release);
        Require(scePthreadRwlockUnlock(&rwlock) == 0);
    });

    while (!writerStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::atomic<int> probeTryrdlock{-1};
    std::atomic<int> probeTimedrdlock{-1};
    std::atomic<int> probePosixTryrdlock{-1};
    std::atomic<int> probePosixTimedrdlock{-1};

    for (int retry = 0; retry < 5000; ++retry) {
        probeTryrdlock.store(scePthreadRwlockTryrdlock(&rwlock));
        if (probeTryrdlock.load() == SCE_KERNEL_ERROR_EBUSY) {
            break;
        }
        Require(scePthreadRwlockUnlock(&rwlock) == 0);
        std::this_thread::yield();
    }
    Require(probeTryrdlock.load() == SCE_KERNEL_ERROR_EBUSY);

    std::thread otherReader([&] {
        probeTimedrdlock.store(scePthreadRwlockTimedrdlock(&rwlock, 1000));
        probePosixTryrdlock.store(pthread_rwlock_tryrdlock_nid_postfix(&rwlock));
        probePosixTimedrdlock.store(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 1000));
    });
    otherReader.join();

    Require(probeTimedrdlock.load() == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(probePosixTryrdlock.load() == GUEST_EBUSY);
    Require(probePosixTimedrdlock.load() == GUEST_ETIMEDOUT);
    Require(!writerAcquired.load(std::memory_order_acquire));

    Require(scePthreadRwlockRdlock(&rwlock) == 0);
    Require(scePthreadRwlockTryrdlock(&rwlock) == 0);
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);

    Require(!writerAcquired.load(std::memory_order_acquire));

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(scePthreadRwlockUnlock(&rwlock) == 0);
    Require(scePthreadRwlockUnlock(&rwlock) == 0);

    Require(!writerAcquired.load(std::memory_order_acquire));

    Require(scePthreadRwlockUnlock(&rwlock) == 0);

    writer.join();
    Require(writerAcquired.load(std::memory_order_acquire));

    Require(scePthreadRwlockDestroy(&rwlock) == 0);
}

static void CheckPosixReaderAcquisitionWithQueuedWriter() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);

    std::atomic<bool> writerStarted{false};
    std::atomic<bool> writerAcquired{false};

    std::thread writer([&] {
        writerStarted.store(true, std::memory_order_release);
        Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);
        writerAcquired.store(true, std::memory_order_release);
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    });

    while (!writerStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::atomic<int> probeTryrdlock{-1};
    for (int retry = 0; retry < 5000; ++retry) {
        probeTryrdlock.store(pthread_rwlock_tryrdlock_nid_postfix(&rwlock));
        if (probeTryrdlock.load() == GUEST_EBUSY) {
            break;
        }
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
        std::this_thread::yield();
    }
    Require(probeTryrdlock.load() == GUEST_EBUSY);

    std::atomic<int> probeTimedrdlock{-1};
    std::thread otherReader([&] {
        probeTimedrdlock.store(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 1000));
    });
    otherReader.join();
    Require(probeTimedrdlock.load() == GUEST_ETIMEDOUT);
    Require(!writerAcquired.load(std::memory_order_acquire));

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);
    Require(!writerAcquired.load(std::memory_order_acquire));

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(!writerAcquired.load(std::memory_order_acquire));

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    writer.join();
    Require(writerAcquired.load(std::memory_order_acquire));

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

static void CheckRecursiveReaderDepthBounds() {
    PthreadRwlock rwlock = nullptr;
    Require(scePthreadRwlockInit(&rwlock, nullptr, "recursion_depth") == 0);

    constexpr int depth = 500;
    for (int i = 0; i < depth; ++i) {
        Require(scePthreadRwlockRdlock(&rwlock) == 0);
    }

    Require(scePthreadRwlockTryrdlock(&rwlock) == 0);
    Require(scePthreadRwlockUnlock(&rwlock) == 0);

    Require(scePthreadRwlockTrywrlock(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);

    std::atomic<int> bgTryrdlock{-1};
    std::atomic<int> bgTrywrlock{-1};
    std::atomic<int> bgTimedwrlock{-1};
    std::thread bgThread([&] {
        bgTryrdlock.store(scePthreadRwlockTryrdlock(&rwlock));
        if (bgTryrdlock.load() == 0) {
            Require(scePthreadRwlockUnlock(&rwlock) == 0);
        }
        bgTrywrlock.store(scePthreadRwlockTrywrlock(&rwlock));
        bgTimedwrlock.store(scePthreadRwlockTimedwrlock(&rwlock, 1000));
    });
    bgThread.join();

    Require(bgTryrdlock.load() == 0);
    Require(bgTrywrlock.load() == SCE_KERNEL_ERROR_EBUSY);
    Require(bgTimedwrlock.load() == SCE_KERNEL_ERROR_ETIMEDOUT);

    for (int i = 0; i < depth; ++i) {
        Require(scePthreadRwlockUnlock(&rwlock) == 0);
    }

    Require(scePthreadRwlockUnlock(&rwlock) == SCE_KERNEL_ERROR_EPERM);
    Require(scePthreadRwlockUnlock(&rwlock) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);

    Require(scePthreadRwlockWrlock(&rwlock) == 0);
    Require(scePthreadRwlockUnlock(&rwlock) == 0);
    Require(scePthreadRwlockDestroy(&rwlock) == 0);
}

static void CheckPosixRecursiveReaderDepthBounds() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);

    constexpr int depth = 500;
    for (int i = 0; i < depth; ++i) {
        Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);

    for (int i = 0; i < depth; ++i) {
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    }

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);
    Require(scePthreadRwlockUnlock(&rwlock) == SCE_KERNEL_ERROR_EPERM);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

static void CheckDeadlockAndUnlockErrors() {
    PthreadRwlock rwlock = nullptr;
    Require(scePthreadRwlockInit(&rwlock, nullptr, "deadlock") == 0);
    Require(scePthreadRwlockWrlock(&rwlock) == 0);
    Require(scePthreadRwlockWrlock(&rwlock) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadRwlockRdlock(&rwlock) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadRwlockTrywrlock(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadRwlockTryrdlock(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == GUEST_EDEADLK);
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == GUEST_EDEADLK);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);

    std::atomic<int> crossThreadUnlock{-1};
    std::atomic<int> crossThreadPosixUnlock{-1};
    std::thread crossThread([&] {
        crossThreadUnlock.store(scePthreadRwlockUnlock(&rwlock));
        crossThreadPosixUnlock.store(pthread_rwlock_unlock_nid_postfix(&rwlock));
    });
    crossThread.join();
    Require(crossThreadUnlock.load() == SCE_KERNEL_ERROR_EPERM);
    Require(crossThreadPosixUnlock.load() == GUEST_EPERM);

    Require(scePthreadRwlockUnlock(&rwlock) == 0);
    Require(scePthreadRwlockUnlock(&rwlock) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);
    Require(scePthreadRwlockDestroy(&rwlock) == 0);
}

int main() {
    CheckInvalidPointerHandling();
    CheckReaderAcquisitionWithQueuedWriter();
    CheckPosixReaderAcquisitionWithQueuedWriter();
    CheckRecursiveReaderDepthBounds();
    CheckPosixRecursiveReaderDepthBounds();
    CheckDeadlockAndUnlockErrors();
    return 0;
}
