#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Capacity = 1u << 18;
alignas(256) std::array<std::uint32_t, Capacity> Input{};
alignas(256) std::array<std::uint32_t, Capacity> Output{};

std::uint32_t HalfToFloatBits(std::uint32_t half) {
    const std::uint32_t exponent = (half >> 10u) & 0x1fu;
    const std::uint32_t mantissa = half & 0x3ffu;
    if (exponent != 0u) return ((exponent + 112u) << 23u) | (mantissa << 13u);
    if (mantissa == 0u) return 0u;
    std::uint32_t shift = 0;
    while ((mantissa << shift & 0x400u) == 0u) ++shift;
    return ((113u - shift) << 23u) | (((mantissa << shift) & 0x3ffu) << 13u);
}

std::vector<std::uint32_t> Inputs() {
    std::vector<std::uint32_t> inputs{
        0x00000000u, 0x00000001u, 0x007fffffu, 0x00800000u, 0x33000000u, 0x33000001u, 0x337fffffu, 0x33800000u,
        0x387fffffu, 0x38800000u, 0x477fe000u, 0x477fefffu, 0x477ff000u, 0x477fffffu, 0x47800000u, 0x7f7fffffu,
        0x7f800000u, 0x7fc00000u, 0x7f800001u, 0x7fffe000u, 0x7fbfffffu, 0x7f802000u,
    };
    for (std::uint32_t half = 0; half < 0x7c00u; ++half) {
        const std::uint32_t exact = HalfToFloatBits(half);
        const std::uint32_t next = HalfToFloatBits(half + 1u);
        const std::uint32_t middle = exact + (next - exact) / 2u;
        for (const std::uint32_t value : {exact, exact + 1u, middle - 1u, middle, middle + 1u}) inputs.push_back(value);
    }
    while (inputs.size() % Threads != 0u) inputs.push_back(0u);
    if (inputs.size() > Capacity) throw std::runtime_error("f16 pack toward zero: too many inputs");
    return inputs;
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
        const auto inputs = Inputs();
        std::copy(inputs.begin(), inputs.end(), Input.begin());
        Output.fill(0xdeadbeefu);
        const std::vector<std::uint32_t> code{
            0x34020082u, 0x8f098708u, 0x4a020209u, 0xe0301000u, 0x80000401u, 0xbf8c3f70u,
            0xd52f0005u, 0x40020904u, 0xe0701000u, 0x80010501u, 0xbf810000u,
        };
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> words(code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, static_cast<std::uint32_t>(inputs.size() / Threads), 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            const std::uint32_t source = inputs[index];
            const std::uint32_t expected = TowardZero(source) | (TowardZero(source ^ 0x80000000u) << 16u);
            Require(Output[index] == expected, "f16 pack toward zero: v_cvt_pkrtz_f16_f32 of " + Hex(source) + ", -" + Hex(source) + " is " + Hex(Output[index]) + ", expected " + Hex(expected));
        }
        std::printf("f16 pack toward zero tests passed (%zu inputs)\n", inputs.size());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
