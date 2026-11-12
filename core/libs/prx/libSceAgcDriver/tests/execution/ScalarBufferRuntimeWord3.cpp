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
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 4;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Records = 20;
alignas(256) std::array<std::uint32_t, 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 25> Code{
    0xe0300000, 0x80000200, 0xbf8c3f70, 0x7e1e0502, 0xbe8c0308, 0xbe8d0309, 0xbe8e030a, 0xf4200406,
    0xfa000004, 0xf4200446, 0xfa000010, 0xf4200486, 0xfa000014, 0xbf8cc07f, 0x7e060210, 0x7e080211,
    0x7e0a0212, 0x340c0084, 0xe0701000, 0x80010306, 0xe0701004, 0x80010406, 0xe0701008, 0x80010506,
    0xbf810000,
};

constexpr std::array<std::uint32_t, 9> Word3s{0x31016facu, 0x01016facu, 0x31000facu, 0x31816facu, 0x71016facu, 0xb1016facu, 0xf1016facu, 0x00000000u, 0xffffffffu};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "scalar buffer runtime word 3: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, false, true);
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
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t word3) {
    Input.fill(word3);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto data = BufferDescriptor(guest.Data(), Records);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(data.begin(), data.begin() + 3, userData.begin() + 8);
    const std::span<const std::uint32_t> code(Code);
    const std::span<const std::uint8_t> guestBytes(guest.Data(), 256);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}, {reinterpret_cast<std::uintptr_t>(guest.Data()), std::as_bytes(guestBytes)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t word3) {
    constexpr std::array<std::uint32_t, 3> expected{0xa0000001u, 0xa0000004u, 0u};
    constexpr std::array<const char*, 3> names{"offset 0x4", "offset 0x10", "offset 0x14 past the records"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < expected.size(); ++i) {
            const auto actual = Output[tid * Results + i];
            Require(actual == expected[i], "scalar buffer runtime word 3 " + Hex(word3) + ": lane " + std::to_string(tid) + " " + names[i] + " is " + Hex(actual) + ", expected " + Hex(expected[i]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        std::memset(guest.Data(), 0, BlockBytes);
        for (std::uint32_t i = 0; i < 64; ++i) {
            const std::uint32_t value = 0xa0000000u | i;
            std::memcpy(guest.Data() + i * 4u, &value, sizeof(value));
        }
        for (const auto word3 : Word3s) {
            Run(*device, guest, word3);
            Check(word3);
        }
        std::puts("scalar buffer runtime word 3 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
