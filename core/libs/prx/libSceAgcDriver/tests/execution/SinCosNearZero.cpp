#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Columns = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u,
    0x7e146b04u, 0x7e166d04u, 0x7e18c105u, 0x7e1ac305u, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][4] = {
    {0x80800000u, 0x00008001u, 0x00000000u, 0x00000000u},
    {0x8d800000u, 0x00008400u, 0x00000000u, 0x00000000u},
    {0xb0800000u, 0x00009000u, 0x00000000u, 0x00000000u},
    {0xb58637bdu, 0x0000868eu, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x358637bdu, 0x00000400u, 0x00000000u, 0x00000000u},
    {0xb87fda40u, 0x000083ffu, 0x00000000u, 0x00000000u},
    {0x387fda40u, 0x000003ffu, 0x00000000u, 0x00000000u},
    {0xbeffffffu, 0x0000b7ffu, 0x00000000u, 0x00000000u},
    {0x3effffffu, 0x000037ffu, 0x00000000u, 0x00000000u},
    {0x3f000000u, 0x00003800u, 0x00000000u, 0x00000000u},
    {0xbf000000u, 0x0000b800u, 0x00000000u, 0x00000000u},
    {0x3e7fffffu, 0x000033ffu, 0x00000000u, 0x00000000u},
    {0xbe7fffffu, 0x0000b3ffu, 0x00000000u, 0x00000000u},
    {0x3e800000u, 0x00003400u, 0x00000000u, 0x00000000u},
    {0x3f400000u, 0x00003a00u, 0x00000000u, 0x00000000u},
    {0x437f8000u, 0x00005bfcu, 0x00000000u, 0x00000000u},
    {0x43802000u, 0x00005c01u, 0x00000000u, 0x00000000u},
    {0xc47a2000u, 0x0000e3d1u, 0x00000000u, 0x00000000u},
    {0x7f7fc99eu, 0x00007bffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00008000u, 0x00000000u, 0x00000000u},
    {0x7f800000u, 0x00007c00u, 0x00000000u, 0x00000000u},
    {0x7fc00000u, 0x00007e00u, 0x00000000u, 0x00000000u},
    {0x3dcccccdu, 0x00002e66u, 0x00000000u, 0x00000000u},
    {0xbdcccccdu, 0x0000ae66u, 0x00000000u, 0x00000000u},
    {0x3e99999au, 0x000034cdu, 0x00000000u, 0x00000000u},
    {0xbf333333u, 0x0000b99au, 0x00000000u, 0x00000000u},
    {0x3fa66666u, 0x00003d33u, 0x00000000u, 0x00000000u},
    {0xc0266666u, 0x0000c133u, 0x00000000u, 0x00000000u},
    {0x41240000u, 0x00004920u, 0x00000000u, 0x00000000u},
    {0xc2c84000u, 0x0000d642u, 0x00000000u, 0x00000000u}
};

