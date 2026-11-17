#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"
#include <array>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr int PosixEfault = 14;
constexpr int BarrierSerialThread = -1;
constexpr int ProcessPrivate = 0;
constexpr int ProcessShared = 1;

void TestBarrierInitBounds() {
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 0) == PosixEinval);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0) == PosixEinval);
    Require(barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0x80000000u) == PosixEinval);
    Require(barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0xFFFFFFFFu) == PosixEinval);
    Require(barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, static_cast<unsigned>(INT_MAX) + 1u) == PosixEinval);
    Require(barrier == nullptr);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0 && attr != nullptr);
    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, 0) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 0) == PosixEinval);
    Require(barrier == nullptr);
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 0x80000000u) == PosixEinval);
    Require(barrier == nullptr);
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 0xFFFFFFFFu) == PosixEinval);
    Require(barrier == nullptr);

    PthreadBarrierattrPrivate* nullAttr = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, &nullAttr, 2) == 0 && barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, static_cast<unsigned>(INT_MAX)) == 0 && barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);

    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 1) == 0 && barrier != nullptr);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == BarrierSerialThread);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == BarrierSerialThread);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0 && attr == nullptr);

    const auto badBarrier = reinterpret_cast<PthreadBarrierPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrier_init_nid_postfix(badBarrier, nullptr, 2) == PosixEfault);
}

void TestBarrierDestroyBounds() {
    Require(pthread_barrier_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierPrivate* uninit = nullptr;
    Require(pthread_barrier_destroy_nid_postfix(&uninit) == PosixEinval);

    const auto badBarrier = reinterpret_cast<PthreadBarrierPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrier_destroy_nid_postfix(badBarrier) == PosixEinval);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 2) == 0 && barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == PosixEinval);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);

    auto* page = static_cast<PthreadBarrierPrivate**>(mmap_nid_postfix(nullptr, 4096, 3, 0x1002, -1, 0));
    Require(page != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(pthread_barrier_init_nid_postfix(page, nullptr, 2) == 0 && *page != nullptr);
    Require(mprotect_nid_postfix(page, 4096, 1) == 0);
    Require(pthread_barrier_destroy_nid_postfix(page) == PosixEfault);
    Require(mprotect_nid_postfix(page, 4096, 3) == 0);
    Require(pthread_barrier_destroy_nid_postfix(page) == 0 && *page == nullptr);
    Require(munmap_nid_postfix(page, 4096) == 0);
}

struct BusyContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<bool> waiting{false};
    std::atomic<int> result{0};
};

void* APS5_VABI BusyWaiter(void* arg) {
    auto* ctx = static_cast<BusyContext*>(arg);
    ctx->waiting.store(true, std::memory_order_release);
    ctx->result.store(pthread_barrier_wait_nid_postfix(&ctx->barrier), std::memory_order_release);
    return nullptr;
}

void TestBarrierDestroyBusyState() {
    BusyContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, 2) == 0 && ctx.barrier != nullptr);
    auto* const handle = ctx.barrier;

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, BusyWaiter, &ctx, "busy_waiter") == 0);

    while (!ctx.waiting.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    for (;;) {
        {
            std::unique_lock lock(handle->mutex);
            if (handle->arrived == 1) {
                break;
            }
        }
        std::this_thread::yield();
    }

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == PosixEbusy);
    Require(ctx.barrier == handle);

    Require(pthread_barrier_wait_nid_postfix(&ctx.barrier) == BarrierSerialThread);

    void* retVal = nullptr;
    Require(scePthreadJoin(thread, &retVal) == 0);
    Require(ctx.result.load(std::memory_order_acquire) == 0);

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0 && ctx.barrier == nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == PosixEinval);
}

void TestBarrierattrLifecycle() {
    Require(pthread_barrierattr_init_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrierattr_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == PosixEinval);

    int pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(nullptr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(nullptr, ProcessPrivate) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == PosixEinval);

    const auto badAttr = reinterpret_cast<PthreadBarrierattrPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrierattr_init_nid_postfix(badAttr) == PosixEfault);
    Require(pthread_barrierattr_destroy_nid_postfix(badAttr) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(badAttr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(badAttr, ProcessPrivate) == PosixEinval);

    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0 && attr != nullptr);
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, reinterpret_cast<int*>(std::uintptr_t{0x10})) == PosixEfault);

    bool caught = false;
    try {
        pthread_barrierattr_getpshared_nid_postfix(&attr, nullptr);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == ProcessPrivate);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessShared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, -1) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, 42) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);

    pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == ProcessPrivate);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0 && attr == nullptr);
    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == PosixEinval);
}

void TestBarrierSetnameBounds() {
    PthreadBarrierPrivate* uninit = nullptr;

    bool caught = false;
    try {
        pthread_barrier_setname_np_nid_postfix(nullptr, "test");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    caught = false;
    try {
        pthread_barrier_setname_np_nid_postfix(&uninit, "test");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 2) == 0 && barrier != nullptr);

    Require(pthread_barrier_setname_np_nid_postfix(&barrier, nullptr) == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "first_name") == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "second_name") == PosixEagain);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);

    caught = false;
    try {
        pthread_barrier_setname_np_nid_postfix(&barrier, "after_destroy");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);
}

struct MultiWaitContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<int> serialCount{0};
    std::atomic<int> regularCount{0};
};

void* APS5_VABI MultiWaiter(void* arg) {
    auto* ctx = static_cast<MultiWaitContext*>(arg);
    const int result = pthread_barrier_wait_nid_postfix(&ctx->barrier);
    if (result == BarrierSerialThread) {
        ctx->serialCount.fetch_add(1, std::memory_order_relaxed);
    } else if (result == 0) {
        ctx->regularCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::abort();
    }
    return nullptr;
}

void TestMultiThreadedSynchronization() {
    constexpr unsigned ThreadCount = 4;
    MultiWaitContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, ThreadCount) == 0 && ctx.barrier != nullptr);

    for (int round = 0; round < 3; ++round) {
        ctx.serialCount.store(0, std::memory_order_relaxed);
        ctx.regularCount.store(0, std::memory_order_relaxed);

        std::array<Pthread, ThreadCount> threads{};
        for (auto& thread : threads) {
            Require(scePthreadCreate(&thread, nullptr, MultiWaiter, &ctx, "multi_waiter") == 0);
        }
        for (auto& thread : threads) {
            void* ret = nullptr;
            Require(scePthreadJoin(thread, &ret) == 0);
        }

        Require(ctx.serialCount.load(std::memory_order_relaxed) == 1);
        Require(ctx.regularCount.load(std::memory_order_relaxed) == static_cast<int>(ThreadCount) - 1);
    }

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0 && ctx.barrier == nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == PosixEinval);
}

} // namespace

int main() {
    TestBarrierInitBounds();
    TestBarrierDestroyBounds();
    TestBarrierDestroyBusyState();
    TestBarrierattrLifecycle();
    TestBarrierSetnameBounds();
    TestMultiThreadedSynchronization();
    return 0;
}
