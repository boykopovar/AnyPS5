#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
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
    0x34020084, 0x34060085, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e1e0306,
    0x64140b04,
    0x66160b04,
    0x68180b04,
    0x6a1a0b04,
    0xd74b000e, 0x041a0b04,
    0x6c1e0b04,
    0x6e200b04, 0x00003c01,
    0x70220b04, 0x00009000,
    0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xbf810000,
};

constexpr std::array<std::uint32_t, 32> Values{
    0x0000u, 0x8000u, 0x3c00u, 0xbc00u, 0x3c01u, 0xbc01u, 0x0001u, 0x8001u, 0x03ffu, 0x83ffu, 0x0400u, 0x8400u, 0x7bffu, 0xfbffu, 0x7c00u, 0xfc00u,
    0x7e00u, 0x7d00u, 0x1000u, 0x9000u, 0x3555u, 0x4248u, 0x0200u, 0x5bffu, 0x3bffu, 0x3c02u, 0x0401u, 0x03feu, 0xbbffu, 0x0c00u, 0x7e01u, 0xfd23u,
};

struct Measured {
    std::uint32_t op;
    std::uint32_t rounding;
    std::uint32_t denorm;
    bool overflow;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::uint32_t expected;
};

constexpr std::array<Measured, 19> MeasuredRows{{
    {0u, 1u, 3u, false, 0x3c00u, 0x1000u, 0x0000u, 0x3c01u},
    {0u, 2u, 3u, false, 0xbc00u, 0x9000u, 0x0000u, 0xbc01u},
    {0u, 2u, 3u, false, 0x3c00u, 0xbc00u, 0x0000u, 0x8000u},
    {1u, 1u, 3u, false, 0x0000u, 0x7e00u, 0x0000u, 0xfe00u},
    {2u, 1u, 3u, false, 0x7e00u, 0x7d00u, 0x0000u, 0xfe00u},
    {3u, 1u, 1u, false, 0x3c01u, 0x03ffu, 0x0000u, 0x0400u},
    {3u, 2u, 1u, false, 0x3c01u, 0x83ffu, 0x0000u, 0x8400u},
    {4u, 1u, 0u, false, 0x0400u, 0x9000u, 0x0400u, 0x0000u},
    {4u, 2u, 1u, false, 0x0001u, 0x83ffu, 0x83ffu, 0x8000u},
    {4u, 3u, 3u, false, 0x7bffu, 0x3c01u, 0x0000u, 0x7bffu},
    {4u, 1u, 3u, false, 0x7bffu, 0x3c01u, 0x0000u, 0x7c00u},
    {4u, 2u, 3u, false, 0x0000u, 0x7c00u, 0x7e00u, 0xfe00u},
    {6u, 2u, 1u, false, 0x83ffu, 0x0000u, 0x0000u, 0x8400u},
    {7u, 1u, 3u, false, 0x3c01u, 0x3c01u, 0x0000u, 0x3c02u},
    {4u, 1u, 0u, true, 0x0400u, 0x9000u, 0x0400u, 0x0000u},
    {4u, 2u, 1u, true, 0x0001u, 0x83ffu, 0x83ffu, 0x8000u},
    {4u, 3u, 3u, true, 0x7bffu, 0x3c01u, 0x0000u, 0x7bffu},
    {4u, 1u, 3u, true, 0x7bffu, 0x3c01u, 0x0000u, 0x7bffu},
    {4u, 2u, 3u, true, 0x0000u, 0x7c00u, 0x7e00u, 0xfe00u},
}};

std::uint32_t Magnitude(std::uint32_t value) { return value & 0x7fffu; }
bool IsNan(std::uint32_t value) { return Magnitude(value) > 0x7c00u; }
bool IsInfinity(std::uint32_t value) { return Magnitude(value) == 0x7c00u; }
std::uint32_t Flush(std::uint32_t value) { return Magnitude(value) < 0x400u ? value & 0x8000u : value; }

__int128 Units(std::uint32_t value) {
    const auto exponent = Magnitude(value) >> 10u;
    const __int128 magnitude = exponent == 0u ? Magnitude(value) : static_cast<__int128>(0x400u | (value & 0x3ffu)) << (exponent - 1u);
    return (value & 0x8000u) ? -magnitude : magnitude;
}

std::uint32_t BitWidth(unsigned __int128 value) {
    std::uint32_t width = 0u;
    while (value != 0u) {
        ++width;
        value >>= 1u;
    }
    return width;
}

unsigned __int128 RoundTo(unsigned __int128 magnitude, std::uint32_t shift, std::uint32_t sign, std::uint32_t rounding) {
    const unsigned __int128 ulp = static_cast<unsigned __int128>(1u) << shift;
    const unsigned __int128 quotient = magnitude >> shift;
    const unsigned __int128 remainder = magnitude & (ulp - 1u);
    if (remainder == 0u) return quotient << shift;
    bool increment = false;
    if (rounding == 0u) increment = remainder > (ulp >> 1u) || (remainder == (ulp >> 1u) && (quotient & 1u) != 0u);
    if (rounding == 1u) increment = sign == 0u;
    if (rounding == 2u) increment = sign != 0u;
    return (quotient + (increment ? 1u : 0u)) << shift;
}

