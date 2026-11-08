#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

constexpr int POSIX_EINVAL = 22;
constexpr std::size_t THREAD_COUNT = 8;
constexpr std::size_t ARRAY_SIZE = 64;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct TestPayload {
    std::uint32_t magic = 0;
    std::array<int, ARRAY_SIZE> values{};
};

GuestOnce g_concurrentControl{};
std::atomic<int> g_concurrentInitCount{0};
TestPayload g_concurrentPayload{};
std::atomic<int> g_concurrentReadyThreads{0};
std::atomic<bool> g_concurrentStart{false};
std::atomic<int> g_concurrentCompleted{0};

void APS5_VABI ConcurrentInitRoutine() {
    g_concurrentInitCount.fetch_add(1, std::memory_order_relaxed);
    for (std::size_t i = 0; i < ARRAY_SIZE; ++i) {
        g_concurrentPayload.values[i] = static_cast<int>((i + 1) * 31);
    }
    for (int y = 0; y < 50; ++y) {
        scePthreadYield();
    }
    g_concurrentPayload.magic = 0x5a5a1234;
}

void* APS5_VABI ConcurrentWorker(void*) {
    g_concurrentReadyThreads.fetch_add(1, std::memory_order_release);
    while (!g_concurrentStart.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    const int rc = pthread_once_nid_postfix(&g_concurrentControl, ConcurrentInitRoutine);
    Require(rc == 0);
    Require(g_concurrentControl.state == 1);
    Require(g_concurrentPayload.magic == 0x5a5a1234);
    for (std::size_t i = 0; i < ARRAY_SIZE; ++i) {
        Require(g_concurrentPayload.values[i] == static_cast<int>((i + 1) * 31));
    }
    Require(g_concurrentInitCount.load(std::memory_order_relaxed) == 1);
    g_concurrentCompleted.fetch_add(1, std::memory_order_release);
    return nullptr;
}

void TestConcurrentExecution8Threads() {
    g_concurrentControl = GuestOnce{};
    g_concurrentInitCount.store(0, std::memory_order_relaxed);
    g_concurrentPayload = TestPayload{};
    g_concurrentReadyThreads.store(0, std::memory_order_relaxed);
    g_concurrentStart.store(false, std::memory_order_relaxed);
    g_concurrentCompleted.store(0, std::memory_order_relaxed);

    std::array<Pthread, THREAD_COUNT> threads{};
    for (auto& thread : threads) {
        Require(scePthreadCreate(&thread, nullptr, ConcurrentWorker, nullptr, nullptr) == 0);
    }

    while (g_concurrentReadyThreads.load(std::memory_order_acquire) < static_cast<int>(THREAD_COUNT)) {
        std::this_thread::yield();
    }

    g_concurrentStart.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        Require(scePthreadJoin(thread, nullptr) == 0);
    }

    Require(g_concurrentCompleted.load(std::memory_order_acquire) == static_cast<int>(THREAD_COUNT));
    Require(g_concurrentInitCount.load(std::memory_order_acquire) == 1);
    Require(g_concurrentControl.state == 1);

    Require(pthread_once_nid_postfix(&g_concurrentControl, ConcurrentInitRoutine) == 0);
    Require(g_concurrentInitCount.load(std::memory_order_acquire) == 1);
}

struct MultiRoundData {
    GuestOnce control{};
    std::atomic<int> initCount{0};
    std::atomic<int> ready{0};
    std::atomic<bool> start{false};
    std::atomic<int> completed{0};
    int publishedValue = 0;
};

MultiRoundData g_multiRound;

void APS5_VABI MultiRoundInitRoutine() {
    g_multiRound.initCount.fetch_add(1, std::memory_order_relaxed);
    for (int y = 0; y < 20; ++y) {
        scePthreadYield();
    }
    g_multiRound.publishedValue = 4242;
}

