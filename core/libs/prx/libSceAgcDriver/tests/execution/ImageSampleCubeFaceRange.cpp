#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 8;
constexpr std::uint32_t ResultWord = 4;
constexpr std::uint32_t Size = 64;
constexpr std::uint32_t Slices = 18;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TypeCube = 11;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, Size * Size * 4 * Slices> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> SampleCode{
    0x34020085, 0xe03c1000, 0x80000201, 0xbf8c3f70, 0xf09c0f18, 0x00610502, 0xbf8c3f70, 0xe0781010, 0x80000501, 0xbf810000,
};

constexpr std::array<float, Threads> Faces{
    0.0f, 1.0f, 5.0f, 5.6f, 6.0f, 7.0f, 7.5f, 8.0f, 9.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 24.0f, 40.0f,
    -0.4f, -0.6f, -1.0f, -2.0f, -8.0f, -9.0f, 1.0e9f, -1.0e9f, 2.5f, 4.5f, 10.0f, 11.0f, 12.0f, 0.5f, 5.4f, 13.5f,
};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t baseArray, std::uint32_t depth) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (TypeCube << 28u),
        depth | (baseArray << 16u),
        0u,
        0u,
        0u,
    };
}

void FillTexture() {
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(0u, Slices - 1u)));
    Require(surface.layers == Slices && surface.guestBytes <= Texels.size(), "image sample cube face range: the slices do not fit the texel storage");
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        const auto value = Bits(static_cast<float>(slice + 1u));
        for (std::uint64_t offset = 0; offset < surface.layerBytes; offset += 4u) {
            std::copy_n(reinterpret_cast<const std::uint8_t*>(&value), 4u, &Texels[surface.GuestLayerOffset(slice) + offset]);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> BilinearSampler() {
    return {0u, 0xfffu << 12u, (1u << 20u) | (1u << 22u) | (1u << 26u), 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t baseArray, std::uint32_t depth) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(baseArray, depth);
    const auto sampler = BilinearSampler();
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

std::uint32_t ExpectedSlice(float face, std::uint32_t baseArray, std::uint32_t depth) {
    const auto guest = static_cast<std::int32_t>(face);
    const auto cubes = static_cast<std::int32_t>((depth + 1u - baseArray) / 6u);
    const auto cube = std::clamp(guest >> 3, 0, cubes - 1);
    return baseArray + static_cast<std::uint32_t>(cube * 6 + std::min(guest & 7, 5));
}

void CheckFaces(AgcDriver::VulkanDevice& device, std::uint32_t baseArray, std::uint32_t depth) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Buffer[tid * Words] = Bits(1.5f);
        Buffer[tid * Words + 1u] = Bits(1.5f);
        Buffer[tid * Words + 2u] = Bits(Faces[tid]);
    }
    Run(device, SampleCode, baseArray, depth);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float value = std::bit_cast<float>(Buffer[tid * Words + ResultWord]);
        const auto slice = ExpectedSlice(Faces[tid], baseArray, depth);
        const float expected = static_cast<float>(slice + 1u);
        Require(value == expected, "image_sample_lz cube with BASE_ARRAY " + std::to_string(baseArray) + " and DEPTH " + std::to_string(depth) + ": face coordinate " + std::to_string(Faces[tid]) + " read " + std::to_string(value) + ", expected slice " + std::to_string(slice) + " (" + std::to_string(expected) + ")");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
        for (const auto [baseArray, depth] : {std::pair{0u, 5u}, std::pair{0u, 11u}, std::pair{6u, 11u}, std::pair{6u, 17u}, std::pair{0u, 17u}}) CheckFaces(*device, baseArray, depth);
        std::puts("image sample cube face range tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