constexpr std::uint32_t Expected[Threads][Columns] = {
    {0x81c90fd5u, 0x3f800000u, 0x00008006u, 0x00003c00u},
    {0x8ec90fd5u, 0x3f800000u, 0x00008e48u, 0x00003c00u},
    {0xb1c90fd5u, 0x3f800000u, 0x00009a48u, 0x00003c00u},
    {0xb6d2d421u, 0x3f800000u, 0x00009126u, 0x00003c00u},
    {0x01c90fd5u, 0x3f800000u, 0x00000006u, 0x00003c00u},
    {0x36d2d421u, 0x3f800000u, 0x00000e48u, 0x00003c00u},
    {0xb9c8f22fu, 0x3f7ffffeu, 0x00008e47u, 0x00003c00u},
    {0x39c8f22fu, 0x3f7ffffeu, 0x00000e47u, 0x00003c00u},
    {0xb4490fdcu, 0xbf800000u, 0x00009648u, 0x0000bc00u},
    {0x34490fdcu, 0xbf800000u, 0x00001648u, 0x0000bc00u},
    {0x00000000u, 0xbf800000u, 0x00000000u, 0x0000bc00u},
    {0x00000000u, 0xbf800000u, 0x00000000u, 0x0000bc00u},
    {0x3f800000u, 0x33c90fd9u, 0x00003c00u, 0x00001248u},
    {0xbf800000u, 0x33c90fd9u, 0x0000bc00u, 0x00001248u},
    {0x3f800000u, 0x00000000u, 0x00003c00u, 0x00000000u},
    {0xbf800000u, 0x00000000u, 0x0000bc00u, 0x00000000u},
    {0x00000000u, 0xbf800000u, 0x00000000u, 0x0000bc00u},
    {0x3f800000u, 0x00000000u, 0x00003c00u, 0x00000000u},
    {0x00000000u, 0xbf800000u, 0x00000000u, 0x0000bc00u},
    {0x00000000u, 0x3f800000u, 0x00000000u, 0x00003c00u},
    {0x00000000u, 0x3f800000u, 0x00000000u, 0x00003c00u},
    {0x80000000u, 0x3f800000u, 0x00008000u, 0x00003c00u},
    {0xffc00000u, 0xffc00000u, 0x0000fe00u, 0x0000fe00u},
    {0x7fc00000u, 0x7fc00000u, 0x00007e00u, 0x00007e00u},
    {0x3f167919u, 0x3f4f1bbdu, 0x000038b4u, 0x00003a79u},
    {0xbf167919u, 0x3f4f1bbdu, 0x0000b8b4u, 0x00003a79u},
    {0x3f737870u, 0xbe9e377cu, 0x00003b9cu, 0x0000b4f3u},
    {0x3f737870u, 0xbe9e377cu, 0x00003b9du, 0x0000b4edu},
    {0x3f737872u, 0xbe9e3770u, 0x00003b9du, 0x0000b4edu},
    {0x3f167910u, 0xbf4f1bc3u, 0x000038b0u, 0x0000ba7cu},
    {0x3f800000u, 0x00000000u, 0x00003c00u, 0x00000000u},
    {0xbf3504f4u, 0x3f3504f3u, 0x0000b9a8u, 0x000039a8u}
};

constexpr const char* Names[Columns] = {"v_sin_f32", "v_cos_f32", "v_sin_f16", "v_cos_f16"};
constexpr std::uint32_t Tolerance[Threads] = {0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 1u, 1u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 12u, 12u, 12u, 12u, 12u, 12u, 12u, 12u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Ordered(std::uint32_t bits, std::uint32_t width) {
    const std::uint32_t sign = 1u << (width - 1u);
    return (bits & sign) != 0u ? sign - (bits & ~sign) : bits | sign;
}

std::uint32_t Distance(std::uint32_t actual, std::uint32_t expected, std::uint32_t width) {
    const std::uint32_t a = Ordered(actual, width);
    const std::uint32_t e = Ordered(expected, width);
    return a > e ? a - e : e - a;
}

bool IsPinned(std::uint32_t bits, std::uint32_t width) {
    const std::uint32_t magnitude = width == 16u ? bits & 0x7fffu : bits & 0x7fffffffu;
    const std::uint32_t infinity = width == 16u ? 0x7c00u : 0x7f800000u;
    return magnitude == 0u || magnitude >= infinity;
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t column = 0; column < Columns; ++column) {
            const std::uint32_t width = column < 2u ? 32u : 16u;
            const std::uint32_t mask = width == 16u ? 0xffffu : 0xffffffffu;
            const std::uint32_t actual = out[column] & mask;
            const std::uint32_t expected = Expected[tid][column] & mask;
            const std::string name = "sin cos near zero: lane " + std::to_string(tid) + " " + Names[column] + " is " + Hex(actual) + ", expected " + Hex(expected);
            if (IsPinned(expected, width)) {
                Require(actual == expected, name);
                continue;
            }
            Require(Distance(actual, expected, width) <= Tolerance[tid], name + " (tolerance " + std::to_string(Tolerance[tid]) + " ulp)");
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
        Check();
        std::puts("sin cos near zero tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
