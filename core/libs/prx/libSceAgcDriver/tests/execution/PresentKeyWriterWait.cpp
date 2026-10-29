#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint64_t DisplayFormat = 0x8000000000000000ull;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeysOffset = SurfaceBytes;
constexpr std::size_t KeysCapacity = 32768;
constexpr std::size_t CopiedKeys = 256;
constexpr std::size_t SourceKeysOffset = KeysOffset + KeysCapacity;
constexpr std::size_t BlockBytes = 2 * 65536;
constexpr std::uint8_t Uncompressed = 0xff;
constexpr std::uint8_t Cleared = 0x00;

alignas(256) constexpr std::array<std::uint32_t, 7> CopyKeysCode{
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
        Require(block != nullptr, "present key writer wait: cannot allocate the guest block");
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

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct Scenario {
    std::uint8_t* surface;
    std::uint8_t* keys;
    std::uint8_t* source;
    std::size_t keyBytes;
    AgcDriver::DisplayBuffer buffer;
    std::vector<std::uint32_t> copyData;
};

Scenario Prepare(std::uint8_t* block) {
    Scenario scenario{block, block + KeysOffset, block + SourceKeysOffset, 0, {AddressOf(block), DisplayFormat, Side, Side}, std::vector<std::uint32_t>(16, 0u)};
    scenario.buffer.dccAddress = AddressOf(scenario.keys);
    scenario.keyBytes = AgcDriver::DisplayBufferKeyBytes(scenario.buffer);
    Require(AgcDriver::DisplayBufferSize(scenario.buffer) <= SurfaceBytes && scenario.keyBytes >= CopiedKeys && scenario.keyBytes <= KeysCapacity, "present key writer wait: the display buffer does not fit the guest block");
    const auto source = BufferDescriptor(scenario.source, CopiedKeys);
    const auto destination = BufferDescriptor(scenario.keys, CopiedKeys);
    std::copy(source.begin(), source.end(), scenario.copyData.begin());
    std::copy(destination.begin(), destination.end(), scenario.copyData.begin() + 4);
    return scenario;
}

void Reset(const Scenario& scenario) {
    std::memset(scenario.surface, 0x55, SurfaceBytes);
    std::memset(scenario.keys, Uncompressed, scenario.keyBytes);
    std::memset(scenario.keys, Cleared, CopiedKeys);
    std::memset(scenario.source, Uncompressed, CopiedKeys);
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

std::string PresentFailure(AgcDriver::VulkanDevice& device, const AgcDriver::DisplayBuffer& buffer) {
    try {
        device.PresentDisplayBuffer(buffer);
    } catch (const std::exception& error) {
        return error.what();
    }
    return "the headless present succeeded";
}

const std::string NoSwapchain = "Vulkan presentation: device has no swapchain";

bool CanCopyKeys(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    const std::array<std::uint32_t, 4> pattern{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const bool filled = device.FillBuffer(AddressOf(scenario.keys), CopiedKeys, pattern);
    device.WaitIdle();
    return filled;
}

void RunPendingWriter(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    const auto copy = Compile(device, CopyKeysCode, scenario.copyData);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Require(AgcDriver::GuestMemory::GpuMutex().DepthOnThisThread() == 1, "present key writer wait: the presenter's hold is not the outermost");
    device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyKeysCode.data()));
    Require(AgcDriver::Graphics::DccKeyWritersPending(scenario.buffer.dccAddress, scenario.keyBytes), "a recorded kernel that copies data over the display keys is not seen as a pending key writer");
    const auto holds = AgcDriver::GuestMemory::GpuHoldSerial();
    const auto failure = PresentFailure(device, scenario.buffer);
    const auto acquired = AgcDriver::GuestMemory::GpuHoldSerial() - holds;
    Require(failure == NoSwapchain, "a present behind a pending writer of the display keys stopped before the swapchain: " + failure);
    Require(acquired != 0, "the presenter synchronized with the display keys' writer without releasing the GPU mutex");
    Require(!AgcDriver::Graphics::DccKeyWritersPending(scenario.buffer.dccAddress, scenario.keyBytes), "the display keys' writer is still pending after the present");
    Require(scenario.keys[0] == Uncompressed && scenario.keys[CopiedKeys - 1] == Uncompressed, "the present read the display keys before their writer landed");
}

void RunNoWriter(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    std::memset(scenario.keys, Uncompressed, scenario.keyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.WaitIdle();
    Require(!AgcDriver::Graphics::DccKeyWritersPending(scenario.buffer.dccAddress, scenario.keyBytes), "the idle device still reports a pending writer of the display keys");
    const auto holds = AgcDriver::GuestMemory::GpuHoldSerial();
    const auto failure = PresentFailure(device, scenario.buffer);
    const auto acquired = AgcDriver::GuestMemory::GpuHoldSerial() - holds;
    Require(failure == NoSwapchain, "a present with no pending writer of the display keys stopped before the swapchain: " + failure);
    Require(acquired == 0, "a present with no pending writer of the display keys released the GPU mutex");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->CanWaitUnlocked()) {
            std::puts("skipped, the device has no timeline semaphores: the presenter cannot wait without the GPU mutex");
            return VulkanTestSkipped;
        }
        GuestBlock block;
        const auto scenario = Prepare(block.Data());
        if (!CanCopyKeys(*device, scenario)) {
            std::puts("skipped, host imports are unavailable: the key writer of the test is a copied writer");
            return VulkanTestSkipped;
        }
        RunPendingWriter(*device, scenario);
        RunNoWriter(*device, scenario);
        std::puts("present key writer wait tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
