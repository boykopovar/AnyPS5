#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr int BarrierSerialThread = -1;
constexpr int ProcessPrivate = 0;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct WorkerContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<bool> entered{false};
    std::atomic<int> result{0};
};

void* APS5_VABI WorkerThread(void* arg) {
    auto* ctx = static_cast<WorkerContext*>(arg);
    ctx->entered.store(true, std::memory_order_release);
    ctx->result.store(pthread_barrier_wait_nid_postfix(&ctx->barrier), std::memory_order_release);
    return nullptr;
}

struct MultiThreadContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<int> serialCount{0};
    std::atomic<int> zeroCount{0};
};

struct MultiThreadArg {
    MultiThreadContext* ctx = nullptr;
    int iterations = 0;
};

void* APS5_VABI MultiThreadWorker(void* arg) {
    auto* data = static_cast<MultiThreadArg*>(arg);
    for (int i = 0; i < data->iterations; ++i) {
        const int res = pthread_barrier_wait_nid_postfix(&data->ctx->barrier);
        if (res == BarrierSerialThread) {
            data->ctx->serialCount.fetch_add(1, std::memory_order_relaxed);
        } else if (res == 0) {
            data->ctx->zeroCount.fetch_add(1, std::memory_order_relaxed);
        } else {
            std::abort();
        }
    }
    return nullptr;
}

void TestBarrierAttributes() {
    Require(pthread_barrierattr_init_nid_postfix(nullptr) == PosixEinval);

    const auto badAttr = reinterpret_cast<PthreadBarrierattrPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrierattr_init_nid_postfix(badAttr) == PosixEfault);
    Require(pthread_barrierattr_destroy_nid_postfix(badAttr) == PosixEinval);

    int pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(badAttr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(badAttr, ProcessPrivate) == PosixEinval);

    Require(pthread_barrierattr_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierattrPrivate* uninit = nullptr;
    Require(pthread_barrierattr_destroy_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(nullptr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&uninit, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(nullptr, ProcessPrivate) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&uninit, ProcessPrivate) == PosixEinval);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    bool caught = false;
    try {
        pthread_barrierattr_getpshared_nid_postfix(&attr, nullptr);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, reinterpret_cast<int*>(std::uintptr_t{0x10})) == PosixEfault);

    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    const int invalidPshared[] = {-100, -1, 1, 2, 42, 9999};
    for (const int val : invalidPshared) {
        Require(pthread_barrierattr_setpshared_nid_postfix(&attr, val) == PosixEinval);
    }

    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);
    pshared = -1;
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    auto* attrPage = static_cast<PthreadBarrierattrPrivate**>(mmap_nid_postfix(nullptr, 4096, 3, 0x1002, -1, 0));
    Require(attrPage != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(pthread_barrierattr_init_nid_postfix(attrPage) == 0 && *attrPage != nullptr);
    Require(mprotect_nid_postfix(attrPage, 4096, 1) == 0);
    Require(pthread_barrierattr_destroy_nid_postfix(attrPage) == PosixEfault);
    Require(mprotect_nid_postfix(attrPage, 4096, 3) == 0);
    Require(pthread_barrierattr_destroy_nid_postfix(attrPage) == 0 && *attrPage == nullptr);
    Require(munmap_nid_postfix(attrPage, 4096) == 0);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == PosixEinval);
    Require(pthread_barrierattr_getpshared_nid_postfix(&attr, &pshared) == PosixEinval);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == PosixEinval);
}

