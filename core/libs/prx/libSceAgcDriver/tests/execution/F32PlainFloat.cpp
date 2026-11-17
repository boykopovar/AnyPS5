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
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Lanes = 4096;
constexpr std::uint32_t InputStride = 4;
constexpr std::uint32_t OutputStride = 32;
alignas(256) std::array<std::uint32_t, Lanes * InputStride> Input{};
alignas(256) std::array<std::uint32_t, Lanes * OutputStride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 48> Code{
    0x34020084u, 0x8f098908u, 0x4a020209u, 0x34040087u, 0x8f0a8c08u, 0x4a04040au, 0xe0381000u, 0x80000401u,
    0xbf8c3f70u, 0x360e08ffu, 0x7fffffffu, 0x06140b04u, 0x08160b04u, 0x10180b04u, 0xd54b000du, 0x041a0b04u,
    0x7e1c0306u, 0x561c0b04u, 0x7e1e5504u, 0x7e205d07u, 0x7e226707u, 0x7e244f07u, 0x7e264b04u, 0x7e284904u,
    0x7e2a4104u, 0x7e2c4304u, 0x7e2e4504u, 0x7e304704u, 0xd5038019u, 0x00020b04u, 0xd508801au, 0x00020b04u,
    0xd54b801bu, 0x041a0b04u, 0x5e380b04u, 0xd52f001du, 0x40020d06u, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xe0781020u, 0x80011202u, 0xe0781030u, 0x80011602u, 0xe0781040u, 0x80011a02u, 0xbf810000u,
};

constexpr std::uint32_t Results = 20;
constexpr const char* Names[Results] = {
    "v_add_f32 v10, a, b",
    "v_sub_f32 v11, a, b",
    "v_mul_f32 v12, a, b",
    "v_fma_f32 v13, a, b, c",
    "v_fmac_f32 v14 (c), a, b",
    "v_rcp_f32 v15, a",
    "v_rsq_f32 v16, |a|",
    "v_sqrt_f32 v17, |a|",
    "v_log_f32 v18, |a|",
    "v_exp_f32 v19, a",
    "v_floor_f32 v20, a",
    "v_fract_f32 v21, a",
    "v_trunc_f32 v22, a",
    "v_ceil_f32 v23, a",
    "v_rndne_f32 v24, a",
    "v_add_f32 v25, a, b clamp",
    "v_mul_f32 v26, a, b clamp",
    "v_fma_f32 v27, a, b, c clamp",
    "v_cvt_pkrtz_f16_f32 v28, a, b",
    "v_cvt_pkrtz_f16_f32 v29, c, -c",
};

struct Operands {
    float a;
    float b;
    float c;
};

constexpr std::array<Operands, 40> Picked{{
    {1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 0.5f}, {-1.0f, 0.5f, -0.25f}, {0.5f, 0.5f, 0.75f},
    {0.75f, -0.25f, 1.0f}, {1.5f, 2.0f, -3.0f}, {64.0f, 64.0f, 4096.0f}, {-64.0f, 0.015625f, 1.0f},
    {0.015625f, -0.015625f, 0.015625f}, {3.0f, 0.333333343f, -1.0f}, {0.1f, 0.2f, 0.3f}, {-0.1f, 0.9f, 0.09f},
    {2.5f, -2.5f, 6.25f}, {-2.5f, 3.5f, 0.0f}, {0.499999970f, 0.5f, 0.5f}, {-0.499999970f, 1.0f, -0.5f},
    {1.0f, 0.999999940f, -1.0f}, {7.0f, 1.0f / 7.0f, -1.0f}, {12.5f, 0.08f, -0.0f}, {-33.25f, -0.03f, 0.25f},
    {1.0f, 1.0f, 65504.0f}, {1.0f, 1.0f, 65519.9961f}, {1.0f, 1.0f, 65520.0f}, {1.0f, 1.0f, 70000.0f},
    {-1.0f, 1.0f, 1048576.0f}, {0.5f, 2.0f, -1.0e6f}, {2.0f, 0.5f, 6.10351562e-05f}, {2.0f, 0.5f, 6.09755516e-05f},
    {0.25f, 4.0f, 3.0e-05f}, {0.25f, 4.0f, -5.96046448e-08f}, {0.25f, 4.0f, 5.96046448e-08f}, {0.25f, 4.0f, 2.98023224e-08f},
    {1.00048828f, -1.00097656f, 1.00146484f}, {2047.0f / 64.0f, 0.0625f, 2047.0f}, {-0.0156250019f, 63.9999962f, 2049.0f}, {0.0234375f, -0.0234375f, 0.000549316406f},
    {48.0f, 0.02f, -0.96f}, {-0.6f, -0.7f, 0.58f}, {0.9f, 1.2f, -0.08f}, {0.3f, 0.3f, 0.91f},
}};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

