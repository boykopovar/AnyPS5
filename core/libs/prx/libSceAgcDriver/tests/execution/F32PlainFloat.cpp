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
#include <initializer_list>
#include <iostream>
#include <limits>
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

constexpr std::array<std::array<std::uint32_t, 3>, 16> Special{{
    {0x7fc12345u, 0x3f800000u, 0x3f800000u}, {0x3f800000u, 0x7fa00001u, 0x40000000u}, {0xffc00042u, 0xc0000000u, 0x7fa00001u},
    {0x7f800000u, 0x3f800000u, 0xff800000u}, {0xff800000u, 0x00000000u, 0x3f800000u}, {0x00000000u, 0x80000000u, 0x80000000u},
    {0x80000000u, 0x7f800000u, 0x00000000u}, {0x00000001u, 0x3f800000u, 0x80000003u}, {0x007fffffu, 0x807fffffu, 0x00400000u},
    {0x0d800000u, 0x30800000u, 0x00020000u}, {0x71800000u, 0x71800000u, 0xff800000u}, {0xc0400000u, 0x00800000u, 0x80600000u},
    {0x3f800001u, 0x3f800001u, 0xbf800002u}, {0x80000001u, 0x80000001u, 0x00000000u}, {0x7f7fffffu, 0x40000000u, 0xff7fffffu},
    {0x00800000u, 0x3f000000u, 0x00000000u},
}};

constexpr std::uint32_t SpecialFirst = static_cast<std::uint32_t>(Picked.size());

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
    if (exponent == 0xffu) return mantissa == 0u ? sign | 0x7c00u : sign | 0x7e00u | (mantissa >> 13u);
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

struct Value {
    bool nan;
    std::uint32_t bits;
};

struct Allowed {
    std::vector<std::uint32_t> bits;
    bool nan = false;
};

struct Tally {
    std::uint32_t nan = 0;
    std::uint32_t denormalKept = 0;
    std::uint32_t denormalFlushed = 0;
    std::uint32_t inputKept = 0;
    std::uint32_t inputFlushed = 0;
    std::uint32_t fmaFused = 0;
    std::uint32_t fmaRounded = 0;
};

Tally Seen;

bool IsNan(std::uint32_t bits) {
    return (bits & 0x7fffffffu) > 0x7f800000u;
}

bool IsDenormal(std::uint32_t bits) {
    return (bits & 0x7f800000u) == 0u && (bits & 0x007fffffu) != 0u;
}

std::uint32_t FlushInput(std::uint32_t bits) {
    return IsDenormal(bits) ? bits & 0x80000000u : bits;
}

double Wide(std::uint32_t bits) {
    const double sign = (bits & 0x80000000u) != 0u ? -1.0 : 1.0;
    const std::uint32_t exponent = (bits >> 23u) & 0xffu;
    const std::uint32_t mantissa = bits & 0x007fffffu;
    if (exponent == 0xffu) return mantissa != 0u ? std::numeric_limits<double>::quiet_NaN() : sign * std::numeric_limits<double>::infinity();
    if (exponent == 0u) return sign * std::ldexp(static_cast<double>(mantissa), -149);
    return sign * std::ldexp(static_cast<double>(mantissa | 0x00800000u), static_cast<int>(exponent) - 150);
}

Value Round(double value) {
    if (std::isnan(value)) return {true, 0u};
    const std::uint32_t sign = std::signbit(value) ? 0x80000000u : 0u;
    if (std::isinf(value)) return {false, sign | 0x7f800000u};
    const double magnitude = std::fabs(value);
    if (magnitude == 0.0) return {false, sign};
    int exponent = 0;
    std::frexp(magnitude, &exponent);
    int scale = std::max(exponent, -125);
    auto mantissa = static_cast<std::uint64_t>(std::nearbyint(std::ldexp(magnitude, 24 - scale)));
    if (mantissa == (1ull << 24u)) {
        mantissa >>= 1u;
        ++scale;
    }
    if (scale > 128) return {false, sign | 0x7f800000u};
    if (mantissa < (1ull << 23u)) return {false, sign | static_cast<std::uint32_t>(mantissa)};
    return {false, sign | (static_cast<std::uint32_t>(scale + 126) << 23u) | static_cast<std::uint32_t>(mantissa & 0x007fffffu)};
}

