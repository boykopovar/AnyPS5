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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Batches = 64;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34020084u, 0x34040085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd564000au, 0x00020d04u, 0xd565000cu,
    0x00020d04u, 0x7e1c1f04u, 0x7e202106u, 0xe0781000u, 0x80010a02u, 0xe0781010u, 0x80010e02u, 0xbf810000u,
};

constexpr std::uint64_t Edges[] = {
    0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000001ull, 0x8000000000000001ull, 0x000fffffffffffffull, 0x0008000000000000ull,
    0x0010000000000000ull, 0x0010000000000001ull, 0x001fffffffffffffull, 0x3ff0000000000000ull, 0x3ff0000000000001ull, 0x3fefffffffffffffull,
    0xbff0000000000000ull, 0x3fe0000000000000ull, 0x4000000000000000ull, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0x7fe0000000000000ull,
    0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000000ull, 0xfff8000000000001ull, 0x7ff0000000000001ull, 0xfff4000000000123ull,
    0x3ca0000000000000ull, 0x3cb0000000000000ull, 0x4330000000000000ull, 0x4340000000000000ull, 0x47efffffe0000000ull, 0x47efffff00000000ull,
    0x36a0000000000000ull, 0x3690000000000000ull, 0x3690000000000001ull, 0x380fffffe0000000ull, 0x3810000000000000ull, 0xc7f0000000000000ull,
};

struct Random {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    std::uint64_t Next() {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        return state;
    }
};

std::uint64_t WithExponent(std::uint64_t bits, std::uint64_t exponent) {
    return (bits & 0x800fffffffffffffull) | ((exponent & 0x7ffu) << 52u);
}

std::uint64_t Operand(Random& random) {
    const auto bits = random.Next();
    switch (random.Next() % 8u) {
    case 0: return Edges[random.Next() % std::size(Edges)];
    case 1: return WithExponent(bits, random.Next() % 4u);
    case 2: return WithExponent(bits, 2044u + random.Next() % 3u);
    case 3: return WithExponent(bits, 1023u - 160u + random.Next() % 300u);
    case 4: return WithExponent(bits, 1023u - 30u + random.Next() % 60u);
    default: return bits;
    }
}

std::uint64_t Partner(Random& random, std::uint64_t lhs) {
    switch (random.Next() % 6u) {
    case 0: return lhs ^ 0x8000000000000000ull;
    case 1: return (lhs ^ 0x8000000000000000ull) + (random.Next() % 5u) - 2u;
    case 2: return WithExponent(random.Next(), ((lhs >> 52u) & 0x7ffu) - 53u + random.Next() % 107u);
    case 3: return WithExponent(random.Next(), 2046u - ((lhs >> 52u) & 0x7ffu) + random.Next() % 5u);
    default: return Operand(random);
    }
}

bool IsNan64(std::uint64_t bits) {
    return (bits & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
}

std::uint64_t ArithmeticResult(std::uint64_t lhs, std::uint64_t rhs, double result) {
    if (IsNan64(lhs)) return lhs | 0x0008000000000000ull;
    if (IsNan64(rhs)) return rhs | 0x0008000000000000ull;
    const auto bits = std::bit_cast<std::uint64_t>(result);
    return IsNan64(bits) ? 0xfff8000000000000ull : bits;
}

std::uint32_t NarrowedResult(std::uint64_t value) {
    if (!IsNan64(value)) return std::bit_cast<std::uint32_t>(static_cast<float>(std::bit_cast<double>(value)));
    const auto high = static_cast<std::uint32_t>(value >> 32u);
    return (high & 0x80000000u) | 0x7fc00000u | ((high & 0x000fffffu) << 3u) | (static_cast<std::uint32_t>(value >> 29u) & 7u);
}

std::uint64_t WidenedResult(std::uint32_t value) {
    if ((value & 0x7fffffffu) > 0x7f800000u) return (static_cast<std::uint64_t>(value & 0x80000000u) << 32u) | 0x7ff8000000000000ull | (static_cast<std::uint64_t>(value & 0x007fffffu) << 29u);
    return std::bit_cast<std::uint64_t>(static_cast<double>(std::bit_cast<float>(value)));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
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
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto target = device->Target();
        std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
        std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityFloat64));
        target.supportedCapabilities = capabilities;
        Random random;
        std::uint32_t checked = 0;
        for (std::uint32_t batch = 0; batch < Batches; ++batch) {
            std::array<std::array<std::uint64_t, 2>, Threads> operands{};
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                const std::size_t pairIndex = static_cast<std::size_t>(batch) * Threads + lane;
                const bool edges = pairIndex < std::size(Edges) * std::size(Edges);
                const auto lhs = edges ? Edges[pairIndex % std::size(Edges)] : Operand(random);
                const auto rhs = edges ? Edges[pairIndex / std::size(Edges)] : Partner(random, lhs);
                operands[lane] = {lhs, rhs};
                Input[lane * Inputs] = static_cast<std::uint32_t>(lhs);
                Input[lane * Inputs + 1u] = static_cast<std::uint32_t>(lhs >> 32u);
                Input[lane * Inputs + 2u] = static_cast<std::uint32_t>(rhs);
                Input[lane * Inputs + 3u] = static_cast<std::uint32_t>(rhs >> 32u);
            }
            Run(*device, target);
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                const auto [lhs, rhs] = operands[lane];
                const auto word = [&](std::uint32_t index) { return static_cast<std::uint64_t>(Output[lane * Results + index]); };
                const auto where = " of " + Hex(lhs) + " and " + Hex(rhs);
                const auto sum = word(0) | (word(1) << 32u);
                const auto expectedSum = ArithmeticResult(lhs, rhs, std::bit_cast<double>(lhs) + std::bit_cast<double>(rhs));
                Require(sum == expectedSum, "v_add_f64" + where + " is " + Hex(sum) + ", expected " + Hex(expectedSum));
                const auto product = word(2) | (word(3) << 32u);
                const auto expectedProduct = ArithmeticResult(lhs, rhs, std::bit_cast<double>(lhs) * std::bit_cast<double>(rhs));
                Require(product == expectedProduct, "v_mul_f64" + where + " is " + Hex(product) + ", expected " + Hex(expectedProduct));
                const auto narrowed = static_cast<std::uint32_t>(word(4));
                Require(narrowed == NarrowedResult(lhs), "v_cvt_f32_f64 of " + Hex(lhs) + " is " + Hex(narrowed) + ", expected " + Hex(NarrowedResult(lhs)));
                const auto widened = word(6) | (word(7) << 32u);
                const auto source = static_cast<std::uint32_t>(rhs);
                Require(widened == WidenedResult(source), "v_cvt_f64_f32 of " + Hex(source) + " is " + Hex(widened) + ", expected " + Hex(WidenedResult(source)));
                checked += 4u;
            }
        }
        std::printf("f64 integer arithmetic tests passed, %u results checked\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
