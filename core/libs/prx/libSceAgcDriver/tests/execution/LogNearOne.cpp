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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Columns = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u,
    0x7e144f04u, 0x7e16af05u, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][4] = {
    {0x3fc00000u, 0x00003e00u, 0x00000000u, 0x00000000u},
    {0x3f000000u, 0x00003800u, 0x00000000u, 0x00000000u},
    {0x3fa00000u, 0x00003d00u, 0x00000000u, 0x00000000u},
    {0x3f400000u, 0x00003a00u, 0x00000000u, 0x00000000u},
    {0x3f900000u, 0x00003c80u, 0x00000000u, 0x00000000u},
    {0x3f600000u, 0x00003b00u, 0x00000000u, 0x00000000u},
    {0x3f880000u, 0x00003c40u, 0x00000000u, 0x00000000u},
    {0x3f700000u, 0x00003b80u, 0x00000000u, 0x00000000u},
    {0x3f840000u, 0x00003c20u, 0x00000000u, 0x00000000u},
    {0x3f780000u, 0x00003bc0u, 0x00000000u, 0x00000000u},
    {0x3f820000u, 0x00003c10u, 0x00000000u, 0x00000000u},
    {0x3f7c0000u, 0x00003be0u, 0x00000000u, 0x00000000u},
    {0x3f810000u, 0x00003c08u, 0x00000000u, 0x00000000u},
    {0x3f7e0000u, 0x00003bf0u, 0x00000000u, 0x00000000u},
    {0x3f808000u, 0x00003c04u, 0x00000000u, 0x00000000u},
    {0x3f7f0000u, 0x00003bf8u, 0x00000000u, 0x00000000u},
    {0x3f804000u, 0x00003c02u, 0x00000000u, 0x00000000u},
    {0x3f7f8000u, 0x00003bfcu, 0x00000000u, 0x00000000u},
    {0x3f802000u, 0x00003c01u, 0x00000000u, 0x00000000u},
    {0x3f7fc000u, 0x00003bfeu, 0x00000000u, 0x00000000u},
    {0x3f801000u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7fe000u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800800u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ff000u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800400u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ff800u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800200u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ffc00u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800100u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ffe00u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800080u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7fff00u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800040u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7fff80u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800020u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7fffc0u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800010u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7fffe0u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800008u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ffff0u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800004u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ffff8u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800002u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ffffcu, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800001u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7ffffeu, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f7fffffu, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0x00003c00u, 0x00000000u, 0x00000000u},
    {0x3f600000u, 0x00003b00u, 0x00000000u, 0x00000000u},
    {0x3f900000u, 0x00003c80u, 0x00000000u, 0x00000000u},
    {0x3f666666u, 0x00003b33u, 0x00000000u, 0x00000000u},
    {0x3f8ccccdu, 0x00003c66u, 0x00000000u, 0x00000000u},
    {0x3f000000u, 0x00003800u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x00004000u, 0x00000000u, 0x00000000u},
    {0x40400000u, 0x00004200u, 0x00000000u, 0x00000000u},
    {0x41200000u, 0x00004900u, 0x00000000u, 0x00000000u},
    {0x3dcccccdu, 0x00002e66u, 0x00000000u, 0x00000000u},
    {0x3a83126fu, 0x00001419u, 0x00000000u, 0x00000000u},
    {0x447a0000u, 0x000063d0u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00000400u, 0x00000000u, 0x00000000u},
    {0x7f7fffffu, 0x00007bffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbf800000u, 0x0000bc00u, 0x00000000u, 0x00000000u}
};

