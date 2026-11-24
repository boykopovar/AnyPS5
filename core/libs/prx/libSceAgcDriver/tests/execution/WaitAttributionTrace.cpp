#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
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

alignas(256) std::array<std::uint32_t, 15> SpinCode{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};

constexpr std::array<std::string_view, 15> DispatchDrainSites{"msaa_clear", "htile_patch", "not_candidate", "memory_value_fill", "cache_miss", "cache_unchecked", "verify_data", "reads_queued", "writes_unbounded", "no_recipe", "not_plain", "item_not_plain", "item_reads_queued", "item_writes_unbounded", "fill_inputs"};

struct Slot {
    alignas(256) std::array<std::uint32_t, 4> spinOutput{};
    alignas(256) std::array<std::uint32_t, 4> spinDescriptor{};
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

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Dispatch(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(words, offset, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
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

template <typename Matches>
std::uint64_t Totals(std::string_view thread, Matches matches) {
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
                if (total.stage != nullptr && matches(std::string_view(total.scope), std::string_view(total.stage))) count += total.count;
            }
        }
        return count;
    }
    throw std::runtime_error("wait attribution: the trace totals stayed locked");
}

std::uint64_t Exits(std::string_view thread, std::string_view stage) {
    return Totals(thread, [&](std::string_view scope, std::string_view name) { return scope == BlockedScope && name == stage; });
}

std::uint64_t DispatchDrains() {
    return Totals(QueueZero, [](std::string_view scope, std::string_view name) {
        return scope == "DrawPipeline.Drain.packet" && std::find(DispatchDrainSites.begin(), DispatchDrainSites.end(), name) != DispatchDrainSites.end();
    });
}

bool DispatchDrainAttempt(Slot& slot, std::uint32_t attempt) {
    using AgcDriver::DriverDetail::DrawPipeline;
    const auto before = DispatchDrains();
    std::atomic<bool> release{false};
    DrawPipeline::Queue0().Enqueue([&release] {
        const auto start = std::chrono::steady_clock::now();
        while (!release.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) std::this_thread::sleep_for(std::chrono::microseconds(100));
    }, {});
    slot.spinDescriptor = {Low(slot.spinOutput.data()), High(slot.spinOutput.data()) & 0xffffu, sizeof(slot.spinOutput), 0x31016facu};
    std::vector<std::uint32_t> graphics;
    Dispatch(graphics, SpinCode.data(), {Low(slot.spinDescriptor.data()), High(slot.spinDescriptor.data()), 64});
    WriteData(graphics, &slot.graphicsDone, 2000 + attempt);
    SubmitGraphics(graphics);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    release.store(true, std::memory_order_release);
    AwaitValue(slot.graphicsDone, 2000 + attempt, "the queue 0 dispatch behind a queued item");
    AgcDriverWaitIdle_nid_postfix();
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    const bool attributed = DispatchDrains() > before;
    std::cout << "dispatch drain attempt " << attempt << ": attributed to a dispatch site " << attributed << '\n';
    return attributed;
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
        if (AgcDriver::DriverDetail::DrawPipeline::Depth() == 0) {
            std::cout << "pipelined draws off: drains inside a dispatch not tested\n";
        } else {
            bool drained = false;
            for (std::uint32_t attempt = 0; attempt < Attempts && !drained; ++attempt) drained = DispatchDrainAttempt(slot, attempt);
            Require(drained, "no drain that queue 0 took inside a dispatch behind a queued item was reported as DrawPipeline.Drain.packet.<dispatch site>");
        }
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
