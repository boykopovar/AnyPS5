#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int BARRIER_SERIAL = -1;
static constexpr int PROCESS_PRIVATE = 0;

static void Require(bool value) {
    if (!value) std::abort();
}

static void TestRwlockAttributesAndInit() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == PROCESS_PRIVATE);

    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, 0) == 0);
    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, 1) == GUEST_EINVAL);

    int type = -1;
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0);
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 1) == 0);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 1);
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 2) == 0);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 2);
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 0) == GUEST_EINVAL);
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 3) == GUEST_EINVAL);

    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, &attr) == 0);
    Require(rwlock != nullptr);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);

    PthreadRwlockattr sceAttr = nullptr;
    Require(scePthreadRwlockattrInit(&sceAttr) == 0);
    Require(sceAttr != nullptr);
    Require(scePthreadRwlockattrSettype(&sceAttr, 1) == 0);
    Require(scePthreadRwlockattrGettype(&sceAttr, &type) == 0 && type == 1);
    Require(scePthreadRwlockattrSettype(&sceAttr, 2) == 0);
    Require(scePthreadRwlockattrGettype(&sceAttr, &type) == 0 && type == 2);
    Require(scePthreadRwlockattrSettype(&sceAttr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockattrSetpshared(&sceAttr, 0) == 0);
    Require(scePthreadRwlockattrGetpshared(&sceAttr, &pshared) == 0 && pshared == 0);
    Require(scePthreadRwlockattrDestroy(&sceAttr) == 0);

    PthreadRwlock defaultRwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&defaultRwlock, nullptr) == 0);
    Require(defaultRwlock != nullptr);
    Require(pthread_rwlock_destroy_nid_postfix(&defaultRwlock) == 0);

    PthreadRwlock namedRwlock = nullptr;
    Require(scePthreadRwlockInit(&namedRwlock, nullptr, "test_rwlock") == 0);
    Require(namedRwlock != nullptr);
    Require(scePthreadRwlockDestroy(&namedRwlock) == 0);
    Require(scePthreadRwlockDestroy(&namedRwlock) == SCE_KERNEL_ERROR_EINVAL);
}

static void TestRwlockConcurrencyAndExclusion() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(rwlock != nullptr);

    std::atomic<int> activeReaders{0};
    std::atomic<bool> holdReaders{true};
    std::array<std::thread, 4> readers;

    for (auto& reader : readers) {
        reader = std::thread([&] {
            Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
            activeReaders.fetch_add(1);
            while (holdReaders.load()) {
                std::this_thread::yield();
            }
            activeReaders.fetch_sub(1);
            Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
        });
    }

    while (activeReaders.load() < 4) {
        std::this_thread::yield();
    }
    holdReaders.store(false);
    for (auto& reader : readers) {
        reader.join();
    }
    Require(activeReaders.load() == 0);

    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == GUEST_EDEADLK);
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == GUEST_EDEADLK);

    std::atomic<bool> writerTested{false};
    std::thread other([&] {
        Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
        Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
        writerTested.store(true);
    });
    other.join();
    Require(writerTested.load());

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    std::thread readerExcludesWriter([&] {
        Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);
        Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
        Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    });
    readerExcludesWriter.join();
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

