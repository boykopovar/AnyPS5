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
#include <optional>
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
bool Ieee = false;

alignas(256) constexpr std::array<std::uint32_t, 30> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xd564000a, 0x02020d04, 0xd565000c,
    0x02020d04, 0xd54c000e, 0x041a0d04, 0xd54c0010, 0x84320d04, 0xd5660012, 0x02020d04, 0xd5670014,
    0x02020d04, 0xd5680016, 0x02020d04, 0xd5640318, 0x22020d04, 0xe0781000, 0x80010a03, 0xe0781010,
    0x80010e03, 0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x3ff00000u, 0x80000000u, 0x40000000u},
    {0x00000000u, 0x3ff80000u, 0x7fffffffu, 0x40000000u},
    {0xffffffffu, 0x7fefffffu, 0xfffff7ccu, 0x3ff00000u},
    {0x00000000u, 0xbff00000u, 0xfffffbcdu, 0x3ff00000u},
    {0x00000000u, 0x3fe00000u, 0x00000401u, 0x3ff00000u},
    {0x00000000u, 0xfff00000u, 0xffc00000u, 0x41dfffffu},
    {0x8a59a474u, 0xbefa45c5u, 0x12eaf8e8u, 0xc020f641u},
    {0x00000000u, 0x3ff00000u, 0x00000001u, 0x7ff00000u},
    {0x00000001u, 0xfff80000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0x7ff00000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff00000u},
    {0xd9787a36u, 0xfff7c837u, 0x00000000u, 0x41f00000u},
    {0x00000000u, 0x3ff80000u, 0xffffffffu, 0xffefffffu},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u},
    {0x047b57e5u, 0x02f22376u, 0xc66ab30du, 0x8e9a2140u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xbff00000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xbfe00000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3ff80000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x40040000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xc0040000u},
    {0x00000000u, 0x00000000u, 0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0x00000000u, 0xffffffffu, 0xffefffffu},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x7ff80000u},
    {0x00000000u, 0x00000000u, 0x00000001u, 0xfff80000u},
    {0x00000000u, 0x00000000u, 0x00000001u, 0x7ff00000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x7ff40000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x41e00000u}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x80000000u, 0x40080000u, 0x80000000u, 0x40000000u, 0x80000000u, 0x40100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x80000000u, 0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00001u},
    {0x7fffffffu, 0x400c0000u, 0xbffffffeu, 0x40080000u, 0x9fffffffu, 0x40140000u, 0x00000000u, 0x3cb00000u, 0x00000000u, 0x3ff80000u, 0x7fffffffu, 0x40000000u, 0x00000000u, 0x7ff00000u, 0xfffffffcu, 0x3fe00001u},
    {0xffffffffu, 0x7fefffffu, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0xfff00000u, 0xfffff7ccu, 0x3ff00000u, 0xffffffffu, 0x7fefffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffefffffu},
    {0x79a00000u, 0x3eafffffu, 0xfffffbcdu, 0xbff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xbff00000u, 0xfffffbcdu, 0x3ff00000u, 0x00000000u, 0x80000000u, 0x79a00000u, 0x3eafffffu},
    {0x00000401u, 0x3ff80000u, 0x00000401u, 0x3fe00000u, 0x00000602u, 0x3ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00000401u, 0x3ff00000u, 0x00000000u, 0x7ff00000u, 0x00000802u, 0x3fe00000u},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xfff00000u, 0xffc00000u, 0x41dfffffu, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u},
    {0x5ba3aa33u, 0xc020f644u, 0x220b9c1cu, 0x3f2bda21u, 0x38c9d6dcu, 0xc020f625u, 0x35aa5b80u, 0xbbc1a7fdu, 0x12eaf8e8u, 0xc020f641u, 0x8a59a474u, 0xbefa45c5u, 0x00000000u, 0xfff00000u, 0xca32479du, 0x4020f63du},
    {0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000000u, 0x40000000u, 0x00000001u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xbff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0xbff00000u, 0x00000000u, 0x7ff00000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0xfff80000u},
    {0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u, 0xd9787a36u, 0xffffc837u},
    {0xffffffffu, 0xffefffffu, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0x7ff00000u, 0xffffffffu, 0xffefffffu, 0x00000000u, 0x3ff80000u, 0x00000000u, 0x3fe80000u, 0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0xfff80000u},
    {0xc66ab30du, 0x8e9a2140u, 0x00000000u, 0x80000000u, 0xc66ab30du, 0x8e9a2140u, 0x00000000u, 0x80000000u, 0xc66ab30du, 0x8e9a2140u, 0x047b57e5u, 0x02f22376u, 0x00000000u, 0x00000000u, 0xc66ab30du, 0x0e9a2140u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x3ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xbff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xbff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u},
    {0x00000000u, 0x3fe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u},
    {0x00000000u, 0xbfe00000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xbfe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xbfe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u},
    {0x00000000u, 0x3ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff80000u},
    {0x00000000u, 0x40040000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40040000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40040000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40040000u},
    {0x00000000u, 0xc0040000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0040000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xc0040000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40040000u},
    {0xffffffffu, 0x7fefffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0x7fefffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x7fefffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu, 0x00000000u, 0x80000000u, 0xffffffffu, 0xffefffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffefffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x7ff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7ff80000u},
    {0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x7ff80000u},
    {0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x7ff80000u},
    {0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7ffc0000u},
    {0x00000000u, 0x41e00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x41e00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x41e00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x41e00000u}
};
constexpr std::uint32_t MinMaxNoIeee[32][4] = {
    {0x00000000u, 0x3ff00000u, 0x80000000u, 0x40000000u},
    {0x00000000u, 0x3ff80000u, 0x7fffffffu, 0x40000000u},
    {0xfffff7ccu, 0x3ff00000u, 0xffffffffu, 0x7fefffffu},
    {0x00000000u, 0xbff00000u, 0xfffffbcdu, 0x3ff00000u},
    {0x00000000u, 0x3fe00000u, 0x00000401u, 0x3ff00000u},
    {0x00000000u, 0xfff00000u, 0xffc00000u, 0x41dfffffu},
    {0x12eaf8e8u, 0xc020f641u, 0x8a59a474u, 0xbefa45c5u},
    {0x00000000u, 0x3ff00000u, 0x00000000u, 0x3ff00000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0x7ff00000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u},
    {0x00000000u, 0x41f00000u, 0x00000000u, 0x41f00000u},
    {0xffffffffu, 0xffefffffu, 0x00000000u, 0x3ff80000u},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u},
    {0xc66ab30du, 0x8e9a2140u, 0x047b57e5u, 0x02f22376u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3fe00000u},
    {0x00000000u, 0xbfe00000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3ff80000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x40040000u},
    {0x00000000u, 0xc0040000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0xffffffffu, 0x7fefffffu},
    {0xffffffffu, 0xffefffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x41e00000u}
};
constexpr const char* Names[16] = {"add lo", "add hi", "mul lo", "mul hi", "fma lo", "fma hi", "fma error lo", "fma error hi", "min lo", "min hi", "max lo", "max hi", "ldexp lo", "ldexp hi", "add abs neg lo", "add abs neg hi"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string(Ieee ? "f64 arithmetic, IEEE mode: lane " : "f64 arithmetic: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::optional<ShaderRecompiler::ShaderFloatMode> mode) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = mode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 16; ++i) {
            std::uint32_t expected = !Ieee && i >= 8u && i < 12u ? MinMaxNoIeee[tid][i - 8u] : Expected[tid][i];
            if (!Ieee && i == 13u && tid == 13u) expected = in[1];
            Expect(tid, out[i], expected, Names[i]);
        }
    }
}

