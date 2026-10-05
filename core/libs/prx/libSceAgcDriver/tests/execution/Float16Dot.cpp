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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 26> DotCode{
    0x34020084, 0x34060085, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xbf8c3f70, 0xcc13400a, 0x1c1a0b04, 0xcc13480b, 0x141a0b04, 0xcc13420c, 0xbc1a0b04, 0xcc13c00d,
    0x1c1a0b04, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c,
    0x80010d03, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 3>, 8> Edges{{
    {0x3c003c00u, 0x3c003c00u, 0x00000000u}, {0x7c000000u, 0x3c000000u, 0x3f800000u}, {0x7c003c00u, 0x00003c00u, 0x3f800000u},
    {0xfc007c00u, 0x3c003c00u, 0x40000000u}, {0x80000000u, 0x00008000u, 0x80000000u}, {0x7bff7bffu, 0x7bff7bffu, 0xcf800000u},
    {0x38003800u, 0xb800b800u, 0x3e800000u}, {0x7e003c00u, 0x3c003c00u, 0x00000000u},
}};

float Half(std::uint32_t bits) {
    const std::uint32_t sign = (bits & 0x8000u) << 16u;
    const std::uint32_t exponent = (bits >> 10u) & 0x1fu;
    const std::uint32_t mantissa = bits & 0x3ffu;
    if (exponent == 0x1fu) return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13u));
    if (exponent == 0u) return std::bit_cast<float>(sign) + (sign != 0u ? -1.0f : 1.0f) * std::ldexp(static_cast<float>(mantissa), -24);
    return std::bit_cast<float>(sign | ((exponent + 112u) << 23u) | (mantissa << 13u));
}

std::uint32_t RandomHalf(std::uint64_t& state) {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    const auto bits = static_cast<std::uint32_t>(state >> 33u);
    return (bits & 0x8000u) | ((8u + (bits >> 16u) % 15u) << 10u) | (bits & 0x3ffu);
}

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        if (tid < Edges.size()) {
            std::copy(Edges[tid].begin(), Edges[tid].end(), words);
            continue;
        }
        words[0] = RandomHalf(state) | (RandomHalf(state) << 16u);
        words[1] = RandomHalf(state) | (RandomHalf(state) << 16u);
        words[2] = std::bit_cast<std::uint32_t>(Half(RandomHalf(state)) * Half(RandomHalf(state)));
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

auto Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    const std::span<const std::uint32_t> code(DotCode);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckRefused(AgcDriver::VulkanDevice& device, std::uint32_t word0, const std::string& what) {
    alignas(256) const std::array<std::uint32_t, 3> code{word0, 0x1c1a0b04u, 0xbf810000u};
    std::string refusal;
    try {
        static_cast<void>(Compile(device, code));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("v_dot2_f32_f16 accumulator op_sel, op_sel_hi and neg_hi are not implemented") != std::string::npos, what + " was not refused");
}

float Dot(float aLow, float aHigh, float bLow, float bHigh, float c) {
    return std::fma(aHigh, bHigh, std::fma(aLow, bLow, c));
}

void Expect(std::uint32_t tid, std::uint32_t actual, float expected, const char* name) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(expected);
    const bool nan = std::isnan(expected) && std::isnan(std::bit_cast<float>(actual));
    Require(nan || actual == bits, std::string("v_dot2_f32_f16: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(bits));
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const auto* out = &Output[tid * Results];
        const float aLow = Half(in[0] & 0xffffu);
        const float aHigh = Half(in[0] >> 16u);
        const float bLow = Half(in[1] & 0xffffu);
        const float bHigh = Half(in[1] >> 16u);
        const float c = std::bit_cast<float>(in[2]);
        const float plain = Dot(aLow, aHigh, bLow, bHigh, c);
        Expect(tid, out[0], plain, "plain");
        Expect(tid, out[1], Dot(aHigh, aLow, bLow, bHigh, c), "op_sel swapping the halves of src0");
        Expect(tid, out[2], Dot(-aLow, aHigh, bLow, -bHigh, -c), "neg_lo src0, neg_hi src1 and neg src2");
        if (!std::isnan(plain)) Expect(tid, out[3], plain > 0.0f ? std::min(plain, 1.0f) : 0.0f, "clamp");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device);
        Check();
        CheckRefused(*device, 0xcc13600au, "op_sel on the accumulator");
        CheckRefused(*device, 0xcc13440au, "neg_hi on the accumulator");
        CheckRefused(*device, 0xcc13000au, "op_sel_hi cleared on the accumulator");
        std::puts("v_dot2_f32_f16 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
