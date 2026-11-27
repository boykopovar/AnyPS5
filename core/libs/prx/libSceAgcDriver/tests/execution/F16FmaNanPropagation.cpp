#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
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

constexpr std::uint32_t Threads = 12;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 19> Code{
    0x34020084u, 0x34040084u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
    0xd74b000au, 0x041a0b04u, 0xd74b020bu, 0x241a0b04u, 0xd74b000cu, 0x0c1a0b04u, 0x7e1a0306u, 0x6c1a0b04u,
    0xe0781000u, 0x80010a02u, 0xbf810000u,
};

using Operand = std::array<std::uint32_t, 3>;
using Row = std::array<std::uint32_t, Results>;

constexpr std::array<Operand, Threads> Operands{{
    {0x7e00u, 0x3c00u, 0x3c00u},
    {0x3c00u, 0x7d01u, 0x3c00u},
    {0x3c00u, 0x3c00u, 0xfe55u},
    {0x7d01u, 0xfe55u, 0x7e00u},
    {0x3c00u, 0x7e00u, 0x7d01u},
    {0x7c00u, 0x0000u, 0x7e00u},
    {0x7c00u, 0x0000u, 0x3c00u},
    {0x7c00u, 0x3c00u, 0xfc00u},
    {0x3c00u, 0x4248u, 0x3800u},
    {0xfd01u, 0x3c00u, 0x0000u},
    {0x0000u, 0x7c00u, 0x7d01u},
    {0x7e55u, 0xfd01u, 0x3c00u},
}};

using Expected = std::array<Row, Threads>;

constexpr Expected IeeeOff{{
    {0x7e00u, 0xfe00u, 0x7e00u, 0x7e00u},
    {0x7d01u, 0x7d01u, 0x7d01u, 0x7d01u},
    {0xfe55u, 0xfe55u, 0xfe55u, 0xfe55u},
    {0x7d01u, 0xfd01u, 0x7d01u, 0x7d01u},
    {0x7e00u, 0x7e00u, 0x7e00u, 0x7e00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfc00u, 0xfe00u, 0xfe00u},
    {0x4348u, 0xc148u, 0x4348u, 0x4348u},
    {0xfd01u, 0x7d01u, 0xfd01u, 0xfd01u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0x7e55u, 0xfe55u, 0x7e55u, 0x7e55u},
}};

constexpr Expected IeeeOn{{
    {0x7e00u, 0xfe00u, 0x7e00u, 0x7e00u},
    {0x7f01u, 0x7f01u, 0x7f01u, 0x7f01u},
    {0xfe55u, 0xfe55u, 0xfe55u, 0xfe55u},
    {0x7f01u, 0xff01u, 0x7f01u, 0x7f01u},
    {0x7e00u, 0x7e00u, 0x7e00u, 0x7e00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfc00u, 0xfe00u, 0xfe00u},
    {0x4348u, 0xc148u, 0x4348u, 0x4348u},
    {0xff01u, 0x7f01u, 0xff01u, 0xff01u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0x7e55u, 0xfe55u, 0x7e55u, 0x7e55u},
}};

constexpr std::array<Operand, 6> FlushOperands{{
    {0x7c00u, 0x0001u, 0x7e55u},
    {0x0001u, 0x7c00u, 0x7d01u},
    {0xfc00u, 0x83ffu, 0x7e55u},
    {0x7c00u, 0x0001u, 0x3c00u},
    {0x0001u, 0x0001u, 0x7e55u},
    {0x7c00u, 0x0400u, 0x7e55u},
}};

constexpr std::array<Row, FlushOperands.size()> InputsFlushed{{
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0xfe00u, 0xfe00u, 0xfe00u, 0xfe00u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
}};

constexpr std::array<Row, FlushOperands.size()> InputsKeptIeeeOff{{
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7d01u, 0x7d01u, 0x7d01u, 0x7d01u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7c00u, 0xfc00u, 0x7c00u, 0x7c00u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
}};

constexpr std::array<Row, FlushOperands.size()> InputsKeptIeeeOn{{
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7f01u, 0x7f01u, 0x7f01u, 0x7f01u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7c00u, 0xfc00u, 0x7c00u, 0x7c00u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%04x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const Operand> operands, std::uint32_t floatMode, bool ieee) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < operands.size(); ++tid) {
        std::copy(operands[tid].begin(), operands[tid].end(), &Input[tid * Inputs]);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{floatMode, true, ieee, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Check(std::span<const Operand> operands, std::span<const Row> expected, const std::string& mode) {
    constexpr const char* Names[Results] = {"v_fma_f16", "v_fma_f16 with -src0 and |src1|", "v_fma_f16 with mul:2", "v_fmac_f16"};
    for (std::uint32_t tid = 0; tid < operands.size(); ++tid) {
        for (std::uint32_t form = 0; form < Results; ++form) {
            const std::uint32_t actual = Output[tid * Results + form];
            Require(actual == expected[tid][form], std::string(Names[form]) + " " + mode + " of " + Hex(operands[tid][0]) + ", " + Hex(operands[tid][1]) + ", " + Hex(operands[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected[tid][form]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, Operands, 0xc0u, false);
        Check(Operands, IeeeOff, "with IEEE mode off");
        Run(*device, Operands, 0xc0u, true);
        Check(Operands, IeeeOn, "with IEEE mode on");
        for (const bool ieee : {false, true}) {
            const std::string suffix = ieee ? " and IEEE mode on" : " and IEEE mode off";
            for (const std::uint32_t floatMode : {0x00u, 0x80u}) {
                Run(*device, FlushOperands, floatMode, ieee);
                Check(FlushOperands, InputsFlushed, "with FLOAT_MODE " + Hex(floatMode) + suffix);
            }
            Run(*device, FlushOperands, 0x40u, ieee);
            Check(FlushOperands, ieee ? InputsKeptIeeeOn : InputsKeptIeeeOff, "with FLOAT_MODE 0x40" + suffix);
        }
        std::puts("f16 fma NaN propagation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
