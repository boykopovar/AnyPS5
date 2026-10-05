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
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ArrayInput = 0;
constexpr std::uint32_t ArrayResult = 8;
constexpr std::uint32_t PlainInput = 16;
constexpr std::uint32_t PlainResult = 24;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 71> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xe0301018, 0x80000801, 0xe030101c,
    0x80000901, 0xe0301040, 0x80001001, 0xe0301044, 0x80001101, 0xe0301048, 0x80001201, 0xe030104c,
    0x80001301, 0xe0301050, 0x80001401, 0xe0301054, 0x80001501, 0xbf8c3f70, 0xf0900f28, 0x00610c02,
    0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01,
    0xf0041f28, 0x00010c06, 0xe0701030, 0x80000c01, 0xe0701034, 0x80000d01, 0xe0701038, 0x80000e01,
    0xe070103c, 0x80000f01, 0xf0900f08, 0x00610c10, 0xe0701060, 0x80000c01, 0xe0701064, 0x80000d01,
    0xe0701068, 0x80000e01, 0xe070106c, 0x80000f01, 0xf0041f08, 0x00010c13, 0xe0701070, 0x80000c01,
    0xe0701074, 0x80000d01, 0xe0701078, 0x80000e01, 0xe070107c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> NarrowCode{
    0x34020087, 0xe0301040, 0x80001001, 0xe0301044, 0x80001101, 0xbf8c3f70, 0xf0900f00, 0x00610c10,
    0xe0701070, 0x80000c01, 0xbf810000,
};

struct Sample {
    std::uint32_t level;
    std::uint32_t x;
    std::uint32_t y;
};

Sample SampleOf(std::uint32_t tid) {
    return {tid % Levels, (tid * 7u + 3u) % Size, (tid * 5u + 1u) % Size};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto u = Bits((static_cast<float>(sample.x) + 0.5f) / static_cast<float>(Size));
        const auto v = Bits((static_cast<float>(sample.y) + 0.5f) / static_cast<float>(Size));
        const auto lod = Bits(static_cast<float>(sample.level));
        const auto x = sample.x >> sample.level;
        const auto y = sample.y >> sample.level;
        auto* words = &Buffer[tid * Words];
        const std::array<std::uint32_t, 8> array{u, v, Bits(0.0f), lod, x, y, 0u, sample.level};
        const std::array<std::uint32_t, 6> plain{u, v, lod, x, y, sample.level};
        std::copy(array.begin(), array.end(), words + ArrayInput);
        std::copy(plain.begin(), plain.end(), words + PlainInput);
    }
}

void FillTexture() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Size, Size, Levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "image address dimension: the mip chain does not fit the texel storage");
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = mips[level];
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                auto* texel = &Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = static_cast<std::uint8_t>(level);
                texel[1] = static_cast<std::uint8_t>(x);
                texel[2] = static_cast<std::uint8_t>(y);
                texel[3] = 255u;
            }
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
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

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data());
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

void Check() {
    constexpr std::array<const char*, 4> names{"image_sample_l 2d_array", "image_load_mip 2d_array", "image_sample_l 2d", "image_load_mip 2d"};
    constexpr std::array<std::uint32_t, 4> results{ArrayResult, ArrayResult + 4u, PlainResult, PlainResult + 4u};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const std::array<std::uint32_t, 4> expected{sample.level, sample.x >> sample.level, sample.y >> sample.level, 255u};
        for (std::uint32_t index = 0; index < names.size(); ++index) {
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const float value = std::bit_cast<float>(Buffer[tid * Words + results[index] + component]) * 255.0f;
                Require(std::lround(value) == static_cast<long>(expected[component]), std::string(names[index]) + " on a 2D texture: thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device) {
    std::string refusal;
    try {
        Run(device, NarrowCode);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("too few coordinate components") != std::string::npos, "image_sample_l 1d on a 2D texture was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        FillTexture();
        Run(*device, Code);
        Check();
        RequireRefused(*device);
        std::puts("image address dimension tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