void* APS5_VABI MultiRoundWorker(void*) {
    g_multiRound.ready.fetch_add(1, std::memory_order_release);
    while (!g_multiRound.start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    const int rc = pthread_once_nid_postfix(&g_multiRound.control, MultiRoundInitRoutine);
    Require(rc == 0);
    Require(g_multiRound.control.state == 1);
    Require(g_multiRound.publishedValue == 4242);
    Require(g_multiRound.initCount.load(std::memory_order_relaxed) == 1);
    g_multiRound.completed.fetch_add(1, std::memory_order_release);
    return nullptr;
}

void TestMultiRoundConcurrentExecution() {
    for (int round = 0; round < 5; ++round) {
        g_multiRound.control = GuestOnce{};
        g_multiRound.initCount.store(0, std::memory_order_relaxed);
        g_multiRound.ready.store(0, std::memory_order_relaxed);
        g_multiRound.start.store(false, std::memory_order_relaxed);
        g_multiRound.completed.store(0, std::memory_order_relaxed);
        g_multiRound.publishedValue = 0;

        std::array<Pthread, THREAD_COUNT> threads{};
        for (auto& thread : threads) {
            Require(scePthreadCreate(&thread, nullptr, MultiRoundWorker, nullptr, nullptr) == 0);
        }

        while (g_multiRound.ready.load(std::memory_order_acquire) < static_cast<int>(THREAD_COUNT)) {
            std::this_thread::yield();
        }

        g_multiRound.start.store(true, std::memory_order_release);

        for (auto& thread : threads) {
            Require(scePthreadJoin(thread, nullptr) == 0);
        }

        Require(g_multiRound.completed.load(std::memory_order_acquire) == static_cast<int>(THREAD_COUNT));
        Require(g_multiRound.initCount.load(std::memory_order_acquire) == 1);
        Require(g_multiRound.control.state == 1);
    }
}

GuestOnce g_retryControl{};
std::atomic<int> g_retryAttempts{0};
std::atomic<int> g_retrySuccessCount{0};
std::atomic<int> g_retryReadyThreads{0};
std::atomic<bool> g_retryStart{false};
std::atomic<int> g_retryCompleted{0};

void APS5_VABI RetryInitRoutine() {
    const int attempt = g_retryAttempts.fetch_add(1, std::memory_order_seq_cst);
    if (attempt == 0) {
        throw 777;
    }
    g_retrySuccessCount.fetch_add(1, std::memory_order_seq_cst);
}

void* APS5_VABI RetryWorker(void*) {
    g_retryReadyThreads.fetch_add(1, std::memory_order_release);
    while (!g_retryStart.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    for (;;) {
        try {
            const int rc = pthread_once_nid_postfix(&g_retryControl, RetryInitRoutine);
            Require(rc == 0);
            Require(g_retryControl.state == 1);
            Require(g_retrySuccessCount.load(std::memory_order_seq_cst) == 1);
            break;
        } catch (int val) {
            Require(val == 777);
            std::this_thread::yield();
        }
    }
    g_retryCompleted.fetch_add(1, std::memory_order_release);
    return nullptr;
}

void TestConcurrentExceptionRecovery() {
    g_retryControl = GuestOnce{};
    g_retryAttempts.store(0, std::memory_order_seq_cst);
    g_retrySuccessCount.store(0, std::memory_order_seq_cst);
    g_retryReadyThreads.store(0, std::memory_order_relaxed);
    g_retryStart.store(false, std::memory_order_relaxed);
    g_retryCompleted.store(0, std::memory_order_relaxed);

    std::array<Pthread, THREAD_COUNT> threads{};
    for (auto& thread : threads) {
        Require(scePthreadCreate(&thread, nullptr, RetryWorker, nullptr, nullptr) == 0);
    }

    while (g_retryReadyThreads.load(std::memory_order_acquire) < static_cast<int>(THREAD_COUNT)) {
        std::this_thread::yield();
    }

    g_retryStart.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        Require(scePthreadJoin(thread, nullptr) == 0);
    }

    Require(g_retryCompleted.load(std::memory_order_acquire) == static_cast<int>(THREAD_COUNT));
    Require(g_retrySuccessCount.load(std::memory_order_seq_cst) == 1);
    Require(g_retryAttempts.load(std::memory_order_seq_cst) >= 2);
    Require(g_retryControl.state == 1);
}

int32_t g_sceOnceControl = 0;
std::atomic<int> g_sceInitCount{0};
std::atomic<int> g_sceReadyThreads{0};
std::atomic<bool> g_sceStart{false};
std::atomic<int> g_sceCompleted{0};
int g_scePublishedData = 0;

void APS5_VABI SceInitRoutine() {
    g_sceInitCount.fetch_add(1, std::memory_order_relaxed);
    for (int y = 0; y < 30; ++y) {
        scePthreadYield();
    }
    g_scePublishedData = 98765;
}

void* APS5_VABI SceWorker(void*) {
    g_sceReadyThreads.fetch_add(1, std::memory_order_release);
    while (!g_sceStart.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    const int rc = scePthreadOnce(&g_sceOnceControl, SceInitRoutine);
    Require(rc == 0);
    Require(g_sceOnceControl == 1);
    Require(g_scePublishedData == 98765);
    Require(g_sceInitCount.load(std::memory_order_relaxed) == 1);
    g_sceCompleted.fetch_add(1, std::memory_order_release);
    return nullptr;
}

void TestScePthreadOnceConcurrent8Threads() {
    g_sceOnceControl = 0;
    g_sceInitCount.store(0, std::memory_order_relaxed);
    g_sceReadyThreads.store(0, std::memory_order_relaxed);
    g_sceStart.store(false, std::memory_order_relaxed);
    g_sceCompleted.store(0, std::memory_order_relaxed);
    g_scePublishedData = 0;

    std::array<Pthread, THREAD_COUNT> threads{};
    for (auto& thread : threads) {
        Require(scePthreadCreate(&thread, nullptr, SceWorker, nullptr, nullptr) == 0);
    }

    while (g_sceReadyThreads.load(std::memory_order_acquire) < static_cast<int>(THREAD_COUNT)) {
        std::this_thread::yield();
    }

    g_sceStart.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        Require(scePthreadJoin(thread, nullptr) == 0);
    }

    Require(g_sceCompleted.load(std::memory_order_acquire) == static_cast<int>(THREAD_COUNT));
    Require(g_sceInitCount.load(std::memory_order_acquire) == 1);
    Require(g_sceOnceControl == 1);

    Require(scePthreadOnce(&g_sceOnceControl, SceInitRoutine) == 0);
    Require(g_sceInitCount.load(std::memory_order_acquire) == 1);
}

void TestArgumentValidation() {
    Require(pthread_once_nid_postfix(nullptr, ConcurrentInitRoutine) == POSIX_EINVAL);
    Require(pthread_once_nid_postfix(&g_concurrentControl, nullptr) == POSIX_EINVAL);
    Require(pthread_once_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);

    GuestOnce invalidState{99, nullptr};
    Require(pthread_once_nid_postfix(&invalidState, ConcurrentInitRoutine) == POSIX_EINVAL);

    GuestOnce negativeState{-1, nullptr};
    Require(pthread_once_nid_postfix(&negativeState, ConcurrentInitRoutine) == POSIX_EINVAL);

    Require(scePthreadOnce(nullptr, SceInitRoutine) == SCE_KERNEL_ERROR_EINVAL);
    int32_t validControl = 0;
    Require(scePthreadOnce(&validControl, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadOnce(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
}

}

int main() {
    TestArgumentValidation();
    TestConcurrentExecution8Threads();
    TestMultiRoundConcurrentExecution();
    TestConcurrentExceptionRecovery();
    TestScePthreadOnceConcurrent8Threads();
    return 0;
}