void Admit(Allowed& allowed, Value value) {
    if (value.nan) {
        allowed.nan = true;
        return;
    }
    allowed.bits.push_back(value.bits);
    if (IsDenormal(value.bits)) allowed.bits.push_back(value.bits & 0x80000000u);
}

std::array<std::uint32_t, 2> InputForms(std::uint32_t bits) {
    return {bits, FlushInput(bits)};
}

template<class TOperation>
Allowed Binary(std::uint32_t a, std::uint32_t b, TOperation operation) {
    Allowed allowed;
    for (const std::uint32_t x : InputForms(a))
        for (const std::uint32_t y : InputForms(b)) Admit(allowed, Round(operation(Wide(x), Wide(y))));
    return allowed;
}

Allowed FusedOnly(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    Allowed allowed;
    for (const std::uint32_t x : InputForms(a))
        for (const std::uint32_t y : InputForms(b))
            for (const std::uint32_t z : InputForms(c)) Admit(allowed, Round(std::fma(Wide(x), Wide(y), Wide(z))));
    return allowed;
}

Allowed RoundedOnly(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    Allowed allowed;
    for (const std::uint32_t x : InputForms(a))
        for (const std::uint32_t y : InputForms(b))
            for (const std::uint32_t z : InputForms(c)) {
                const Value product = Round(Wide(x) * Wide(y));
                if (product.nan) {
                    allowed.nan = true;
                    continue;
                }
                for (const std::uint32_t p : InputForms(product.bits)) Admit(allowed, Round(Wide(p) + Wide(z)));
            }
    return allowed;
}

bool Contains(const Allowed& allowed, std::uint32_t got) {
    if (IsNan(got)) return allowed.nan;
    return std::find(allowed.bits.begin(), allowed.bits.end(), got) != allowed.bits.end();
}

Allowed Merge(Allowed first, const Allowed& second) {
    first.nan = first.nan || second.nan;
    first.bits.insert(first.bits.end(), second.bits.begin(), second.bits.end());
    return first;
}

Allowed Saturated(const Allowed& source) {
    Allowed allowed;
    if (source.nan) allowed.bits.push_back(0u);
    for (const std::uint32_t bits : source.bits) {
        const double value = Wide(bits);
        if (value >= 1.0) {
            allowed.bits.push_back(0x3f800000u);
        } else if ((bits & 0x80000000u) != 0u) {
            allowed.bits.push_back(0u);
            if ((bits & 0x7f800000u) == 0u) allowed.bits.push_back(0x80000000u);
        } else {
            allowed.bits.push_back(bits);
            if (IsDenormal(bits)) allowed.bits.push_back(0u);
        }
    }
    return allowed;
}

Allowed Exactly(std::initializer_list<std::uint32_t> bits) {
    Allowed allowed;
    allowed.bits.assign(bits);
    return allowed;
}

Allowed AnyNan() {
    Allowed allowed;
    allowed.nan = true;
    return allowed;
}

std::string Describe(const Allowed& allowed) {
    std::string text;
    for (const std::uint32_t bits : allowed.bits) text += (text.empty() ? "" : ", ") + Hex(bits);
    if (allowed.nan) text += std::string(text.empty() ? "" : ", ") + "any NaN";
    return text;
}