constexpr std::uint32_t Expected[Threads][Columns] = {
    {0x3f15c01au, 0x000038aeu},
    {0xbf800000u, 0x0000bc00u},
    {0x3ea4d3c2u, 0x00003527u},
    {0xbed47fccu, 0x0000b6a4u},
    {0x3e2e00d2u, 0x00003170u},
    {0xbe4544c0u, 0x0000b22au},
    {0x3db31fb8u, 0x00002d99u},
    {0xbdbeb025u, 0x0000adf6u},
    {0x3d35d69cu, 0x000029afu},
    {0xbd3b9ca6u, 0x0000a9ddu},
    {0x3cb73cb4u, 0x000025bau},
    {0xbcba1f74u, 0x0000a5d1u},
    {0x3c37f286u, 0x000021c0u},
    {0xbc3963ddu, 0x0000a1cbu},
    {0x3bb84e24u, 0x00001dc2u},
    {0xbbb906ceu, 0x00009dc8u},
    {0x3b387c20u, 0x000019c4u},
    {0xbb38d875u, 0x000099c7u},
    {0x3ab8932au, 0x000015c5u},
    {0xbab8c155u, 0x000095c6u},
    {0x3a389eb2u, 0x00000000u},
    {0xba38b5c7u, 0x000091c6u},
    {0x39b8a476u, 0x00000000u},
    {0xb9b8b001u, 0x000091c6u},
    {0x3938a759u, 0x00000000u},
    {0xb938ad1eu, 0x000091c6u},
    {0x38b8a8cau, 0x00000000u},
    {0xb8b8abadu, 0x000091c6u},
    {0x3838a983u, 0x00000000u},
    {0xb838aaf4u, 0x000091c6u},
    {0x37b8a9dfu, 0x00000000u},
    {0xb7b8aa98u, 0x000091c6u},
    {0x3738aa0du, 0x00000000u},
    {0xb738aa6au, 0x000091c6u},
    {0x36b8aa24u, 0x00000000u},
    {0xb6b8aa53u, 0x000091c6u},
    {0x3638aa30u, 0x00000000u},
    {0xb638aa47u, 0x000091c6u},
    {0x35b8aa36u, 0x00000000u},
    {0xb5b8aa41u, 0x000091c6u},
    {0x3538aa39u, 0x00000000u},
    {0xb538aa3eu, 0x000091c6u},
    {0x34b8aa3au, 0x00000000u},
    {0xb4b8aa3du, 0x000091c6u},
    {0x3438aa3bu, 0x00000000u},
    {0xb438aa3cu, 0x000091c6u},
    {0x00000000u, 0x00000000u},
    {0xb3b8aa3cu, 0x000091c6u},
    {0x00000000u, 0x00000000u},
    {0xbe4544c0u, 0x0000b22au},
    {0x3e2e00d2u, 0x00003170u},
    {0xbe1ba6b5u, 0x0000b0deu},
    {0x3e0ccdbbu, 0x00003062u},
    {0xbf800000u, 0x0000bc00u},
    {0x3f800000u, 0x00003c00u},
    {0x3fcae00du, 0x00003e57u},
    {0x40549a78u, 0x000042a5u},
    {0xc0549a79u, 0x0000c2a5u},
    {0xc11f73dbu, 0x0000c8fcu},
    {0x411f73dau, 0x000048fcu},
    {0xc2fc0000u, 0x0000cb00u},
    {0x42ffffffu, 0x00004c00u},
    {0xff800000u, 0x0000fc00u},
    {0xffc00000u, 0x0000fe00u}
};

constexpr const char* Names[Columns] = {"v_log_f32", "v_log_f16"};
constexpr std::uint32_t Tolerance[Threads] = {12u, 12u, 12u, 12u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u, 12u, 12u, 12u, 12u, 12u, 12u, 12u, 2u, 2u, 2u, 2u};

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

void Run(AgcDriver::VulkanDevice& device) {
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t column = 0; column < Columns; ++column) {
            const std::uint32_t width = column == 0u ? 32u : 16u;
            const std::uint32_t mask = width == 16u ? 0xffffu : 0xffffffffu;
            const std::uint32_t actual = out[column] & mask;
            const std::uint32_t expected = Expected[tid][column] & mask;
            const std::string name = "log near one: lane " + std::to_string(tid) + " " + Names[column] + " of " + Hex(Rows[tid][column]) + " is " + Hex(actual) + ", expected " + Hex(expected);
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
        Run(*device);
        Check();
        std::puts("log near one tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
