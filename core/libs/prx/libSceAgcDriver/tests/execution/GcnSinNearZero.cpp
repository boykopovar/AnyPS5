#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uintptr_t GcnAddressLimit = std::uintptr_t{1} << 44u;
constexpr std::uint32_t InputOffset = 0;
constexpr std::uint32_t OutputOffset = 1024;

alignas(256) constexpr std::array<std::uint32_t, 9> Code{
    0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c0f70u, 0x7e066b02u, 0xe0701000u, 0x80010301u, 0xbf8c0f00u, 0xbf810000u,
};

struct Row {
    std::uint32_t input;
    std::uint32_t expected;
    std::uint32_t tolerance;
};

constexpr std::array<Row, 11> Rows{{
    {0x39bf48b8u, 0x3b163bedu, 0u},
    {0x39bff171u, 0x3b16c071u, 0u},
    {0xb9bff171u, 0xbb16c071u, 0u},
    {0x39bf0da3u, 0x3b160d86u, 0u},
    {0x39bfffffu, 0x3b16cbdfu, 0u},
    {0xb9bfffffu, 0xbb16cbdfu, 0u},
    {0x3941b44eu, 0x3a98228cu, 0u},
    {0x39c00000u, 0x3b16cbdfu, 6u},
    {0xb9c00000u, 0xbb16cbdfu, 6u},
    {0x39c00001u, 0x3b16cbdfu, 6u},
    {0xb9c00001u, 0xbb16cbdfu, 6u},
}};

class LowBlock {
public:
    LowBlock() : block(static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes))) {
        Require(block != nullptr, "GCN sin near zero: cannot allocate the buffer block in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04u, BlockBytes);
    }

    ~LowBlock() {
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    bool Addressable() const { return Address() + BlockBytes <= GcnAddressLimit; }
    std::uint32_t* Words(std::uint32_t offset) const { return block + offset / 4u; }

private:
    std::uint32_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uintptr_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x00027facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Ordered(std::uint32_t bits) {
    return (bits & 0x80000000u) != 0u ? 0x80000000u - (bits & 0x7fffffffu) : bits | 0x80000000u;
}

std::uint32_t Distance(std::uint32_t actual, std::uint32_t expected) {
    const std::uint32_t a = Ordered(actual);
    const std::uint32_t e = Ordered(expected);
    return a > e ? a - e : e - a;
}

void Run(AgcDriver::VulkanDevice& device, const LowBlock& block) {
    for (std::size_t lane = 0; lane < Rows.size(); ++lane) {
        block.Words(InputOffset)[lane] = Rows[lane].input;
        block.Words(OutputOffset)[lane] = 0xdeadbeefu;
    }
    std::vector<std::uint32_t> userData(8, 0u);
    const auto bytes = static_cast<std::uint32_t>(Rows.size() * 4u);
    const auto input = BufferDescriptor(reinterpret_cast<std::uintptr_t>(block.Words(InputOffset)), bytes);
    const auto output = BufferDescriptor(reinterpret_cast<std::uintptr_t>(block.Words(OutputOffset)), bytes);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{static_cast<std::uint32_t>(Rows.size()), 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const LowBlock& block) {
    std::array<std::uint32_t, Rows.size()> stored{};
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(block.Words(OutputOffset)), std::as_writable_bytes(std::span(stored)), 4);
    for (std::size_t lane = 0; lane < Rows.size(); ++lane) {
        const auto& row = Rows[lane];
        const std::string name = "GCN sin near zero: v_sin_f32(" + Hex(row.input) + ") is " + Hex(stored[lane]) + ", the PS4 returns " + Hex(row.expected);
        Require(Distance(stored[lane], row.expected) <= row.tolerance, name + " (tolerance " + std::to_string(row.tolerance) + " ulp)");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Addressable()) {
            std::printf("skipped, the guest arena block at 0x%llx lies above the 44-bit GCN V# address range\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        Run(*device, block);
        Check(block);
        std::puts("GCN sin near zero tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
