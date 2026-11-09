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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0xe0302000, 0x80000400, 0xbf8c3f70, 0x7e141704, 0x2c0a0890, 0x7e161705, 0x7e0c0280,
    0x76180cf9, 0x06001604, 0x761a0cf9, 0x06011604, 0xe0782000, 0x80010a00, 0xbf810000,
};

struct Vector {
    std::uint32_t input;
    std::array<std::uint32_t, Results> expected;
};

constexpr Vector Vectors[] = {
    {0x3c003c00u, {0x3f800000u, 0x3f800000u, 0x00000000u, 0x0000003cu}},
    {0xc000bc00u, {0xbf800000u, 0xc0000000u, 0x00000000u, 0x000000bcu}},
    {0x80000000u, {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u}},
    {0x00008000u, {0x80000000u, 0x00000000u, 0x00000000u, 0x00000080u}},
    {0x03ff0001u, {0x33800000u, 0x387fc000u, 0x00000001u, 0x00000000u}},
    {0x040003ffu, {0x387fc000u, 0x38800000u, 0x000000ffu, 0x00000003u}},
    {0x7bff0400u, {0x38800000u, 0x477fe000u, 0x00000000u, 0x00000004u}},
    {0xfbff7bffu, {0x477fe000u, 0xc77fe000u, 0x000000ffu, 0x0000007bu}},
    {0xfc007c00u, {0x7f800000u, 0xff800000u, 0x00000000u, 0x0000007cu}},
    {0x7e01fe01u, {0xffc02000u, 0x7fc02000u, 0x00000001u, 0x000000feu}},
    {0xfe017e01u, {0x7fc02000u, 0xffc02000u, 0x00000001u, 0x0000007eu}},
    {0x7fff7e55u, {0x7fcaa000u, 0x7fffe000u, 0x00000055u, 0x0000007eu}},
    {0xfffffe55u, {0xffcaa000u, 0xffffe000u, 0x00000055u, 0x000000feu}},
    {0xfff0fff0u, {0xfffe0000u, 0xfffe0000u, 0x000000f0u, 0x000000ffu}},
    {0xa5a500f0u, {0x37700000u, 0xbcb4a000u, 0x000000f0u, 0x00000000u}},
    {0xdead00ffu, {0x377f0000u, 0xc3d5a000u, 0x000000ffu, 0x00000000u}},
    {0x7c003555u, {0x3eaaa000u, 0x7f800000u, 0x00000055u, 0x00000035u}},
    {0x3555fc00u, {0xff800000u, 0x3eaaa000u, 0x00000000u, 0x000000fcu}},
    {0x3c000200u, {0x38000000u, 0x3f800000u, 0x00000000u, 0x00000002u}},
    {0x80018001u, {0xb3800000u, 0xb3800000u, 0x00000001u, 0x00000080u}},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), Threads, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid] = Vectors[tid % std::size(Vectors)].input;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), 4u);
    const auto output = BufferDescriptor(Output.data(), Results * 4u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    constexpr std::array<const char*, Results> names{"low half", "high half", "BYTE_0 ldexp", "BYTE_1 ldexp"};
    for (std::uint32_t tid = 0; tid < waveSize; ++tid) {
        const auto& vector = Vectors[tid % std::size(Vectors)];
        for (std::uint32_t i = 0; i < Results; ++i) {
            const auto actual = Output[tid * Results + i];
            Require(actual == vector.expected[i],
                "half expansion: wave " + std::to_string(waveSize) + ", lane " + std::to_string(tid) +
                ", input " + Hex(vector.input) + ", " + names[i] + " is " + Hex(actual) +
                ", expected " + Hex(vector.expected[i]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 32u, device->ComputeTarget(32u));
        Run(*device, 64u, device->Target());
        Run(*device, 64u, device->ComputeTarget(32u));
        std::puts("half expansion tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