std::uint64_t MinMaxReference(std::uint64_t lhs, std::uint64_t rhs, bool maximum, bool ieee) {
    constexpr std::uint64_t magnitude = 0x7fffffffffffffffull;
    constexpr std::uint64_t infinity = 0x7ff0000000000000ull;
    constexpr std::uint64_t quiet = 0x0008000000000000ull;
    const bool lhsNan = (lhs & magnitude) > infinity;
    const bool rhsNan = (rhs & magnitude) > infinity;
    if (ieee && lhsNan && (lhs & quiet) == 0u) return lhs | quiet;
    if (ieee && rhsNan && (rhs & quiet) == 0u) return rhs | quiet;
    if (rhsNan) return lhs;
    if (lhsNan) return rhs;
    if ((lhs & magnitude) == 0u && (rhs & magnitude) == 0u) return maximum ? lhs & rhs : lhs | rhs;
    return maximum ? (std::bit_cast<double>(lhs) > std::bit_cast<double>(rhs) ? lhs : rhs) : (std::bit_cast<double>(lhs) < std::bit_cast<double>(rhs) ? lhs : rhs);
}

void CheckMinMaxDenormals(AgcDriver::VulkanDevice& device) {
    const bool canPreserveDenormals = device.Target().float64DenormPreserve;
    constexpr std::array<std::array<std::uint64_t, 2>, Threads> rows{{
        {0x0000000000000001ull, 0x0000000000000000ull}, {0x0000000000000000ull, 0x0000000000000001ull},
        {0x8000000000000001ull, 0x0000000000000000ull}, {0x0000000000000000ull, 0x8000000000000001ull},
        {0x0000000000000001ull, 0x8000000000000001ull}, {0x8000000000000001ull, 0x0000000000000001ull},
        {0x000fffffffffffffull, 0x800fffffffffffffull}, {0x800fffffffffffffull, 0x000fffffffffffffull},
        {0x000fffffffffffffull, 0x0010000000000000ull}, {0x0010000000000000ull, 0x000fffffffffffffull},
        {0x800fffffffffffffull, 0x8010000000000000ull}, {0x8010000000000000ull, 0x800fffffffffffffull},
        {0x0000000000000001ull, 0x000fffffffffffffull}, {0x000fffffffffffffull, 0x0000000000000001ull},
        {0x8000000000000001ull, 0x800fffffffffffffull}, {0x800fffffffffffffull, 0x8000000000000001ull},
        {0x7ff8000000000123ull, 0x0000000000000001ull}, {0x0000000000000001ull, 0x7ff8000000000123ull},
        {0xfff8000000000123ull, 0x800fffffffffffffull}, {0x800fffffffffffffull, 0xfff8000000000123ull},
        {0x7ff0000000000123ull, 0x0000000000000001ull}, {0x0000000000000001ull, 0x7ff0000000000123ull},
        {0xfff0000000000123ull, 0x800fffffffffffffull}, {0x800fffffffffffffull, 0xfff0000000000123ull},
        {0x7ff0000000000000ull, 0x000fffffffffffffull}, {0x000fffffffffffffull, 0x7ff0000000000000ull},
        {0xfff0000000000000ull, 0x800fffffffffffffull}, {0x800fffffffffffffull, 0xfff0000000000000ull},
        {0x0000000000000000ull, 0x8000000000000000ull}, {0x8000000000000000ull, 0x0000000000000000ull},
        {0x0010000000000000ull, 0x8010000000000000ull}, {0x8010000000000000ull, 0x0010000000000000ull},
    }};
    constexpr std::array<std::array<std::uint32_t, 2>, 4> modifiers{{{0u, 0u}, {1u, 2u}, {2u, 1u}, {3u, 3u}}};
    for (const auto& modifier : modifiers) {
        alignas(256) auto code = Code;
        for (const auto word : {13u, 15u}) {
            code[word] |= modifier[0] << 8u;
            code[word + 1u] |= modifier[1] << 29u;
        }
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t source = 0; source < 2u; ++source) {
                Input[tid * Inputs + source * 2u] = static_cast<std::uint32_t>(rows[tid][source]);
                Input[tid * Inputs + source * 2u + 1u] = static_cast<std::uint32_t>(rows[tid][source] >> 32u);
            }
        }
        for (const int denormals : {-1, 0, 2, 3}) {
            for (const bool ieee : {false, true}) {
                if (denormals < 0 && ieee) continue;
                std::optional<ShaderRecompiler::ShaderFloatMode> mode;
                if (denormals >= 0) mode = ShaderRecompiler::ShaderFloatMode{0x30u | (static_cast<std::uint32_t>(denormals) << 6u), true, ieee, false};
                Run(device, code, mode);
                const bool keepsDenormals = denormals < 0 || (denormals & 1) != 0;
                std::uint32_t skippedWords = 0u;
                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                    auto pair = rows[tid];
                    for (std::uint32_t source = 0; source < 2u; ++source) {
                        if (((modifier[0] >> source) & 1u) != 0u) pair[source] &= 0x7fffffffffffffffull;
                        if (((modifier[1] >> source) & 1u) != 0u) pair[source] ^= 0x8000000000000000ull;
                        if ((denormals == 0 || denormals == 2) && (pair[source] & 0x7ff0000000000000ull) == 0u) pair[source] &= 0x8000000000000000ull;
                    }
                    for (std::uint32_t operation = 0; operation < 2u; ++operation) {
                        const auto expected = MinMaxReference(pair[0], pair[1], operation != 0u, ieee);
                        for (std::uint32_t word = 0; word < 2u; ++word) {
                            const auto actual = Output[tid * Results + 8u + operation * 2u + word];
                            const auto expectedWord = static_cast<std::uint32_t>(expected >> (word * 32u));
                            if (actual != expectedWord && keepsDenormals && !canPreserveDenormals) {
                                ++skippedWords;
                                continue;
                            }
                            Require(actual == expectedWord, "f64 min/max denormal mode " + std::to_string(denormals) + " IEEE " + std::to_string(ieee) + " abs " + std::to_string(modifier[0]) + " neg " + std::to_string(modifier[1]) + " lane " + std::to_string(tid) + " " + Names[8u + operation * 2u + word] + " is " + Hex(actual) + ", expected " + Hex(expectedWord));
                        }
                    }
                }
                if (skippedWords != 0u) std::printf("skipped %u f64 min/max result words in denormal mode %d IEEE %u abs %u neg %u: device has no f64 DenormPreserve\n", skippedWords, denormals, ieee ? 1u : 0u, modifier[0], modifier[1]);
            }
        }
    }
    std::puts("f64 min/max denormal input checks completed");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        for (const bool ieee : {false, true}) {
            Ieee = ieee;
            for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
            Run(*device, Code, ieee ? std::optional{ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}} : std::nullopt);
            Check();
        }
        CheckMinMaxDenormals(*device);
        std::puts("f64 arithmetic tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
