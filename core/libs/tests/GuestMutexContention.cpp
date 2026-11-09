#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <windows.h>

namespace {

using Clock = std::chrono::steady_clock;
constexpr int TimedOut = static_cast<int>(0x8002003cu);

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

std::uint64_t ThreadCycles() {
    ULONG64 cycles = 0;
    Require(QueryThreadCycleTime(GetCurrentThread(), &cycles) != 0, "QueryThreadCycleTime failed");
    return cycles;
}

std::uint64_t BusyCycles(int milliseconds) {
    const auto before = ThreadCycles();
    const auto until = Clock::now() + std::chrono::milliseconds(milliseconds);
    while (Clock::now() < until) YieldProcessor();
    return ThreadCycles() - before;
}

struct Handoff {
    PthreadMutex mutex = nullptr;
    bool timed = false;
    bool destroy = false;
    std::atomic<bool> waiting{false};
    std::atomic<std::int64_t> released{0};
    std::int64_t latencyNanos = 0;
    std::uint64_t waitCycles = 0;
};

struct HandoffResult {
    std::int64_t latencyNanos;
    std::uint64_t waitCycles;
};

void* APS5_VABI HandoffWorker(void* opaque) {
    auto& context = *static_cast<Handoff*>(opaque);
    const auto cyclesBefore = ThreadCycles();
    context.waiting.store(true);
    Require((context.timed ? scePthreadMutexTimedlock(&context.mutex, 5000000) : scePthreadMutexLock(&context.mutex)) == 0, "the waiter did not acquire the mutex");
    const auto acquired = Clock::now().time_since_epoch().count();
    context.waitCycles = ThreadCycles() - cyclesBefore;
    context.latencyNanos = acquired - context.released.load();
    Require(scePthreadMutexUnlock(&context.mutex) == 0, "the waiter could not unlock");
    if (context.destroy) Require(scePthreadMutexDestroy(&context.mutex) == 0, "the waiter could not destroy the mutex");
    return nullptr;
}

HandoffResult RunHandoff(PthreadMutex mutex, bool timed, bool destroy, int holdMilliseconds, int extraMicroseconds = 0) {
    Handoff context;
    context.mutex = mutex;
    context.timed = timed;
    context.destroy = destroy;
    PthreadMutex holder = mutex;
    Require(scePthreadMutexLock(&holder) == 0, "the holder could not lock");
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, HandoffWorker, &context, "mutex waiter") == 0, "cannot create the waiter");
    while (!context.waiting.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMilliseconds));
    const auto extra = Clock::now() + std::chrono::microseconds(extraMicroseconds);
    while (Clock::now() < extra) YieldProcessor();
    context.released.store(Clock::now().time_since_epoch().count());
    Require(scePthreadMutexUnlock(&holder) == 0, "the holder could not unlock");
    Require(scePthreadJoin(worker, nullptr) == 0, "cannot join the waiter");
    return {context.latencyNanos, context.waitCycles};
}

PthreadMutex CreateMutex(bool recursive) {
    PthreadMutexattr attributes = nullptr;
    Require(scePthreadMutexattrInit(&attributes) == 0 && scePthreadMutexattrSettype(&attributes, recursive ? 2 : 1) == 0, "cannot prepare mutex attributes");
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attributes, nullptr) == 0, "cannot create the mutex");
    Require(scePthreadMutexattrDestroy(&attributes) == 0, "cannot destroy mutex attributes");
    return mutex;
}

struct Contention {
    PthreadMutex mutex = nullptr;
    std::uint64_t rounds = 0;
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::uint64_t shared = 0;
    std::array<std::uint64_t, 4> acquired{};
    std::atomic<int> next{0};
};

void* APS5_VABI ContentionWorker(void* opaque) {
    auto& context = *static_cast<Contention*>(opaque);
    const int index = context.next.fetch_add(1);
    PthreadMutex mutex = context.mutex;
    while (!context.start.load()) std::this_thread::yield();
    std::uint64_t count = 0;
    while (context.rounds ? count < context.rounds : !context.stop.load(std::memory_order_relaxed)) {
        Require(scePthreadMutexLock(&mutex) == 0, "contended lock failed");
        const std::uint64_t seen = context.shared;
        for (int spin = 0; spin < 64; ++spin) YieldProcessor();
        context.shared = seen + 1;
        Require(scePthreadMutexUnlock(&mutex) == 0, "contended unlock failed");
        ++count;
        for (int spin = 0; spin < 64; ++spin) YieldProcessor();
    }
    context.acquired[index] = count;
    return nullptr;
}

