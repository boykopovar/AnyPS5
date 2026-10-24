#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Stride = 8;
constexpr std::uint32_t Batches = 32;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 28> ScalarCode{
    0x34020084, 0x34060085, 0xe0381000, 0x80000401, 0xbf8c3f70,
    0x74140b04,
    0x72160b04,
    0xd539810c, 0x20020d04,
    0xd53a800d, 0x40020d05,
    0xd751000e, 0x041a0b04,
    0xd754280f, 0x441a0b04,
    0xd7570010, 0x041a0b04,
    0xd7579111, 0x841e0b04,
    0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> PackedCode{
    0x34020084, 0x34060085, 0xe0381000, 0x80000401, 0xbf8c3f70,
    0xcc11400a, 0x18020b04,
    0xcc12400b, 0x18020b04,
    0xcc12ca0c, 0x30020d04,
    0xcc11c00d, 0x58020f05,
    0xe0781000, 0x80010a03, 0xbf810000,
};

enum class Kind { Min, Max, Min3, Max3, Med3 };

struct Source {
    std::uint32_t reg;
    bool opSel = false;
    bool opSelHi = true;
    bool absolute = false;
    bool negate = false;
    bool negateHi = false;
};

struct Check {
    const char* name;
    Kind kind;
    std::array<Source, 3> sources;
    bool clamp = false;
};

const std::array<Check, 8> ScalarChecks{{
    {"v_min_f16 v10, v4, v5", Kind::Min, {{{0u}, {1u}, {}}}},
    {"v_max_f16 v11, v4, v5", Kind::Max, {{{0u}, {1u}, {}}}},
    {"v_max_f16 v12, -|v4|, v6 clamp", Kind::Max, {{{0u, false, true, true, true}, {2u}, {}}}, true},
    {"v_min_f16 v13, v5, -v6 clamp", Kind::Min, {{{1u}, {2u, false, true, false, true}, {}}}, true},
    {"v_min3_f16 v14, v4, v5, v6", Kind::Min3, {{{0u}, {1u}, {2u}}}},
    {"v_max3_f16 v15, v4, -v5, v6 op_sel:[1,0,1,0]", Kind::Max3, {{{0u, true}, {1u, false, true, false, true}, {2u, true}}}},
    {"v_med3_f16 v16, v4, v5, v6", Kind::Med3, {{{0u}, {1u}, {2u}}}},
    {"v_med3_f16 v17, |v4|, v5, -v7 op_sel:[0,1,0,0] clamp", Kind::Med3, {{{0u, false, true, true}, {1u, true}, {3u, false, true, false, true}}}, true},
}};

const std::array<Check, 4> PackedChecks{{
    {"v_pk_min_f16 v10, v4, v5", Kind::Min, {{{0u}, {1u}, {}}}},
    {"v_pk_max_f16 v11, v4, v5", Kind::Max, {{{0u}, {1u}, {}}}},
    {"v_pk_max_f16 v12, v4, v6 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[0,1] clamp", Kind::Max, {{{0u, true, false, false, true, false}, {2u, false, true, false, false, true}, {}}}, true},
    {"v_pk_min_f16 v13, v5, v7 neg_lo:[0,1] clamp", Kind::Min, {{{1u}, {3u, false, true, false, true}, {}}}, true},
}};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
};