void TestBarrierInitBounds() {
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 0) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, static_cast<unsigned>(INT_MAX) + 1u) == PosixEinval);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0) == PosixEinval);
    Require(barrier == nullptr);

    const unsigned invalidCounts[] = {
        static_cast<unsigned>(INT_MAX) + 1u,
        static_cast<unsigned>(INT_MAX) + 2u,
        0x80000000u,
        0xC0000000u,
        0xFFFFFFFFu,
        static_cast<unsigned>(-1),
        static_cast<unsigned>(-50)
    };
    for (const unsigned count : invalidCounts) {
        Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, count) == PosixEinval);
        Require(barrier == nullptr);
    }

    const auto badBarrier = reinterpret_cast<PthreadBarrierPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrier_init_nid_postfix(badBarrier, nullptr, 2) == PosixEfault);

    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, &attr, 0) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 0) == PosixEinval);
    Require(barrier == nullptr);

    for (const unsigned count : invalidCounts) {
        Require(pthread_barrier_init_nid_postfix(&barrier, &attr, count) == PosixEinval);
        Require(barrier == nullptr);
    }

    const unsigned validCounts[] = {1, 2, 4, 16, static_cast<unsigned>(INT_MAX)};
    for (const unsigned count : validCounts) {
        Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, count) == 0);
        Require(barrier != nullptr);
        Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
        Require(barrier == nullptr);
    }

    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 2) == 0);
    Require(barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    auto* barrierPage = static_cast<PthreadBarrierPrivate**>(mmap_nid_postfix(nullptr, 4096, 3, 0x1002, -1, 0));
    Require(barrierPage != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(pthread_barrier_init_nid_postfix(barrierPage, nullptr, 2) == 0 && *barrierPage != nullptr);
    Require(mprotect_nid_postfix(barrierPage, 4096, 1) == 0);
    Require(pthread_barrier_destroy_nid_postfix(barrierPage) == PosixEfault);
    Require(mprotect_nid_postfix(barrierPage, 4096, 3) == 0);
    Require(pthread_barrier_destroy_nid_postfix(barrierPage) == 0 && *barrierPage == nullptr);
    Require(munmap_nid_postfix(barrierPage, 4096) == 0);
}

void TestBarrierWaitAndDestroyPrecedence() {
    Require(pthread_barrier_wait_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierPrivate* uninit = nullptr;
    Require(pthread_barrier_wait_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(&uninit) == PosixEinval);

    const auto badBarrier = reinterpret_cast<PthreadBarrierPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrier_wait_nid_postfix(badBarrier) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(badBarrier) == PosixEinval);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(barrier != nullptr);

    Require(pthread_barrier_wait_nid_postfix(&barrier) == BarrierSerialThread);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == BarrierSerialThread);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == BarrierSerialThread);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == PosixEinval);
}

void TestBarrierBusyDestroy() {
    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 2) == 0);
    Require(barrier != nullptr);

    WorkerContext ctx;
    ctx.barrier = barrier;

    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, WorkerThread, &ctx, "barrier_worker") == 0);

    while (!ctx.entered.load(std::memory_order_acquire)) {
        scePthreadYield();
    }

    for (;;) {
        {
            std::lock_guard lock(barrier->mutex);
            if (barrier->arrived == 1) {
                break;
            }
        }
        scePthreadYield();
    }

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == PosixEbusy);
    Require(barrier != nullptr);

    const int waitRet = pthread_barrier_wait_nid_postfix(&barrier);
    Require(scePthreadJoin(worker, nullptr) == 0);

    const int workerRet = ctx.result.load(std::memory_order_acquire);
    const bool validSerial = (waitRet == BarrierSerialThread && workerRet == 0) ||
                             (waitRet == 0 && workerRet == BarrierSerialThread);
    Require(validSerial);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == PosixEinval);
}

void TestMultiThreadSynchronization() {
    constexpr unsigned ThreadCount = 4;
    constexpr int Iterations = 5;

    MultiThreadContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, ThreadCount) == 0);
    Require(ctx.barrier != nullptr);

    std::array<Pthread, ThreadCount> threads{};
    std::array<MultiThreadArg, ThreadCount> args{};

    for (unsigned i = 0; i < ThreadCount; ++i) {
        args[i].ctx = &ctx;
        args[i].iterations = Iterations;
        Require(scePthreadCreate(&threads[i], nullptr, MultiThreadWorker, &args[i], "mt_worker") == 0);
    }

    for (unsigned i = 0; i < ThreadCount; ++i) {
        Require(scePthreadJoin(threads[i], nullptr) == 0);
    }

    Require(ctx.serialCount.load(std::memory_order_relaxed) == Iterations);
    Require(ctx.zeroCount.load(std::memory_order_relaxed) == static_cast<int>(ThreadCount - 1) * Iterations);

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0);
    Require(ctx.barrier == nullptr);
}

void TestBarrierSetnameBounds() {
    bool caught = false;
    try {
        pthread_barrier_setname_np_nid_postfix(nullptr, "test");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    PthreadBarrierPrivate* uninit = nullptr;
    caught = false;
    try {
        pthread_barrier_setname_np_nid_postfix(&uninit, "test");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 2) == 0);
    Require(barrier != nullptr);

    Require(pthread_barrier_setname_np_nid_postfix(&barrier, nullptr) == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "first_barrier") == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "second_barrier") == PosixEagain);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    caught = false;
    try {
        pthread_barrier_setname_np_nid_postfix(&barrier, "after_destroy");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);
}

} // namespace

int main() {
    TestBarrierAttributes();
    TestBarrierInitBounds();
    TestBarrierWaitAndDestroyPrecedence();
    TestBarrierBusyDestroy();
    TestMultiThreadSynchronization();
    TestBarrierSetnameBounds();
    return 0;
}
