#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEinval = 22;
constexpr int PosixEbusy = 16;
constexpr int PthreadBarrierSerialThread = -1;
constexpr int ProcessPrivate = 0;
constexpr int MultiThreadCount = 4;
constexpr int GenerationCount = 5;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestNullAndInvalidHandleWaitBounds() {
    Require(pthread_barrier_wait_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierPrivate* uninit = nullptr;
    Require(pthread_barrier_wait_nid_postfix(&uninit) == PosixEinval);

    const auto badBarrier = reinterpret_cast<PthreadBarrierPrivate**>(std::uintptr_t{0x10});
    Require(pthread_barrier_wait_nid_postfix(badBarrier) == PosixEinval);

    const auto badBarrier2 = reinterpret_cast<PthreadBarrierPrivate**>(std::uintptr_t{0xdeadbeef});
    Require(pthread_barrier_wait_nid_postfix(badBarrier2) == PosixEinval);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0 && barrier != nullptr);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);

    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);
}

void TestGuardedMemoryWaitBounds() {
    auto* page = static_cast<PthreadBarrierPrivate**>(mmap_nid_postfix(nullptr, 4096, 3, 0x1002, -1, 0));
    Require(page != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(pthread_barrier_init_nid_postfix(page, nullptr, 1) == 0 && *page != nullptr);

    Require(mprotect_nid_postfix(page, 4096, 0) == 0);
    Require(pthread_barrier_wait_nid_postfix(page) == PosixEinval);

    Require(mprotect_nid_postfix(page, 4096, 3) == 0);
    Require(pthread_barrier_wait_nid_postfix(page) == PthreadBarrierSerialThread);

    Require(pthread_barrier_destroy_nid_postfix(page) == 0 && *page == nullptr);
    Require(munmap_nid_postfix(page, 4096) == 0);
}

void TestSingleThreadWaitSemantics() {
    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0 && barrier != nullptr);

    for (int cycle = 0; cycle < 10; ++cycle) {
        Require(pthread_barrier_wait_nid_postfix(&barrier) == PthreadBarrierSerialThread);
    }

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);
}

struct TwoThreadContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<int> workerResult{0};
    std::atomic<bool> workerArrived{false};
};

void* APS5_VABI TwoThreadWorker(void* arg) {
    auto* ctx = static_cast<TwoThreadContext*>(arg);
    ctx->workerArrived.store(true, std::memory_order_release);
    ctx->workerResult.store(pthread_barrier_wait_nid_postfix(&ctx->barrier), std::memory_order_release);
    return nullptr;
}

void TestTwoThreadRendezvousBounds() {
    TwoThreadContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, 2) == 0 && ctx.barrier != nullptr);

    for (int round = 0; round < 5; ++round) {
        ctx.workerArrived.store(false, std::memory_order_relaxed);
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, TwoThreadWorker, &ctx, "two_thread_worker") == 0);

        while (!ctx.workerArrived.load(std::memory_order_acquire)) {
            scePthreadYield();
        }

        const int mainResult = pthread_barrier_wait_nid_postfix(&ctx.barrier);
        Require(scePthreadJoin(thread, nullptr) == 0);

        const int workerRes = ctx.workerResult.load(std::memory_order_acquire);
        const bool validOutcome = (mainResult == PthreadBarrierSerialThread && workerRes == 0) ||
                                  (mainResult == 0 && workerRes == PthreadBarrierSerialThread);
        Require(validOutcome);
    }

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0 && ctx.barrier == nullptr);
}

struct MultiGenerationContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<int> serialCount{0};
    std::atomic<int> regularCount{0};
    std::atomic<int> completedGenerations[MultiThreadCount]{};
};

struct MultiGenerationArgs {
    MultiGenerationContext* ctx = nullptr;
    int workerIndex = 0;
};

void* APS5_VABI MultiGenerationWorker(void* arg) {
    auto* args = static_cast<MultiGenerationArgs*>(arg);
    auto* ctx = args->ctx;

    for (int gen = 0; gen < GenerationCount; ++gen) {
        const int result = pthread_barrier_wait_nid_postfix(&ctx->barrier);
        if (result == PthreadBarrierSerialThread) {
            ctx->serialCount.fetch_add(1, std::memory_order_relaxed);
        } else if (result == 0) {
            ctx->regularCount.fetch_add(1, std::memory_order_relaxed);
        } else {
            std::abort();
        }
        ctx->completedGenerations[args->workerIndex].store(gen + 1, std::memory_order_release);
    }
    return nullptr;
}

