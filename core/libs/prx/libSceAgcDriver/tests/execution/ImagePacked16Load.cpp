#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "Recompiler.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Groups = Width / Threads;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t SwizzleZYXW = 0xf2eu;
constexpr std::uint32_t One = 0x3f800000u;
constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};

struct Packed {
    std::uint32_t format;
    std::array<std::uint16_t, 4> masks;
    const char* name;
};

constexpr std::array<Packed, 4> Formats{{
    {133u, {0x001fu, 0x07e0u, 0xf800u, 0x0000u}, "5_6_5_UNORM"},
    {134u, {0x001fu, 0x03e0u, 0x7c00u, 0x8000u}, "1_5_5_5_UNORM"},
    {135u, {0x0001u, 0x003eu, 0x07c0u, 0xf800u}, "1_5_5_5_UNORM_code135"},
    {136u, {0x000fu, 0x00f0u, 0x0f00u, 0xf000u}, "4_4_4_4_UNORM"},
}};

alignas(256) std::array<std::uint32_t, Width * 4> Output{};
alignas(256) std::array<std::uint16_t, Width> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 15> LoadXyzw{
    0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x7e3e0280u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
    0x7e1a0280u, 0xf0001f08u, 0x00010a1eu, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 9> SampleLz{
    0x34060084u, 0x7e280280u, 0x7e2a0280u, 0xf09c0f08u, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
    0xbf810000u,
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Pattern(std::uint32_t x) {
    return (x + 1u) & 15u;
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Fill(const Packed& format) {
    for (std::uint32_t x = 0; x < Width; ++x) {
        std::uint16_t texel = 0;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            if (((Pattern(x) >> component) & 1u) != 0u) texel = static_cast<std::uint16_t>(texel | format.masks[component]);
        }
        Texels[x] = texel;
    }
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t format, std::uint32_t swizzle, std::uint32_t groups, bool linear = false) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = TextureDescriptor(format, swizzle);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(PointSampler.begin(), PointSampler.end(), userData.begin() + 12);
    if (linear) userData[14] = 0x00500000u;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Verify(const Packed& format, std::uint32_t swizzle, std::uint32_t texels, bool sampled, const std::string& what) {
    for (std::uint32_t index = 0; index < texels; ++index) {
        const auto x = sampled ? 0u : index;
        std::array<std::uint32_t, 4> channels{};
        for (std::uint32_t component = 0; component < 4u; ++component) {
            channels[component] = format.masks[component] != 0u && ((Pattern(x) >> component) & 1u) != 0u ? One : 0u;
        }
        if (format.masks[3] == 0u) channels[3] = channels[0];
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto selector = (swizzle >> (component * 3u)) & 7u;
            const auto expected = selector == 0u ? 0u : selector == 1u ? One : channels[selector - 4u];
            const auto actual = Output[index * 4u + component];
            Require(actual == expected, what + ": " + (sampled ? "thread " : "texel ") + std::to_string(index) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

void Check(AgcDriver::VulkanDevice& device, const Packed& format, std::uint32_t swizzle, const std::string& swizzleName) {
    Fill(format);
    Run(device, LoadXyzw, format.format, swizzle, Groups);
    Verify(format, swizzle, Width, false, std::string(format.name) + " image_load " + swizzleName);
    Run(device, SampleLz, format.format, swizzle, 1u);
    Verify(format, swizzle, Threads, true, std::string(format.name) + " image_sample_lz " + swizzleName);
}

void CheckBits(AgcDriver::VulkanDevice& device, const Packed& format, std::uint32_t swizzle) {
    for (std::uint32_t x = 0; x < Width; ++x) {
        Texels[x] = x < 16u ? static_cast<std::uint16_t>(1u << x) : x < 32u ? static_cast<std::uint16_t>((x & 1u) != 0u ? 0xffffu : 0u) : static_cast<std::uint16_t>(x * 4051u);
    }
    const auto channel = [&](std::uint32_t x, std::uint32_t component) {
        const auto selector = (swizzle >> (component * 3u)) & 7u;
        if (selector < 2u) return static_cast<float>(selector);
        auto mask = format.masks[selector - 4u];
        if (mask == 0u) mask = format.masks[0];
        const auto shift = std::countr_zero(mask);
        return static_cast<float>((Texels[x] & mask) >> shift) / static_cast<float>(mask >> shift);
    };
    for (std::uint32_t pass = 0; pass < 3u; ++pass) {
        alignas(256) std::array<std::uint32_t, 13> sample{
            0x34060084u, 0x7e280d00u, 0x102828ffu, 0x3b800000u, 0x062828ffu, pass == 2u ? 0x3b400000u : 0x3b000000u,
            0x7e2a0280u, 0xf09c0f08u, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
        };
        Run(device, pass == 0u ? std::span<const std::uint32_t>(LoadXyzw) : std::span<const std::uint32_t>(sample), format.format, swizzle, pass == 0u ? Groups : 1u, pass == 2u);
        for (std::uint32_t x = 0; x < (pass == 0u ? Width : Threads); ++x) {
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto left = channel(x, component);
                const auto right = channel(std::min(x + 1u, Width - 1u), component);
                const auto expected = pass == 2u ? left * 0.75f + right * 0.25f : left;
                const auto actual = std::bit_cast<float>(Output[x * 4u + component]);
                Require(std::isfinite(actual) && (expected == 0.0f || expected == 1.0f ? actual == expected : std::abs(actual - expected) <= (1.0f / 255.0f + 0.00001f)), "packed bit walk/sample format " + std::to_string(format.format) + " pass " + std::to_string(pass) + " texel " + std::to_string(x) + " component " + std::to_string(component) + " actual " + std::to_string(actual) + " expected " + std::to_string(expected));
            }
        }
    }
}

}

int main() {
    try {
        bool bufferRejected = false;
        try {
            static_cast<void>(ShaderRecompiler::GetFormatInfo(ShaderRecompiler::IrBufferFormat::Format1_5_5_5UNorm));
        } catch (const std::exception& error) {
            bufferRejected = std::string(error.what()).find("unsupported buffer format 135") != std::string::npos;
        }
        Require(bufferRejected, "image-only format 135 was accepted as a buffer format");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto& format : Formats) {
            CheckBits(*device, format, SwizzleXYZW);
            CheckBits(*device, format, SwizzleZYXW);
            CheckBits(*device, format, 0x244u);
            Check(*device, format, SwizzleXYZW, "X Y Z W");
            Check(*device, format, SwizzleZYXW, "Z Y X W");
        }
        std::puts("image packed 16-bit load tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
