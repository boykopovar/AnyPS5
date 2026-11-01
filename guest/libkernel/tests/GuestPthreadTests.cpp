#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <thread>

struct PthreadBarrierPrivate;
struct PthreadBarrierattrPrivate;

using PthreadBarrier = PthreadBarrierPrivate*;
using PthreadBarrierattr = PthreadBarrierattrPrivate*;

static constexpr int POSIX_EINVAL = 22;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int BARRIER_SERIAL_THREAD = -1;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCallable>
static bool CatchesError(TCallable&& callable, int expectedPosix, int expectedSce) {
    try {
        const int result = callable();
        return result == expectedPosix || result == expectedSce;
    } catch (const std::exception&) {
        return true;
    }
}

extern "C" {
int pthread_barrier_init(PthreadBarrier* barrier, const PthreadBarrierattr* attr, unsigned count);
int pthread_barrier_init_nid_postfix(PthreadBarrier* barrier, const PthreadBarrierattr* attr, unsigned count);
int pthread_barrier_wait(PthreadBarrier* barrier);
int pthread_barrier_wait_nid_postfix(PthreadBarrier* barrier);
int pthread_barrier_destroy(PthreadBarrier* barrier);
int pthread_barrier_destroy_nid_postfix(PthreadBarrier* barrier);
int scePthreadBarrierInit(PthreadBarrier* barrier, const PthreadBarrierattr* attr, unsigned count, const char* name);
int scePthreadBarrierWait(PthreadBarrier* barrier);
int scePthreadBarrierDestroy(PthreadBarrier* barrier);
}

static void TestPthreadBarrierWaitNullPointer() {
    Require(CatchesError([] { return pthread_barrier_wait_nid_postfix(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([] { return scePthreadBarrierWait(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

static void TestPthreadBarrierWaitNullHandle() {
    PthreadBarrier nullBarrier = nullptr;
    Require(CatchesError([&] { return pthread_barrier_wait_nid_postfix(&nullBarrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadBarrierWait(&nullBarrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(nullBarrier == nullptr);
}

static void TestPthreadBarrierWaitDestroyedHandle() {
    PthreadBarrier barrier = nullptr;
    Require(CatchesError([&] { return pthread_barrier_init_nid_postfix(&barrier, nullptr, 1); }, 0, 0));
    Require(barrier != nullptr);
    Require(CatchesError([&] { return pthread_barrier_destroy_nid_postfix(&barrier); }, 0, 0));
    Require(barrier == nullptr);

    Require(CatchesError([&] { return pthread_barrier_wait_nid_postfix(&barrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadBarrierWait(&barrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

static void TestPthreadBarrierWaitDestroyedMultipleWaiters() {
    PthreadBarrier barrier = nullptr;
    Require(CatchesError([&] { return pthread_barrier_init_nid_postfix(&barrier, nullptr, 4); }, 0, 0));
    Require(barrier != nullptr);
    Require(CatchesError([&] { return pthread_barrier_destroy_nid_postfix(&barrier); }, 0, 0));
    Require(barrier == nullptr);

    for (int i = 0; i < 5; ++i) {
        Require(CatchesError([&] { return pthread_barrier_wait_nid_postfix(&barrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
        Require(CatchesError([&] { return scePthreadBarrierWait(&barrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    }
}

static void TestPthreadBarrierWaitDestroyedAfterCycle() {
    PthreadBarrier single = nullptr;
    Require(CatchesError([&] { return pthread_barrier_init_nid_postfix(&single, nullptr, 1); }, 0, 0));
    Require(single != nullptr);
    Require(CatchesError([&] { return pthread_barrier_wait_nid_postfix(&single); }, BARRIER_SERIAL_THREAD, BARRIER_SERIAL_THREAD));
    Require(CatchesError([&] { return pthread_barrier_destroy_nid_postfix(&single); }, 0, 0));
    Require(single == nullptr);

    Require(CatchesError([&] { return pthread_barrier_wait_nid_postfix(&single); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadBarrierWait(&single); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

static void TestScePthreadBarrierWaitDestroyedHandle() {
    PthreadBarrier barrier = nullptr;
    Require(CatchesError([&] { return scePthreadBarrierInit(&barrier, nullptr, 2, "test_barrier"); }, 0, 0));
    Require(barrier != nullptr);
    Require(CatchesError([&] { return scePthreadBarrierDestroy(&barrier); }, 0, 0));
    Require(barrier == nullptr);

    Require(CatchesError([&] { return scePthreadBarrierWait(&barrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_barrier_wait_nid_postfix(&barrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

static void TestPthreadBarrierWaitConcurrentDestroyed() {
    std::array<std::thread, 4> threads;
    std::atomic<int> failureCount{0};

    for (auto& thread : threads) {
        thread = std::thread([&] {
            PthreadBarrier destroyedBarrier = nullptr;
            if (CatchesError([&] { return pthread_barrier_wait_nid_postfix(&destroyedBarrier); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL)) {
                ++failureCount;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    Require(failureCount.load() == 4);
}

int main() {
    TestPthreadBarrierWaitNullPointer();
    TestPthreadBarrierWaitNullHandle();
    TestPthreadBarrierWaitDestroyedHandle();
    TestPthreadBarrierWaitDestroyedMultipleWaiters();
    TestPthreadBarrierWaitDestroyedAfterCycle();
    TestScePthreadBarrierWaitDestroyedHandle();
    TestPthreadBarrierWaitConcurrentDestroyed();
    return 0;
}
