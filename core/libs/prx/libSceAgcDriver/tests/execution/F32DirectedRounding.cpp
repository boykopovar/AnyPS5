#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 1024;
constexpr std::uint32_t Ops = 8;
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * Ops> Output{};
alignas(256) constexpr std::array<std::uint32_t, 22> Code{
    0x34020084, 0x34060085, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x06140b04, 0x08160b04, 0x0a180b04,
    0x101a0b04, 0xd54b000e, 0x041a0b04, 0x7e1e0306, 0x3e1e0b04, 0x40200d04, 0x33800000, 0x42220b04,
    0x33800000, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xbf810000,
};

constexpr std::array<std::uint32_t, 32> Values{
    0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3f800001u, 0xbf800001u, 0x00000001u, 0x80000001u,
    0x007fffffu, 0x807fffffu, 0x00800000u, 0x80800000u, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
    0x7fc00000u, 0x7fa00000u, 0x33800000u, 0xb3800000u, 0x3eaaaaabu, 0x40490fdbu, 0x00800001u, 0x3e000000u,
    0x3f7fffffu, 0xbf7fffffu, 0x00000234u, 0x3f000000u, 0x40000000u, 0x4e800000u, 0x7e967699u, 0x80800001u,
};

struct Measured {
    std::uint32_t op;
    std::uint32_t rounding;
    std::uint32_t denorm;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::uint32_t expected;
};

constexpr std::array<Measured, 6> MeasuredRows{{
    {0u, 1u, 3u, 0x3f800000u, 0x33800000u, 0x00000000u, 0x3f800001u},
    {0u, 2u, 3u, 0xbf800000u, 0xb3800000u, 0x00000000u, 0xbf800001u},
    {0u, 1u, 3u, 0x3f800000u, 0x7f7fffffu, 0x00000000u, 0x7f800000u},
    {0u, 2u, 3u, 0x3f800000u, 0x7f7fffffu, 0x00000000u, 0x7f7fffffu},
    {4u, 1u, 3u, 0x3f800001u, 0x3f800001u, 0xbf800000u, 0x34800001u},
    {4u, 2u, 3u, 0xbf800001u, 0x3f800001u, 0x3f800000u, 0xb4800001u},
}};

const char* Names[Ops] = {
    "v_add_f32 v10, v4, v5",
    "v_sub_f32 v11, v4, v5",
    "v_subrev_f32 v12, v4, v5",
    "v_mul_f32 v13, v4, v5",
    "v_fma_f32 v14, v4, v5, v6",
    "v_mac_f32 v15, v4, v5",
    "v_madmk_f32 v16, v4, 0x33800000, v6",
    "v_madak_f32 v17, v4, v5, 0x33800000",
};

std::uint32_t Magnitude(std::uint32_t value) { return value & 0x7fffffffu; }
bool IsNan(std::uint32_t value) { return Magnitude(value) > 0x7f800000u; }
bool IsInfinity(std::uint32_t value) { return Magnitude(value) == 0x7f800000u; }
std::uint32_t Flush(std::uint32_t value) { return Magnitude(value) < 0x00800000u ? value & 0x80000000u : value; }

double F32(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4u);
    return static_cast<double>(value);
}

std::uint32_t Bits(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, 4u);
    return bits;
}

std::uint32_t Fma(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t rounding, std::uint32_t denorm, bool ieee) {
    if ((denorm & 1u) == 0u) {
        a = Flush(a);
        b = Flush(b);
        c = Flush(c);
    }
    const auto quiet = [&](std::uint32_t value) { return value | (ieee ? 0x00400000u : 0u); };
    if (IsNan(a)) return quiet(a);
    if (IsNan(b)) return quiet(b);
    const std::uint32_t productSign = (a ^ b) & 0x80000000u;
    if ((IsInfinity(a) && Magnitude(b) == 0u) || (IsInfinity(b) && Magnitude(a) == 0u)) return 0xffc00000u;
    if (IsNan(c)) return quiet(c);
    if (IsInfinity(a) || IsInfinity(b)) return IsInfinity(c) && (c & 0x80000000u) != productSign ? 0xffc00000u : productSign | 0x7f800000u;
    if (IsInfinity(c)) return c;
    const double product = F32(a) * F32(b);
    const double hi = product + F32(c);
    const double bp = hi - F32(c);
    const double err = (product - bp) + (F32(c) - (hi - bp));
    if (hi == 0.0 && err == 0.0) {
        const std::uint32_t result = productSign == (c & 0x80000000u) ? productSign : (rounding == 2u ? 0x80000000u : 0u);
        return (denorm & 2u) == 0u ? result & 0x80000000u : result;
    }
    const bool negative = hi != 0.0 ? std::signbit(hi) : std::signbit(err);
    const float nearest = static_cast<float>(hi);
    const double residual = (hi - static_cast<double>(nearest)) + err;
    const float below = residual >= 0.0 ? nearest : std::nextafterf(nearest, -std::numeric_limits<float>::infinity());
    const float above = residual > 0.0 ? std::nextafterf(nearest, std::numeric_limits<float>::infinity()) : nearest;
    const float picked = rounding == 1u ? above : (rounding == 2u ? below : (negative ? above : below));
    std::uint32_t result = Bits(picked);
    if (Magnitude(result) == 0u) result = negative ? 0x80000000u : 0u;
    if ((denorm & 2u) == 0u && Magnitude(result) < 0x00800000u) result &= 0x80000000u;
    return result;
}