static void TestBarrierCountLimitsAndSync() {
    PthreadBarrierattrPrivate* battr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&battr) == 0);
    Require(battr != nullptr);

    int pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(&battr, &pshared) == 0);
    Require(pshared == PROCESS_PRIVATE);

    Require(pthread_barrierattr_setpshared_nid_postfix(&battr, 0) == 0);
    Require(pthread_barrierattr_setpshared_nid_postfix(&battr, 1) == GUEST_EINVAL);
    Require(pthread_barrierattr_setpshared_nid_postfix(nullptr, 0) == GUEST_EINVAL);

    Require(pthread_barrierattr_destroy_nid_postfix(&battr) == 0);
    Require(battr == nullptr);
    Require(pthread_barrierattr_destroy_nid_postfix(&battr) == GUEST_EINVAL);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0) == GUEST_EINVAL);
    Require(barrier == nullptr);
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0x80000000u) == GUEST_EINVAL);
    Require(barrier == nullptr);
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0xffffffffu) == GUEST_EINVAL);
    Require(barrier == nullptr);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 2) == GUEST_EINVAL);
    Require(pthread_barrier_wait_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_barrier_destroy_nid_postfix(nullptr) == GUEST_EINVAL);

    constexpr unsigned THREAD_COUNT = 4;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, THREAD_COUNT) == 0);
    Require(barrier != nullptr);

    std::atomic<int> serialCount{0};
    std::atomic<int> normalReleaseCount{0};
    std::array<std::thread, THREAD_COUNT> barrierThreads;

    for (auto& thread : barrierThreads) {
        thread = std::thread([&] {
            const int res = pthread_barrier_wait_nid_postfix(&barrier);
            if (res == BARRIER_SERIAL) {
                serialCount.fetch_add(1);
            } else if (res == 0) {
                normalReleaseCount.fetch_add(1);
            } else {
                std::abort();
            }
        });
    }

    for (auto& thread : barrierThreads) {
        thread.join();
    }

    Require(serialCount.load() == 1);
    Require(normalReleaseCount.load() == static_cast<int>(THREAD_COUNT) - 1);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == GUEST_EINVAL);
}

static std::atomic<int> globalOnceCount{0};
static void APS5_VABI SingleThreadInit() {
    globalOnceCount.fetch_add(1);
}

static std::atomic<int> concurrentOnceCount{0};
static void APS5_VABI ConcurrentInit() {
    concurrentOnceCount.fetch_add(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

static std::atomic<int> throwAttemptCount{0};
static void APS5_VABI ThrowingInit() {
    throwAttemptCount.fetch_add(1);
    throw 42;
}

static void APS5_VABI RecoveredInit() {
    throwAttemptCount.fetch_add(100);
}

static std::atomic<int> sceOnceCount{0};
static void APS5_VABI SceOnceInit() {
    sceOnceCount.fetch_add(1);
}

static void TestPthreadOnceSemantics() {
    GuestOnce control{};
    Require(pthread_once_nid_postfix(nullptr, SingleThreadInit) == GUEST_EINVAL);
    Require(pthread_once_nid_postfix(&control, nullptr) == GUEST_EINVAL);

    Require(pthread_once_nid_postfix(&control, SingleThreadInit) == 0);
    Require(globalOnceCount.load() == 1);
    Require(control.state == 1);

    Require(pthread_once_nid_postfix(&control, SingleThreadInit) == 0);
    Require(globalOnceCount.load() == 1);

    GuestOnce concurrentControl{};
    constexpr int CONCURRENT_THREADS = 8;
    std::array<std::thread, CONCURRENT_THREADS> threads;
    for (auto& t : threads) {
        t = std::thread([&] {
            Require(pthread_once_nid_postfix(&concurrentControl, ConcurrentInit) == 0);
            Require(concurrentControl.state == 1);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    Require(concurrentOnceCount.load() == 1);

    GuestOnce throwControl{};
    bool caught = false;
    try {
        pthread_once_nid_postfix(&throwControl, ThrowingInit);
    } catch (int val) {
        caught = (val == 42);
    }
    Require(caught);
    Require(throwAttemptCount.load() == 1);
    Require(throwControl.state == 0);

    Require(pthread_once_nid_postfix(&throwControl, RecoveredInit) == 0);
    Require(throwAttemptCount.load() == 101);
    Require(throwControl.state == 1);

    int32_t sceOnceState = 0;
    Require(scePthreadOnce(&sceOnceState, SceOnceInit) == 0);
    Require(sceOnceCount.load() == 1);
    Require(sceOnceState == 1);

    Require(scePthreadOnce(&sceOnceState, SceOnceInit) == 0);
    Require(sceOnceCount.load() == 1);
}

int main() {
    TestRwlockAttributesAndInit();
    TestRwlockConcurrencyAndExclusion();
    TestBarrierCountLimitsAndSync();
    TestPthreadOnceSemantics();
    return 0;
}
