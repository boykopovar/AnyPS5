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
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t PositiveMax = 0x7f7fffffu;
constexpr std::uint32_t NegativeMax = 0xff7fffffu;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uintptr_t GcnAddressLimit = std::uintptr_t{1} << 44u;
constexpr std::array<std::uint32_t, 11> Inputs{
    0x00000000u,
    0x80000000u,
    0x00000001u,
    0x80000001u,
    0x7f800000u,
    0xff800000u,
    0x3f800000u,
    0xbf800000u,
    0x7fc00001u,
    0x7f800001u,
    0x00800000u,
};
constexpr std::uint32_t ResultsPerInput = 3u;
constexpr std::uint32_t WordsPerInput = 12u;
constexpr std::uint32_t ProgramWords = static_cast<std::uint32_t>(Inputs.size()) * WordsPerInput + 1u;

alignas(256) std::array<std::uint32_t, ProgramWords> Code{};

void BuildProgram() {
    std::size_t cursor = 0;
    std::uint32_t offset = 0;
    const auto store = [&](std::uint32_t source) {
        Code[cursor++] = 0xe0700000u | offset;
        Code[cursor++] = 0x80000000u | (source << 8u);
        offset += 4u;
    };
    for (const auto input : Inputs) {
        Code[cursor++] = 0x7e0002ffu;
        Code[cursor++] = input;
        Code[cursor++] = 0x7e025900u;
        Code[cursor++] = 0x7e045d00u;
        Code[cursor++] = (0x34u << 26u) | (0x1acu << 17u) | 3u;
        Code[cursor++] = 256u;
        store(1u);
        store(2u);
        store(3u);
    }
    Code[cursor++] = 0xbf810000u;
    Require(cursor == Code.size(), "GCN rsq clamp: the program does not fill its code");
}

class LowBlock {
public:
    LowBlock() : block(static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes))) {
        Require(block != nullptr, "GCN rsq clamp: cannot allocate the buffer block in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04u, BlockBytes);
    }

    ~LowBlock() {
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    std::uint32_t* Data() const { return block; }
    bool Addressable() const { return reinterpret_cast<std::uintptr_t>(block) + BlockBytes <= GcnAddressLimit; }

private:
    std::uint32_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool IsNan(std::uint32_t bits) {
    return (bits & 0x7fffffffu) > 0x7f800000u;
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
        BuildProgram();
        const auto words = static_cast<std::uint32_t>(Inputs.size()) * ResultsPerInput;
        std::fill_n(block.Data(), words, 0xdeadbeefu);
        const auto address = reinterpret_cast<std::uintptr_t>(block.Data());
        const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), words * 4u, 0xfacu | (4u << 12u) | (4u << 15u)};
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
            {64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device->WaitIdle();
        std::vector<std::uint32_t> stored(words);
        AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 4);
        const auto check = [&](std::size_t input, std::uint32_t clamped, std::uint32_t plain, const char* form) {
            const auto what = std::string("GCN rsq clamp, ") + form + " of " + Hex(Inputs[input]) + ": " + Hex(clamped) + " with v_rsq_f32 " + Hex(plain);
            if (Inputs[input] == 0x00000000u) Require(clamped == PositiveMax, what + ", expected the largest positive float");
            if (Inputs[input] == 0x80000000u) Require(clamped == NegativeMax, what + ", expected the largest negative float");
            if (plain == 0x7f800000u) {
                Require(clamped == PositiveMax, what + ", expected +infinity to clamp to the largest positive float");
            } else if (plain == 0xff800000u) {
                Require(clamped == NegativeMax, what + ", expected -infinity to clamp to the largest negative float");
            } else if (IsNan(plain)) {
                Require(IsNan(clamped), what + ", expected a NaN as v_rsq_f32 gives");
            } else {
                Require(clamped == plain, what + ", expected the v_rsq_f32 result");
            }
        };
        for (std::size_t input = 0; input < Inputs.size(); ++input) {
            const auto plain = stored[input * ResultsPerInput + 1u];
            check(input, stored[input * ResultsPerInput], plain, "VOP1");
            check(input, stored[input * ResultsPerInput + 2u], plain, "VOP3");
        }
        std::puts("GCN rsq clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