std::uint32_t Fma(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t rounding, std::uint32_t denorm, bool ieee, bool overflow) {
    if ((denorm & 1u) == 0u) {
        a = Flush(a);
        b = Flush(b);
        c = Flush(c);
    }
    const auto quiet = [&](std::uint32_t value) { return value | (ieee ? 0x200u : 0u); };
    if (IsNan(a)) return quiet(a);
    if (IsNan(b)) return quiet(b);
    const std::uint32_t productSign = (a ^ b) & 0x8000u;
    if ((IsInfinity(a) && Magnitude(b) == 0u) || (IsInfinity(b) && Magnitude(a) == 0u)) return 0xfe00u;
    if (IsNan(c)) return quiet(c);
    if (IsInfinity(a) || IsInfinity(b)) return IsInfinity(c) && (c & 0x8000u) != productSign ? 0xfe00u : productSign | 0x7c00u;
    if (IsInfinity(c)) return c;
    const __int128 exact = Units(a) * Units(b) + (Units(c) << 24u);
    if (exact == 0) return productSign == (c & 0x8000u) ? productSign : (rounding == 2u ? 0x8000u : 0u);
    const std::uint32_t sign = exact < 0 ? 0x8000u : 0u;
    const auto magnitude = static_cast<unsigned __int128>(exact < 0 ? -exact : exact);
    const std::uint32_t precisionShift = BitWidth(magnitude) > 11u ? BitWidth(magnitude) - 11u : 0u;
    if ((denorm & 2u) == 0u && RoundTo(magnitude, precisionShift, sign, rounding) < (static_cast<unsigned __int128>(1u) << 34u)) return sign;
    const auto rounded = RoundTo(magnitude, std::max(precisionShift, 24u), sign, rounding);
    if (rounded >= (static_cast<unsigned __int128>(1u) << 64u)) {
        const bool infinity = !overflow && (rounding == 0u || (rounding == 1u && sign == 0u) || (rounding == 2u && sign != 0u));
        return sign | (infinity ? 0x7c00u : 0x7bffu);
    }
    if (rounded < (static_cast<unsigned __int128>(1u) << 34u)) return sign | static_cast<std::uint32_t>(rounded >> 24u);
    const std::uint32_t exponent = BitWidth(rounded) - 11u;
    return sign | ((exponent - 23u) << 10u) | (static_cast<std::uint32_t>(rounded >> exponent) & 0x3ffu);
}

std::uint32_t Expected(std::uint32_t op, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t rounding, std::uint32_t denorm, bool ieee, bool overflow) {
    constexpr std::uint32_t one = 0x3c00u;
    switch (op) {
    case 0u: return Fma(a, one, b, rounding, denorm, ieee, overflow);
    case 1u: return Fma(a, one, b ^ 0x8000u, rounding, denorm, ieee, overflow);
    case 2u: return Fma(a ^ 0x8000u, one, b, rounding, denorm, ieee, overflow);
    case 3u: return Fma(a, b, (a ^ b) & 0x8000u, rounding, denorm, ieee, overflow);
    case 4u:
    case 5u: return Fma(a, b, c, rounding, denorm, ieee, overflow);
    case 6u: return Fma(a, 0x3c01u, b, rounding, denorm, ieee, overflow);
    default: return Fma(a, b, 0x9000u, rounding, denorm, ieee, overflow);
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
            Require(Expected(row.op, row.a, row.b, row.c, row.rounding, row.denorm, true, row.overflow) == row.expected, "measured gfx1036 reference");
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
        for (std::uint32_t denorm = 0u; denorm < 4u; ++denorm) {
            for (const bool ieee : {false, true}) {
                for (const bool overflow : {false, true}) {
                    ShaderRecompiler::RecompileRequest request{
                        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                        {wave, 0, userData, compute, std::nullopt, std::nullopt, memory}, target, {0, 0, 0, 128}
                    };
                    request.useCache = false;
                    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x30u | (selectedRounding << 2u) | (denorm << 6u), false, ieee, overflow};
                    const auto result = ShaderRecompiler::Recompile(request);
                    Output.fill(0xdeadbeefu);
                    device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
                    device->WaitIdle();
                    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                        const std::uint32_t a = Input[tid * 4u];
                        const std::uint32_t b = Input[tid * 4u + 1u];
                        const std::uint32_t c = Input[tid * 4u + 2u];
                        for (std::uint32_t op = 0; op < Ops; ++op) {
                            const std::uint32_t expected = Expected(op, a, b, c, selectedRounding, denorm, ieee, overflow);
                            const std::uint32_t actual = Output[tid * Ops + op] & 0xffffu;
                            if (actual != expected) {
                                std::printf("integer=%u round=%u denorm=%u ieee=%u overflow=%u op=%u a=%04x b=%04x c=%04x actual=%04x expected=%04x\n",
                                    selectedInteger, selectedRounding, denorm, ieee, overflow, op, a, b, c, actual, expected);
                                throw std::runtime_error("f16 directed rounding mismatch");
                            }
                            ++compared;
                        }
                    }
                }
            }
        }
        std::cout << "f16 directed rounding: " << compared << " results passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
