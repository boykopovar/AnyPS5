#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t ValueCount = 16;
constexpr std::uint32_t Threads = ValueCount * ValueCount;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34040083u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x5e140b04u, 0xd711000bu, 0x00020f06u,
    0xe0741000u, 0x80010a02u, 0xbf810000u,
};

constexpr std::uint32_t Singles[ValueCount] = {
    0x00000000u, 0x80000000u, 0x33800000u, 0xb3800000u, 0x33c00000u, 0x33000000u, 0x387fc000u, 0xb87fc000u,
    0x387fffffu, 0x38800000u, 0xb8800000u, 0x37730edfu, 0x3f800000u, 0x477ff000u, 0x7f800000u, 0xff800000u,
};

constexpr std::uint32_t SinglesTowardZero[ValueCount] = {
    0x0000u, 0x8000u, 0x0001u, 0x8001u, 0x0001u, 0x0000u, 0x03ffu, 0x83ffu,
    0x03ffu, 0x0400u, 0x8400u, 0x00f3u, 0x3c00u, 0x7bffu, 0x7c00u, 0xfc00u,
};

constexpr std::uint32_t Halves[ValueCount] = {
    0x0000u, 0x8000u, 0x0001u, 0x8001u, 0x03ffu, 0x83ffu, 0x0200u, 0x0400u,
    0x8400u, 0x3c00u, 0xbc00u, 0x7bffu, 0x7c00u, 0xfc00u, 0x7e00u, 0x7d00u,
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
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

void Fill() {
    for (std::uint32_t pair = 0; pair < Threads; ++pair) {
        Input[pair * Inputs] = Singles[pair / ValueCount];
        Input[pair * Inputs + 1u] = Singles[pair % ValueCount];
        Input[pair * Inputs + 2u] = Halves[pair / ValueCount];
        Input[pair * Inputs + 3u] = Halves[pair % ValueCount];
    }
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = mode.value;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Flush(std::uint32_t half, bool flushed) {
    return flushed && (half & 0x7fffu) < 0x0400u ? half & 0x8000u : half;
}

void Check(const Mode& mode) {
    const std::uint32_t field = mode.value.has_value() ? mode.value->floatMode >> 6u : 3u;
    const bool ieee = mode.value.has_value() && mode.value->ieeeMode;
    for (std::uint32_t pair = 0; pair < Threads; ++pair) {
        const std::uint32_t lhs = pair / ValueCount;
        const std::uint32_t rhs = pair % ValueCount;
        const bool flushResults = (field & 2u) == 0u;
        const std::uint32_t packed = Flush(SinglesTowardZero[lhs], flushResults) | (Flush(SinglesTowardZero[rhs], flushResults) << 16u);
        Require(Output[pair * Results] == packed, std::string("v_cvt_pkrtz_f16_f32 ") + mode.name + " of " + Hex(Singles[lhs]) + " and " + Hex(Singles[rhs]) + " is " + Hex(Output[pair * Results]) + ", expected " + Hex(packed));
        const auto half = [&](std::uint32_t value) {
            const std::uint32_t bits = Flush(value, (field & 1u) == 0u);
            return ieee && (bits & 0x7fffu) > 0x7c00u ? bits | 0x0200u : bits;
        };
        const std::uint32_t joined = half(Halves[lhs]) | (half(Halves[rhs]) << 16u);
        Require(Output[pair * Results + 1u] == joined, std::string("v_pack_b32_f16 ") + mode.name + " of " + Hex(Halves[lhs]) + " and " + Hex(Halves[rhs]) + " is " + Hex(Output[pair * Results + 1u]) + ", expected " + Hex(joined));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Fill();
        const Mode modes[] = {
            {"without a known float mode", std::nullopt},
            {"f16 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}},
            {"f16 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}},
            {"f16 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}},
            {"f16 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}},
            {"f16 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}},
            {"f16 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}},
            {"f16 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}},
            {"f16 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}},
        };
        for (const Mode& mode : modes) {
            Run(*device, mode);
            Check(mode);
        }
        std::puts("f16 pack denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
