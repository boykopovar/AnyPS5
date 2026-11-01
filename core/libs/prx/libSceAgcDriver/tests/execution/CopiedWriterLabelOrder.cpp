#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 32;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t RecordBytes = 16;
constexpr std::size_t StoredOffset = 8;
constexpr std::size_t CopiedRecordOffset = 0x1004;
constexpr std::size_t DirectRecordOffset = 0x1000;
constexpr std::uint32_t CpuValue = 0xc0001000u;
constexpr std::uint32_t GpuValue = 1;
constexpr std::uint32_t LabelValue = 1;

alignas(256) constexpr std::array<std::uint32_t, 6> StoreCode{
    0x7e000281u, 0x7e020288u, 0xe0701000u, 0x80000001u, 0xbf810000u, 0xbf810000u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "copied writer label order: cannot allocate a guest block");
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

    volatile std::uint32_t* Word(std::size_t offset) const { return reinterpret_cast<volatile std::uint32_t*>(block + offset); }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void RecordDispatch(AgcDriver::VulkanDevice& device, std::uint64_t record) {
    std::vector<std::uint32_t> userData(16, 0u);
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(record), static_cast<std::uint32_t>((record >> 32u) & 0xffffu), RecordBytes, 0x31016facu};
    std::copy(descriptor.begin(), descriptor.end(), userData.begin());
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

bool Drains() {
    return std::getenv("APS5_DRAIN_COMPLETION_LABELS") != nullptr;
}

VkDeviceSize StorageAlignment(AgcDriver::VulkanDevice& device) {
    return device.graphicsContext().limits.minStorageBufferOffsetAlignment;
}

std::array<std::byte, 4> LabelBytes(std::uint32_t value) {
    std::array<std::byte, 4> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    return bytes;
}

