#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <x86intrin.h>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main(int argc, char** argv) {
    constexpr std::uint64_t minuteMicros = 60ULL * 1000 * 1000;
    constexpr std::uint64_t sleptMicros = 50ULL * 1000;
    const char* scaleText = std::getenv("APS5_TIME_SCALE");
    const double scale = scaleText ? std::strtod(scaleText, nullptr) : 1.0;
    const auto elapsedMicros = static_cast<std::uint64_t>(static_cast<double>(sleptMicros) * (scale > 0.0 ? scale : 1.0));
    const auto minimumElapsedMicros = elapsedMicros * 19 / 20;
    if (argc > 1 && std::strcmp(argv[1], "rdtsc") == 0) {
        const std::uint64_t frequency = sceKernelGetTscFrequency();
        const std::uint64_t nativeStart = __rdtsc();
        const std::uint64_t readStart = sceKernelReadTsc();
        const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(sleptMicros);
        while (std::chrono::steady_clock::now() < until) std::this_thread::sleep_until(until);
        const std::uint64_t nativeMicros = (__rdtsc() - nativeStart) * 1000000 / frequency;
        const std::uint64_t readMicros = (sceKernelReadTsc() - readStart) * 1000000 / frequency;
        Require(nativeMicros >= minimumElapsedMicros && nativeMicros < elapsedMicros * 3 / 2, "native rdtsc ticks over the reported TSC frequency do not follow the time scale");
        Require(readMicros >= minimumElapsedMicros && readMicros < elapsedMicros * 3 / 2, "sceKernelReadTsc ticks over the reported TSC frequency do not follow the time scale");
        return 0;
    }
    const auto slept = std::chrono::steady_clock::now() + std::chrono::microseconds(sleptMicros);
    while (std::chrono::steady_clock::now() < slept) std::this_thread::sleep_until(slept);
    if (argc > 1 && std::strcmp(argv[1], "tsc") == 0) {
        const std::uint64_t frequency = sceKernelGetTscFrequency();
        const auto start = std::chrono::steady_clock::now();
        const std::uint64_t before = sceKernelReadTsc();
        const auto until = start + std::chrono::microseconds(sleptMicros);
        while (std::chrono::steady_clock::now() < until) std::this_thread::sleep_until(until);
        const std::uint64_t ticks = sceKernelReadTsc() - before;
        const auto measuredMicros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        const std::uint64_t tscMicros = ticks / frequency * 1000000 + ticks % frequency * 1000000 / frequency;
        Require(tscMicros >= minimumElapsedMicros, "TSC advanced less than the sleep at the TSC frequency");
        Require(static_cast<double>(tscMicros) <= static_cast<double>(measuredMicros) * (scale > 0.0 ? scale : 1.0) * 1.05, "TSC advanced more than the measured time at the time scale");
        Require(sceKernelReadTsc() >= before + ticks, "TSC went backwards");
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "counter") == 0) {
        const std::uint64_t counter = sceKernelGetProcessTimeCounter();
        const std::uint64_t frequency = sceKernelGetProcessTimeCounterFrequency();
        Require(frequency == sceKernelGetTscFrequency(), "process time counter frequency differs from the TSC frequency");
        const std::uint64_t counterMicros = counter / frequency * 1000000 + counter % frequency * 1000000 / frequency;
        Require(counterMicros < minuteMicros, "first process time counter wrapped");
        Require(counterMicros >= minimumElapsedMicros, "first process time counter does not include the time before the first call");
        Require(sceKernelGetProcessTimeCounter() >= counter, "process time counter went backwards");
        return 0;
    }
    const std::uint64_t first = sceKernelGetProcessTime();
    Require(first < minuteMicros, "first process time wrapped");
    Require(first >= minimumElapsedMicros, "first process time does not include the time before the first call");
    Require(sceKernelGetProcessTime() >= first, "process time went backwards");
    return 0;
}
