#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t OutputBytes = Threads * 16u * 4u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;

alignas(256) constexpr std::array<std::uint32_t, 78> LoadsCode{
    0x34020084, 0x34040086, 0xdc388000, 0x04080001, 0xbf8c3f70, 0x7e1402ff, 0x5a5a0000, 0x7e1602ff,
    0x5a5a0001, 0x7e1802ff, 0x5a5a0002, 0x7e1a02ff, 0x5a5a0003, 0x7e1c02ff, 0x5a5a0004, 0x7e1e02ff,
    0x5a5a0005, 0x7e2002ff, 0x5a5a0006, 0x7e2202ff, 0x5a5a0007, 0x7e2402ff, 0x5a5a0008, 0x7e2602ff,
    0x5a5a0009, 0x7e2802ff, 0x5a5a000a, 0x7e2a02ff, 0x5a5a000b, 0x7e2c02ff, 0x5a5a000c, 0x7e2e02ff,
    0x5a5a000d, 0x7e3002ff, 0x5a5a000e, 0x7e3202ff, 0x5a5a000f, 0x4a4008ff, 0x00000a00, 0x4a420eff,
    0x00000200, 0xd70f6a22, 0x02024008, 0x7e460209, 0x50464680, 0x4a4c0aff, 0x00000200, 0xd70f6a24,
    0x02024c08, 0x7e4a0209, 0x504a4a80, 0x36500c81, 0x7daa5080, 0xdc208800, 0x0a080020, 0xdc2487ff,
    0x0b080020, 0xdc288ffd, 0x0c080020, 0xdc2c8001, 0x0d7d0022, 0xdc308fff, 0x0e080020, 0xdc308000,
    0x18080021, 0xdc208800, 0x197d0022, 0xbf8c0070, 0xbefe04c1, 0xe0781000, 0x80010a02, 0xe0781010,
    0x80010e02, 0xe0781020, 0x80011202, 0xe0781030, 0x80011602, 0xbf810000
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "copied writer: cannot allocate a guest block");
        std::memset(block, Fill, BlockBytes);
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

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

void RecordDispatch(AgcDriver::VulkanDevice& device, const GuestBlock& input, const GuestBlock& output) {
    std::vector<std::uint32_t> userData(10, 0u);
    const auto address = output.Address();
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), OutputBytes, 0x31016facu};
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    userData[8] = static_cast<std::uint32_t>(input.Address());
    userData[9] = static_cast<std::uint32_t>(input.Address() >> 32u);
    const std::span<const std::uint32_t> code(LoadsCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const GuestBlock input;
        std::memset(input.Data(), 0, BlockBytes);
        const GuestBlock output;
        const GuestBlock destination;
        RecordDispatch(*device, input, output);
        AgcDriver::VulkanDevice::CopyOutcome outcome{};
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            outcome = device->CopyBuffer(destination.Address(), output.Address(), OutputBytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        }
        if (outcome.path == 2) {
            device->WaitIdle();
            std::puts("skipped, the device imports no host memory");
            return VulkanTestSkipped;
        }
        Require(outcome.path == 1, "copied writer: the copy did not run as a transfer between the host imports (path " + std::to_string(outcome.path) + ")");
        Require(!outcome.synced, "copied writer: a copy of what an address-based dispatch stored through a bound buffer waited for its batch, although no CPU write-back follows it");
        device->WaitIdle();
        Require(std::any_of(output.Data(), output.Data() + OutputBytes, [](std::uint8_t byte) { return byte != Fill; }), "copied writer: the dispatch stored nothing");
        for (std::uint32_t offset = 0; offset < OutputBytes; ++offset) {
            Require(destination.Data()[offset] == output.Data()[offset], "copied writer: copied byte " + std::to_string(offset) + " is " + std::to_string(destination.Data()[offset]) + ", the dispatch stored " + std::to_string(output.Data()[offset]));
        }
        std::puts("copied writer copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