void WaitForGpu(AgcDriver::VulkanDevice& device, std::uint64_t epoch) {
    if (device.CanWaitUnlocked()) device.WaitRecorded(epoch);
    else std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

std::uint64_t SubmitLocked(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    return device.SubmitAndEpoch();
}

int LabelPass(AgcDriver::VulkanDevice& device, const GuestBlock& records, std::size_t recordOffset, const GuestBlock& labels, std::size_t labelOffset, std::uint64_t stamp, std::uint64_t writeBacks, int expectedReason, const std::string& what) {
    volatile std::uint32_t* stored = records.Word(recordOffset + StoredOffset);
    volatile std::uint32_t* label = labels.Word(labelOffset);
    *stored = CpuValue;
    *label = 0;
    RecordDispatch(device, records.Address() + recordOffset);
    const auto pending = Recorder::PendingWriteBackCompletions();
    if (writeBacks != 0 && pending == 0 && recordOffset % StorageAlignment(device) == 0) {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
        return VulkanTestSkipped;
    }
    Require(pending == writeBacks, what + ": " + std::to_string(pending) + " write-back completions are pending after the dispatch, expected " + std::to_string(writeBacks));
    const auto bytes = LabelBytes(LabelValue);
    int reason = 0;
    std::uint64_t epoch = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        reason = device.WriteLabelOnGpu(labels.Address() + labelOffset, bytes, stamp, 0, false);
        if (reason == 3) {
            device.WaitIdle();
            return 1;
        }
        Require(reason == expectedReason, what + ": the label after the dispatch was handled with reason " + std::to_string(reason) + ", expected " + std::to_string(expectedReason));
        if (reason >= 1 && reason <= 4) {
            if (reason != 1) device.WaitIdle();
            AgcDriver::GuestMemory::Write(labels.Address() + labelOffset, bytes, 4);
        } else if (reason == 5) {
            Require(Recorder::PendingWriteBackCompletions() == 1 && Recorder::PendingCompletionStores() == 1, what + ": the label behind the write-back drained the device or is not a pending completion store");
            const std::array<std::byte, 16> wide{};
            Require(device.WriteLabelOnGpu(labels.Address() + labelOffset + 0x100, wide, stamp, 0, false) == 0, what + ": a 16-byte store away from the copied writer and the pending completion store was not recorded on the GPU");
            epoch = device.SubmitAndEpoch();
        } else {
            device.SubmitRecorded(true);
        }
    }
    if (reason == 5) {
        WaitForGpu(device, epoch);
        Require(*label == 0, what + ": the label is visible before the batch that writes back the dispatch's record was reaped");
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.ReapRecorded(epoch);
        Require(Recorder::PendingCompletionStores() == 0 && Recorder::PendingWriteBackCompletions() == 0, what + ": completions outlived the reap of their batch");
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (*label != LabelValue) {
        Require(std::chrono::steady_clock::now() < deadline, what + ": the label never landed");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    const std::uint32_t seen = *stored;
    Require(seen == GpuValue, what + ": the label is visible but the record holds " + Hex(seen) + ", the dispatch stored " + Hex(GpuValue));
    *stored = CpuValue;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
    }
    const std::uint32_t kept = *stored;
    Require(kept == CpuValue, what + ": the record holds " + Hex(kept) + " after the drain, a write-back landed over the value the CPU wrote after the label (" + Hex(CpuValue) + ")");
    Require(Recorder::PendingWriteBackCompletions() == 0, what + ": write-back completions outlived the drain");
    return 0;
}

void StreamOrderPass(AgcDriver::VulkanDevice& device, const GuestBlock& copied, const GuestBlock& direct, const GuestBlock& labels, std::uint64_t stamp) {
    constexpr std::uint32_t First = 0x11u;
    constexpr std::uint32_t Second = 0x22u;
    const std::uint64_t address = labels.Address() + 0x4000;
    volatile std::uint32_t* label = labels.Word(0x4000);
    *label = 0;
    *copied.Word(CopiedRecordOffset + StoredOffset) = CpuValue;
    RecordDispatch(device, copied.Address() + CopiedRecordOffset);
    Require(Recorder::PendingWriteBackCompletions() == 1, "stream order: the copied dispatch has no pending write-back");
    const auto copiedEpoch = SubmitLocked(device);
    RecordDispatch(device, direct.Address() + DirectRecordOffset);
    std::uint64_t firstEpoch = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(Recorder::PendingWriteBackCompletions() == 1, "stream order: the copied writer was written back before the first label");
        const int reason = device.WriteLabelOnGpu(address, LabelBytes(First), stamp, 0, false);
        Require(reason == 5, "stream order: the first label behind the copied writer was handled with reason " + std::to_string(reason) + ", expected 5");
        firstEpoch = device.SubmitAndEpoch();
    }
    WaitForGpu(device, firstEpoch);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.ReapRecorded(copiedEpoch);
        Require(Recorder::PendingWriteBackCompletions() == 0, "stream order: the copied writer was not written back by the reap of its batch");
        Require(Recorder::PendingCompletionStores() == 1, "stream order: the first label's completion store is not pending after the copied writer's reap");
        Require(*label == 0, "stream order: the first label landed before its batch was reaped");
    }
    RecordDispatch(device, direct.Address() + DirectRecordOffset);
    std::uint64_t secondEpoch = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const int reason = device.WriteLabelOnGpu(address, LabelBytes(Second), stamp + 1, 0, false);
        Require(reason == 5, "stream order: a later label to the address of a pending completion store was handled with reason " + std::to_string(reason) + ", expected 5");
        secondEpoch = device.SubmitAndEpoch();
        Require(secondEpoch > firstEpoch, "stream order: the second label went into the first label's batch");
    }
    WaitForGpu(device, secondEpoch);
    const std::uint32_t early = *label;
    Require(early != Second, "stream order: the later label is visible while the earlier completion store to its address has not landed");
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.ReapRecorded(firstEpoch);
    Require(*label == First, "stream order: the label holds " + Hex(*label) + " after the first label's batch was reaped, expected " + Hex(First));
    device.ReapRecorded(secondEpoch);
    const std::uint32_t last = *label;
    Require(last == Second, "stream order: the label holds " + Hex(last) + " after the reap, the later label stored " + Hex(Second));
    Require(Recorder::PendingCompletionStores() == 0 && Recorder::PendingCompletionLabels() == 0, "stream order: completion stores outlived the reap of their batches");
    device.WaitIdle();
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_CPU_COPIES", "1");
        _putenv_s("APS5_NO_OPPORTUNISTIC_REAP", "1");
#else
        setenv("APS5_CPU_COPIES", "1", 1);
        setenv("APS5_NO_OPPORTUNISTIC_REAP", "1", 1);
#endif
        const GuestBlock copied;
        const GuestBlock direct;
        const GuestBlock labels;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const int copiedPass = LabelPass(*device, copied, CopiedRecordOffset, labels, 0x2000, 1, 1, Drains() ? 2 : 5, "copied writer");
        if (copiedPass == VulkanTestSkipped) {
            std::printf("skipped, the device reports a minStorageBufferOffsetAlignment of %llu, so the dispatch binds its record at +0x%zx in place and nothing is written back; the test needs an alignment above 4\n",
                static_cast<unsigned long long>(StorageAlignment(*device)), CopiedRecordOffset);
            return VulkanTestSkipped;
        }
        if (copiedPass != 0 || LabelPass(*device, direct, DirectRecordOffset, labels, 0x3000, 2, 0, 0, "direct writer") != 0) {
            std::puts("skipped, the device imports no host memory");
            return VulkanTestSkipped;
        }
        if (!Drains()) StreamOrderPass(*device, copied, direct, labels, 3);
        std::puts("copied writer label order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
