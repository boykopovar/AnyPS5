#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uintptr_t GcnAddressLimit = std::uintptr_t{1} << 44u;
constexpr std::uint32_t Untouched = 0x0badf00du;
constexpr std::array<std::uint32_t, 4> Resident{0x12345678u, 0x9abcdef0u, 0x3f800000u, 0x600dcafeu};
constexpr std::array<std::uint32_t, 4> Written{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};

alignas(256) constexpr std::array<std::uint32_t, 31> Code{
    0x7e0202ffu, Untouched, 0x7e0402ffu, Untouched, 0x7e0602ffu, Untouched, 0x7e0802ffu, Untouched,
    0x7e0a02ffu, Written[0], 0x7e0c02ffu, Written[1], 0x7e0e02ffu, Written[2], 0x7e1002ffu, Written[3],
    0xe00c0000u, 0x80010100u, 0xbf8c0f70u,
    0xe01c0000u, 0x80010500u,
    0xe0700000u, 0x80000100u, 0xe0700004u, 0x80000200u, 0xe0700008u, 0x80000300u, 0xe070000cu, 0x80000400u,
    0xbf8c0f70u, 0xbf810000u,
};

class LowBlock {
public:
    LowBlock() : block(static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes))) {
        Require(block != nullptr, "GCN invalid buffer format: cannot allocate the buffer block in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04u, BlockBytes);
    }

    ~LowBlock() {
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    std::uint32_t* Output() const { return block; }
    std::uint32_t* Source() const { return block + 64; }
    bool Addressable() const { return reinterpret_cast<std::uintptr_t>(block) + BlockBytes <= GcnAddressLimit; }

private:
    std::uint32_t* block = nullptr;
};

std::array<std::uint32_t, 4> Descriptor(const std::uint32_t* data, std::uint32_t dataFormat, std::uint32_t numberFormat) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), 16u, 0xfacu | (numberFormat << 12u) | (dataFormat << 15u)};
}

std::array<std::uint32_t, 4> Read(const std::uint32_t* data) {
    std::array<std::uint32_t, 4> words{};
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(data), std::as_writable_bytes(std::span(words)), 4);
    return words;
}

std::string Words(const std::array<std::uint32_t, 4>& words) {
    std::string text;
    for (const auto word : words) {
        char value[16];
        std::snprintf(value, sizeof(value), " 0x%08x", word);
        text += value;
    }
    return text;
}

void Run(AgcDriver::VulkanDevice& device, const LowBlock& block, std::uint32_t dataFormat, std::uint32_t numberFormat) {
    std::fill_n(block.Output(), 4, 0xdeadbeefu);
    std::copy(Resident.begin(), Resident.end(), block.Source());
    const auto output = Descriptor(block.Output(), 4u, 4u);
    const auto source = Descriptor(block.Source(), dataFormat, numberFormat);
    std::array<std::uint32_t, 8> userData{};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(source.begin(), source.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto what = "GCN invalid buffer format, DATA_FORMAT " + std::to_string(dataFormat) + " NUM_FORMAT " + std::to_string(numberFormat);
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    const auto loaded = Read(block.Output());
    const auto kept = Read(block.Source());
    if (dataFormat == 0u) {
        Require(loaded == std::array<std::uint32_t, 4>{}, what + ": buffer_load_format_xyzw returned" + Words(loaded) + ", expected zeros");
        Require(kept == Resident, what + ": buffer_store_format_xyzw changed the buffer to" + Words(kept) + ", expected the store to be dropped");
        return;
    }
    Require(loaded == Resident, what + ": buffer_load_format_xyzw returned" + Words(loaded) + ", expected" + Words(Resident));
    Require(kept == Written, what + ": buffer_store_format_xyzw left" + Words(kept) + ", expected" + Words(Written));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Addressable()) {
            std::puts("skipped, the guest arena block lies above the 44-bit GCN V# address range");
            return VulkanTestSkipped;
        }
        Run(*device, block, 14u, 4u);
        for (std::uint32_t numberFormat = 0; numberFormat < 8u; ++numberFormat) Run(*device, block, 0u, numberFormat);
        std::puts("GCN invalid buffer format tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
