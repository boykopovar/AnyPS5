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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 22;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 26> Code{
    0x34020084u, 0x34040085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd58f000au, 0x02010104u, 0xd58f000bu,
    0x0a010104u, 0xd58f000cu, 0x12010104u, 0xd58f000du, 0x1a010104u, 0xd58f800eu, 0x02010104u, 0xd58f800fu,
    0x0a010104u, 0xd58f8010u, 0x12010104u, 0xd58f8011u, 0x1a010104u, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xbf810000u,
};

constexpr std::uint64_t Pairs[Threads] = {
    0x3ff0000000000000ull,
    0xbff0000000000000ull,
    0x4000000000000000ull,
    0x3fe0000000000000ull,
    0x4330000000000000ull,
    0x47efffffe0000000ull,
    0x47f0000000000000ull,
    0xc7f0000000000000ull,
    0x3810000000000000ull,
    0x380fffffffffffffull,
    0x381fffffefffffffull,
    0x3690000000000000ull,
    0x3e80000000000000ull,
    0x7ff8000000000000ull,
    0x7ff4000000000000ull,
    0xfff8000000000000ull,
    0x7ff0000000000000ull,
    0xfff0000000000000ull,
    0x0000000000000000ull,
    0x8000000000000000ull,
    0xbfe0000000000000ull,
    0xc000000000000000ull,
};
constexpr std::uint32_t ExpectedApply[Threads][Results] = {
    {0x3f800000u, 0x40000000u, 0x40800000u, 0x3f000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f000000u},
    {0xbf800000u, 0xc0000000u, 0xc0800000u, 0xbf000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x40800000u, 0x41000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x3f000000u, 0x3f800000u, 0x40000000u, 0x3e800000u, 0x3f000000u, 0x3f800000u, 0x3f800000u, 0x3e800000u},
    {0x59800000u, 0x5a000000u, 0x5a800000u, 0x59000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x7f7fffffu, 0x7f800000u, 0x7f800000u, 0x7effffffu, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x01000000u, 0x01800000u, 0x00000000u, 0x00800000u, 0x01000000u, 0x01800000u, 0x00000000u},
    {0x00800000u, 0x01000000u, 0x01800000u, 0x00000000u, 0x00800000u, 0x01000000u, 0x01800000u, 0x00000000u},
    {0x00ffffffu, 0x017fffffu, 0x01ffffffu, 0x00000000u, 0x00ffffffu, 0x017fffffu, 0x01ffffffu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x34000000u, 0x34800000u, 0x35000000u, 0x33800000u, 0x34000000u, 0x34800000u, 0x35000000u, 0x33800000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbf000000u, 0xbf800000u, 0xc0000000u, 0xbe800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xc0000000u, 0xc0800000u, 0xc1000000u, 0xbf800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
};
constexpr std::uint32_t ExpectedIgnore[Threads][Results] = {
    {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0xbf800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u},
    {0x59800000u, 0x59800000u, 0x59800000u, 0x59800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x34000000u, 0x34000000u, 0x34000000u, 0x34000000u, 0x34000000u, 0x34000000u, 0x34000000u, 0x34000000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbf000000u, 0xbf000000u, 0xbf000000u, 0xbf000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
};
constexpr std::uint8_t Checked[Threads] = {0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xf0u, 0xf0u, 0xf0u, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu};
constexpr std::uint8_t AnyNan[Threads] = {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0fu, 0x0fu, 0x0fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
constexpr const char* Names[Results] = {
    "v_cvt_f32_f64 plain",
    "v_cvt_f32_f64 mul:2",
    "v_cvt_f32_f64 mul:4",
    "v_cvt_f32_f64 div:2",
    "v_cvt_f32_f64 clamp",
    "v_cvt_f32_f64 clamp mul:2",
    "v_cvt_f32_f64 clamp mul:4",
    "v_cvt_f32_f64 clamp div:2",
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

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const std::string& name) {
    Require(actual == expected, "v_cvt_f32_f64 output modifier: lane " + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = static_cast<std::uint32_t>(Pairs[tid]);
        Input[tid * Inputs + 1u] = static_cast<std::uint32_t>(Pairs[tid] >> 32u);
        Input[tid * Inputs + 2u] = 0u;
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
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const std::uint32_t (&expected)[Threads][Results], const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < Results; ++i) {
            const std::string name = std::string(mode) + " " + Names[i];
            if ((Checked[tid] >> i) & 1u) Expect(tid, out[i], expected[tid][i], name);
            if ((AnyNan[tid] >> i) & 1u) Require((out[i] & 0x7fffffffu) > 0x7f800000u, "v_cvt_f32_f64 output modifier: lane " + std::to_string(tid) + " " + name + " is " + Hex(out[i]) + ", expected a NaN");
        }
    }
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
        Run(*device, std::nullopt);
        Check(ExpectedApply, "no float mode");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
        Check(ExpectedApply, "0xc0 IEEE=0");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xd0u, true, false, false});
        Check(ExpectedApply, "0xd0 IEEE=0 f32 denormal mode 1");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x40u, true, false, false});
        Check(ExpectedApply, "0x40 IEEE=0 f64 denormals flushed");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xe0u, true, false, false});
        Check(ExpectedIgnore, "0xe0 IEEE=0 f32 denormal mode 2");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false});
        Check(ExpectedIgnore, "0xf0 IEEE=0 f32 denormal mode 3");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false});
        Check(ExpectedIgnore, "0xc0 IEEE=1");
        std::puts("v_cvt_f32_f64 output modifier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
