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
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020081, 0xe0302000, 0x80000400, 0xbf8c3f70, 0x7e146b04, 0x7e166d04, 0xe0702000, 0x80010a01,
    0xe0702004, 0x80010b01, 0xbf810000,
};

struct Row {
    std::uint32_t x, sin, cos;
};

constexpr std::array<Row, Threads> Rows{{
    {0x80800000u, 0x81c90fd5u, 0x3f800000u},
    {0x8f800000u, 0x90c90fd5u, 0x3f800000u},
    {0xa3d42337u, 0xa5269cc5u, 0x3f800000u},
    {0xb0800000u, 0xb1c90fd5u, 0x3f800000u},
    {0xb2800000u, 0xb3c90fd5u, 0x3f800000u},
    {0xb3000000u, 0xb4490fd5u, 0x3f800000u},
    {0xb3800000u, 0xb4c90fd5u, 0x3f800000u},
    {0xb3c00000u, 0xb516cbe0u, 0x3f800000u},
    {0xb8732b55u, 0xb9befc10u, 0x3f7ffffeu},
    {0xbc23d70au, 0xbd80984du, 0x3f7f7eadu},
    {0xbe82a4d7u, 0xbf7fdd81u, 0xbd04dfbau},
    {0xbe000000u, 0xbf3504f4u, 0x3f3504f3u},
    {0xbeb33333u, 0xbf4f1bbdu, 0xbf167918u},
    {0xbef5c28fu, 0xbe005760u, 0xbf7dfb3au},
    {0x00800000u, 0x01c90fd5u, 0x3f800000u},
    {0x30800000u, 0x31c90fd5u, 0x3f800000u},
    {0x33800000u, 0x34c90fd5u, 0x3f800000u},
    {0x3c23d70au, 0x3d80984du, 0x3f7f7eadu},
    {0x3e82a4d7u, 0x3f7fdd81u, 0xbd04dfbau},
    {0x3f3d70a4u, 0xbf7f7eadu, 0xbd809845u},
    {0xbf3d70a4u, 0x3f7f7eadu, 0xbd809845u},
    {0x42c8999au, 0x3f73780du, 0xbe9e39ddu},
    {0xc2c8999au, 0xbf73780du, 0xbe9e39ddu},
    {0xc348b333u, 0x3f73780du, 0xbe9e39ddu},
    {0x80000000u, 0x80000000u, 0x3f800000u},
    {0x00000000u, 0x00000000u, 0x3f800000u},
    {0xbf000000u, 0x00000000u, 0xbf800000u},
    {0xbe800000u, 0xbf800000u, 0x00000000u},
    {0xbf800000u, 0x00000000u, 0x3f800000u},
    {0xcb000001u, 0x00000000u, 0x3f800000u},
    {0x7f7fffffu, 0x00000000u, 0x3f800000u},
    {0xff800000u, 0xffc00000u, 0xffc00000u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid] = Rows[tid].x;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool Near(std::uint32_t actual, std::uint32_t expected) {
    const float value = std::bit_cast<float>(expected);
    return std::abs(static_cast<double>(std::bit_cast<float>(actual)) - value) <= std::abs(value) * 0x1p-10;
}

void Expect(std::uint32_t tid, const char* name, std::uint32_t actual, std::uint32_t expected, bool accurate) {
    const float value = std::bit_cast<float>(expected);
    const bool exact = !std::isfinite(value) || value == 0.0f;
    Require(exact ? actual == expected : !accurate || Near(actual, expected), std::string("sin cos reduction: lane ") + std::to_string(tid) + " " + name + " of " + Hex(Rows[tid].x) + " is " + Hex(actual) + (exact ? ", expected " : ", expected near ") + Hex(expected));
}

void Check() {
    bool accurate = true;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float x = std::bit_cast<float>(Rows[tid].x);
        if (x > 0.0f && x < 0.5f) {
            accurate = accurate && Near(Output[tid * Results], Rows[tid].sin) && Near(Output[tid * Results + 1], Rows[tid].cos);
        }
    }
    if (!accurate) {
        std::puts("the device's sin and cos are not within 2^-10 of the hardware for small positive inputs, so only zero, infinite and NaN results are checked");
    }
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Expect(tid, "v_sin_f32", Output[tid * Results], Rows[tid].sin, accurate);
        Expect(tid, "v_cos_f32", Output[tid * Results + 1], Rows[tid].cos, accurate);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("sin cos reduction tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
