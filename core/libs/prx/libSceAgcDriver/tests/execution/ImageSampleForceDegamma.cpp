#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Size = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format8888Srgb = 130;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t FilterPoint = 0;
constexpr std::uint32_t FilterBilinear = 1;
constexpr std::uint32_t ForceDegamma = 1u << 20u;

alignas(256) std::array<float, Threads * 3> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0900f08, 0x00820401, 0xbf8c3f70,
    0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t filter, std::uint32_t forced) {
    return {0x92u | forced, 0x00fff000u, (filter << 20u) | (filter << 22u) | (1u << 24u), 0u};
}

std::uint8_t Code8(std::uint32_t x, std::uint32_t y, std::uint32_t channel) {
    return static_cast<std::uint8_t>((y * 61u + x * 17u + channel * 97u + 5u) % 256u);
}

void FillTexels() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888Srgb, Size, Size, 1);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "image sample force degamma: the texture does not fit its storage");
    Texels.fill(0);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            auto* texel = &Texels[mips[0].tiledOffset + static_cast<std::uint64_t>(y) * mips[0].pitchBytes + x * 4u];
            for (std::uint32_t channel = 0; channel < 4u; ++channel) texel[channel] = Code8(x, y, channel);
        }
    }
}

void FillInput(bool centers) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = tid % Size;
        const auto y = (tid / Size) % Size;
        Input[tid * 3u + 0u] = centers ? (static_cast<float>(x) + 0.5f) / static_cast<float>(Size) : (static_cast<float>((tid * 5u + 1u) % 13u) + 0.3f) / 13.0f;
        Input[tid * 3u + 1u] = centers ? (static_cast<float>(y) + 0.5f) / static_cast<float>(Size) : (static_cast<float>((tid * 7u + 3u) % 13u) + 0.6f) / 13.0f;
        Input[tid * 3u + 2u] = 0.0f;
    }
}

float SrgbToLinear(std::uint8_t code) {
    const float c = static_cast<float>(code) / 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

std::array<float, Threads * 4> Sample(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t filter, std::uint32_t forced) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(format);
    const auto sampler = SamplerDescriptor(filter, forced);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Output.fill(-1.0f);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    return Output;
}

void RequireSame(const std::array<float, Threads * 4>& forced, const std::array<float, Threads * 4>& plain, const char* name) {
    for (std::uint32_t index = 0; index < forced.size(); ++index) {
        Require(std::bit_cast<std::uint32_t>(forced[index]) == std::bit_cast<std::uint32_t>(plain[index]), std::string(name) + ": thread " + std::to_string(index / 4u) + " component " + std::to_string(index % 4u) + " sampled " + std::to_string(forced[index]) + " through FORCE_DEGAMMA, " + std::to_string(plain[index]) + " without it");
    }
}

void RequireDecoded(const std::array<float, Threads * 4>& sampled) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = tid % Size;
        const auto y = (tid / Size) % Size;
        for (std::uint32_t channel = 0; channel < 4u; ++channel) {
            const auto code = Code8(x, y, channel);
            const float expected = channel == 3u ? static_cast<float>(code) / 255.0f : SrgbToLinear(code);
            const float value = sampled[tid * 4u + channel];
            Require(std::fabs(value - expected) <= 0.005f, "8_8_8_8_SRGB through FORCE_DEGAMMA: thread " + std::to_string(tid) + " component " + std::to_string(channel) + " sampled " + std::to_string(value) + ", expected " + std::to_string(expected));
        }
    }
}

void Reject(AgcDriver::VulkanDevice& device, std::uint32_t format, std::string_view reason) {
    try {
        static_cast<void>(Sample(device, format, FilterPoint, ForceDegamma));
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected rejection: ") + error.what());
        return;
    }
    Require(false, std::string("expected rejection: ") + std::string(reason));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexels();
        FillInput(true);
        const auto forcedPoint = Sample(*device, Format8888Srgb, FilterPoint, ForceDegamma);
        RequireDecoded(forcedPoint);
        RequireSame(forcedPoint, Sample(*device, Format8888Srgb, FilterPoint, 0u), "8_8_8_8_SRGB, point");
        FillInput(false);
        const auto forcedBilinear = Sample(*device, Format8888Srgb, FilterBilinear, ForceDegamma);
        RequireSame(forcedBilinear, Sample(*device, Format8888Srgb, FilterBilinear, 0u), "8_8_8_8_SRGB, bilinear");
        Reject(*device, Format8888UNorm, "guest format 56, which is not sRGB, is sampled through a sampler that forces sRGB decoding");
        std::puts("image sample force degamma tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
