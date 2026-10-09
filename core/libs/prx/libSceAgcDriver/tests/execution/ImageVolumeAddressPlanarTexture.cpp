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
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Layers = 2;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t SampleInput = 0;
constexpr std::uint32_t LoadInput = 4;
constexpr std::uint32_t SampleResult = 8;
constexpr std::uint32_t LoadResult = 12;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 39> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0xf0900f10, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xf0001f10, 0x00010c06, 0xbf8c3f70, 0xe0701030, 0x80000c01,
    0xe0701034, 0x80000d01, 0xe0701038, 0x80000e01, 0xe070103c, 0x80000f01, 0xbf810000,
};

struct Lane {
    std::uint32_t x;
    std::uint32_t y;
    float r;
    std::uint32_t z;
    std::uint32_t sampledLayer;
    bool loadInside;
};

constexpr std::array<float, 6> PlanarR{0.0f, 0.5f, 1.0f, 1.5f, -0.25f, 3.0f};
constexpr std::array<std::uint32_t, 8> PlanarZ{0u, 1u, 0u, 4u, 0u, 0xffffffffu, 0u, 2u};
constexpr std::array<float, 6> ArrayR{0.0f, 1.0f, 3.0f, -0.25f, 0.75f, 1.2f};
constexpr std::array<std::uint32_t, 6> ArrayLayer{0u, 1u, 1u, 0u, 1u, 1u};

Lane LaneOf(std::uint32_t tid, bool array) {
    const auto x = (tid * 7u + 3u) % Size;
    const auto y = (tid * 5u + 1u) % Size;
    if (array) return {x, y, ArrayR[tid % ArrayR.size()], tid % Layers, ArrayLayer[tid % ArrayLayer.size()], true};
    const auto z = PlanarZ[tid % PlanarZ.size()];
    return {x, y, PlanarR[tid % PlanarR.size()], z, 0u, z == 0u};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint8_t LayerMarker(std::uint32_t layer) {
    return static_cast<std::uint8_t>(layer * 100u + 7u);
}

void FillInput(bool array) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid, array);
        auto* words = &Buffer[tid * Words];
        const std::array<std::uint32_t, 4> sample{Bits((static_cast<float>(lane.x) + 0.5f) / static_cast<float>(Size)), Bits((static_cast<float>(lane.y) + 0.5f) / static_cast<float>(Size)), Bits(lane.r), Bits(0.0f)};
        const std::array<std::uint32_t, 3> load{lane.x, lane.y, lane.z};
        std::copy(sample.begin(), sample.end(), words + SampleInput);
        std::copy(load.begin(), load.end(), words + LoadInput);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(bool array) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((array ? Type2DArray : Type2D) << 28u),
        array ? Layers - 1u : 0u,
        0u,
        0u,
        0u,
    };
}

void FillTexture(bool array) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(array)));
    Require(geometry.guestBytes <= Texels.size(), "volume address on a planar texture: the texture does not fit the texel storage");
    const auto& mip = geometry.mips.at(0);
    Texels.fill(0xeeu);
    for (std::uint32_t layer = 0; layer < (array ? Layers : 1u); ++layer) {
        for (std::uint32_t y = 0; y < Size; ++y) {
            for (std::uint32_t x = 0; x < Size; ++x) {
                auto* texel = &Texels[geometry.GuestLayerOffset(layer) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = LayerMarker(layer);
                texel[1] = static_cast<std::uint8_t>(x);
                texel[2] = static_cast<std::uint8_t>(y);
                texel[3] = 255u;
            }
        }
    }
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device, bool array) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(array);
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
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
}

void Check(bool array) {
    const std::string texture = array ? "a 2D-array texture" : "a 2D texture";
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid, array);
        const std::array<std::uint32_t, 4> sampled{LayerMarker(lane.sampledLayer), lane.x, lane.y, 255u};
        const std::array<std::uint32_t, 4> loaded = lane.loadInside ? std::array<std::uint32_t, 4>{LayerMarker(lane.z), lane.x, lane.y, 255u} : std::array<std::uint32_t, 4>{0u, 0u, 0u, 0u};
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float sample = std::bit_cast<float>(Buffer[tid * Words + SampleResult + component]) * 255.0f;
            Require(std::lround(sample) == static_cast<long>(sampled[component]), "image_sample_l 3d on " + texture + ": thread " + std::to_string(tid) + " (r " + std::to_string(lane.r) + ") component " + std::to_string(component) + " is " + std::to_string(sample) + ", expected " + std::to_string(sampled[component]));
            const float load = std::bit_cast<float>(Buffer[tid * Words + LoadResult + component]) * 255.0f;
            Require(std::lround(load) == static_cast<long>(loaded[component]), "image_load 3d on " + texture + ": thread " + std::to_string(tid) + " (z " + std::to_string(lane.z) + ") component " + std::to_string(component) + " is " + std::to_string(load) + ", expected " + std::to_string(loaded[component]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool array : {false, true}) {
            FillInput(array);
            FillTexture(array);
            Run(*device, array);
            Check(array);
        }
        std::puts("volume address on planar texture tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
