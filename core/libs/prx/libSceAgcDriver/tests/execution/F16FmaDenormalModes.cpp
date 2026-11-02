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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34020084u, 0x34040083u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd74b000au, 0x041a0b04u,
    0x7e160306u, 0x6c160b04u, 0xe0741000u, 0x80010a02u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][3] = {
    {0x0001u, 0x0001u, 0x8400u},
    {0x7bffu, 0x7bffu, 0x0000u},
    {0x7c00u, 0x0000u, 0x0000u},
    {0x3bffu, 0x0400u, 0x0000u},
    {0x3c01u, 0x03ffu, 0x0000u},
    {0x0400u, 0x8400u, 0x0400u},
    {0x0400u, 0x0400u, 0x8400u},
    {0x0000u, 0x0000u, 0x0000u},
    {0x0000u, 0x0000u, 0x8000u},
    {0x0000u, 0x0000u, 0x0400u},
    {0x0000u, 0x0001u, 0x0001u},
    {0x0000u, 0x0001u, 0x03ffu},
    {0x0000u, 0x0001u, 0x0200u},
    {0x0000u, 0x7c00u, 0x0001u},
    {0x0000u, 0x7c00u, 0x8001u},
    {0x0000u, 0x7c00u, 0x03ffu},
    {0x0000u, 0x0001u, 0x0000u},
    {0x0000u, 0x0001u, 0x8000u},
    {0x0000u, 0x0001u, 0x0400u},
    {0x0001u, 0x8001u, 0x0000u},
    {0x0001u, 0x83ffu, 0x0000u},
    {0x0001u, 0x8400u, 0x0000u},
    {0x0000u, 0x0000u, 0x0001u},
    {0x0000u, 0x0000u, 0x03ffu},
    {0x0000u, 0x0000u, 0x0200u},
    {0x0001u, 0x3800u, 0x8001u},
    {0x0001u, 0x3c00u, 0x03ffu},
    {0x0001u, 0xbc00u, 0x83ffu},
    {0x0400u, 0x3c00u, 0x0001u},
    {0x0400u, 0x3c00u, 0x03ffu},
    {0x0400u, 0x3c00u, 0x0200u},
    {0x0000u, 0x0001u, 0x8001u},
    {0x0000u, 0x0001u, 0x83ffu},
    {0x0000u, 0x03ffu, 0x8001u},
    {0x0000u, 0x0000u, 0x8001u},
    {0x0000u, 0x0000u, 0x83ffu},
    {0x0000u, 0x0400u, 0x8001u},
    {0x0400u, 0x3800u, 0x0000u},
    {0x0400u, 0x3800u, 0x8000u},
    {0x0400u, 0x3800u, 0x8400u},
    {0x0001u, 0x3c00u, 0x8400u},
    {0x0001u, 0xbc00u, 0x0000u},
    {0x0001u, 0xbc00u, 0x0400u},
    {0x0001u, 0x3c00u, 0x0000u},
    {0x0001u, 0x3c00u, 0x8000u},
    {0x0001u, 0xbc00u, 0x8000u},
    {0x0400u, 0x3800u, 0x0001u},
    {0x0400u, 0x3800u, 0x8001u},
    {0x0400u, 0xb800u, 0x0001u},
    {0x0400u, 0x3800u, 0x03ffu},
    {0x0400u, 0x3800u, 0x0200u},
    {0x0400u, 0xb800u, 0x83ffu},
    {0x0001u, 0xb800u, 0x0001u},
    {0x0001u, 0x3c00u, 0x8001u},
    {0x0001u, 0xbc00u, 0x0001u},
    {0x0400u, 0x3800u, 0x83ffu},
    {0x0400u, 0xb800u, 0x03ffu},
    {0x0400u, 0x1fffu, 0x83ffu},
    {0x0001u, 0x3800u, 0x8400u},
    {0x0001u, 0xb800u, 0x0400u},
    {0x8001u, 0x3800u, 0x0400u},
    {0x0400u, 0x3bffu, 0x83ffu},
    {0x8400u, 0x3bffu, 0x03ffu},
    {0x0401u, 0xb800u, 0x0200u},
};
constexpr std::uint32_t Expected[Threads][4] = {
    {0x8400u, 0x8400u, 0x8400u, 0x8400u},
    {0x7c00u, 0x7c00u, 0x7c00u, 0x7c00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0x0000u, 0x0000u, 0x0400u, 0x0400u},
    {0x0000u, 0x0400u, 0x0000u, 0x0400u},
    {0x0400u, 0x0400u, 0x0400u, 0x0400u},
    {0x8400u, 0x8400u, 0x8400u, 0x8400u},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0400u, 0x0400u, 0x0400u, 0x0400u},
    {0x0000u, 0x0000u, 0x0000u, 0x0001u},
    {0x0000u, 0x0000u, 0x0000u, 0x03ffu},
    {0x0000u, 0x0000u, 0x0000u, 0x0200u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0400u, 0x0400u, 0x0400u, 0x0400u},
    {0x0000u, 0x8000u, 0x0000u, 0x8000u},
    {0x0000u, 0x8000u, 0x0000u, 0x8000u},
    {0x0000u, 0x8000u, 0x0000u, 0x8000u},
    {0x0000u, 0x0000u, 0x0000u, 0x0001u},
    {0x0000u, 0x0000u, 0x0000u, 0x03ffu},
    {0x0000u, 0x0000u, 0x0000u, 0x0200u},
    {0x0000u, 0x8000u, 0x0000u, 0x8000u},
    {0x0000u, 0x0400u, 0x0000u, 0x0400u},
    {0x8000u, 0x8400u, 0x8000u, 0x8400u},
    {0x0400u, 0x0401u, 0x0400u, 0x0401u},
    {0x0400u, 0x07ffu, 0x0400u, 0x07ffu},
    {0x0400u, 0x0600u, 0x0400u, 0x0600u},
    {0x0000u, 0x8000u, 0x0000u, 0x8001u},
    {0x0000u, 0x8000u, 0x0000u, 0x83ffu},
    {0x0000u, 0x8000u, 0x0000u, 0x8001u},
    {0x0000u, 0x8000u, 0x0000u, 0x8001u},
    {0x0000u, 0x8000u, 0x0000u, 0x83ffu},
    {0x0000u, 0x8000u, 0x0000u, 0x8001u},
    {0x0000u, 0x0000u, 0x0200u, 0x0200u},
    {0x0000u, 0x0000u, 0x0200u, 0x0200u},
    {0x8000u, 0x8000u, 0x8200u, 0x8200u},
    {0x8400u, 0x8000u, 0x8400u, 0x83ffu},
    {0x0000u, 0x8000u, 0x0000u, 0x8001u},
    {0x0400u, 0x0000u, 0x0400u, 0x03ffu},
    {0x0000u, 0x0000u, 0x0000u, 0x0001u},
    {0x0000u, 0x0000u, 0x0000u, 0x0001u},
    {0x8000u, 0x8000u, 0x8000u, 0x8001u},
    {0x0000u, 0x0000u, 0x0200u, 0x0201u},
    {0x0000u, 0x0000u, 0x0200u, 0x01ffu},
    {0x8000u, 0x8000u, 0x8200u, 0x81ffu},
    {0x0000u, 0x05ffu, 0x0200u, 0x05ffu},
    {0x0000u, 0x0400u, 0x0200u, 0x0400u},
    {0x8000u, 0x85ffu, 0x8200u, 0x85ffu},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0000u, 0x0000u, 0x0000u, 0x0000u},
    {0x0000u, 0x8000u, 0x0200u, 0x81ffu},
    {0x8000u, 0x0000u, 0x8200u, 0x01ffu},
    {0x0000u, 0x8000u, 0x0008u, 0x83f7u},
    {0x8400u, 0x8000u, 0x8400u, 0x8400u},
    {0x0400u, 0x0000u, 0x0400u, 0x0400u},
    {0x0400u, 0x0000u, 0x0400u, 0x0400u},
    {0x0000u, 0x0000u, 0x0400u, 0x0000u},
    {0x8000u, 0x8000u, 0x8400u, 0x8000u},
    {0x8000u, 0x8000u, 0x8200u, 0x8000u},
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
    std::uint32_t column;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%04x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
        Input[tid * Inputs + 3u] = 0u;
    }
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

bool Matches(std::uint32_t actual, std::uint32_t expected) {
    const auto nan = [](std::uint32_t half) { return (half & 0x7fffu) > 0x7c00u; };
    return actual == expected || (nan(expected) && actual <= 0xffffu && nan(actual));
}

void Check(const Mode& mode) {
    constexpr const char* Names[Results] = {"v_fma_f16", "v_fmac_f16"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t expected = Expected[tid][mode.column];
        for (std::uint32_t form = 0; form < Results; ++form) {
            const std::uint32_t actual = Output[tid * Results + form];
            Require(Matches(actual, expected), std::string(Names[form]) + " " + mode.name + " of " + Hex(Rows[tid][0]) + ", " + Hex(Rows[tid][1]) + ", " + Hex(Rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Mode modes[] = {
            {"without a known float mode", std::nullopt, 3u},
            {"f16 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}, 0u},
            {"f16 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}, 0u},
            {"f16 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}, 1u},
            {"f16 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}, 1u},
            {"f16 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}, 2u},
            {"f16 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}, 2u},
            {"f16 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, 3u},
            {"f16 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, 3u},
        };
        for (const Mode& mode : modes) {
            Run(*device, mode);
            Check(mode);
        }
        std::puts("f16 fma denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