void TestMultiThreadMultiGenerationWaitPhasing() {
    MultiGenerationContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, MultiThreadCount) == 0 && ctx.barrier != nullptr);

    std::array<Pthread, MultiThreadCount> threads{};
    std::array<MultiGenerationArgs, MultiThreadCount> args{};

    for (int i = 0; i < MultiThreadCount; ++i) {
        args[i].ctx = &ctx;
        args[i].workerIndex = i;
        Require(scePthreadCreate(&threads[i], nullptr, MultiGenerationWorker, &args[i], "multi_gen_worker") == 0);
    }

    for (int i = 0; i < MultiThreadCount; ++i) {
        Require(scePthreadJoin(threads[i], nullptr) == 0);
        Require(ctx.completedGenerations[i].load(std::memory_order_acquire) == GenerationCount);
    }

    Require(ctx.serialCount.load(std::memory_order_relaxed) == GenerationCount);
    Require(ctx.regularCount.load(std::memory_order_relaxed) == (MultiThreadCount - 1) * GenerationCount);

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0 && ctx.barrier == nullptr);
    Require(pthread_barrier_wait_nid_postfix(&ctx.barrier) == PosixEinval);
}

struct DestroyWaitContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<bool> threadWaiting{false};
    std::atomic<int> waitResult{0};
};

void* APS5_VABI DestroyWaitWorker(void* arg) {
    auto* ctx = static_cast<DestroyWaitContext*>(arg);
    ctx->threadWaiting.store(true, std::memory_order_release);
    ctx->waitResult.store(pthread_barrier_wait_nid_postfix(&ctx->barrier), std::memory_order_release);
    return nullptr;
}

void TestDestroyContentionWhileWaiting() {
    DestroyWaitContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, 2) == 0 && ctx.barrier != nullptr);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, DestroyWaitWorker, &ctx, "destroy_wait_worker") == 0);

    while (pthread_barrier_destroy_nid_postfix(&ctx.barrier) != PosixEbusy) {
        scePthreadYield();
    }

    Require(ctx.barrier != nullptr);

    const int mainResult = pthread_barrier_wait_nid_postfix(&ctx.barrier);
    Require(scePthreadJoin(thread, nullptr) == 0);

    const int workerRes = ctx.waitResult.load(std::memory_order_acquire);
    const bool validOutcome = (mainResult == PthreadBarrierSerialThread && workerRes == 0) ||
                              (mainResult == 0 && workerRes == PthreadBarrierSerialThread);
    Require(validOutcome);

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0 && ctx.barrier == nullptr);
    Require(pthread_barrier_wait_nid_postfix(&ctx.barrier) == PosixEinval);
}

void TestAttributesCompatibilityWait() {
    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0 && attr != nullptr);
    Require(pthread_barrierattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 1) == 0 && barrier != nullptr);
    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0 && attr == nullptr);

    Require(pthread_barrier_wait_nid_postfix(&barrier) == PthreadBarrierSerialThread);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0 && barrier == nullptr);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);
}

void TestThreeThreadWaitSemantics() {
    constexpr int ThreeThreads = 3;
    MultiGenerationContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, ThreeThreads) == 0 && ctx.barrier != nullptr);

    std::array<Pthread, ThreeThreads> threads{};
    std::array<MultiGenerationArgs, ThreeThreads> args{};

    for (int i = 0; i < ThreeThreads; ++i) {
        args[i].ctx = &ctx;
        args[i].workerIndex = i;
        Require(scePthreadCreate(&threads[i], nullptr, MultiGenerationWorker, &args[i], "three_thread_worker") == 0);
    }

    for (int i = 0; i < ThreeThreads; ++i) {
        Require(scePthreadJoin(threads[i], nullptr) == 0);
        Require(ctx.completedGenerations[i].load(std::memory_order_acquire) == GenerationCount);
    }

    Require(ctx.serialCount.load(std::memory_order_relaxed) == GenerationCount);
    Require(ctx.regularCount.load(std::memory_order_relaxed) == (ThreeThreads - 1) * GenerationCount);

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0 && ctx.barrier == nullptr);
}

} // namespace

int main() {
    TestNullAndInvalidHandleWaitBounds();
    TestGuardedMemoryWaitBounds();
    TestSingleThreadWaitSemantics();
    TestTwoThreadRendezvousBounds();
    TestMultiThreadMultiGenerationWaitPhasing();
    TestDestroyContentionWhileWaiting();
    TestAttributesCompatibilityWait();
    TestThreeThreadWaitSemantics();
    return 0;
}
