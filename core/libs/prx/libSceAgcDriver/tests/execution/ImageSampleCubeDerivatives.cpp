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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t ResultWord = 8;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t Faces = 6;
constexpr std::uint32_t Gradients = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t TypeCube = 11;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 65536> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 28> Code{
    0x34020087, 0xe0301000, 0x80000501, 0xe0301004, 0x80000601, 0xe0301008, 0x80000701, 0xe030100c,
    0x80000801, 0xe0301010, 0x80000901, 0xe0301014, 0x80000a01, 0xe0301018, 0x80000b01, 0xbf8c3f70,
    0xf0880f18, 0x00610c05, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

struct Sample {
    std::uint32_t level;
    std::uint32_t face;
    std::uint32_t gradient;
    std::uint32_t x;
    std::uint32_t y;
};

Sample SampleOf(std::uint32_t tid) {
    return {tid % Levels, (tid / Levels) % Faces, (tid / 3u) % Gradients, (tid * 7u + 3u) % Size, (tid * 5u + 1u) % Size};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (TypeCube << 28u),
        Faces - 1u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        auto* words = &Buffer[tid * Words];
        for (std::uint32_t gradient = 0; gradient < Gradients; ++gradient) {
            words[gradient] = Bits(gradient == sample.gradient ? std::exp2(static_cast<float>(sample.level) - 0.25f) / static_cast<float>(Size) : 0.0f);
        }
        words[4] = Bits(1.0f + (static_cast<float>(sample.x) + 0.5f) / static_cast<float>(Size));
        words[5] = Bits(1.0f + (static_cast<float>(sample.y) + 0.5f) / static_cast<float>(Size));
        words[6] = Bits(static_cast<float>(sample.face));
    }
}

void FillTexture() {
    Texels.fill(0xeeu);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(Texels.data())));
    Require(surface.layers == Faces && surface.mips.size() == Levels && surface.guestBytes <= Texels.size(), "image sample cube derivatives: the cube mip chain does not fit the texel storage");
    for (std::uint32_t face = 0; face < Faces; ++face) {
        for (std::uint32_t level = 0; level < Levels; ++level) {
            const auto& mip = surface.mips[level];
            for (std::uint32_t y = 0; y < mip.height; ++y) {
                for (std::uint32_t x = 0; x < mip.width; ++x) {
                    auto* texel = &Texels[surface.GuestLayerOffset(face) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                    texel[0] = static_cast<std::uint8_t>(level);
                    texel[1] = static_cast<std::uint8_t>(face);
                    texel[2] = 0u;
                    texel[3] = 255u;
                }
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

void Run(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data());
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

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const std::array<std::uint32_t, 4> expected{sample.level, sample.face, 0u, 255u};
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float value = std::bit_cast<float>(Buffer[tid * Words + ResultWord + component]) * 255.0f;
            Require(std::lround(value) == static_cast<long>(expected[component]), "image_sample_d cube: thread " + std::to_string(tid) + " (face " + std::to_string(sample.face) + ", level " + std::to_string(sample.level) + ") component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        FillTexture();
        Run(*device);
        Check();
        std::puts("image sample cube derivatives tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
