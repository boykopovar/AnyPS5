#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEinval = 22;
constexpr int PthreadBarrierSerialThread = -1;
constexpr int ThreadCount = 4;
constexpr int GenerationCount = 3;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct MultiThreadContext {
    PthreadBarrierPrivate* barrier = nullptr;
    std::atomic<int> serialCount{0};
    std::atomic<int> zeroCount{0};
    std::atomic<int> completedGenerations[ThreadCount]{};
};

struct WorkerArgs {
    MultiThreadContext* ctx = nullptr;
    int workerIndex = 0;
};

void* APS5_VABI BarrierWorkerThread(void* arg) {
    auto* args = static_cast<WorkerArgs*>(arg);
    auto* ctx = args->ctx;

    for (int gen = 0; gen < GenerationCount; ++gen) {
        const int result = pthread_barrier_wait_nid_postfix(&ctx->barrier);
        if (result == PthreadBarrierSerialThread) {
            ctx->serialCount.fetch_add(1, std::memory_order_relaxed);
        } else if (result == 0) {
            ctx->zeroCount.fetch_add(1, std::memory_order_relaxed);
        } else {
            Require(false);
        }
        ctx->completedGenerations[args->workerIndex].store(gen + 1, std::memory_order_release);
    }
    return nullptr;
}

void TestCountValidationAndBounds() {
    PthreadBarrierPrivate* barrier = nullptr;

    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 0) == PosixEinval);
    Require(barrier == nullptr);

    const unsigned invalidCounts[] = {
        static_cast<unsigned>(INT_MAX) + 1u,
        0x80000000u,
        0x90000000u,
        static_cast<unsigned>(-1),
        static_cast<unsigned>(-50),
        static_cast<unsigned>(-1000)
    };
    for (const unsigned invalidCount : invalidCounts) {
        Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, invalidCount) == PosixEinval);
        Require(barrier == nullptr);
    }

    const unsigned validCounts[] = {1, 2, 8, 64, static_cast<unsigned>(INT_MAX)};
    for (const unsigned validCount : validCounts) {
        Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, validCount) == 0);
        Require(barrier != nullptr);
        Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
        Require(barrier == nullptr);
    }
}

void TestNullAndUninitializedBarrierRejection() {
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 1) == PosixEinval);
    Require(pthread_barrier_init_nid_postfix(nullptr, nullptr, 0) == PosixEinval);
    Require(pthread_barrier_destroy_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_barrier_wait_nid_postfix(nullptr) == PosixEinval);

    PthreadBarrierPrivate* uninit = nullptr;
    Require(pthread_barrier_destroy_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_barrier_wait_nid_postfix(&uninit) == PosixEinval);
}

void TestDestroyedBarrierPrecedence() {
    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(barrier != nullptr);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == PosixEinval);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == PosixEinval);
}

void TestSingleThreadBarrierRendezvous() {
    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(barrier != nullptr);

    for (int i = 0; i < 5; ++i) {
        Require(pthread_barrier_wait_nid_postfix(&barrier) == PthreadBarrierSerialThread);
    }

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);
}

void TestMultiThreadBarrierRendezvous() {
    MultiThreadContext ctx;
    Require(pthread_barrier_init_nid_postfix(&ctx.barrier, nullptr, ThreadCount) == 0);
    Require(ctx.barrier != nullptr);

    Pthread threads[ThreadCount]{};
    WorkerArgs args[ThreadCount]{};

    for (int i = 0; i < ThreadCount; ++i) {
        args[i].ctx = &ctx;
        args[i].workerIndex = i;
        Require(scePthreadCreate(&threads[i], nullptr, BarrierWorkerThread, &args[i], nullptr) == 0);
    }

    for (int i = 0; i < ThreadCount; ++i) {
        Require(scePthreadJoin(threads[i], nullptr) == 0);
        Require(ctx.completedGenerations[i].load(std::memory_order_acquire) == GenerationCount);
    }

    Require(ctx.serialCount.load(std::memory_order_relaxed) == GenerationCount);
    Require(ctx.zeroCount.load(std::memory_order_relaxed) == (ThreadCount - 1) * GenerationCount);

    Require(pthread_barrier_destroy_nid_postfix(&ctx.barrier) == 0);
    Require(ctx.barrier == nullptr);
}

void TestBarrierAttributeCompatibility() {
    PthreadBarrierattrPrivate* attr = nullptr;
    Require(pthread_barrierattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, &attr, 1) == 0);
    Require(barrier != nullptr);

    Require(pthread_barrier_wait_nid_postfix(&barrier) == PthreadBarrierSerialThread);

    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(barrier == nullptr);

    Require(pthread_barrierattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

} // namespace

int main() {
    TestCountValidationAndBounds();
    TestNullAndUninitializedBarrierRejection();
    TestDestroyedBarrierPrecedence();
    TestSingleThreadBarrierRendezvous();
    TestMultiThreadBarrierRendezvous();
    TestBarrierAttributeCompatibility();
    return 0;
}
