#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t VccLow = 106u;
constexpr std::uint32_t VectorSourceBase = 256u;
constexpr std::uint32_t ShaderCmpEqF16 = 0xcau;
constexpr std::uint32_t ShaderCmpLeF64 = 0x23u;
constexpr std::uint32_t ShaderCmpNgtF64 = 0x2bu;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

struct Row {
    std::array<std::uint32_t, 4> sources;
    std::uint32_t expected;
};

struct Vop3CompareWords {
    std::uint32_t word0;
    std::uint32_t word1;
};

Vop3CompareWords Vop3Compare(std::uint32_t opcode, std::uint32_t source0, std::uint32_t source1) {
    const std::uint32_t encoded0 = VectorSourceBase + source0;
    const std::uint32_t encoded1 = VectorSourceBase + source1;
    return {0xd4000000u | (opcode << 16u) | VccLow | 0x8000u, encoded0 | (encoded1 << 9u)};
}

std::vector<std::uint32_t> ClampCode(const Vop3CompareWords& compare) {
    return {
        0x34020082u, 0x34060082u,
        0xe0302000u, 0x80000401u,
        0xe0302004u, 0x80000501u,
        0xe0302008u, 0x80000601u,
        0xe030200cu, 0x80000701u,
        0xbf8c3f70u,
        compare.word0, compare.word1,
        0xd5010014u, 0x01a90280u,
        0xe0702000u, 0x80011403u,
        0xbf810000u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void RunClampRows(AgcDriver::VulkanDevice& device, const char* name, const Vop3CompareWords& compare,
                  const ShaderRecompiler::ShaderFloatMode& floatMode, const std::vector<Row>& rows) {
    Input.fill(0u);
    for (std::size_t index = 0; index < rows.size(); ++index) {
        std::copy(rows[index].sources.begin(), rows[index].sources.end(),
                  Input.begin() + static_cast<std::ptrdiff_t>(index * Inputs));
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto code = ClampCode(compare);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(words.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(words.data()), words, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.context.floatMode = floatMode;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(words.data()));
    device.WaitIdle();
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto actual = Output[index * Results];
        Require(actual == rows[index].expected, std::string(name) + " clamp row " + std::to_string(index) + " is " + Hex(actual) + ", expected " + Hex(rows[index].expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const ShaderRecompiler::ShaderFloatMode denormalsKept{0xf0u, true, true, false};
        const ShaderRecompiler::ShaderFloatMode denorm32Flushed{0xc0u, true, false, false};
        RunClampRows(*device, "v_cmp_eq_f16_e64 vcc_lo, v4, v5", Vop3Compare(ShaderCmpEqF16, 4u, 5u), denormalsKept, {
            {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, 0x00000001u},
            {{0x0000bc00u, 0x00000000u, 0x00000000u, 0x00000000u}, 0x00000000u},
            {{0x00003555u, 0x00000000u, 0x00000000u, 0x00000000u}, 0x00000000u},
            {{0x00000001u, 0x00008000u, 0x00000000u, 0x00000000u}, 0x00000000u},
            {{0x0000fc00u, 0x00008000u, 0x00000000u, 0x00000000u}, 0x00000000u},
            {{0x00007c01u, 0x00003c00u, 0x00000000u, 0x00000000u}, 0x00000000u},
            {{0x00006400u, 0x00003c00u, 0x00000000u, 0x00000000u}, 0x00000000u},
            {{0xbeef8000u, 0xbeefbc00u, 0xbeef0000u, 0x00000000u}, 0x00000000u},
        });
        RunClampRows(*device, "v_cmp_le_f64_e64 vcc_lo, v[4:5], v[6:7]", Vop3Compare(ShaderCmpLeF64, 4u, 6u), denorm32Flushed, {
            {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, 0x00000001u},
            {{0x00000000u, 0xbff00000u, 0x00000000u, 0xfff00000u}, 0x00000000u},
            {{0x00000000u, 0xc0040000u, 0x00000000u, 0xbff00000u}, 0x00000001u},
            {{0x00000000u, 0x00100000u, 0x00000001u, 0x7ff00000u}, 0x00000000u},
            {{0x00000000u, 0xfff00000u, 0x00000000u, 0x40040000u}, 0x00000001u},
            {{0x00000001u, 0x7ff00000u, 0x00000000u, 0x41e00000u}, 0x00000000u},
        });
        RunClampRows(*device, "v_cmp_ngt_f64_e64 vcc_lo, v[4:5], v[6:7]", Vop3Compare(ShaderCmpNgtF64, 4u, 6u), denormalsKept, {
            {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, 0x00000001u},
            {{0x00000000u, 0xbff00000u, 0x00000000u, 0xfff00000u}, 0x00000000u},
            {{0x00000000u, 0xc0040000u, 0x00000000u, 0xbff00000u}, 0x00000001u},
            {{0x00000000u, 0x00100000u, 0x00000001u, 0x7ff00000u}, 0x00000001u},
        });
        std::puts("v_cmp clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