void Note(std::uint32_t got, Value ieee) {
    if (ieee.nan || !IsDenormal(ieee.bits)) return;
    if (got == ieee.bits) ++Seen.denormalKept;
    if ((got & 0x7fffffffu) == 0u) ++Seen.denormalFlushed;
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
    if (!clamp && Bits(fused) != Bits(unfused)) ++(got == Bits(fused) ? Seen.fmaFused : Seen.fmaRounded);
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


void RequireAllowed(std::uint32_t lane, std::uint32_t column, const Operands& operands, std::uint32_t got, const Allowed& allowed) {
    Require(Contains(allowed, got), Where(lane, column, operands, got) + ", expected one of " + Describe(allowed));
    if (IsNan(got)) ++Seen.nan;
}

Allowed UnarySpecial(std::uint32_t column, std::uint32_t bits) {
    const bool negative = (bits & 0x80000000u) != 0u;
    const bool zero = (bits & 0x7fffffffu) == 0u;
    const std::uint32_t sign = bits & 0x80000000u;
    if (IsNan(bits)) return AnyNan();
    switch (column) {
    case 5u: return Exactly({zero ? (sign | 0x7f800000u) : sign});
    case 6u: return Exactly({zero ? 0x7f800000u : 0u});
    case 7u: return Exactly({zero ? 0u : 0x7f800000u});
    case 8u: return Exactly({zero ? 0xff800000u : 0x7f800000u});
    case 9u: return Exactly({zero ? 0x3f800000u : (negative ? 0u : 0x7f800000u)});
    case 11u: return zero ? Exactly({0u, 0x80000000u}) : AnyNan();
    default: return Exactly({bits});
    }
}

bool UnaryMatches(std::uint32_t column, std::uint32_t input, std::uint32_t got, double ulps) {
    if (IsNan(input) || (input & 0x7fffffffu) == 0u || (input & 0x7fffffffu) == 0x7f800000u) return Contains(UnarySpecial(column, input), got);
    const double x = Wide(input);
    double reference = 0.0;
    switch (column) {
    case 5u: reference = 1.0 / x; break;
    case 6u: reference = 1.0 / std::sqrt(x); break;
    case 7u: reference = std::sqrt(x); break;
    case 8u: reference = std::log2(x); break;
    case 9u: reference = std::exp2(x); break;
    case 10u: reference = std::floor(x); break;
    case 11u: reference = x - std::floor(x); break;
    case 12u: reference = std::trunc(x); break;
    case 13u: reference = std::ceil(x); break;
    default: reference = std::nearbyint(x); break;
    }
    if (column >= 10u) {
        Allowed allowed;
        Admit(allowed, Round(reference));
        return Contains(allowed, got);
    }
    if (IsNan(got)) return false;
    if (column == 8u && x >= 0.5 && x <= 2.0) return std::fabs(Wide(got) - reference) < std::ldexp(1.0, -21);
    if (std::fabs(reference) > static_cast<double>(std::numeric_limits<float>::max())) return got == (reference < 0.0 ? 0xff800000u : 0x7f800000u);
    if (std::fabs(reference) < std::ldexp(1.0, -126)) return (got & 0x7fffffffu) == 0u || std::fabs(Wide(got) - reference) <= std::ldexp(3.0, -149);
    return Ulps(Float(got), reference) <= ulps;
}

void RequireUnary(std::uint32_t lane, std::uint32_t column, const Operands& operands, std::uint32_t got, std::uint32_t input, double ulps) {
    const bool kept = UnaryMatches(column, input, got, ulps);
    const bool flushed = UnaryMatches(column, FlushInput(input), got, ulps);
    Require(kept || flushed, Where(lane, column, operands, got) + ", outside what this instruction allows for " + Hex(input) +
                                 (IsDenormal(input) ? " or for its flushed form " + Hex(FlushInput(input)) : std::string()));
    if (IsNan(got)) ++Seen.nan;
    if (IsDenormal(input) && kept != flushed) ++(kept ? Seen.inputKept : Seen.inputFlushed);
}

void CheckSpecial(std::uint32_t lane, const std::array<std::uint32_t, 3>& bits, const std::uint32_t* result) {
    const Operands operands{Float(bits[0]), Float(bits[1]), Float(bits[2])};
    const std::uint32_t a = bits[0];
    const std::uint32_t b = bits[1];
    const std::uint32_t c = bits[2];
    const auto add = Binary(a, b, [](double x, double y) { return x + y; });
    const auto mul = Binary(a, b, [](double x, double y) { return x * y; });
    const auto fused = FusedOnly(a, b, c);
    const auto rounded = RoundedOnly(a, b, c);
    const auto fma = Merge(fused, rounded);
    RequireAllowed(lane, 0u, operands, result[0], add);
    Note(result[0], Round(Wide(a) + Wide(b)));
    RequireAllowed(lane, 1u, operands, result[1], Binary(a, b, [](double x, double y) { return x - y; }));
    Note(result[1], Round(Wide(a) - Wide(b)));
    RequireAllowed(lane, 2u, operands, result[2], mul);
    Note(result[2], Round(Wide(a) * Wide(b)));
    for (const std::uint32_t column : {3u, 4u}) {
        RequireAllowed(lane, column, operands, result[column], fma);
        Note(result[column], Round(std::fma(Wide(a), Wide(b), Wide(c))));
        if (Contains(fused, result[column]) != Contains(rounded, result[column])) ++(Contains(fused, result[column]) ? Seen.fmaFused : Seen.fmaRounded);
    }
    const std::uint32_t magnitude = a & 0x7fffffffu;
    RequireUnary(lane, 5u, operands, result[5], a, 2.5);
    RequireUnary(lane, 6u, operands, result[6], magnitude, 2.0);
    RequireUnary(lane, 7u, operands, result[7], magnitude, 4.5);
    RequireUnary(lane, 8u, operands, result[8], magnitude, 3.0);
    RequireUnary(lane, 9u, operands, result[9], a, 3.0 + 2.0 * std::fabs(Wide(a)));
    for (std::uint32_t column = 10u; column <= 14u; ++column) RequireUnary(lane, column, operands, result[column], a, 0.0);
    RequireAllowed(lane, 15u, operands, result[15], Saturated(add));
    RequireAllowed(lane, 16u, operands, result[16], Saturated(mul));
    RequireAllowed(lane, 17u, operands, result[17], Saturated(fma));
    const std::uint32_t packed = TowardZero(a) | (TowardZero(b) << 16u);
    Require(result[18] == packed, Where(lane, 18u, operands, result[18]) + ", expected " + Hex(packed));
    const std::uint32_t negated = TowardZero(c) | (TowardZero(c ^ 0x80000000u) << 16u);
    Require(result[19] == negated, Where(lane, 19u, operands, result[19]) + ", expected " + Hex(negated));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto inputs = Inputs();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            Input[lane * InputStride] = Bits(inputs[lane].a);
            Input[lane * InputStride + 1u] = Bits(inputs[lane].b);
            Input[lane * InputStride + 2u] = Bits(inputs[lane].c);
        }
        for (std::uint32_t index = 0; index < Special.size(); ++index) {
            std::copy(Special[index].begin(), Special[index].end(), Input.begin() + (SpecialFirst + index) * InputStride);
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
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            if (lane >= SpecialFirst && lane < SpecialFirst + Special.size()) {
                CheckSpecial(lane, Special[lane - SpecialFirst], &Output[lane * OutputStride]);
            } else {
                Check(lane, inputs[lane], &Output[lane * OutputStride]);
            }
        }
        const bool plain = ShaderRecompiler::ShaderOptOn(ShaderRecompiler::ShaderOpt::PlainFloat);
        std::printf("f32 plain float tests passed with APS5_SHADER_OPT_PLAIN_FLOAT=%d (%u lanes, %zu of them NaN, infinity, zero and denormal inputs, %u results each): "
                    "%u NaN results (payloads not compared), %u denormal results kept and %u flushed to zero, "
                    "%u unary results that used a denormal input as is and %u that flushed it, "
                    "fma where fused and product-rounded results differ: %u fused, %u product-rounded\n",
                    plain ? 1 : 0, Lanes, Special.size(), Results, Seen.nan, Seen.denormalKept, Seen.denormalFlushed, Seen.inputKept, Seen.inputFlushed,
                    Seen.fmaFused, Seen.fmaRounded);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
