#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pipelined clock copy: " + what);
}

std::array<std::uint32_t, 6> ClockCopy(std::uint64_t destination) {
    return {0xc0044000u, 0x00010509u, 0u, 0u, static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u)};
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
}

struct Gate {
    std::atomic<bool> release{false};
    ~Gate() { release.store(true, std::memory_order_release); }
};

void Hold(Gate& gate, std::uint64_t address, std::size_t bytes) {
    DrawPipeline::Queue0().Enqueue([&gate] {
        while (!gate.release.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {{address, address + bytes}});
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        alignas(256) static std::array<std::uint64_t, 2> pending{};
        alignas(256) static std::array<std::uint64_t, 2> elsewhere{};
        const auto held = reinterpret_cast<std::uint64_t>(pending.data());
        const auto other = reinterpret_cast<std::uint64_t>(elsewhere.data());
        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            const auto before = DrawPipeline::DrainRequestsByOpcode(0x40);
            std::vector<std::uint32_t> words;
            for (const auto destination : {other, other + 8}) {
                const auto packet = ClockCopy(destination);
                words.insert(words.end(), packet.begin(), packet.end());
            }
            Submit(words);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (reinterpret_cast<volatile std::uint64_t*>(elsewhere.data())[1] == 0 && DrawPipeline::DrainRequestsByOpcode(0x40) == before && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            check(DrawPipeline::DrainRequestsByOpcode(0x40) == before, "a GPU-clock COPY_DATA off the pending range drained the pipeline");
        }
        AgcDriverWaitIdle_nid_postfix();
        check(elsewhere[0] != 0 && elsewhere[1] >= elsewhere[0], "a GPU-clock COPY_DATA did not store an increasing clock");

        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            const auto before = DrawPipeline::DrainRequestsByOpcode(0x40);
            std::thread releaser([&] {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                while (DrawPipeline::DrainRequestsByOpcode(0x40) == before && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                gate.release.store(true, std::memory_order_release);
            });
            std::vector<std::uint32_t> words;
            const auto packet = ClockCopy(held);
            words.insert(words.end(), packet.begin(), packet.end());
            Submit(words);
            releaser.join();
            check(DrawPipeline::DrainRequestsByOpcode(0x40) == before + 1, "a GPU-clock COPY_DATA onto an uncommitted pipeline write did not drain");
        }
        AgcDriverWaitIdle_nid_postfix();
        check(pending[0] != 0, "a GPU-clock COPY_DATA onto a pending range did not store the clock");

        LibcRunShutdown_nid_postfix();
        std::puts("pipelined clock copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
