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
#include <stdexcept>
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

alignas(256) constexpr std::array<std::uint32_t, 48> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e3402f4u, 0x7e3602f0u, 0x7e3802ffu, 0x3f7ffffeu, 0x7e3a02f2u, 0x7e3c02ffu, 0x00400000u, 0x7e3e02f6u,
    0x7e4002ffu, 0x3e800000u, 0x7e4202f4u,
    0x101408f4u, 0x1016091au,
    0x101808f0u, 0x101a091bu,
    0x101c08ffu, 0x3f7ffffeu, 0x101e091cu,
    0x102008ffu, 0x3f800000u, 0x1022091du,
    0x102408ffu, 0x00400000u, 0x1026091eu,
    0x0e2808f6u, 0x0e2a091fu,
    0x582c0b04u, 0x3e800000u, 0xd54b0017u, 0x04164104u,
    0xd54b0218u, 0x4415e904u, 0xd54b0219u, 0x44164304u,
    0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u,
    0xbf810000u,
};

constexpr std::uint32_t Rows[32][2] = {
    {0x00800000u, 0x00000000u}, {0x80800000u, 0x00000000u}, {0x00800001u, 0x00000000u}, {0x00ffffffu, 0x00000000u},
    {0x01000000u, 0x00000000u}, {0x017fffffu, 0x00000000u}, {0x01800000u, 0x00000000u}, {0x00000001u, 0x00000000u},
    {0x807fffffu, 0x00000000u}, {0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u}, {0x7f800000u, 0x00000000u},
    {0xff800000u, 0x00000000u}, {0x7fc00000u, 0x00000000u}, {0x7f7fffffu, 0x00000000u}, {0x3f800000u, 0x00000000u},
    {0xbf800000u, 0x3f800000u}, {0x00c00000u, 0x00000000u}, {0x80ffffffu, 0x80000000u}, {0x01000001u, 0x00000000u},
    {0x3f7fffffu, 0x00000000u}, {0x40490fdbu, 0x00000000u}, {0x00fffffeu, 0x00000000u}, {0x81800000u, 0x00000000u},
    {0x017ffffeu, 0x00000000u}, {0x01800001u, 0x00000000u}, {0x0080ffffu, 0x00000000u}, {0x7f000000u, 0x00000000u},
    {0x00800000u, 0x3f800000u}, {0x817fffffu, 0x80000000u}, {0x017fffffu, 0x00800000u}, {0x017fffffu, 0x80000001u}
};

constexpr const char* Names[8] = {
    "v_mul_f32 2.0",
    "v_mul_f32 0.5",
    "v_mul_f32 0x3f7ffffe",
    "v_mul_f32 0x3f800000",
    "v_mul_f32 0x00400000",
    "v_mul_legacy_f32 4.0",
    "v_fmamk_f32 0x3e800000",
    "v_fma_f32 -|2.0|"
};

struct Pinned {
    std::uint32_t tid;
    std::uint32_t pair;
    std::uint32_t value;
};

constexpr std::array<Pinned, 6> TinyProducts{{
    {0u, 2u, 0x00000000u},
    {3u, 1u, 0x00000000u},
    {18u, 1u, 0x80000000u},
    {5u, 6u, 0x00000000u},
    {29u, 6u, 0x80000000u},
    {31u, 6u, 0x00000000u},
}};

constexpr Pinned TinyOnlyBeforeRounding{2u, 2u, 0x00800000u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t pair = 0; pair < 8u; ++pair) {
            const std::uint32_t constant = out[pair * 2u];
            const std::uint32_t reg = out[pair * 2u + 1u];
            const std::string where = std::string("f32 constant factor: lane ") + std::to_string(tid) + " " + mode + " " + Names[pair];
            if (IsNan(reg)) {
                Require(IsNan(constant), where + " is " + Hex(constant) + " with a constant and a NaN with a register");
                continue;
            }
            Require(constant == reg, where + " is " + Hex(constant) + " with a constant and " + Hex(reg) + " with a register");
        }
    }
}

void CheckTinyProducts(const char* mode) {
    for (const Pinned& pinned : TinyProducts) {
        const std::uint32_t actual = Output[pinned.tid * Results + pinned.pair * 2u];
        Require(actual == pinned.value, std::string("f32 constant factor: lane ") + std::to_string(pinned.tid) + " " + mode + " " + Names[pinned.pair] + " is " + Hex(actual) + ", expected the flushed tiny product " + Hex(pinned.value));
    }
}

void CheckTininessAfterRounding(const char* mode) {
    const Pinned& pinned = TinyOnlyBeforeRounding;
    const std::uint32_t actual = Output[pinned.tid * Results + pinned.pair * 2u];
    Require(actual == pinned.value, std::string("f32 tininess after rounding: lane ") + std::to_string(pinned.tid) + " " + mode + " " + Names[pinned.pair] + " is " + Hex(actual) + ", expected " + Hex(pinned.value) + ": the exact product 0x3f7ffffe * 0x00800001 is below 2^-126 and rounds to it, and RDNA2 (gfx1036) keeps it (#2682), where a check before rounding flushes it");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, std::nullopt);
        Check("no float mode");
        CheckTinyProducts("no float mode");
        CheckTininessAfterRounding("no float mode");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
        Check("IEEE=0 f32 denormals flushed");
        CheckTinyProducts("IEEE=0 f32 denormals flushed");
        CheckTininessAfterRounding("IEEE=0 f32 denormals flushed");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false});
        Check("IEEE=1 all denormals flushed");
        CheckTinyProducts("IEEE=1 all denormals flushed");
        CheckTininessAfterRounding("IEEE=1 all denormals flushed");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false});
        Check("f32 denormals kept");
        std::puts("f32 constant factor tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
