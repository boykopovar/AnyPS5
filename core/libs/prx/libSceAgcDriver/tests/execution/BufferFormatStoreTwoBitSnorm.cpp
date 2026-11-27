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

constexpr std::uint32_t Threads = 20;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34040083u, 0x4a060484u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xe99f1000u, 0x80010402u,
    0xe96f1000u, 0x80010403u, 0xbf810000u,
};

constexpr std::array<std::array<std::uint32_t, Inputs>, Threads> Values{{
    {0x00000000u,0x3e800000u,0xbf400000u,0x00000000u},
    {0x80000000u,0x3e800000u,0xbf400000u,0x80000000u},
    {0x3e800000u,0x3e800000u,0xbf400000u,0x3e800000u},
    {0x3efffffdu,0x3e800000u,0xbf400000u,0x3efffffdu},
    {0x3f000000u,0x3e800000u,0xbf400000u,0x3f000000u},
    {0x3f000002u,0x3e800000u,0xbf400000u,0x3f000002u},
    {0x3f400000u,0x3e800000u,0xbf400000u,0x3f400000u},
    {0x3f800000u,0x3e800000u,0xbf400000u,0x3f800000u},
    {0x3fc00000u,0x3e800000u,0xbf400000u,0x3fc00000u},
    {0xbe800000u,0x3e800000u,0xbf400000u,0xbe800000u},
    {0xbefffffdu,0x3e800000u,0xbf400000u,0xbefffffdu},
    {0xbf000000u,0x3e800000u,0xbf400000u,0xbf000000u},
    {0xbf000002u,0x3e800000u,0xbf400000u,0xbf000002u},
    {0xbf400000u,0x3e800000u,0xbf400000u,0xbf400000u},
    {0xbf800000u,0x3e800000u,0xbf400000u,0xbf800000u},
    {0xc0000000u,0x3e800000u,0xbf400000u,0xc0000000u},
    {0x7f800000u,0x3e800000u,0xbf400000u,0x7f800000u},
    {0xff800000u,0x3e800000u,0xbf400000u,0xff800000u},
    {0x7fc00000u,0x3e800000u,0xbf400000u,0x7fc00000u},
    {0x0da24260u,0x3e800000u,0xbf400000u,0x0da24260u},
}};

constexpr std::array<std::array<std::uint32_t, Results>, Threads> Expected{{
    {0x28120000u, 0x00281200u},
    {0x28120000u, 0x00281200u},
    {0x28120080u, 0x20281200u},
    {0x281200ffu, 0x3fe81200u},
    {0x68120100u, 0x40281201u},
    {0x68120100u, 0x40281201u},
    {0x6812017fu, 0x5fe81201u},
    {0x681201ffu, 0x7fe81201u},
    {0x681201ffu, 0x7fe81201u},
    {0x28120380u, 0xe0281200u},
    {0x28120301u, 0xc0681200u},
    {0xe8120300u, 0xc0281203u},
    {0xe8120300u, 0xc0281203u},
    {0xe8120281u, 0xa0681203u},
    {0xe8120201u, 0x80681203u},
    {0xe8120201u, 0x80681203u},
    {0x681201ffu, 0x7fe81201u},
    {0xe8120201u, 0x80681203u},
    {0x28120000u, 0x00281200u},
    {0x28120000u, 0x00281200u},
}};

constexpr const char* Labels[Threads] = {"0.0", "-0.0", "0.25", "0.4999999", "0.5", "0.5000001", "0.75", "1.0", "1.5", "-0.25", "-0.4999999", "-0.5", "-0.5000001", "-0.75", "-1.0", "-2.0", "+inf", "-inf", "NaN", "1e-30"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(Values[tid].begin(), Values[tid].end(), &Input[tid * Inputs]);
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
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device->WaitIdle();
        constexpr const char* Formats[Results] = {"2_10_10_10_SNORM", "10_10_10_2_SNORM"};
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t format = 0; format < Results; ++format) {
                const std::uint32_t actual = Output[tid * Results + format];
                Require(actual == Expected[tid][format], std::string("tbuffer_store_format_xyzw ") + Formats[format] + " of x = w = " + Labels[tid] + " is " + Hex(actual) + ", expected " + Hex(Expected[tid][format]));
            }
        }
        std::puts("two-bit snorm store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
