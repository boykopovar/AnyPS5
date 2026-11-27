#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>

namespace {

using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
using Settle = AgcDriver::VulkanDevice::CpuStoreSettle;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t RangeBytes = 4096;
constexpr std::size_t Written = 4096;
constexpr std::size_t Stored = 32768;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "cannot allocate the CPU store test block");
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

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    const std::uint32_t* Words() const { return block; }

private:
    std::uint32_t* block = nullptr;
};

constexpr std::array<std::uint32_t, 4> Pattern{0x11223344u, 0x55667788u, 0x99aabbccu, 0xddeeff00u};

Recorder& Prepare(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const std::array<std::uint32_t, 4> zero{};
    Require(device.FillBuffer(block.Address(), BlockBytes, zero), "the device must import the CPU store test block");
    device.WaitIdle();
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr && recorder->Idle(), "the recorder is not idle after the setup drain");
    return *recorder;
}

void RecordWriter(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Require(device.FillBuffer(block.Address() + Written, RangeBytes, Pattern), "the GPU writer was not recorded");
}

void CheckWriter(const GuestBlock& block) {
    for (std::size_t i = 0; i < RangeBytes / 4; ++i) Require(block.Words()[Written / 4 + i] == Pattern[i % 4], "the GPU writer did not land");
}

void Idle(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(device, block);
    Require(device.SettleForCpuStore(block.Address() + Stored, RangeBytes) == Settle::None, "an idle device made a CPU store wait");
}

void DisjointWriter(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    auto& recorder = Prepare(device, block);
    RecordWriter(device, block);
    if (!Recorder::ReadTracking()) {
        Require(device.SettleForCpuStore(block.Address() + Stored, RangeBytes) == Settle::Drain, "without read tracking a CPU store beside recorded work did not drain");
        CheckWriter(block);
        return;
    }
    Require(device.SettleForCpuStore(block.Address() + Stored, RangeBytes) == Settle::None, "a CPU store waited for a writer of another range");
    Require(!recorder.Idle(), "a CPU store beside a recorded writer drained the recorder");
    device.WaitIdle();
    CheckWriter(block);
}

void OverlappingWriter(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    auto& recorder = Prepare(device, block);
    RecordWriter(device, block);
    Require(device.SettleForCpuStore(block.Address() + Written + RangeBytes / 2, RangeBytes) == (Recorder::ReadTracking() ? Settle::Writers : Settle::Drain), "a CPU store over a recorded writer did not wait for it");
    Require(!recorder.PendingWriteOverlaps(block.Address() + Written, RangeBytes), "the writer is still pending after the settle");
    CheckWriter(block);
}

void Label(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    auto& recorder = Prepare(device, block);
    RecordWriter(device, block);
    const std::uint32_t value = 0x5a5a5a5au;
    Require(device.WriteLabelOnGpu(block.Address() + Stored, std::as_bytes(std::span(&value, 1)), 10, 0, false) == 0, "the label was not stored on the GPU");
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(device.SettleForCpuStore(block.Address() + Stored, RangeBytes) == Settle::Drain, "a CPU store over a pending label did not drain");
    Require(recorder.Idle(), "the drain left work in flight");
    Require(block.Words()[Stored / 4] == value, "the label did not land before the CPU store");
    CheckWriter(block);
}

void Reader(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    auto& recorder = Prepare(device, block);
    RecordWriter(device, block);
    recorder.NotePendingRead(block.Address() + Stored, RangeBytes, Recorder::ReadKind::GpuCopy);
    Require(!Recorder::ReadTracking() || recorder.PendingReadOverlaps(block.Address() + Stored, RangeBytes), "the pending read was not noted");
    Require(device.SettleForCpuStore(block.Address() + Stored, RangeBytes) == Settle::Drain, "a CPU store over a pending reader did not drain");
    Require(recorder.Idle(), "the drain left work in flight");
    CheckWriter(block);
}

}

int main() {
    try {
        GuestBlock block;
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        if (std::getenv("APS5_CPU_FILL_WAIT_IDLE") != nullptr) {
            Prepare(*device, block);
            RecordWriter(*device, block);
            Require(device->SettleForCpuStore(block.Address() + Stored, RangeBytes) == Settle::Drain, "APS5_CPU_FILL_WAIT_IDLE did not drain");
            CheckWriter(block);
        } else {
            Idle(*device, block);
            DisjointWriter(*device, block);
            OverlappingWriter(*device, block);
            Label(*device, block);
            Reader(*device, block);
        }
        std::cout << "CPU store settle tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
