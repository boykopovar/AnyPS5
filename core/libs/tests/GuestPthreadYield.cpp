#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int LONE_YIELDS = 4096;
static constexpr int ROUNDS = 64;
static constexpr std::uint64_t YIELD_LIMIT = 100000ull;

static void Require(bool value) { if (!value) std::abort(); }

static unsigned CurrentHostCpu() {
#ifdef _WIN32
    return GetCurrentProcessorNumber();
#else
    const int cpu = sched_getcpu();
    Require(cpu >= 0 && cpu < CPU_SETSIZE);
    return static_cast<unsigned>(cpu);
#endif
}

static bool PinHostThread(unsigned cpu) {
#ifdef _WIN32
    return SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << cpu) != 0;
#else
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(cpu, &one);
    return pthread_setaffinity_np(pthread_self(), sizeof(one), &one) == 0;
#endif
}

static std::atomic<bool> workerPinned{false};
static std::atomic<bool> workerReady{false};
static std::atomic<bool> start{false};
static std::atomic<int> turn{0};

static void WaitTurn(int value, std::uint64_t& yields) {
    while (turn.load() != value) {
        scePthreadYield();
        Require(++yields < YIELD_LIMIT);
    }
}

static void* APS5_VABI PingPong(void* arg) {
    const auto cpu = *static_cast<const unsigned*>(arg);
    workerPinned.store(PinHostThread(cpu));
    workerReady.store(true);
    workerReady.notify_one();
    start.wait(false);
    std::uint64_t yields = 0;
    for (int round = 0; round < ROUNDS; ++round) {
        WaitTurn(2 * round + 1, yields);
        turn.store(2 * round + 2);
    }
    return nullptr;
}

int main() {
    unsigned cpu = CurrentHostCpu();
    Require(PinHostThread(cpu));

    for (int call = 0; call < LONE_YIELDS; ++call) scePthreadYield();

    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, PingPong, &cpu, nullptr) == SCE_OK);
    workerReady.wait(false);
    Require(workerPinned.load());

    std::uint64_t yields = 0;
    start.store(true);
    start.notify_one();
    for (int round = 0; round < ROUNDS; ++round) {
        Require(turn.load() == 2 * round);
        turn.store(2 * round + 1);
        WaitTurn(2 * round + 2, yields);
    }
    Require(scePthreadJoin(worker, nullptr) == SCE_OK);
    return 0;
}
