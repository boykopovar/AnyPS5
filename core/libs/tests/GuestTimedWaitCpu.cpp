#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

static void Require(bool value) {
    if (!value) std::abort();
}

#ifdef _WIN32
static void RequireCpuBound(std::uint64_t cpuNanos, std::uint64_t wallNanos, const char* name) {
    Require(wallNanos != 0);
    std::cout << name << ": calling-thread CPU " << std::fixed << std::setprecision(1)
              << (100.0 * static_cast<double>(cpuNanos) / static_cast<double>(wallNanos))
              << "% (limit <= 40%)" << std::defaultfloat << std::endl;
    Require(cpuNanos <= (wallNanos * 40ULL) / 100ULL);
}
#endif

constexpr int WAIT_ITERATIONS = 500;
constexpr int SHORT_WAIT_ITERATIONS = 1000;
constexpr std::uint64_t P99_WAIT_MAX_NANOS = 3000000ULL;
constexpr std::uint64_t MAX_WAIT_NANOS = 10000000ULL;

template<std::size_t N>
static void RequireLatencyBounds(std::array<std::uint64_t, N> samples, std::uint64_t requestedNanos, const char* name) {
    std::sort(samples.begin(), samples.end());
    constexpr std::size_t p99Index = (N * 99 + 99) / 100 - 1;
    Require(samples[p99Index] < P99_WAIT_MAX_NANOS);
    Require(samples.back() < MAX_WAIT_NANOS);
    std::cout << name << ": P99 overshoot " << (samples[p99Index] - requestedNanos) / 1000
              << " us (limit < " << (P99_WAIT_MAX_NANOS - requestedNanos) / 1000
              << " us), max overshoot " << (samples.back() - requestedNanos) / 1000
              << " us (limit < " << (MAX_WAIT_NANOS - requestedNanos) / 1000 << " us)" << std::endl;
}

#ifdef _WIN32
static std::uint64_t CurrentThreadCpuNanos() {
    FILETIME creation{}, exit{}, kernel{}, user{};
    Require(GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) != 0);
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return (k.QuadPart + u.QuadPart) * 100ULL;
}
#endif

static void TestUsleep500us() {
    std::array<std::uint64_t, WAIT_ITERATIONS> samples{};
#ifdef _WIN32
    const auto cpuStart = CurrentThreadCpuNanos();
#endif
    const auto wallStart = std::chrono::steady_clock::now();

    for (int i = 0; i < WAIT_ITERATIONS; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        Require(sceKernelUsleep_nid_postfix(500) == 0);
        const auto t1 = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
        samples[i] = static_cast<std::uint64_t>(elapsed.count());
        Require(elapsed >= std::chrono::microseconds(500));
    }

    const auto wallEnd = std::chrono::steady_clock::now();
#ifdef _WIN32
    const auto cpuEnd = CurrentThreadCpuNanos();
    const auto wallNanos = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count());
    const auto cpuNanos = cpuEnd - cpuStart;
    RequireCpuBound(cpuNanos, wallNanos, "sceKernelUsleep(500 us)");
#endif
    RequireLatencyBounds(samples, 500000ULL, "sceKernelUsleep(500 us)");
}

static void TestUsleep100us() {
    std::array<std::uint64_t, SHORT_WAIT_ITERATIONS> samples{};
#ifdef _WIN32
    const auto cpuStart = CurrentThreadCpuNanos();
#endif
    const auto wallStart = std::chrono::steady_clock::now();

    for (int i = 0; i < SHORT_WAIT_ITERATIONS; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        Require(sceKernelUsleep_nid_postfix(100) == 0);
        const auto t1 = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
        samples[i] = static_cast<std::uint64_t>(elapsed.count());
        Require(elapsed >= std::chrono::microseconds(100));
    }

    const auto wallEnd = std::chrono::steady_clock::now();
    RequireLatencyBounds(samples, 100000ULL, "sceKernelUsleep(100 us)");
#ifdef _WIN32
    const auto cpuEnd = CurrentThreadCpuNanos();
    const auto wallNanos = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count());
    const auto cpuNanos = cpuEnd - cpuStart;
    RequireCpuBound(cpuNanos, wallNanos, "sceKernelUsleep(100 us)");
#endif
}

static void TestCondTimedwait500us() {
    std::array<std::uint64_t, WAIT_ITERATIONS> samples{};
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == 0);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);
    Require(scePthreadMutexLock(&mutex) == 0);

#ifdef _WIN32
    const auto cpuStart = CurrentThreadCpuNanos();
#endif
    const auto wallStart = std::chrono::steady_clock::now();

    for (int i = 0; i < WAIT_ITERATIONS; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        const int result = scePthreadCondTimedwait(&cond, &mutex, 500);
        Require(result == SCE_KERNEL_ERROR_ETIMEDOUT);
        const auto t1 = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
        samples[i] = static_cast<std::uint64_t>(elapsed.count());
        Require(elapsed >= std::chrono::microseconds(500));
    }

    const auto wallEnd = std::chrono::steady_clock::now();
#ifdef _WIN32
    const auto cpuEnd = CurrentThreadCpuNanos();
    const auto wallNanos = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count());
    const auto cpuNanos = cpuEnd - cpuStart;
    RequireCpuBound(cpuNanos, wallNanos, "scePthreadCondTimedwait(500 us)");
#endif

    Require(scePthreadMutexUnlock(&mutex) == 0);
    Require(scePthreadCondDestroy(&cond) == 0);
    Require(scePthreadMutexDestroy(&mutex) == 0);

    RequireLatencyBounds(samples, 500000ULL, "scePthreadCondTimedwait(500 us)");
}

int main() {
    TestUsleep100us();
    TestUsleep500us();
    TestCondTimedwait500us();
    return 0;
}
