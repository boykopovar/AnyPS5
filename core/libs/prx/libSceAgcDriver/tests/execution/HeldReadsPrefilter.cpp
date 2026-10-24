#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

constexpr std::uint64_t HeldAddress = 0x10000000ull;
constexpr std::size_t HeldBytes = 0x1000;
constexpr std::uint64_t OtherAddress = 0x20000000ull;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

struct Parked {
    std::atomic<bool> release{false};
    std::atomic<bool> running{false};
};

void Park(Parked& parked, std::vector<DrawPipeline::Range> reads) {
    DrawPipeline::Queue0().Enqueue([&parked] {
        parked.running = true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!parked.release.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {}, 0, {}, std::move(reads), true);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!parked.running.load()) {
        Require(std::chrono::steady_clock::now() < deadline, "the parked commit never started");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Release(Parked& parked) {
    parked.release = true;
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
}

}

int main() {
    try {
        if (DrawPipeline::Depth() == 0) return 77;
        {
            Parked parked;
            Park(parked, {});
            const auto locked = DrawPipeline::HeldReadQueries();
            Require(!DrawPipeline::HeldReads(HeldAddress, HeldBytes), "a queue holding no read ranges reported a held read");
            Require(DrawPipeline::HeldReadQueries() == locked, "a query against a queue holding no read ranges took the pipeline lock");
            Release(parked);
        }
        {
            Parked parked;
            Park(parked, {{HeldAddress, HeldAddress + HeldBytes}});
            Require(DrawPipeline::HeldReads(HeldAddress + 16, 16), "a read range held by the queued item was not reported");
            Require(!DrawPipeline::HeldReads(OtherAddress, HeldBytes), "a range no queued item reads was reported as held");
            Release(parked);
        }
        Require(!DrawPipeline::HeldReads(HeldAddress, HeldBytes), "a drained queue still reported a held read");
        std::puts("held read prefilter tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
