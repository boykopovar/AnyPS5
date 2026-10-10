#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>
struct Timeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};
struct ResourceUsage {
    Timeval ru_utime;
    Timeval ru_stime;
    std::int64_t rest[14];
};
extern "C" int APS5_VABI getrusage_nid_postfix(int who, ResourceUsage* usage);
static void Require(bool value) { if (!value) std::abort(); }
static std::int64_t CpuMicros(int who) {
    ResourceUsage usage{};
    Require(getrusage_nid_postfix(who, &usage) == 0);
    return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000 + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
}
static void SpinUntil(int who, std::int64_t micros) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    volatile std::uint64_t sink = 0;
    while (CpuMicros(who) < micros) {
        for (int i = 0; i < 100000; ++i) sink = sink + i;
        Require(std::chrono::steady_clock::now() < deadline);
    }
}
int main() {
    SpinUntil(0, 400000);
    const std::int64_t mainThread = CpuMicros(1);
    Require(mainThread >= 200000);
    std::thread worker([] {
        const std::int64_t fresh = CpuMicros(1);
        Require(fresh < 100000);
        SpinUntil(1, fresh + 100000);
        Require(CpuMicros(0) >= CpuMicros(1) + 300000);
    });
    worker.join();
    Require(CpuMicros(0) >= 500000);
}
