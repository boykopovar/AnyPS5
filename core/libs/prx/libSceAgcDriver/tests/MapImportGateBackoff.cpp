#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

namespace Gate = AgcDriver::Graphics::MapImportGate;
using Clock = std::chrono::steady_clock;

constexpr std::uint64_t Address = 0x200000000ull;
constexpr std::uint64_t Bytes = 0x10000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::fflush(stderr);
    std::exit(1);
}

double DrainLatencyMs(std::chrono::microseconds releaseOffset) {
    std::atomic<bool> published{false};
    std::atomic<bool> release{false};
    std::atomic<bool> imported{false};
    Clock::time_point releasedAt{};
    Clock::time_point importedAt{};

    std::thread publisher([&] {
        {
            const Gate::PublishHold hold(Address, Bytes);
            published.store(true, std::memory_order_release);
            while (!release.load(std::memory_order_acquire)) std::this_thread::yield();
            releasedAt = Clock::now();
        }
    });
    while (!published.load(std::memory_order_acquire)) std::this_thread::yield();

    const auto before = Gate::CountersForTest();
    std::thread importer([&] {
        const Gate::ImportHold hold(Gate::StripesOf(Address, Address + Bytes));
        importedAt = Clock::now();
        imported.store(true, std::memory_order_release);
    });
    while (Gate::DrainingImportsForTest() == 0) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto releaseAt = Clock::now() + releaseOffset;
    while (Clock::now() < releaseAt) std::this_thread::yield();
    const bool waited = !imported.load(std::memory_order_acquire);
    release.store(true, std::memory_order_release);
    publisher.join();
    importer.join();
    const auto after = Gate::CountersForTest();

    Require(waited, "the map-time import did not wait for the open publication");
    Require(after.drainWaits == before.drainWaits + 1, "the map-time import did not drain the fast publication");
    Require(after.importHolds == before.importHolds + 1, "the map-time import did not take its stripes");
    Require(Gate::DrainingImportsForTest() == 0, "a map-time import is still draining");
    return std::chrono::duration<double, std::milli>(importedAt - releasedAt).count();
}

}

int main() {
    std::array<double, 9> latencies{};
    for (std::size_t index = 0; index < latencies.size(); ++index) latencies[index] = DrainLatencyMs(std::chrono::microseconds(113 * index));
    for (const double latency : latencies) std::printf("import drain wake latency %.3f ms\n", latency);
    std::sort(latencies.begin(), latencies.end());
    const double median = latencies[latencies.size() / 2];
    std::printf("median %.3f ms (limit <= 4 ms, longest backoff step 1.024 ms)\n", median);
    std::fflush(stdout);
    Require(median <= 4.0, "the map-time import is retried later than its backoff's longest step");
    std::puts("a map-time import retries at its backoff's longest step after the publication ends");
    return 0;
}
