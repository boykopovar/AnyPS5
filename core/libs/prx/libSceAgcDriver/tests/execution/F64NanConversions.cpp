#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e142104, 0x7e181f06, 0xe0701000,
    0x80010a03, 0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x7fc00001u, 0x00000000u, 0x00000001u, 0x7ff80000u},
    {0xffc00001u, 0x00000000u, 0x20000000u, 0x7ff80000u},
    {0x7f800001u, 0x00000000u, 0xffffffffu, 0x7fffffffu},
    {0xff800001u, 0x00000000u, 0x00000001u, 0x7ff00000u},
    {0x7fffffffu, 0x00000000u, 0xe0000000u, 0xfff00001u},
    {0x7fa5a5a5u, 0x00000000u, 0x12345678u, 0x7ff9abcdu},
    {0xffbfffffu, 0x00000000u, 0x00000000u, 0xfff80000u},
    {0x7fc00000u, 0x00000000u, 0x1fffffffu, 0x7ff00000u},
    {0x3f800000u, 0x00000000u, 0x00000000u, 0x3ff00000u},
    {0x7fc00001u, 0x00000000u, 0x00000001u, 0x7ff80000u},
    {0xffc00001u, 0x00000000u, 0x20000000u, 0x7ff80000u},
    {0x7f800001u, 0x00000000u, 0xffffffffu, 0x7fffffffu},
    {0xff800001u, 0x00000000u, 0x00000001u, 0x7ff00000u},
    {0x7fffffffu, 0x00000000u, 0xe0000000u, 0xfff00001u},
    {0x7fa5a5a5u, 0x00000000u, 0x12345678u, 0x7ff9abcdu},
    {0xffbfffffu, 0x00000000u, 0x00000000u, 0xfff80000u},
    {0x7fc00000u, 0x00000000u, 0x1fffffffu, 0x7ff00000u},
    {0x3f800000u, 0x00000000u, 0x00000000u, 0x3ff00000u},
    {0x7fe5b1f5u, 0x00000000u, 0x91b7584au, 0xfff16adfu},
    {0xffe13e31u, 0x00000000u, 0xc386bbc4u, 0x7ff7c4d1u},
    {0x7fcc343du, 0x00000000u, 0x1e2feb89u, 0x7ff4d57bu},
    {0xffce6f45u, 0x00000000u, 0x7311d8a3u, 0x7ff51061u},
    {0xffcecc1bu, 0x00000000u, 0x612e7696u, 0xfff9c617u},
    {0x7fbf992du, 0x00000000u, 0x18072e8cu, 0x7ff42c83u},
    {0x7fc1c7a9u, 0x00000000u, 0xe4b06ce6u, 0xfff4b3b3u},
    {0x7fca828du, 0x00000000u, 0x6ec9d286u, 0xfff10e77u},
    {0xffa4c985u, 0x00000000u, 0xc4647159u, 0x7ffa05a7u},
    {0xffa21a59u, 0x00000000u, 0x7204e52du, 0x7ffe3d43u},
    {0xffb6d8ffu, 0x00000000u, 0xcd447e35u, 0x7ff02931u},
    {0xffd5d4c1u, 0x00000000u, 0xf1fd42a2u, 0x7ffb8f1fu},
    {0xffc3f339u, 0x00000000u, 0x51431193u, 0x7ff4beddu},
    {0x7fb6e6e3u, 0x00000000u, 0x06839eb9u, 0xfff8a7ddu},
};
constexpr std::uint32_t Expected[32][3] = {
    {0x20000000u, 0x7ff80000u, 0x7fc00000u},
    {0x20000000u, 0xfff80000u, 0x7fc00001u},
    {0x20000000u, 0x7ff80000u, 0x7fffffffu},
    {0x20000000u, 0xfff80000u, 0x7fc00000u},
    {0xe0000000u, 0x7fffffffu, 0xffc0000fu},
    {0xa0000000u, 0x7ffcb4b4u, 0x7fcd5e68u},
    {0xe0000000u, 0xffffffffu, 0xffc00000u},
    {0x00000000u, 0x7ff80000u, 0x7fc00000u},
    {0x00000000u, 0x3ff00000u, 0x3f800000u},
    {0x20000000u, 0x7ff80000u, 0x7fc00000u},
    {0x20000000u, 0xfff80000u, 0x7fc00001u},
    {0x20000000u, 0x7ff80000u, 0x7fffffffu},
    {0x20000000u, 0xfff80000u, 0x7fc00000u},
    {0xe0000000u, 0x7fffffffu, 0xffc0000fu},
    {0xa0000000u, 0x7ffcb4b4u, 0x7fcd5e68u},
    {0xe0000000u, 0xffffffffu, 0xffc00000u},
    {0x00000000u, 0x7ff80000u, 0x7fc00000u},
    {0x00000000u, 0x3ff00000u, 0x3f800000u},
    {0xa0000000u, 0x7ffcb63eu, 0xffcb56fcu},
    {0x20000000u, 0xfffc27c6u, 0x7ffe268eu},
    {0xa0000000u, 0x7ff98687u, 0x7fe6abd8u},
    {0xa0000000u, 0xfff9cde8u, 0x7fe8830bu},
    {0x60000000u, 0xfff9d983u, 0xffce30bbu},
    {0xa0000000u, 0x7ffff325u, 0x7fe16418u},
    {0x20000000u, 0x7ff838f5u, 0xffe59d9fu},
    {0xa0000000u, 0x7ff95051u, 0xffc873bbu},
    {0xa0000000u, 0xfffc9930u, 0x7fd02d3eu},
    {0x20000000u, 0xfffc434bu, 0x7ff1ea1bu},
    {0xe0000000u, 0xfffedb1fu, 0x7fc1498eu},
    {0x20000000u, 0xfffaba98u, 0x7fdc78ffu},
    {0x20000000u, 0xfff87e67u, 0x7fe5f6eau},
    {0x60000000u, 0x7ffedcdcu, 0xffc53ee8u},
};

void Fill(std::uint32_t tid, std::uint32_t* words) { std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words); }

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("f64 nan conversions: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        Expect(tid, out[0], Expected[tid][0], "cvt f64 f32 lo");
        Expect(tid, out[1], Expected[tid][1], "cvt f64 f32 hi");
        Expect(tid, out[2], Expected[tid][2], "cvt f32 f64");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("f64 nan conversions tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
