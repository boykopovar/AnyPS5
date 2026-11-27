#include "Ngs2Test.hpp"
#include "prx/libSceNgs2.native/include/Api.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>

namespace {

struct ModuleQueueRecord {
    const void* buffer;
    std::uint64_t reserved;
    std::uint64_t count;
    std::uint64_t first;
};

struct ModuleQueue {
    const void* reservedPointer;
    ModuleQueueRecord rings[2];
    std::uint32_t lock;
    std::uint8_t currentRing;
    std::uint8_t padding[3];
    std::uint32_t itemSize;
    std::uint32_t ringLength;
};
static_assert(sizeof(ModuleQueue) == 0x58);
static_assert(offsetof(ModuleQueue, lock) == 0x48);

using Clock = std::chrono::steady_clock;

double WakeLatencyMs() {
    const std::uint32_t items[] = {0x11111111, 0x22222222};
    ModuleQueue queue{};
    queue.rings[0].buffer = items;
    queue.rings[0].count = 2;
    queue.itemSize = sizeof(items[0]);
    queue.ringLength = 2;
    std::atomic_ref<std::uint32_t> lock(queue.lock);
    lock.store(1, std::memory_order_release);

    std::atomic<bool> entered{false};
    std::atomic<bool> returned{false};
    std::uint32_t output[2] = {};
    int result = -1;
    Clock::time_point returnedAt{};
    std::thread enumerate([&] {
        entered.store(true, std::memory_order_release);
        result = sceNgs2ModuleQueueEnumItems(&queue, output, 2);
        returnedAt = Clock::now();
        returned.store(true, std::memory_order_release);
    });
    while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool blocked = !returned.load(std::memory_order_acquire);
    const auto releasedAt = Clock::now();
    lock.store(0, std::memory_order_release);
    enumerate.join();

    Require(blocked);
    Require(result == 2);
    Require(output[0] == items[0] && output[1] == items[1]);
    Require(lock.load(std::memory_order_acquire) == 0);
    return std::chrono::duration<double, std::milli>(returnedAt - releasedAt).count();
}

}

int main() {
    std::array<double, 9> latencies{};
    for (auto& latency : latencies) latency = WakeLatencyMs();
    for (const double latency : latencies) std::printf("queue lock wake latency %.3f ms\n", latency);
    std::sort(latencies.begin(), latencies.end());
    const double median = latencies[latencies.size() / 2];
    std::printf("median %.3f ms (limit <= 4 ms, longest backoff step 1 ms)\n", median);
    std::fflush(stdout);
    Require(median <= 4.0);
    std::puts("a held module queue lock is retried at the backoff's 1 ms step after it is released");
    return 0;
}
