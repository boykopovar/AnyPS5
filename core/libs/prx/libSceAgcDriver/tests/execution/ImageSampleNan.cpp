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
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Size = 4;
constexpr std::uint32_t Float32 = 22;
constexpr std::uint32_t Float16 = 13;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Result = 8;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t CanonicalNan = 0xffc00000u;
constexpr std::array<std::uint32_t, 4> PointSampler{0u, 0xfffu << 12u, 1u << 26u, 0u};
constexpr std::array<std::uint32_t, 4> BilinearSampler{0u, 0xfffu << 12u, (1u << 20u) | (1u << 22u), 0u};
constexpr std::array<const char*, 3> SampleNames{"image_sample_lz", "image_sample_l", "image_sample"};
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 30> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xbf8c3f70, 0xf09c0108, 0x00610c02, 0xf0900108, 0x00610d02,
    0xf0800108, 0x00610e02, 0xf0001108, 0x00010f05, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

constexpr std::array<std::uint32_t, Size * Size> Float32Texels{
    0x7f800001u, 0xff812345u, 0x7fc00000u, 0x3f800000u,
    0xffffffffu, 0x40000000u, 0x7f800000u, 0xff800000u,
    0x3f800000u, 0x7fbfffffu, 0x40400000u, 0x40800000u,
    0xc0000000u, 0x7f800000u, 0x3e800000u, 0xffc00001u,
};

constexpr std::array<std::uint16_t, Size * Size> Float16Texels{
    0x7c01u, 0xfdeeu, 0x7e00u, 0x3c00u,
    0xffffu, 0x4000u, 0x7c00u, 0xfc00u,
    0x3c00u, 0x7dffu, 0x4200u, 0x4400u,
    0xc000u, 0x7c00u, 0x3400u, 0xfe01u,
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool IsNan32(std::uint32_t bits) {
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0u;
}

bool IsNan16(std::uint16_t bits) {
    return (bits & 0x7c00u) == 0x7c00u && (bits & 0x03ffu) != 0u;
}

float Half(std::uint16_t bits) {
    const auto exponent = (bits >> 10u) & 31u;
    const auto mantissa = bits & 0x3ffu;
    const float sign = (bits & 0x8000u) != 0u ? -1.0f : 1.0f;
    if (exponent == 31u) return mantissa == 0u ? sign * INFINITY : NAN;
    if (exponent == 0u) return sign * std::ldexp(static_cast<float>(mantissa), -24);
    return sign * std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, static_cast<int>(exponent) - 15);
}

std::array<std::uint32_t, 8> Descriptor(std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        SwizzleXYZW | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Fill(const std::array<std::uint32_t, 8>& descriptor, std::uint32_t texelBytes, const void* values) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(geometry.guestBytes <= Texels.size(), "image sample NaN: the texture does not fit the texel storage");
    std::fill(Texels.begin(), Texels.end(), 0u);
    const auto& mip = geometry.mips[0];
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            std::memcpy(&Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * texelBytes], static_cast<const std::uint8_t*>(values) + (y * Size + x) * texelBytes, texelBytes);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Input(float shift) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = tid % Size;
        const auto y = (tid / Size) % Size;
        const float u = (static_cast<float>(x) + 0.5f + shift) / static_cast<float>(Size);
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(Size);
        const std::array<std::uint32_t, 5> words{std::bit_cast<std::uint32_t>(u), std::bit_cast<std::uint32_t>(v), 0u, x, y};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

void Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& sampler) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

std::uint32_t Sampled(std::uint32_t tid, std::uint32_t op) {
    return Buffer[tid * Words + Result + op];
}

void CheckPoint32(AgcDriver::VulkanDevice& device) {
    const auto texture = Descriptor(Float32);
    Fill(texture, 4u, Float32Texels.data());
    Input(0.0f);
    Run(device, texture, PointSampler);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto texel = Float32Texels[(tid / Size) % Size * Size + tid % Size];
        const auto expected = IsNan32(texel) ? CanonicalNan : texel;
        for (std::uint32_t op = 0; op < SampleNames.size(); ++op) {
            Require(Sampled(tid, op) == expected, std::string("image sample NaN: point ") + SampleNames[op] + " of 32_FLOAT texel " + Hex(texel) + " (thread " + std::to_string(tid) + ") is " + Hex(Sampled(tid, op)) + ", expected " + Hex(expected));
        }
        Require(Sampled(tid, 3u) == texel, "image sample NaN: image_load of 32_FLOAT texel " + Hex(texel) + " (thread " + std::to_string(tid) + ") is " + Hex(Sampled(tid, 3u)) + ", expected the texel's bits");
    }
}

void CheckPoint16(AgcDriver::VulkanDevice& device) {
    const auto texture = Descriptor(Float16);
    Fill(texture, 2u, Float16Texels.data());
    Input(0.0f);
    Run(device, texture, PointSampler);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto texel = Float16Texels[(tid / Size) % Size * Size + tid % Size];
        const auto expected = IsNan16(texel) ? CanonicalNan : std::bit_cast<std::uint32_t>(Half(texel));
        for (std::uint32_t op = 0; op < SampleNames.size(); ++op) {
            Require(Sampled(tid, op) == expected, std::string("image sample NaN: point ") + SampleNames[op] + " of 16_FLOAT texel " + Hex(texel) + " (thread " + std::to_string(tid) + ") is " + Hex(Sampled(tid, op)) + ", expected " + Hex(expected));
        }
    }
}

void CheckBilinear32(AgcDriver::VulkanDevice& device) {
    const auto texture = Descriptor(Float32);
    Fill(texture, 4u, Float32Texels.data());
    Input(0.5f);
    Run(device, texture, BilinearSampler);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = tid % Size;
        const auto y = (tid / Size) % Size;
        const auto left = Float32Texels[y * Size + x];
        const auto right = Float32Texels[y * Size + (x + 1u) % Size];
        const float blend = 0.5f * std::bit_cast<float>(left) + 0.5f * std::bit_cast<float>(right);
        if (!std::isnan(blend)) continue;
        for (std::uint32_t op = 0; op < SampleNames.size(); ++op) {
            Require(Sampled(tid, op) == CanonicalNan, std::string("image sample NaN: bilinear ") + SampleNames[op] + " between 32_FLOAT texels " + Hex(left) + " and " + Hex(right) + " (thread " + std::to_string(tid) + ") is " + Hex(Sampled(tid, op)) + ", expected " + Hex(CanonicalNan));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckPoint32(*device);
        CheckPoint16(*device);
        CheckBilinear32(*device);
        std::puts("image sample NaN tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
