#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Attempts = 5;
constexpr std::string_view BlockedScope = "Driver.MemoryWait.blocked";
constexpr std::string_view QueueZero = "queue worker 0x0";
constexpr std::string_view ComputeQueue = "queue worker 0x20";

struct Slot {
    alignas(256) volatile std::uint32_t label = 0;
    alignas(256) volatile std::uint32_t gate = 0;
    alignas(256) volatile std::uint32_t flag = 0;
    alignas(256) volatile std::uint32_t graphicsDone = 0;
    alignas(256) volatile std::uint32_t computeDone = 0;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "wait attribution: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    Slot& Data() { return *reinterpret_cast<Slot*>(block); }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

void ReleaseLabel(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), Low(target), High(target), value, 0u, 0u});
}

void WriteData(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void WaitEqual(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(target), High(target), value, 0xffffffffu, 0x19u});
}

void SubmitGraphics(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

void SubmitCompute(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute submit failed");
}

void AwaitValue(volatile std::uint32_t& target, std::uint32_t value, const char* what) {
    const auto start = std::chrono::steady_clock::now();
    while (target != value) {
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(10), std::string(what) + " did not finish");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

std::uint64_t Exits(std::string_view thread, std::string_view stage) {
    for (std::uint32_t tries = 0; tries < 1000; ++tries) {
        const auto threads = AgcDriver::PerformanceTrace::ThreadTotals();
        if (!threads) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        std::uint64_t count = 0;
        for (const auto& entry : *threads) {
            if (entry.name != thread) continue;
            for (const auto& total : entry.totals) {
                if (total.stage != nullptr && std::string_view(total.scope) == BlockedScope && std::string_view(total.stage) == stage) count += total.count;
            }
        }
        return count;
    }
    throw std::runtime_error("wait attribution: the trace totals stayed locked");
}

std::uint64_t ExitsByComputeQueue() {
    return Exits(QueueZero, "recorder.queue_0x20") + Exits(QueueZero, "store.queue_0x20");
}

bool CrossQueueAttempt(Slot& slot, std::uint32_t attempt) {
    const std::uint32_t value = 100 + attempt;
    const auto computeBefore = Exits(ComputeQueue, "memory");
    const auto graphicsBefore = ExitsByComputeQueue();
    std::vector<std::uint32_t> compute;
    WaitEqual(compute, &slot.gate, attempt + 1);
    ReleaseLabel(compute, &slot.label, value);
    WriteData(compute, &slot.computeDone, attempt + 1);
    SubmitCompute(compute);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::vector<std::uint32_t> graphics;
    WaitEqual(graphics, &slot.label, value);
    WriteData(graphics, &slot.graphicsDone, attempt + 1);
    SubmitGraphics(graphics);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    slot.gate = attempt + 1;
    AwaitValue(slot.computeDone, attempt + 1, "the compute submission");
    AwaitValue(slot.graphicsDone, attempt + 1, "the queue 0 submission waiting on the compute label");
    AgcDriverWaitIdle_nid_postfix();
    const bool computeByStore = Exits(ComputeQueue, "memory") > computeBefore;
    const bool graphicsByCompute = ExitsByComputeQueue() > graphicsBefore;
    std::cout << "cross-queue attempt " << attempt << ": queue 0x20 wait ended by a CPU store " << computeByStore << ", queue 0 wait attributed to queue 0x20 " << graphicsByCompute << '\n';
    Require(computeByStore, "a queue 0x20 wait that a title CPU store satisfied was not reported as Driver.MemoryWait.blocked.memory");
    return graphicsByCompute;
}

void CpuStoreAttempt(Slot& slot, std::uint32_t attempt) {
    const auto before = Exits(QueueZero, "memory");
    std::vector<std::uint32_t> graphics;
    WaitEqual(graphics, &slot.flag, attempt + 1);
    WriteData(graphics, &slot.graphicsDone, 1000 + attempt);
    SubmitGraphics(graphics);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    slot.flag = attempt + 1;
    AwaitValue(slot.graphicsDone, 1000 + attempt, "the queue 0 submission waiting on a CPU flag");
    AgcDriverWaitIdle_nid_postfix();
    Require(Exits(QueueZero, "memory") > before, "a queue 0 wait that a title CPU store satisfied was not reported as Driver.MemoryWait.blocked.memory");
}

}

int main() {
    try {
        {
            auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Require(!AgcDriver::PerformanceTrace::Configured(), "wait attribution: run without APS5_TRACE_FRAMES, APS5_TRACE_SECONDS and APS5_TRACE_SUMMARY_EVERY");
        AgcDriver::PerformanceTrace::SetOverlayTotals(true);
        GuestBlock block;
        auto& slot = block.Data();
        for (std::uint32_t attempt = 0; attempt < Attempts; ++attempt) CpuStoreAttempt(slot, attempt);
        bool attributed = false;
        for (std::uint32_t attempt = 0; attempt < Attempts && !attributed; ++attempt) attributed = CrossQueueAttempt(slot, attempt);
        Require(attributed, "no queue 0 wait on a label that queue 0x20 released was reported as Driver.MemoryWait.blocked.recorder.queue_0x20 or .store.queue_0x20");
        AgcDriverShutdown_nid_postfix();
        std::cout << "wait attribution trace test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
