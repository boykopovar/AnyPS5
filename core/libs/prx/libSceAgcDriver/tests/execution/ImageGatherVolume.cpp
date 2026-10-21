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
constexpr std::uint32_t Width = 8;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Depth = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t ClampWrap = 0;
constexpr std::uint32_t ClampMirror = 1;
constexpr std::uint32_t ClampEdge = 2;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> GatherRed{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0110, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 10> GatherBlue{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0410, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};

struct Variant {
    std::span<const std::uint32_t> code;
    bool blue;
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
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Type3D << 28u),
        Depth - 1u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t clamp) {
    return {clamp | (clamp << 3u) | (clamp << 6u), 0u, 0u, 0u};
}

std::uint8_t Red(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return static_cast<std::uint8_t>(x + Width * y + Width * Height * z);
}

std::uint8_t Blue(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return static_cast<std::uint8_t>(255u - 2u * Red(x, y, z));
}

void FillTexels() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor()));
    Require(geometry.guestBytes <= Texels.size(), "image gather volume: the volume does not fit the texel storage");
    const auto& mip = geometry.mips.at(0);
    Texels.fill(0);
    for (std::uint32_t z = 0; z < Depth; ++z) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                auto* texel = &Texels[geometry.GuestLayerOffset(z) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = Red(x, y, z);
                texel[1] = 0x11;
                texel[2] = Blue(x, y, z);
                texel[3] = 0xff;
            }
        }
    }
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = 0u;
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(-0.23f + 0.047f * static_cast<float>(tid));
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(1.29f - 0.053f * static_cast<float>(tid));
        Input[tid * 4u + 3u] = std::bit_cast<std::uint32_t>(-0.37f + 0.061f * static_cast<float>((tid * 7u) % Threads));
    }
}

int Address(int coordinate, std::uint32_t size, std::uint32_t clamp) {
    const int extent = static_cast<int>(size);
    if (clamp == ClampEdge) return std::clamp(coordinate, 0, extent - 1);
    if (clamp == ClampWrap) return ((coordinate % extent) + extent) % extent;
    const int period = ((coordinate % (2 * extent)) + 2 * extent) % (2 * extent);
    return period < extent ? period : 2 * extent - 1 - period;
}

std::array<std::uint32_t, 4> Expected(std::uint32_t tid, const Variant& variant) {
    const float u = std::bit_cast<float>(Input[tid * 4u + 1u]);
    const float v = std::bit_cast<float>(Input[tid * 4u + 2u]);
    const float r = std::bit_cast<float>(Input[tid * 4u + 3u]);
    const int left = static_cast<int>(std::floor(u * static_cast<float>(Width) - 0.5f));
    const int top = static_cast<int>(std::floor(v * static_cast<float>(Height) - 0.5f));
    const auto z = static_cast<std::uint32_t>(Address(static_cast<int>(std::floor(r * static_cast<float>(Depth))), Depth, variant.clamp));
    const auto texel = [&](int x, int y) {
        const auto column = static_cast<std::uint32_t>(Address(x, Width, variant.clamp));
        const auto row = static_cast<std::uint32_t>(Address(y, Height, variant.clamp));
        return static_cast<std::uint32_t>(variant.blue ? Blue(column, row, z) : Red(column, row, z));
    };
    return {texel(left, top + 1), texel(left + 1, top + 1), texel(left + 1, top), texel(left, top)};
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
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(variant.code.data()), std::as_bytes(variant.code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(variant.code.data()), variant.code, 0, {}},
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
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(variant.code.data()));
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
        Run(*device, {GatherRed, false, ClampEdge, "3D gather of red, clamp to edge"});
        Run(*device, {GatherBlue, true, ClampEdge, "3D gather of blue, clamp to edge"});
        Run(*device, {GatherRed, false, ClampWrap, "3D gather of red, wrap"});
        Run(*device, {GatherRed, false, ClampMirror, "3D gather of red, mirror"});
        std::puts("image 3D gather tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
