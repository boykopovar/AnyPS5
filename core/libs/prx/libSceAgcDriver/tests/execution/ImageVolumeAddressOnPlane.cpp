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
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t Depth = 2;
constexpr std::uint32_t SampleResult = 8;
constexpr std::uint32_t LoadResult = 12;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Plane{};
alignas(256) std::array<std::uint8_t, 16384> Volume{};

alignas(256) constexpr std::array<std::uint32_t, 36> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf09c0f10, 0x00610c02,
    0xf0001f10, 0x00011005, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038,
    0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> ExplicitLodCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xbf8c3f70, 0xf0900f10, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xbf810000,
};

constexpr std::array<float, 8> PlaneR{0.0f, 0.5f, 1.0f, 1.5f, -0.25f, 3.0f, 0.25f, 0.75f};
constexpr std::array<std::uint32_t, 8> PlaneZ{0u, 1u, 0u, 2u, 0xffffffffu, 4u, 0u, 3u};

struct Lane {
    std::uint32_t x;
    std::uint32_t y;
};

Lane LaneOf(std::uint32_t tid) {
    return {(tid * 7u + 3u) % Size, (tid * 5u + 1u) % Size};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void FillInput(bool volume) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid);
        const auto u = Bits((static_cast<float>(lane.x) + 0.5f) / static_cast<float>(Size));
        const auto v = Bits((static_cast<float>(lane.y) + 0.5f) / static_cast<float>(Size));
        const auto r = volume ? Bits((static_cast<float>(tid % Depth) + 0.5f) / static_cast<float>(Depth)) : Bits(PlaneR[tid % PlaneR.size()]);
        const auto z = volume ? tid % Depth : PlaneZ[tid % PlaneZ.size()];
        const std::array<std::uint32_t, 6> words{u, v, r, lane.x, lane.y, z};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

std::uint8_t SliceMarker(std::uint32_t z) {
    return static_cast<std::uint8_t>(z * 100u + 7u);
}

std::array<std::uint32_t, 8> PlaneDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Plane.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 8> VolumeDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Volume.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (Type3D << 28u),
        Depth - 1u,
        0u,
        0u,
        0u,
    };
}

void FillTexture(std::span<std::uint8_t> storage, const std::array<std::uint32_t, 8>& descriptor, std::uint32_t slices) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(geometry.guestBytes <= storage.size(), "image volume address on a plane: the texture does not fit the texel storage");
    const auto& mip = geometry.mips.at(0);
    std::fill(storage.begin(), storage.end(), 0xeeu);
    for (std::uint32_t z = 0; z < slices; ++z) {
        for (std::uint32_t y = 0; y < Size; ++y) {
            for (std::uint32_t x = 0; x < Size; ++x) {
                auto* texel = &storage[geometry.GuestLayerOffset(z) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = SliceMarker(z);
                texel[1] = static_cast<std::uint8_t>(x);
                texel[2] = static_cast<std::uint8_t>(y);
                texel[3] = 255u;
            }
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
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

void Expect(std::uint32_t tid, std::uint32_t result, const std::array<std::uint32_t, 4>& expected, const std::string& name) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float value = std::bit_cast<float>(Buffer[tid * Words + result + component]) * 255.0f;
        Require(std::lround(value) == static_cast<long>(expected[component]), name + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
    }
}

void CheckPlane() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid);
        const std::array<std::uint32_t, 4> texel{SliceMarker(0), lane.x, lane.y, 255u};
        const auto z = PlaneZ[tid % PlaneZ.size()];
        Expect(tid, SampleResult, texel, "image_sample_lz 3d on a 2D texture with r " + std::to_string(PlaneR[tid % PlaneR.size()]));
        Expect(tid, LoadResult, z == 0u ? texel : std::array<std::uint32_t, 4>{}, "image_load 3d on a 2D texture with z " + std::to_string(static_cast<std::int32_t>(z)));
    }
}

void CheckVolume() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid);
        const std::array<std::uint32_t, 4> texel{SliceMarker(tid % Depth), lane.x, lane.y, 255u};
        Expect(tid, SampleResult, texel, "image_sample_lz 3d on a 3D texture");
        Expect(tid, LoadResult, texel, "image_load 3d on a 3D texture");
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device) {
    std::string refusal;
    try {
        Run(device, ExplicitLodCode, PlaneDescriptor());
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("incompatible with the static runtime image interface") != std::string::npos, "image_sample_l 3d on a 2D texture was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture(Plane, PlaneDescriptor(), 1u);
        FillInput(false);
        Run(*device, Code, PlaneDescriptor());
        CheckPlane();
        FillTexture(Volume, VolumeDescriptor(), Depth);
        FillInput(true);
        Run(*device, Code, VolumeDescriptor());
        CheckVolume();
        RequireRefused(*device);
        std::puts("image volume address on plane tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