std::uint32_t Expected(std::uint32_t op, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t rounding, std::uint32_t denorm, bool ieee) {
    constexpr std::uint32_t one = 0x3f800000u;
    constexpr std::uint32_t constant = 0x33800000u;
    const std::uint32_t inner = rounding == 0u ? denorm : 3u;
    if (rounding != 0u && (denorm & 2u) == 0u && (op == 3u || op == 4u)) {
        const std::uint32_t fa = (denorm & 1u) == 0u ? Flush(a) : a;
        const std::uint32_t fb = (denorm & 1u) == 0u ? Flush(b) : b;
        const std::uint32_t fc = (denorm & 1u) == 0u ? Flush(c) : c;
        const auto finiteNonZero = [](std::uint32_t value) {
            const std::uint32_t biased = value & 0x7f800000u;
            return biased != 0u && biased != 0x7f800000u;
        };
        if (finiteNonZero(fa) && finiteNonZero(fb) && std::abs(F32(fa) * F32(fb)) < 0x1.0p-126 && (op == 3u || Magnitude(fc) == 0u)) {
            return (fa ^ fb) & 0x80000000u;
        }
    }
    switch (op) {
    case 0u: return Fma(a, one, b, rounding, denorm, ieee);
    case 1u: return Fma(a, one, b ^ 0x80000000u, rounding, denorm, ieee);
    case 2u:
        if (IsNan(a)) return (a ^ 0x80000000u) | (ieee ? 0x00400000u : 0u);
        return Fma(b, one, a ^ 0x80000000u, rounding, denorm, ieee);
    case 3u: return Fma(a, b, (a ^ b) & 0x80000000u, rounding, denorm, ieee);
    case 4u: return Fma(a, b, c, rounding, denorm, ieee);
    case 5u: return Fma(Fma(a, b, (a ^ b) & 0x80000000u, rounding, inner, ieee), one, c, rounding, inner, ieee);
    case 6u: return Fma(Fma(a, constant, (a ^ constant) & 0x80000000u, rounding, inner, ieee), one, c, rounding, inner, ieee);
    default: return Fma(Fma(a, b, (a ^ b) & 0x80000000u, rounding, inner, ieee), one, constant, rounding, inner, ieee);
    }
}

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

}

int main(int argc, char** argv) {
    try {
        const auto wave = argc > 1 ? static_cast<std::uint32_t>(std::stoul(argv[1])) : 32u;
        const auto selectedRounding = argc > 2 ? static_cast<std::uint32_t>(std::stoul(argv[2])) : 1u;
        const bool selectedInteger = argc > 3 && std::stoul(argv[3]) != 0u;
        for (const auto& row : MeasuredRows) {
            Require(Expected(row.op, row.a, row.b, row.c, row.rounding, row.denorm, true) == row.expected, "measured gfx1036 reference");
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> userData(8);
        const auto input = Descriptor(Input.data(), sizeof(Input));
        const auto output = Descriptor(Output.data(), sizeof(Output));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        auto target = device->Target();
        if (selectedInteger) {
            std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
            std::erase(capabilities, 10u);
            target.supportedCapabilities = capabilities;
            target.featureDigestSealed = false;
        }
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Input[tid * 4u] = Values[tid % Values.size()];
            Input[tid * 4u + 1u] = Values[(tid / Values.size()) % Values.size()];
            Input[tid * 4u + 2u] = Values[(tid * 13u + 7u) % Values.size()];
            Input[tid * 4u + 3u] = 0u;
        }
        std::uint64_t compared = 0u;
        for (const std::uint32_t denorm : {0u, 3u}) {
            for (const bool ieee : {false, true}) {
                ShaderRecompiler::RecompileRequest request{
                    {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                    {wave, 0, userData, compute, std::nullopt, std::nullopt, memory}, target, {0, 0, 0, 128}
                };
                request.useCache = false;
                request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u | selectedRounding | (denorm << 4u), true, ieee, false};
                const auto result = ShaderRecompiler::Recompile(request);
                Output.fill(0xdeadbeefu);
                device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
                device->WaitIdle();
                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                    const std::uint32_t a = Input[tid * 4u];
                    const std::uint32_t b = Input[tid * 4u + 1u];
                    const std::uint32_t c = Input[tid * 4u + 2u];
                    for (std::uint32_t op = 0; op < Ops; ++op) {
                        const std::uint32_t expected = Expected(op, a, b, c, selectedRounding, denorm, ieee);
                        const std::uint32_t actual = Output[tid * Ops + op];
                        if (actual != expected) {
                            std::printf("integer=%u round=%u denorm=%u ieee=%u op=%s a=%08x b=%08x c=%08x actual=%08x expected=%08x\n",
                                selectedInteger, selectedRounding, denorm, ieee, Names[op], a, b, c, actual, expected);
                            throw std::runtime_error("f32 directed rounding mismatch");
                        }
                        ++compared;
                    }
                }
            }
        }
        std::cout << "f32 directed rounding: " << compared << " results passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
