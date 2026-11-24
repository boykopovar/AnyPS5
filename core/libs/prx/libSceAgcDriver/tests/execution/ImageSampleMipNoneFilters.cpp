#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
constexpr std::uint32_t Width = 4;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Unorm8_8_8_8 = 56;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t ClampLastTexel = 0x92u;
constexpr std::uint32_t Point = 0u;
constexpr std::uint32_t Bilinear = 1u;
constexpr std::uint32_t MipNone = 0u;
constexpr std::uint32_t MipPoint = 1u;
constexpr std::uint32_t LodZero = 0x80u;
constexpr std::uint32_t LodOne = 0xf2u;
constexpr std::uint32_t MaxLodRange = 0xfffu;

alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) std::array<std::uint32_t, Width> Texels{};

alignas(256) std::array<std::uint32_t, 10> SampleBetweenTexels{
    0x34060084u, 0x7e2802f0u, 0x7e2a02f0u, 0x7e2c0200u, 0xf0900f08u, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u,
    0x80000a03u, 0xbf810000u,
};

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Unorm8_8_8_8 << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        SwizzleXYZW | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

float Run(AgcDriver::VulkanDevice& device, std::uint32_t magFilter, std::uint32_t minFilter, std::uint32_t mipFilter, std::uint32_t maxLod, std::uint32_t lod, const std::string& what) {
    Output.fill(0xdeadbeefu);
    Texels = {0u, 0u, 0xffffffffu, 0xffffffffu};
    SampleBetweenTexels[3] = 0x7e2c0200u | lod;
    std::vector<std::uint32_t> userData(24, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = TextureDescriptor();
    const std::array<std::uint32_t, 4> sampler{ClampLastTexel, maxLod << 12u, (magFilter << 20u) | (minFilter << 22u) | (mipFilter << 26u), 0u};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(SampleBetweenTexels);
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
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto first = std::bit_cast<float>(Output[0]);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto value = std::bit_cast<float>(Output[thread * 4u + component]);
            Require(value == first, what + ": thread " + std::to_string(thread) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", thread 0 red is " + std::to_string(first));
        }
    }
    return first;
}

void RequireBlend(float value, const std::string& what) {
    Require(std::fabs(value - 0.5f) < 0.02f, what + ": sampled " + std::to_string(value) + ", expected the 0.5 blend of the two texels the coordinate lies between");
}

void RequireTexel(float value, const std::string& what) {
    Require(value == 0.0f || value == 1.0f, what + ": sampled " + std::to_string(value) + ", expected one unblended texel");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RequireTexel(Run(*device, Point, Bilinear, MipNone, MaxLodRange, LodZero, "mip filter none at LOD 0"), "point magnification without a mip filter at LOD 0");
        RequireBlend(Run(*device, Bilinear, Point, MipNone, MaxLodRange, LodZero, "mip filter none at LOD 0"), "bilinear magnification without a mip filter at LOD 0");
        RequireBlend(Run(*device, Point, Bilinear, MipPoint, MaxLodRange, LodOne, "point mip filter at LOD 1"), "bilinear minification with a point mip filter at LOD 1");
        RequireTexel(Run(*device, Bilinear, Point, MipPoint, MaxLodRange, LodOne, "point mip filter at LOD 1"), "point minification with a point mip filter at LOD 1");
        RequireBlend(Run(*device, Point, Bilinear, MipNone, MaxLodRange, LodOne, "mip filter none at LOD 1"), "bilinear minification without a mip filter at LOD 1");
        RequireTexel(Run(*device, Bilinear, Point, MipNone, MaxLodRange, LodOne, "mip filter none at LOD 1"), "point minification without a mip filter at LOD 1");
        std::puts("image sample mip filter none tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
