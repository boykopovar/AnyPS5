#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t DmaDataOpcode = 0x50;

using AgcDriver::DriverDetail::DrawPipeline;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pipelined prefetch: " + what);
}

alignas(256) std::array<std::uint32_t, 1024> Source{};
alignas(256) std::array<std::uint32_t, 1024> Destination{};
alignas(64) std::array<std::uint32_t, 16> Label{};

std::array<std::uint32_t, 7> DmaData(std::uint32_t destinationSelect, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes) {
    return {0xc0055000u, destinationSelect << 20u, static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), bytes};
}

void AppendLabel(std::vector<std::uint32_t>& words, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(Label.data());
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, 0, 0});
}

struct Blocker {
    std::atomic<bool> release{false};
    void Enqueue() {
        DrawPipeline::Queue0().Enqueue([this] {
            while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }, {});
    }
};

void Finish() {
    auto idle = std::async(std::launch::async, [] { AgcDriverWaitIdle_nid_postfix(); });
    if (idle.wait_for(std::chrono::seconds(20)) != std::future_status::ready) {
        std::fputs("pipelined prefetch: the queue worker and the draw pipeline deadlocked\n", stderr);
        std::fflush(stderr);
        std::_Exit(1);
    }
    idle.get();
}

struct Behind {
    std::size_t outstanding;
    std::uint64_t drains;
};

Behind SubmitBehindBusyPipeline(const std::array<std::uint32_t, 7>& dma, std::uint32_t labelValue, bool expectQueued) {
    Label.fill(0);
    const auto before = DrawPipeline::DrainRequestsByOpcode(DmaDataOpcode);
    Blocker blocker;
    blocker.Enqueue();
    std::vector<std::uint32_t> words(dma.begin(), dma.end());
    AppendLabel(words, labelValue);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
    const auto start = std::chrono::steady_clock::now();
    std::size_t outstanding = 0;
    for (;;) {
        outstanding = DrawPipeline::Queue0().Outstanding();
        const auto waited = std::chrono::steady_clock::now() - start;
        if (expectQueued ? outstanding >= 2 : waited > std::chrono::milliseconds(1500)) break;
        if (waited > std::chrono::seconds(10)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    blocker.release = true;
    Finish();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Label.data()), sizeof(Label));
    check(Label[0] == labelValue, "the label behind the DMA_DATA did not land");
    return {outstanding, DrawPipeline::DrainRequestsByOpcode(DmaDataOpcode) - before};
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i * 2654435761u);
        Destination.fill(0);
        const auto source = reinterpret_cast<std::uintptr_t>(Source.data());
        const auto destination = reinterpret_cast<std::uintptr_t>(Destination.data());
        const auto prefetch = DmaData(2u, source, 0, static_cast<std::uint32_t>(sizeof(Source)));
        const auto copy = DmaData(0u, source, destination, 512);
        const auto prefetched = SubmitBehindBusyPipeline(prefetch, 0x11111111u, true);
        check(prefetched.outstanding >= 2, "a DMA_DATA prefetch drained the draw pipeline: the label behind it never queued behind the held commit (" + std::to_string(prefetched.outstanding) + " outstanding)");
        check(prefetched.drains == 0, "a DMA_DATA prefetch drained the draw pipeline (" + std::to_string(prefetched.drains) + " packet drains)");
        const auto copied = SubmitBehindBusyPipeline(copy, 0x22222222u, true);
        check(copied.outstanding >= 2, "a DMA_DATA copy that misses every queued write drained the draw pipeline: the label behind it never queued behind the held commit (" + std::to_string(copied.outstanding) + " outstanding)");
        check(copied.drains == 0, "a DMA_DATA copy that misses every queued write drained the draw pipeline (" + std::to_string(copied.drains) + " packet drains)");
        for (std::size_t i = 0; i < Destination.size(); ++i) check(Destination[i] == (i < 128 ? Source[i] : 0u), "the copy did not land, or wrote past its bytes");
        LibcRunShutdown_nid_postfix();
        std::puts("a DMA_DATA prefetch and a copy that misses every queued write keep the draw pipeline");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
