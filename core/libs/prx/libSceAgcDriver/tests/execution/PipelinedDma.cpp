#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pipelined DMA: " + what);
}

std::array<std::uint32_t, 7> DmaData(bool immediate, std::uint64_t source, std::uint32_t data, std::uint64_t destination, std::uint32_t bytes) {
    return {0xc0055000u, immediate ? (2u << 29u) : 0u, immediate ? data : static_cast<std::uint32_t>(source), immediate ? 0u : static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), bytes};
}

std::array<std::uint32_t, 7> DmaSelect(std::uint32_t destinationSelect, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes) {
    return {0xc0055000u, destinationSelect << 20u, static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), bytes};
}

template <std::size_t Count>
void Append(std::vector<std::uint32_t>& words, const std::array<std::uint32_t, Count>& packet) {
    words.insert(words.end(), packet.begin(), packet.end());
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
}

struct Gate {
    std::shared_ptr<std::atomic<bool>> release = std::make_shared<std::atomic<bool>>(false);
    ~Gate() { release->store(true, std::memory_order_release); }
};

void Hold(Gate& gate, std::uint64_t address, std::size_t bytes) {
    DrawPipeline::Queue0().Enqueue([release = gate.release] {
        while (!release->load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {{address, address + bytes}});
}

void ExpectDrains(std::vector<std::uint32_t>& words, std::uint32_t opcode, Gate& gate, const char* what) {
    const auto before = DrawPipeline::DrainRequestsByOpcode(opcode);
    std::thread releaser([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (DrawPipeline::DrainRequestsByOpcode(opcode) == before && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        gate.release->store(true, std::memory_order_release);
    });
    Submit(words);
    releaser.join();
    check(DrawPipeline::DrainRequestsByOpcode(opcode) == before + 1, what);
}

void ExpectNoDrain(std::uint32_t opcode, std::uint64_t before, const char* what) {
    check(DrawPipeline::DrainRequestsByOpcode(opcode) == before, what);
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        alignas(256) static std::array<std::uint32_t, 1024> first{};
        alignas(256) static std::array<std::uint32_t, 1024> second{};
        alignas(256) static std::array<std::uint32_t, 4> pending{};
        alignas(256) static std::array<std::uint32_t, 4> elsewhere{};
        first.fill(0);
        second.fill(0);
        pending.fill(0);
        elsewhere.fill(0);
        using namespace AgcDriver::DriverDetail;
        const auto drainedBefore = storesDrained.load();
        const auto cpuBefore = storesOnCpu.load() + storesOnGpu.load() + storesBehindCompletions.load();
        std::vector<std::uint32_t> words;
        const auto fill = DmaData(true, 0, 0x5eed1234u, reinterpret_cast<std::uintptr_t>(first.data()), static_cast<std::uint32_t>(sizeof(first)));
        const auto copy = DmaData(false, reinterpret_cast<std::uintptr_t>(first.data()), 0, reinterpret_cast<std::uintptr_t>(second.data()), 512);
        words.insert(words.end(), fill.begin(), fill.end());
        words.insert(words.end(), copy.begin(), copy.end());
        Submit(words);
        AgcDriverWaitIdle_nid_postfix();
        for (const auto word : first) check(word == 0x5eed1234u, "the immediate fill did not land");
        for (std::size_t i = 0; i < second.size(); ++i) check(second[i] == (i < 128 ? 0x5eed1234u : 0u), "the copy did not see the fill before it, or wrote past its bytes");
        const auto stores = storesOnCpu.load() + storesOnGpu.load() + storesBehindCompletions.load() - cpuBefore;
        check(stores == 2, "the DMA_DATA packets were not committed (" + std::to_string(stores) + " committed stores)");
        check(storesDrained.load() == drainedBefore, "a DMA_DATA packet still drained");

        const auto source = reinterpret_cast<std::uint64_t>(first.data());
        const auto held = reinterpret_cast<std::uint64_t>(pending.data());
        const auto other = reinterpret_cast<std::uint64_t>(elsewhere.data());
        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            const auto dmaBefore = DrawPipeline::DrainRequestsByOpcode(0x50);
            const auto predicationBefore = DrawPipeline::DrainRequestsByOpcode(0x20);
            const auto nopBefore = DrawPipeline::DrainRequestsByOpcode(0x10);
            words.clear();
            Append(words, DmaSelect(2, source, 0, 64));
            Append(words, DmaData(false, source, 0, other, 0));
            Append(words, DmaSelect(1, source, 0, 4));
            Append(words, DmaData(false, source, 0, other, 2));
            Append(words, std::array<std::uint32_t, 4>{0xc0022000u, 0x31100u, static_cast<std::uint32_t>(held), static_cast<std::uint32_t>(held >> 32u)});
            Append(words, std::array<std::uint32_t, 2>{0xc000102cu, 0x00636261u});
            Append(words, std::array<std::uint32_t, 2>{0xc0001030u, 0u});
            Submit(words);
            ExpectNoDrain(0x50, dmaBefore, "a prefetch, a zero-byte copy, a GDS copy or an unaligned copy off the pending range drained");
            ExpectNoDrain(0x20, predicationBefore, "SET_PREDICATION drained the pipeline");
            ExpectNoDrain(0x10, nopBefore, "a marker push or pop drained the pipeline");
        }
        AgcDriverWaitIdle_nid_postfix();

        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            words.clear();
            Append(words, DmaData(false, source, 0, held, 2));
            ExpectDrains(words, 0x50, gate, "a copy onto an uncommitted pipeline write did not drain");
        }
        AgcDriverWaitIdle_nid_postfix();

        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            words.clear();
            Append(words, DmaData(false, held, 0, other, 2));
            ExpectDrains(words, 0x50, gate, "an unaligned copy of an uncommitted pipeline write did not drain");
        }
        AgcDriverWaitIdle_nid_postfix();

        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            words.clear();
            Append(words, std::array<std::uint32_t, 4>{0xc0022000u, 0x31100u, static_cast<std::uint32_t>(held), static_cast<std::uint32_t>(held >> 32u)});
            Append(words, std::array<std::uint32_t, 2>{0xc0001301u, 32u});
            ExpectDrains(words, 0x13, gate, "a predicated packet whose predicate overlaps an uncommitted write did not drain");
        }
        AgcDriverWaitIdle_nid_postfix();

        {
            Gate gate;
            Hold(gate, held, sizeof(pending));
            const auto resetBefore = DrawPipeline::DrainRequestsByOpcode(0x10);
            words.clear();
            Append(words, std::array<std::uint32_t, 2>{0xc0001024u, 0u});
            Submit(words);
            ExpectNoDrain(0x10, resetBefore, "DISPATCH_RESET drained the pipeline");
        }

        LibcRunShutdown_nid_postfix();
        std::puts("pipelined DMA tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
