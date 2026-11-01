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

constexpr std::uint32_t Threads = 16;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0x34020084u, 0x34040084u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0xcc0f400au, 0x18020b04u, 0xcc10400bu,
    0x18020b04u, 0xcc0e400cu, 0x1c1a0b04u, 0xe07c1000u, 0x80010a02u, 0xbf810000u,
};
constexpr std::uint32_t Rows[Threads][3] = {
    {0x02000001u, 0x02000000u, 0x02000000u},
    {0x83ff8001u, 0x03ff0001u, 0x00000000u},
    {0x7c0003ffu, 0x00010001u, 0x00000000u},
    {0x7bff0400u, 0x3c008001u, 0x00010000u},
    {0x04013bffu, 0xb8000400u, 0x02000000u},
    {0xbc003c01u, 0x000103ffu, 0x04000000u},
    {0x3c000400u, 0x84010400u, 0x04008400u},
    {0x04000400u, 0x3bff8400u, 0x83ff0400u},
    {0x84000001u, 0x3bff0001u, 0x03ff8400u},
    {0x35553c00u, 0x05550001u, 0x00000000u},
    {0x01553800u, 0x44000001u, 0x00000001u},
    {0x81553800u, 0x44000400u, 0x04000000u},
    {0x00008400u, 0x80003800u, 0x80000000u},
    {0x80003c00u, 0x80003c00u, 0x00000001u},
    {0x5bff1fffu, 0x04002001u, 0x00000000u},
    {0x3bff2001u, 0x3bff1fffu, 0x00018001u},
};
constexpr std::uint32_t Expected[4][Threads][3] = {
    {
        {0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x80008000u, 0x00000000u},
        {0x7c000000u, 0xfe000000u, 0xfe000000u},
        {0x7bff0400u, 0x7bff8000u, 0x7bff0000u},
        {0xb8003bffu, 0x80000000u, 0x80000000u},
        {0xbc003c01u, 0x80000000u, 0x04000000u},
        {0x3c000800u, 0x84010000u, 0x80008400u},
        {0x3bff0000u, 0x00008000u, 0x00000400u},
        {0x3bff0000u, 0x80000000u, 0x80008400u},
        {0x35553c00u, 0x00000000u, 0x00000000u},
        {0x44003800u, 0x00000000u, 0x00000000u},
        {0x44003800u, 0x80000000u, 0x04000000u},
        {0x00003800u, 0x80008000u, 0x80008000u},
        {0x80004000u, 0x00003c00u, 0x00003c00u},
        {0x5bff2400u, 0x23ff0400u, 0x23ff0400u},
        {0x3fff2400u, 0x3bfe0400u, 0x3bfe0400u},
    },
    {
        {0x04000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x80008000u, 0x80008000u},
        {0x7c000400u, 0x7c000000u, 0x7c000000u},
        {0x7bff0000u, 0x7bff8000u, 0x7bff8000u},
        {0xb8003bffu, 0x80000000u, 0x80000000u},
        {0xbc003c01u, 0x80000400u, 0x00000400u},
        {0x3c000800u, 0x84010000u, 0x80008400u},
        {0x3bff0000u, 0x00008000u, 0x00000400u},
        {0x3bff0000u, 0x80000000u, 0x80008400u},
        {0x35553c00u, 0x00000000u, 0x00000000u},
        {0x44003800u, 0x05540000u, 0x05540000u},
        {0x44003800u, 0x85540000u, 0x80000000u},
        {0x00003800u, 0x80008000u, 0x80008000u},
        {0x80004000u, 0x00003c00u, 0x00003c00u},
        {0x5bff2400u, 0x23ff0400u, 0x23ff0400u},
        {0x3fff2400u, 0x3bfe0400u, 0x3bfe0000u},
    },
    {
        {0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x80008000u, 0x00000000u},
        {0x7c000000u, 0xfe000000u, 0xfe000000u},
        {0x7bff0400u, 0x7bff8000u, 0x7bff0000u},
        {0xb8003bffu, 0x82000400u, 0x82000400u},
        {0xbc003c01u, 0x80000000u, 0x04000000u},
        {0x3c000800u, 0x84010000u, 0x80018400u},
        {0x3bff0000u, 0x04008000u, 0x04000400u},
        {0x3bff0000u, 0x84000000u, 0x84008400u},
        {0x35553c00u, 0x01c70000u, 0x01c70000u},
        {0x44003800u, 0x00000000u, 0x00000000u},
        {0x44003800u, 0x80000200u, 0x04000200u},
        {0x00003800u, 0x80008200u, 0x80008200u},
        {0x80004000u, 0x00003c00u, 0x00003c00u},
        {0x5bff2400u, 0x23ff0400u, 0x23ff0400u},
        {0x3fff2400u, 0x3bfe0400u, 0x3bfe0400u},
    },
    {
        {0x04000001u, 0x00000000u, 0x02000000u},
        {0x00000000u, 0x80008000u, 0x80008000u},
        {0x7c000400u, 0x7c000000u, 0x7c000000u},
        {0x7bff03ffu, 0x7bff8000u, 0x7bff8000u},
        {0xb8003bffu, 0x82000400u, 0x80000400u},
        {0xbc003c01u, 0x80010400u, 0x03ff0400u},
        {0x3c000800u, 0x84010000u, 0x80018400u},
        {0x3bff0000u, 0x04008000u, 0x00000400u},
        {0x3bff0002u, 0x84000000u, 0x80008400u},
        {0x35553c00u, 0x01c70001u, 0x01c70001u},
        {0x44003800u, 0x05540000u, 0x05540002u},
        {0x44003800u, 0x85540200u, 0x81540200u},
        {0x00003800u, 0x80008200u, 0x80008200u},
        {0x80004000u, 0x00003c00u, 0x00003c00u},
        {0x5bff2400u, 0x23ff0400u, 0x23ff0400u},
        {0x3fff2400u, 0x3bfe0400u, 0x3bfe03ffu},
    },
};

struct Mode {
    const char* name;
    ShaderRecompiler::ShaderFloatMode value;
    std::uint32_t field;
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

void Check(const Mode& mode) {
    constexpr const char* Names[3] = {"v_pk_add_f16", "v_pk_mul_f16", "v_pk_fma_f16"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t form = 0; form < 3u; ++form) {
            const std::uint32_t actual = Output[tid * Results + form];
            const std::uint32_t expected = Expected[mode.field][tid][form];
            Require(actual == expected, std::string(Names[form]) + " " + mode.name + " of " + Hex(Rows[tid][0]) + ", " + Hex(Rows[tid][1]) + ", " + Hex(Rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Mode modes[] = {
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
        std::puts("packed f16 denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
