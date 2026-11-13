#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <semaphore>
#include <span>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static void UseDevice(std::shared_ptr<VulkanDevice> device) { Driver::Get().device = std::move(device); }
    static void Stop() { Driver::Get().stop(); }
    static void SubmitDueAfterCommit(VulkanDevice& device) { Driver::Get().submitDueAfterCommit(&device); }
    static void FlushBetweenPackets() { Driver::Get().flushBetweenPackets(0, 0xc0002d00u, false); }
};

}

namespace {

using namespace AgcDriver::DriverDetail;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Value = 0x31415926u;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = static_cast<std::byte*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::byte*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(data != nullptr, "stale packet flush: allocation failed");
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }
    std::uint64_t Address(std::size_t offset = 0) const { return reinterpret_cast<std::uintptr_t>(data + offset); }
    std::uint32_t Label() const {
        std::uint32_t value = 0;
        std::memcpy(&value, data + 256, sizeof(value));
        return value;
    }
private:
    std::byte* data = nullptr;
};

void Check(AgcDriver::VulkanDevice& device, GuestBlock& block, const std::string& mode) {
    const std::array<std::uint32_t, 4> zero{};
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Require(device.FillBuffer(block.Address(), BlockBytes, zero), "stale packet flush: test block was not imported");
        device.WaitIdle();
        Require(device.FillBuffer(block.Address(BlockBytes - 16), 16, zero), "stale packet flush: old batch did not open");
        if (mode == "stale-deadline") {
            const auto bytes = std::as_bytes(std::span{&Value, 1});
            const int outcome = device.WriteLabelOnGpu(block.Address(128), bytes, 1, 0, false);
            Require(outcome == 0 || outcome == 5, "stale packet flush: old label was not recorded on the GPU");
            Require(Recorder::PendingLabelSince().has_value(), "stale packet flush: old label has no deadline");
        } else {
            for (unsigned i = 0; i < 128; ++i) Recorder::CountRecordedWork();
        }
    }
    if (mode == "stale-deadline") std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto before = device.SubmittedEpoch();
    auto& deferred = deferredLabels();
    Require(deferred.labels.empty(), "stale packet flush: unexpected queued labels");
    DeferredLabel label{block.Address(256), sizeof(Value), {}};
    std::memcpy(label.bytes.data(), &Value, sizeof(Value));
    deferred.labels.push_back(label);
    deferred.since = std::chrono::steady_clock::now();
    if (mode == "queued-overdue") deferred.since -= std::chrono::seconds(120);
    const auto drains = DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Flush);
    std::binary_semaphore entered{0};
    DrawPipeline::Queue0().Enqueue([&] {
        entered.release();
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Flush) == drains) {
            Require(std::chrono::steady_clock::now() < limit, "stale packet flush: the caller never drained the pipeline");
            std::this_thread::yield();
        }
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        DriverTestAccess::SubmitDueAfterCommit(device);
        Require(device.SubmittedEpoch() == before + 1, "stale packet flush: committer did not submit the old batch");
        Require(device.FillBuffer(block.Address(BlockBytes - 16), 16, zero), "stale packet flush: replacement batch did not open");
        if (mode == "current-cap") {
            for (unsigned i = 0; i < 64; ++i) Recorder::CountRecordedWork();
        }
    }, {});
    entered.acquire();
    DrawPipeline::Active() = true;
    try {
        DriverTestAccess::FlushBetweenPackets();
    } catch (...) {
        DrawPipeline::Active() = false;
        throw;
    }
    DrawPipeline::Active() = false;
    const bool submit = mode == "current-cap" || mode == "queued-overdue";
    Require(device.SubmittedEpoch() == before + (submit ? 2 : 1), "stale packet flush: " + mode + " submitted " + std::to_string(device.SubmittedEpoch() - before) + " batches, expected " + (submit ? "2" : "1"));
    Require(Recorder::PendingLabelSince().has_value() != submit, "stale packet flush: replacement label has the wrong pending state");
    device.WaitIdle();
    Require(block.Label() == Value, "stale packet flush: explicit idle did not publish the GPU label");
}

}

int main(int argc, char** argv) {
    std::shared_ptr<AgcDriver::VulkanDevice> device;
    std::exception_ptr failure;
    try {
        Require(argc == 1 || argc == 2, "stale packet flush: expected at most one test mode");
        const std::string mode = argc == 2 ? argv[1] : "stale-cap";
        Require(mode == "stale-cap" || mode == "stale-deadline" || mode == "current-cap" || mode == "queued-overdue", "stale packet flush: unknown mode");
        device = OpenVulkanTestDevice();
        if (device == nullptr) return VulkanTestSkipped;
        DriverTestAccess::UseDevice(device);
        {
            GuestBlock block;
            try { Check(*device, block, mode); } catch (...) { failure = std::current_exception(); }
            DrawPipeline::Active() = false;
            try { DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission); } catch (...) { if (!failure) failure = std::current_exception(); }
            device->WaitIdle();
        }
        DriverTestAccess::Stop();
        device.reset();
        if (failure) std::rethrow_exception(failure);
        std::cout << "stale packet flush " << mode << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { DriverTestAccess::Stop(); } catch (...) {}
        return 1;
    }
}
