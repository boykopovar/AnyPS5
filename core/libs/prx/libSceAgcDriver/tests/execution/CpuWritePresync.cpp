#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::size_t CopyBytes = 256;
constexpr std::size_t BlockBytes = 2 * 65536;
constexpr std::size_t LabelOffset = 0x40;
constexpr std::size_t OutsideOffset = 0x8000;
constexpr std::uint32_t LabelValue = 0x5aa55aa5u;

alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{
    0x34020083, 0xe0341000, 0x80000201, 0xbf8c3f70, 0xe0741000, 0x80010201, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "CPU write pre-sync: cannot allocate the guest block");
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

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(CopyCode.data()), std::as_bytes(std::span(CopyCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(CopyCode.data()), CopyCode, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct Scenario {
    std::uint8_t* source;
    std::uint8_t* destination;
    std::vector<std::uint32_t> userData;
};

Scenario Prepare(std::uint8_t* block) {
    Scenario scenario{block, block + 65536, std::vector<std::uint32_t>(16, 0u)};
    const auto source = BufferDescriptor(scenario.source, CopyBytes);
    const auto destination = BufferDescriptor(scenario.destination, CopyBytes);
    std::copy(source.begin(), source.end(), scenario.userData.begin());
    std::copy(destination.begin(), destination.end(), scenario.userData.begin() + 4);
    return scenario;
}

void Reset(const Scenario& scenario) {
    std::memset(scenario.source, 0x11, BlockBytes / 2);
    std::memset(scenario.destination, 0x00, CopyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

void QueueLabel(std::uint64_t address) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "CPU write pre-sync: the device has no active recorder");
    static_cast<void>(recorder->Commands());
    const auto value = LabelValue;
    recorder->AfterCompletions(address, std::as_bytes(std::span(&value, 1)), 9000, 0, false);
    Require(Recorder::PendingCompletionLabels() == 1, "the label stored by a completion is not pending");
}

std::uint32_t CopiedWord(const Scenario& scenario, std::size_t offset) {
    std::uint32_t word = 0;
    std::memcpy(&word, scenario.destination + offset, sizeof(word));
    return word;
}

void RunLabelInSource(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    const auto copy = Compile(device, scenario.userData);
    QueueLabel(AddressOf(scenario.source + LabelOffset));
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(copy, {}, Threads, reinterpret_cast<std::uintptr_t>(CopyCode.data()));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.cpuWriteChecks > before.cpuWriteChecks, "stage A did not check the copy's source range for pending CPU-side stores");
    Require(after.cpuWriteWaits == before.cpuWriteWaits + 1 && after.presyncs == before.presyncs + 1, "stage A did not wait for the completion label in the copy's source range before the GPU mutex");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyCode.data()), std::move(prepared));
    }
    device.WaitIdle();
    Require(Recorder::PendingCompletionLabels() == 0, "the completion label is still pending after the copy");
    Require(CopiedWord(scenario, LabelOffset) == LabelValue, "the copy did not read the label its stage A waited for");
    Require(CopiedWord(scenario, 0) == 0x11111111u, "the copy did not read the rest of its source");
}

void RunLabelOutsideSource(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    const auto copy = Compile(device, scenario.userData);
    QueueLabel(AddressOf(scenario.source + OutsideOffset));
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(copy, {}, Threads, reinterpret_cast<std::uintptr_t>(CopyCode.data()));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.cpuWriteWaits == before.cpuWriteWaits && after.presyncs == before.presyncs, "stage A waited for a completion label outside the copy's source range");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyCode.data()), std::move(prepared));
    }
    device.WaitIdle();
    Require(CopiedWord(scenario, LabelOffset) == 0x11111111u, "the copy read something other than its source");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PRESYNC_CPU_WRITES", "1");
#else
    setenv("APS5_PRESYNC_CPU_WRITES", "1", 1);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->CanWaitUnlocked()) {
            std::puts("skipped, the device has no timeline semaphores: the pre-sync is not tested");
            return VulkanTestSkipped;
        }
        GuestBlock block;
        const auto scenario = Prepare(block.Data());
        RunLabelInSource(*device, scenario);
        RunLabelOutsideSource(*device, scenario);
        std::puts("CPU write pre-sync tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
