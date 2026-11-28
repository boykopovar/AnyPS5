#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uintptr_t GcnAddressLimit = std::uintptr_t{1} << 44u;
constexpr std::array<std::uint32_t, 4> Resident{0x12345678u, 0x5ca1ab1eu, 0x3f800000u, 0x600dcafeu};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x7e0202ffu, 0x11111111u,
    0xc2040501u, 0xbf8c007fu,
    0xbf008008u, 0xbf850001u,
    0x7e020208u,
    0xe0700000u, 0x80000100u,
    0xbf8c0f70u, 0xbf810000u,
};

class LowBlock {
public:
    LowBlock() : block(static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes))) {
        Require(block != nullptr, "GCN scalar buffer base: cannot allocate the buffer block in the guest arena");
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

std::array<std::uint32_t, 4> Descriptor(const std::uint32_t* data, std::uint32_t highBits) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | highBits, 64u, 0x00027facu};
}

std::uint32_t Read(const std::uint32_t* data) {
    std::uint32_t word = 0;
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(data), std::as_writable_bytes(std::span(&word, 1)), 4);
    return word;
}

void Run(AgcDriver::VulkanDevice& device, const LowBlock& block, std::uint32_t highBits) {
    std::fill_n(block.Output(), 4, 0xdeadbeefu);
    std::copy(Resident.begin(), Resident.end(), block.Source());
    const auto output = Descriptor(block.Output(), 0u);
    const auto source = Descriptor(block.Source(), highBits);
    std::array<std::uint32_t, 8> userData{};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(source.begin(), source.end(), userData.begin() + 4);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {64u, 0, userData, compute, std::nullopt, std::nullopt, {}},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    char what[96];
    std::snprintf(what, sizeof(what), "GCN scalar buffer base, V# word 1 bits 0x%04x", highBits);
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto regions = memory.Regions();
    request.context.memory = regions;
    const auto result = ShaderRecompiler::Recompile(request, *capture);
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    const auto loaded = Read(block.Output());
    char got[64];
    std::snprintf(got, sizeof(got), ": s_buffer_load_dword returned 0x%08x, expected 0x%08x", loaded, Resident[1]);
    Require(loaded == Resident[1], std::string(what) + got);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Addressable()) {
            Require(GuestArena::GuestAddressOffset != 0, "GCN scalar buffer base: the guest arena block lies above the 44-bit GCN V# address range");
            std::puts("skipped, the ASan guest arena offset places the block above the 44-bit GCN V# address range");
            return VulkanTestSkipped;
        }
        for (const auto highBits : {0x0000u, 0x1000u, 0x8000u, 0xf000u}) Run(*device, block, highBits);
        std::puts("GCN scalar buffer base tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