const std::array<Mode, 7> Modes{{
    {"no FLOAT_MODE", std::nullopt},
    {"dx10 ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}},
    {"dx10", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
    {"ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, false}},
    {"none", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, false}},
    {"f16 denormals flushed, dx10 ieee", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}},
    {"f16 input denormals flushed", ShaderRecompiler::ShaderFloatMode{0xb0u, false, false, false}},
}};

std::uint64_t Seed = 0x2545f4914f6cdd1dull;

std::uint32_t Next() {
    Seed += 0x9e3779b97f4a7c15ull;
    std::uint64_t z = Seed;
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
    return static_cast<std::uint32_t>(z ^ (z >> 31u));
}

std::uint32_t Half() {
    const std::uint32_t bits = Next();
    const std::uint32_t sign = (bits >> 31u) << 15u;
    const std::uint32_t mantissa = bits & 0x3ffu;
    switch ((bits >> 16u) % 9u) {
    case 0u: return sign;
    case 1u: return sign | std::max(mantissa, 1u);
    case 2u: return sign | 0x7c00u;
    case 3u: return sign | 0x7c00u | std::max(mantissa, 1u);
    case 4u: return sign | 0x3c00u | (bits & 0x3u);
    default: return bits & 0xffffu;
    }
}

bool IsNan(std::uint32_t bits) { return (bits & 0x7fffu) > 0x7c00u; }

bool IsSnan(std::uint32_t bits) { return IsNan(bits) && (bits & 0x0200u) == 0u; }

double Value(std::uint32_t bits) {
    const double sign = (bits & 0x8000u) != 0u ? -1.0 : 1.0;
    const std::uint32_t exponent = (bits >> 10u) & 0x1fu;
    const std::uint32_t mantissa = bits & 0x3ffu;
    if (exponent == 0u) return sign * std::ldexp(static_cast<double>(mantissa), -24);
    if (exponent == 31u) return mantissa != 0u ? std::nan("") : sign * INFINITY;
    return sign * std::ldexp(static_cast<double>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
}

struct Model {
    bool ieee;
    bool dx10;
    bool flushInputs;

    struct Half {
        double value;
        std::uint32_t bits;
    };

    std::uint32_t Quiet(std::uint32_t bits) const { return ieee ? bits | 0x0200u : bits; }

    Half Numeric(bool maxValue, const Half& lhs, const Half& rhs) const {
        const bool ordered = maxValue ? lhs.value >= rhs.value : lhs.value < rhs.value;
        const bool bothZero = ((lhs.bits | rhs.bits) & 0x7fffu) == 0u;
        const std::uint32_t zero = maxValue ? lhs.bits & rhs.bits : lhs.bits | rhs.bits;
        return {ordered ? lhs.value : rhs.value, bothZero ? zero : (ordered ? lhs.bits : rhs.bits)};
    }

    Half Pair(bool maxValue, const Half& lhs, const Half& rhs) const {
        const Half numeric = Numeric(maxValue, lhs, rhs);
        std::uint32_t bits = IsNan(lhs.bits) ? rhs.bits : numeric.bits;
        bits = IsNan(rhs.bits) ? lhs.bits : bits;
        if (ieee) {
            bits = IsSnan(rhs.bits) ? Quiet(rhs.bits) : bits;
            bits = IsSnan(lhs.bits) ? Quiet(lhs.bits) : bits;
        }
        return {IsNan(rhs.bits) ? lhs.value : numeric.value, bits};
    }

    std::uint32_t Clamp(bool clamp, std::uint32_t bits) const {
        if (!clamp) return bits;
        const std::uint32_t magnitude = bits & 0x7fffu;
        const bool zero = bits > 0x7fffu || magnitude > 0x7c00u;
        const std::uint32_t clamped = zero ? 0u : (magnitude > 0x3c00u ? 0x3c00u : bits);
        return dx10 || magnitude <= 0x7c00u ? clamped : bits;
    }

    Half Read(std::uint32_t bits) const {
        if (flushInputs && (bits & 0x7fffu) < 0x0400u) bits &= 0x8000u;
        return {Value(bits), bits};
    }

    std::uint32_t Scalar(const Check& check, const std::uint32_t* registers) const {
        std::array<Half, 3> args{};
        for (std::uint32_t index = 0; index < 3u; ++index) {
            const Source& source = check.sources[index];
            std::uint32_t bits = (registers[source.reg] >> (source.opSel ? 16u : 0u)) & 0xffffu;
            if (source.absolute) bits &= 0x7fffu;
            if (source.negate) bits ^= 0x8000u;
            args[index] = Read(bits);
        }
        std::uint32_t result = 0u;
        switch (check.kind) {
        case Kind::Min: result = Pair(false, args[0], args[1]).bits; break;
        case Kind::Max: result = Pair(true, args[0], args[1]).bits; break;
        case Kind::Min3: result = Pair(false, Pair(false, args[0], args[1]), args[2]).bits; break;
        case Kind::Max3: result = Pair(true, Pair(true, args[0], args[1]), args[2]).bits; break;
        case Kind::Med3: {
            const bool anyNan = IsNan(args[0].bits) || IsNan(args[1].bits) || IsNan(args[2].bits);
            const Half minimum = Pair(false, Pair(false, args[0], args[1]), args[2]);
            const Half median = Numeric(true, Numeric(false, args[0], args[1]), Numeric(false, Numeric(true, args[0], args[1]), args[2]));
            result = anyNan ? minimum.bits : median.bits;
            break;
        }
        }
        return Clamp(check.clamp, result);
    }

    std::uint32_t Packed(const Check& check, const std::uint32_t* registers) const {
        std::uint32_t result = 0u;
        for (std::uint32_t high = 0; high < 2u; ++high) {
            std::array<std::uint32_t, 2> bits{};
            for (std::uint32_t index = 0; index < 2u; ++index) {
                const Source& source = check.sources[index];
                const bool selectHigh = high != 0u ? source.opSelHi : source.opSel;
                bits[index] = (registers[source.reg] >> (selectHigh ? 16u : 0u)) & 0xffffu;
                if (high != 0u ? source.negateHi : source.negate) bits[index] ^= 0x8000u;
            }
            result |= Clamp(check.clamp, Pair(check.kind == Kind::Max, Read(bits[0]), Read(bits[1])).bits) << (16u * high);
        }
        return result;
    }
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

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        const auto compile = [&](std::span<const std::uint32_t> code, const Mode& mode) {
            const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
            ShaderRecompiler::RecompileRequest request{
                {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
                device->Target(),
                {0, 0, 0, 128}
            };
            request.useCache = false;
            request.context.floatMode = mode.floatMode;
            return ShaderRecompiler::Recompile(request);
        };
        std::uint64_t compared = 0u;
        for (const auto& mode : Modes) {
            const bool flushInputs = mode.floatMode.has_value() && (mode.floatMode->floatMode & 0x40u) == 0u;
            const Model model{mode.floatMode.has_value() && mode.floatMode->ieeeMode, !mode.floatMode.has_value() || mode.floatMode->dx10Clamp, flushInputs};
            const auto scalar = compile(ScalarCode, mode);
            const auto packed = compile(PackedCode, mode);
            for (std::uint32_t batch = 0; batch < Batches; ++batch) {
                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                    std::uint32_t* row = &Input[tid * Inputs];
                    for (std::uint32_t index = 0; index < Inputs; ++index) row[index] = Half() | (Half() << 16u);
                    if (tid % 8u == 0u) row[1] = row[0];
                    if (tid % 8u == 1u) row[1] = row[0] ^ 0x80008000u;
                    if (tid % 8u == 2u) row[2] = row[1];
                    if (tid % 8u == 3u) row[2] = row[0] ^ 0x80008000u;
                }
                const auto run = [&](const ShaderRecompiler::RecompileResult& result, std::span<const std::uint32_t> code, std::span<const Check> checks, bool isPacked) {
                    Output.fill(0xdeadbeefu);
                    device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
                    device->WaitIdle();
                    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                        const std::uint32_t* row = &Input[tid * Inputs];
                        for (std::uint32_t index = 0; index < checks.size(); ++index) {
                            const std::uint32_t actual = Output[tid * Stride + index];
                            const std::uint32_t expected = isPacked ? model.Packed(checks[index], row) : model.Scalar(checks[index], row);
                            Require(actual == expected, std::string("f16 min/max: ") + mode.name + ", " + checks[index].name + " with v4=" + Hex(row[0]) + " v5=" + Hex(row[1]) + " v6=" + Hex(row[2]) + " v7=" + Hex(row[3]) + " is " + Hex(actual) + ", expected " + Hex(expected));
                            ++compared;
                        }
                    }
                };
                run(scalar, ScalarCode, ScalarChecks, false);
                run(packed, PackedCode, PackedChecks, true);
            }
        }
        std::printf("f16 min/max tests passed (%llu results)\n", static_cast<unsigned long long>(compared));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