float Float(std::uint32_t bits) {
    return std::bit_cast<float>(bits);
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t TowardZero(std::uint32_t bits) {
    if ((bits & 0x7f800000u) == 0u) bits &= 0x80000000u;
    const std::uint32_t sign = (bits >> 16u) & 0x8000u;
    const std::uint32_t exponent = (bits >> 23u) & 0xffu;
    const std::uint32_t mantissa = bits & 0x7fffffu;
    if (exponent >= 143u) return sign | 0x7bffu;
    if (exponent >= 113u) return sign | ((exponent - 112u) << 10u) | (mantissa >> 13u);
    if (exponent >= 103u) return sign | ((mantissa | 0x800000u) >> (126u - exponent));
    return sign;
}

float Clamp01(float value) {
    return std::min(std::max(value, 0.0f), 1.0f);
}

float Fused(float a, float b, float c) {
    return std::fma(a, b, c);
}

float Unfused(float a, float b, float c) {
    volatile float product = a * b;
    volatile float sum = product + c;
    return sum;
}

double Ulps(float value, double reference) {
    if (!std::isfinite(value) || reference == 0.0 || std::signbit(value) != std::signbit(reference)) return INFINITY;
    return std::fabs(static_cast<double>(value) - reference) / std::ldexp(1.0, std::ilogb(static_cast<float>(reference)) - 23);
}

std::uint32_t State = 0x2545f491u;

std::uint32_t Next() {
    State = State * 1664525u + 1013904223u;
    return State;
}

float RandomFloat(int lowExponent, int highExponent) {
    const std::uint32_t sign = (Next() >> 31u) << 31u;
    const auto exponent = static_cast<std::uint32_t>(lowExponent + static_cast<int>(Next() % static_cast<std::uint32_t>(highExponent - lowExponent + 1)) + 127);
    return Float(sign | (exponent << 23u) | (Next() >> 9u));
}

std::vector<Operands> Inputs() {
    std::vector<Operands> inputs(Picked.begin(), Picked.end());
    while (inputs.size() < Lanes) inputs.push_back({RandomFloat(-6, 5), RandomFloat(-6, 5), RandomFloat(-6, 20)});
    return inputs;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Where(std::uint32_t lane, std::uint32_t column, const Operands& operands, std::uint32_t got) {
    return std::string("f32 plain float: ") + Names[column] + " in lane " + std::to_string(lane) + " with a=" + Hex(Bits(operands.a)) +
           " b=" + Hex(Bits(operands.b)) + " c=" + Hex(Bits(operands.c)) + " is " + Hex(got);
}

void RequireExact(std::uint32_t lane, std::uint32_t column, const Operands& operands, std::uint32_t got, float expected) {
    Require(got == Bits(expected), Where(lane, column, operands, got) + ", expected " + Hex(Bits(expected)));
}

void RequireFma(std::uint32_t lane, std::uint32_t column, const Operands& operands, std::uint32_t got, bool clamp) {
    const float fused = clamp ? Clamp01(Fused(operands.a, operands.b, operands.c)) : Fused(operands.a, operands.b, operands.c);
    const float unfused = clamp ? Clamp01(Unfused(operands.a, operands.b, operands.c)) : Unfused(operands.a, operands.b, operands.c);
    Require(got == Bits(fused) || got == Bits(unfused),
            Where(lane, column, operands, got) + ", expected " + Hex(Bits(fused)) + " (fused) or " + Hex(Bits(unfused)) + " (product rounded first)");
}

void RequireWithin(std::uint32_t lane, std::uint32_t column, const Operands& operands, std::uint32_t got, double reference, double ulps) {
    const double error = Ulps(Float(got), reference);
    Require(error <= ulps, Where(lane, column, operands, got) + ", " + std::to_string(error) + " ULP from " + std::to_string(reference) + ", allowed " + std::to_string(ulps));
}

void Check(std::uint32_t lane, const Operands& operands, const std::uint32_t* result) {
    const float a = operands.a;
    const float b = operands.b;
    const float c = operands.c;
    const double magnitude = std::fabs(static_cast<double>(a));
    RequireExact(lane, 0u, operands, result[0], a + b);
    RequireExact(lane, 1u, operands, result[1], a - b);
    RequireExact(lane, 2u, operands, result[2], a * b);
    RequireFma(lane, 3u, operands, result[3], false);
    RequireFma(lane, 4u, operands, result[4], false);
    RequireWithin(lane, 5u, operands, result[5], 1.0 / static_cast<double>(a), 2.5);
    RequireWithin(lane, 6u, operands, result[6], 1.0 / std::sqrt(magnitude), 2.0);
    RequireWithin(lane, 7u, operands, result[7], std::sqrt(magnitude), 4.5);
    const double logarithm = std::log2(magnitude);
    if (magnitude >= 0.5 && magnitude <= 2.0) {
        const double error = std::fabs(static_cast<double>(Float(result[8])) - logarithm);
        Require(error < std::ldexp(1.0, -21), Where(lane, 8u, operands, result[8]) + ", " + std::to_string(error) + " from " + std::to_string(logarithm) + ", allowed 2^-21");
    } else {
        RequireWithin(lane, 8u, operands, result[8], logarithm, 3.0);
    }
    RequireWithin(lane, 9u, operands, result[9], std::exp2(static_cast<double>(a)), 3.0 + 2.0 * magnitude);
    RequireExact(lane, 10u, operands, result[10], std::floor(a));
    RequireExact(lane, 11u, operands, result[11], a - std::floor(a));
    RequireExact(lane, 12u, operands, result[12], std::trunc(a));
    RequireExact(lane, 13u, operands, result[13], std::ceil(a));
    RequireExact(lane, 14u, operands, result[14], std::nearbyint(a));
    RequireExact(lane, 15u, operands, result[15], Clamp01(a + b));
    RequireExact(lane, 16u, operands, result[16], Clamp01(a * b));
    RequireFma(lane, 17u, operands, result[17], true);
    const std::uint32_t packed = TowardZero(Bits(a)) | (TowardZero(Bits(b)) << 16u);
    Require(result[18] == packed, Where(lane, 18u, operands, result[18]) + ", expected " + Hex(packed));
    const std::uint32_t negated = TowardZero(Bits(c)) | (TowardZero(Bits(c) ^ 0x80000000u) << 16u);
    Require(result[19] == negated, Where(lane, 19u, operands, result[19]) + ", expected " + Hex(negated));
}

}

int main() {
    try {
        Require(ShaderRecompiler::ShaderOptOn(ShaderRecompiler::ShaderOpt::PlainFloat), "f32 plain float: run with APS5_SHADER_OPT_PLAIN_FLOAT on, the default");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto inputs = Inputs();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            Input[lane * InputStride] = Bits(inputs[lane].a);
            Input[lane * InputStride + 1u] = Bits(inputs[lane].b);
            Input[lane * InputStride + 2u] = Bits(inputs[lane].c);
        }
        Output.fill(0xdeadbeefu);
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> words(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(words)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), words, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, Lanes / Threads, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device->WaitIdle();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) Check(lane, inputs[lane], &Output[lane * OutputStride]);
        std::printf("f32 plain float tests passed (%u lanes, %u results each)\n", Lanes, Results);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
