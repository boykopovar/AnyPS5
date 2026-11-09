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
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Capacity = 1u << 20;
alignas(256) std::array<std::uint32_t, Capacity> Input{};
alignas(256) std::array<std::uint32_t, Capacity * 4u> Output{};

std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }

std::vector<std::uint32_t> Inputs() {
    std::vector<std::uint32_t> inputs{
        0x00000000u, 0x80000000u, 0x00000001u, 0x807fffffu, 0x00800000u, 0x3f000000u, 0xbf000000u, 0x3f7fffffu,
        0xbf7fffffu, 0x3f800000u, 0xbf800000u, 0x3f800001u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00001u,
        0x7f800001u, 0x437f0000u, 0x437f8000u, 0x43800000u, 0x7f7fffffu, 0x37800000u, 0x377fffffu, 0x38000000u,
    };
    const auto around = [&](double exact) {
        const std::uint32_t nearest = Bits(static_cast<float>(exact));
        for (const int delta : {-2, -1, 0, 1, 2}) inputs.push_back(nearest + static_cast<std::uint32_t>(delta));
    };
    for (const double scale : {65535.0, 32767.0}) {
        for (std::uint32_t k = 0; k + 1u < static_cast<std::uint32_t>(scale); ++k) around((k + 0.5) / scale);
    }
    for (std::uint32_t k = 0; k < 256u; ++k) around(k + 0.5);
    const std::size_t positive = inputs.size();
    for (std::size_t index = 0; index < positive; index += 7u) inputs.push_back(inputs[index] ^ 0x80000000u);
    while (inputs.size() % Threads != 0u) inputs.push_back(0u);
    if (inputs.size() > Capacity) throw std::runtime_error("f32 pack rounding: too many inputs");
    return inputs;
}

bool Nan(std::uint32_t bits) { return (bits & 0x7fffffffu) > 0x7f800000u; }

std::uint32_t Flushed(std::uint32_t bits) { return (bits & 0x7f800000u) == 0u ? bits & 0x80000000u : bits; }

std::uint32_t Norm(std::uint32_t source, bool signedValue) {
    const std::uint32_t bits = Flushed(source);
    if (Nan(bits)) return 0u;
    const double scale = signedValue ? 32767.0 : 65535.0;
    const bool negative = (bits & 0x80000000u) != 0u;
    const double magnitude = std::fabs(static_cast<double>(std::bit_cast<float>(bits)));
    const std::uint32_t value = magnitude >= 1.0 ? static_cast<std::uint32_t>(scale) : static_cast<std::uint32_t>(std::nearbyint(magnitude * scale));
    if (!negative) return value;
    return signedValue ? (0u - value) & 0xffffu : 0u;
}

std::uint32_t U8(std::uint32_t source) {
    const float value = std::bit_cast<float>(Flushed(source));
    if (Nan(source) || !(value > 0.0f)) return 0u;
    if (value >= 255.0f) return 255u;
    return static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(value)));
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
            0x34020082u, 0x8f098708u, 0x4a020209u, 0xe0301000u, 0x80000401u, 0x34060282u, 0xbf8c3f70u,
            0xd7690005u, 0x00020904u, 0xd7680006u, 0x00020904u, 0xd55e0007u, 0x02010104u,
            0xe0781000u, 0x80010503u, 0xbf810000u,
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
            const std::uint32_t* out = &Output[index * 4u];
            const std::uint32_t source = inputs[index];
            const std::uint32_t unsignedNorm = Norm(source, false);
            const std::uint32_t signedNorm = Norm(source, true);
            Require(out[0] == (unsignedNorm | (unsignedNorm << 16u)), "f32 pack rounding: v_cvt_pknorm_u16_f32 of " + Hex(source) + " is " + Hex(out[0]) + ", expected " + Hex(unsignedNorm | (unsignedNorm << 16u)));
            Require(out[1] == (signedNorm | (signedNorm << 16u)), "f32 pack rounding: v_cvt_pknorm_i16_f32 of " + Hex(source) + " is " + Hex(out[1]) + ", expected " + Hex(signedNorm | (signedNorm << 16u)));
            Require(out[2] == U8(source), "f32 pack rounding: v_cvt_pk_u8_f32 of " + Hex(source) + " is " + Hex(out[2]) + ", expected " + Hex(U8(source)));
        }
        std::printf("f32 pack rounding tests passed (%zu inputs)\n", inputs.size());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