void* APS5_VABI TimeoutWorker(void* opaque) {
    PthreadMutex mutex = *static_cast<PthreadMutex*>(opaque);
    const auto started = Clock::now();
    Require(scePthreadMutexTimedlock(&mutex, 20000) == TimedOut, "a timed lock of a held mutex did not time out");
    Require(Clock::now() - started >= std::chrono::milliseconds(20), "a timed lock of a held mutex timed out before its deadline");
    return nullptr;
}

}

int main(int argc, char** argv) {
    const bool report = argc > 1 && std::strcmp(argv[1], "--report") == 0;
    const std::uint64_t busyCycles = report ? BusyCycles(20) * 10 : 0;
    for (const bool recursive : {false, true}) {
        PthreadMutex mutex = CreateMutex(recursive);
        for (const bool timed : {false, true}) {
            if (!report) {
                for (int round = 0; round < 4; ++round) RunHandoff(mutex, timed, false, 3);
                continue;
            }
            std::vector<std::int64_t> latencies;
            for (int round = 0; round < 31; ++round) latencies.push_back(RunHandoff(mutex, timed, false, 3, round * 617 % 1000).latencyNanos);
            std::sort(latencies.begin(), latencies.end());
            const auto blocked = RunHandoff(mutex, timed, false, 200);
            const double share = 100.0 * static_cast<double>(blocked.waitCycles) / static_cast<double>(busyCycles);
            std::printf("%s %s: handoff median %.1f us, p90 %.1f us, max %.1f us; a waiter blocked for 200 ms used %.1f %% of one processor\n",
                recursive ? "recursive" : "errorcheck", timed ? "timed lock" : "lock",
                latencies[15] / 1000.0, latencies[27] / 1000.0, latencies[30] / 1000.0, share);
        }
        PthreadMutex held = mutex;
        Require(scePthreadMutexLock(&held) == 0, "cannot hold the mutex for the timeout");
        Pthread timeout = nullptr;
        Require(scePthreadCreate(&timeout, nullptr, TimeoutWorker, &mutex, "mutex timeout") == 0, "cannot create the timeout waiter");
        Require(scePthreadJoin(timeout, nullptr) == 0, "cannot join the timeout waiter");
        Require(scePthreadMutexUnlock(&held) == 0, "cannot release the mutex after the timeout");
        Contention context;
        context.mutex = mutex;
        context.rounds = report ? 0 : 20000;
        std::array<Pthread, 4> workers{};
        for (auto& worker : workers) Require(scePthreadCreate(&worker, nullptr, ContentionWorker, &context, "mutex contender") == 0, "cannot create a contender");
        const auto started = Clock::now();
        context.start.store(true);
        if (report) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            context.stop.store(true);
        }
        for (auto& worker : workers) Require(scePthreadJoin(worker, nullptr) == 0, "cannot join a contender");
        const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
        std::uint64_t total = 0;
        for (const auto count : context.acquired) total += count;
        Require(total == context.shared, "the mutex did not exclude its contenders");
        const auto fewest = *std::min_element(context.acquired.begin(), context.acquired.end());
        if (report) {
            std::printf("%s: four contenders made %.0f acquisitions per second, the slowest %.0f per second\n",
                recursive ? "recursive" : "errorcheck", total / seconds, fewest / seconds);
        } else {
            Require(fewest == context.rounds, "a contender did not finish its acquisitions");
        }
        Require(scePthreadMutexDestroy(&mutex) == 0, "cannot destroy the mutex");
        for (int round = 0; round < 200; ++round) RunHandoff(CreateMutex(recursive), round % 2 != 0, true, 0);
    }
    std::puts("guest mutex contention: PASS");
    return 0;
}
