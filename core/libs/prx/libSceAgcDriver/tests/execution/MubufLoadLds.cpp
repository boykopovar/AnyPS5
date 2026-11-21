#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
constexpr std::uint32_t Threads = 128u;
constexpr std::uint32_t LdsDwords = 256u;
constexpr std::uint32_t Guard = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, Threads + 16u> Input{};
alignas(256) std::array<std::uint32_t, Threads * 2u + 8u> Output{};

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t size, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), size, format};
}

void CheckRefused() {
    for (std::uint32_t opcode = 0u; opcode < 256u; ++opcode) {
        if (opcode == 12u) continue;
        const std::array<std::uint32_t, 2> code{0xe0010000u | (opcode << 18u), 0x80000000u};
        bool refused = false;
        try {
            static_cast<void>(ShaderRecompiler::DecodeRdnaEncodedInstruction(0u, code, 0u));
        } catch (const std::runtime_error&) {
            refused = true;
        }
        Require(refused, "unsupported MUBUF LDS opcode accepted");
    }
    for (const auto code : {std::array{0xe0310000u, 0x80000100u}, std::array{0xe0310000u, 0x80800000u},
                            std::array{0xe0330000u, 0x80000000u}, std::array{0xe0310000u, 0x80200000u}}) {
        bool refused = false;
        try {
            static_cast<void>(ShaderRecompiler::DecodeRdnaEncodedInstruction(0u, code, 0u));
        } catch (const std::runtime_error&) {
            refused = true;
        }
        Require(refused, "MUBUF LDS accepted nonzero VDATA, TFE or reserved bits");
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t wave, std::uint32_t mode, bool partial, std::uint32_t base) {
    Output.fill(Guard);
    alignas(256) const auto code = std::to_array<std::uint32_t>({
        0x34020082u, 0x360a00ffu, ~(wave - 1u), 0x340a0a82u,
        0x4a0a0a0cu, 0x7ef80505u, 0x887cff7cu, 0xabcd0000u, 0x36040080u | ((wave - 1u) & 63u),
        0x34040482u, 0x4a040505u, 0x4a040490u, 0x7e0602ffu, Guard,
        0xd8340000u, 0x00000302u, 0xbf8cc07fu, 0xbe88038cu,
        0xbe8a047eu, 0x877e0d7eu, 0x877f0d7fu, 0x7e0c0300u, 0x7e0e0288u,
        0xe0310010u | ((mode == 0u ? 1u : mode == 1u ? 2u : 3u) << 12u),
        0x08000000u | (mode == 0u ? 1u : mode == 1u ? 0u : 6u),
        0xbf8c0070u, 0xbefe040au, 0xd8d80000u, 0x04000002u, 0xbf8cc07fu,
        0xe0701000u, 0x80010401u, 0xe0701200u, 0x80010001u, 0xbf810000u
    });
    std::array<std::uint32_t, 14> userData{};
    auto input = Descriptor(Input.data(), Input.size() * 4u, 0x31016facu);
    if (mode != 0u) input[1] |= 4u << 16u;
    const auto output = Descriptor(Output.data() + 4u, Threads * 8u, 0x31016facu);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    userData[12] = base;
    userData[13] = partial ? 0x55555555u : 0xffffffffu;
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, LdsDwords, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0u, {}},
        {wave, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::size_t index = 0u; index < 4u; ++index) {
        Require(Output[index] == Guard && Output[Threads * 2u + 4u + index] == Guard, "MUBUF LDS output guard changed");
    }
    for (std::uint32_t tid = 0u; tid < Threads; ++tid) {
        const bool inBounds = base + tid * 4u + 19u < LdsDwords * 4u;
        const bool active = !partial || (tid & 1u) == 0u;
        const auto expected = !inBounds ? 0u : active ? Input[tid + 7u + (mode == 2u ? 2u : 0u)] : Guard;
        if (Output[tid + 4u] != expected) {
            std::fprintf(stderr, "wave=%u mode=%u partial=%d base=%u tid=%u actual=%08x expected=%08x\n", wave, mode, partial, base, tid, Output[tid + 4u], expected);
        }
        Require(Output[tid + 4u] == expected, "MUBUF LDS dword differs");
        Require(Output[Threads + tid + 4u] == tid, "MUBUF LDS load wrote a VGPR");
    }
}
}

int main(int argc, char** argv) {
    try {
        const auto wave = argc == 2 ? static_cast<std::uint32_t>(std::stoul(argv[1])) : 32u;
        Require(wave == 32u || wave == 64u, "invalid wave size");
        CheckRefused();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, the device's subgroups are narrower than a wave (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (std::uint32_t tid = 0u; tid < Input.size(); ++tid) Input[tid] = 0x81234000u + tid * 0x10001u;
        for (std::uint32_t mode = 0u; mode < 3u; ++mode) {
            for (const bool partial : {false, true}) {
                Run(*device, wave, mode, partial, 64u);
                Run(*device, wave, mode, partial, LdsDwords * 4u - 32u);
            }
        }
        std::puts("MUBUF LDS dword: 12 cases passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
