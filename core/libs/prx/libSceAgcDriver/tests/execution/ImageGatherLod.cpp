#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ClampWrap = 0;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t MipFilterPoint = 1;
constexpr std::uint32_t MaxLod = 15u << 8u;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 65536> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> Gather2DLod{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf1100108, 0x00820602, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};

struct Variant {
    std::uint32_t clamp;
    const char* name;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t clamp) {
    return {clamp | (clamp << 3u) | (ClampEdge << 6u), MaxLod << 12u, MipFilterPoint << 26u, 0u};
}

std::uint8_t Red(std::uint32_t level, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint8_t>(level * 61u + x * 13u + y * 7u + 3u);
}

void FillTexels() {
    Texels.fill(0);
    const auto descriptor = TextureDescriptor();
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(surface.mips.size() == Levels, "the test surface has " + std::to_string(surface.mips.size()) + " mips, expected " + std::to_string(Levels));
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = surface.mips.at(level);
        const auto extent = Size >> level;
        for (std::uint32_t y = 0; y < extent; ++y) {
            for (std::uint32_t x = 0; x < extent; ++x) {
                const auto offset = mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u;
                Require(offset + 4u <= Texels.size(), "the test surface does not fit its buffer");
                auto* texel = Texels.data() + offset;
                texel[0] = Red(level, x, y);
                texel[1] = 0x11;
                texel[2] = 0x22;
                texel[3] = 0xff;
            }
        }
    }
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(-0.07f + 0.037f * static_cast<float>(tid));
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(1.06f - 0.033f * static_cast<float>(tid));
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(static_cast<float>(tid % (Levels + 2u)));
        Input[tid * 4u + 3u] = 0u;
    }
}

std::uint32_t Texel(std::uint32_t level, int x, int y, std::uint32_t clamp) {
    const int extent = static_cast<int>(Size >> level);
    const auto address = [&](int coordinate) { return clamp == ClampEdge ? std::clamp(coordinate, 0, extent - 1) : ((coordinate % extent) + extent) % extent; };
    return Red(level, static_cast<std::uint32_t>(address(x)), static_cast<std::uint32_t>(address(y)));
}

std::array<std::uint32_t, 4> Expected(std::uint32_t tid, const Variant& variant) {
    const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
    const float v = std::bit_cast<float>(Input[tid * 4u + 1u]);
    const float lod = std::bit_cast<float>(Input[tid * 4u + 2u]);
    const auto level = std::min(static_cast<std::uint32_t>(lod), Levels - 1u);
    const auto extent = static_cast<float>(Size >> level);
    const int left = static_cast<int>(std::floor(u * extent - 0.5f));
    const int top = static_cast<int>(std::floor(v * extent - 0.5f));
    return {Texel(level, left, top + 1, variant.clamp), Texel(level, left + 1, top + 1, variant.clamp), Texel(level, left + 1, top, variant.clamp), Texel(level, left, top, variant.clamp)};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const Variant& variant) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor();
    const auto sampler = SamplerDescriptor(variant.clamp);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::span<const std::uint32_t> code(Gather2DLod);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const Variant& variant) {
    Output.fill(-1.0f);
    const auto result = Compile(device, variant);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Gather2DLod.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Expected(tid, variant);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float actual = Output[tid * 4u + component];
            const float scaled = actual * 255.0f;
            Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && static_cast<std::uint32_t>(std::lround(scaled)) == expected[component], std::string(variant.name) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexels();
        FillInput();
        Run(*device, {ClampEdge, "2D gather at an explicit LOD, clamp to edge"});
        Run(*device, {ClampWrap, "2D gather at an explicit LOD, wrap"});
        std::puts("image 2D explicit-LOD gather tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
