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
constexpr std::uint32_t RedWord = 8;
constexpr std::uint32_t GreenWord = 12;
constexpr std::uint32_t BlueWord = 16;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Faces = 12;
constexpr std::uint32_t BaseFace = 6;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t TypeCube = 11;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 65536> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 38> Code{
    0x34020087, 0xe0301004, 0x80000201, 0xe0301008, 0x80000301, 0xbf8c3f70, 0xf11c0108, 0x00610602,
    0xf11c0208, 0x00610a02, 0xf1000408, 0x00610e02, 0xbf8c3f70, 0xe0701020, 0x80000601, 0xe0701024,
    0x80000701, 0xe0701028, 0x80000801, 0xe070102c, 0x80000901, 0xe0701030, 0x80000a01, 0xe0701034,
    0x80000b01, 0xe0701038, 0x80000c01, 0xe070103c, 0x80000d01, 0xe0701040, 0x80000e01, 0xe0701044,
    0x80000f01, 0xe0701048, 0x80001001, 0xe070104c, 0x80001101, 0xbf810000,
};

struct Sample {
    std::uint32_t x;
    std::uint32_t y;
};

Sample SampleOf(std::uint32_t tid) {
    return {1u + (tid * 7u) % 13u, 1u + (tid * 5u) % 13u};
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
        0xfacu | (TypeCube << 28u),
        (Faces - 1u) | (BaseFace << 16u),
        0u,
        0u,
        0u,
    };
}

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        auto* words = &Buffer[tid * Words];
        words[1] = Bits(1.0f + (static_cast<float>(sample.x) + 0.5f) / static_cast<float>(Size));
        words[2] = Bits(1.0f + (static_cast<float>(sample.y) + 0.5f) / static_cast<float>(Size));
    }
}

void FillTexture() {
    Texels.fill(0xeeu);
    auto descriptor = TextureDescriptor(Texels.data());
    descriptor[4] = Faces - 1u;
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(surface.layers == Faces && surface.mips.size() == 1u && surface.guestBytes <= Texels.size(), "image gather face on cube: the cube array does not fit the texel storage");
    const auto& mip = surface.mips[0];
    for (std::uint32_t face = 0; face < Faces; ++face) {
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                auto* texel = &Texels[surface.GuestLayerOffset(face) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = static_cast<std::uint8_t>(x);
                texel[1] = static_cast<std::uint8_t>(y);
                texel[2] = static_cast<std::uint8_t>(face);
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
    return {1u << 28u, 0xfffu << 12u, 0u, 0u};
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

long Unorm(std::uint32_t tid, std::uint32_t word) {
    return std::lround(std::bit_cast<float>(Buffer[tid * Words + word]) * 255.0f);
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto x = static_cast<long>(sample.x);
        const auto y = static_cast<long>(sample.y);
        const std::array<long, 4> red{x, x + 1, x + 1, x};
        const std::array<long, 4> green{y + 1, y + 1, y, y};
        const auto where = "thread " + std::to_string(tid) + " (texel " + std::to_string(sample.x) + "," + std::to_string(sample.y) + ")";
        for (std::uint32_t component = 0; component < 4u; ++component) {
            Require(Unorm(tid, RedWord + component) == red[component], "image_gather4_lz red with DIM 2D on a cube T#: " + where + " texel " + std::to_string(component) + " is " + std::to_string(Unorm(tid, RedWord + component)));
            Require(Unorm(tid, GreenWord + component) == green[component], "image_gather4_lz green with DIM 2D on a cube T#: " + where + " texel " + std::to_string(component) + " is " + std::to_string(Unorm(tid, GreenWord + component)));
            Require(Unorm(tid, BlueWord + component) == static_cast<long>(BaseFace), "image_gather4 with DIM 2D on a cube T# did not read the BASE_ARRAY face: " + where + " texel " + std::to_string(component) + " is " + std::to_string(Unorm(tid, BlueWord + component)));
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
        std::puts("image gather face on cube tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
