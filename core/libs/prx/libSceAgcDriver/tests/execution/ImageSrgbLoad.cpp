#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Groups = Width / Threads;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Unorm8 = 1;
constexpr std::uint32_t Unorm8_8 = 14;
constexpr std::uint32_t Srgb8 = 128;
constexpr std::uint32_t Srgb8_8 = 129;
constexpr std::uint32_t SwizzleXY01 = 0x22cu;
constexpr std::uint32_t SwizzleYX10 = 0x065u;
constexpr std::uint32_t SwizzleXXX1 = 0x324u;
constexpr std::uint32_t One = 0x3f800000u;
constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};
constexpr std::size_t BlockBytes = 65536;

alignas(256) std::array<std::uint32_t, Width * 4> Output{};
alignas(256) std::array<std::uint8_t, Width * 2> Texels{};

constexpr std::array<std::uint32_t, 15> LoadCode(std::uint32_t dmask) {
    return {
        0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x7e3e0280u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
        0x7e1a0280u, 0xf0001008u | (dmask << 8u), 0x00010a1eu, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 15> StoreCode(std::uint32_t dmask) {
    return {
        0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x7e3e0280u, 0x7e140d1eu, 0x4c163cffu, 0x000000ffu, 0x7e160d0bu,
        0x101414ffu, 0x3b808081u, 0x101616ffu, 0x3b808081u, 0xf0201008u | (dmask << 8u), 0x00010a1eu, 0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 9> SamplerCode(std::uint32_t word0) {
    return {
        0x34060084u, 0x7e280280u, 0x7e2a0280u, word0, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
        0xbf810000u,
    };
}

alignas(256) constexpr auto LoadXyzw = LoadCode(0xfu);
alignas(256) constexpr auto LoadXy = LoadCode(0x3u);
alignas(256) constexpr auto StoreX = StoreCode(0x1u);
alignas(256) constexpr auto StoreXy = StoreCode(0x3u);
alignas(256) constexpr auto SampleLz = SamplerCode(0xf09c0f08u);
alignas(256) constexpr auto Gather4Lz = SamplerCode(0xf11c0108u);

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image sRGB load: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Linear(std::uint32_t code) {
    const double encoded = code / 255.0;
    const double linear = encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
    int exponent = 0;
    const double mantissa = std::frexp(linear, &exponent);
    return std::bit_cast<std::uint32_t>(static_cast<float>(std::ldexp(std::nearbyint(mantissa * 256.0), exponent - 8)));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels, std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Fill(std::uint32_t format) {
    for (std::uint32_t x = 0; x < Width; ++x) {
        if (format == Srgb8) {
            Texels[x] = static_cast<std::uint8_t>(x);
        } else {
            Texels[x * 2u] = static_cast<std::uint8_t>(x);
            Texels[x * 2u + 1u] = static_cast<std::uint8_t>(255u - x);
        }
    }
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const void* texels, std::uint32_t format, std::uint32_t swizzle, std::uint32_t groups, bool useCache = false) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto texture = TextureDescriptor(texels, format, swizzle);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(PointSampler.begin(), PointSampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = useCache;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Verify(std::uint32_t dmask, std::uint32_t format, std::uint32_t swizzle, const std::string& what) {
    for (std::uint32_t x = 0; x < Width; ++x) {
        const std::array<std::uint32_t, 4> channels{Linear(x), format == Srgb8 ? 0u : Linear(255u - x), 0u, One};
        std::uint32_t slot = 0;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            if (((dmask >> component) & 1u) == 0u) continue;
            const auto selector = (swizzle >> (component * 3u)) & 7u;
            const auto expected = selector == 0u ? 0u : selector == 1u ? One : channels[selector - 4u];
            const auto actual = Output[x * 4u + slot++];
            Require(actual == expected, what + ": texel " + std::to_string(x) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

void CheckLoad(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t dmask, std::uint32_t format, std::uint32_t swizzle, const std::string& what) {
    Fill(format);
    Run(device, code, Texels.data(), format, swizzle, Groups);
    Verify(dmask, format, swizzle, what);
}

void CheckStoredLoad(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<const std::uint32_t> code, std::uint32_t dmask, std::uint32_t format, std::uint32_t swizzle, const std::string& what) {
    const auto address = reinterpret_cast<std::uintptr_t>(block.Data());
    const auto bytes = Width * (format == Srgb8 ? 1u : 2u);
    std::fill(block.Data(), block.Data() + BlockBytes, std::uint8_t{0x5au});
    Run(device, format == Srgb8 ? std::span<const std::uint32_t>(StoreX) : std::span<const std::uint32_t>(StoreXy), block.Data(), format == Srgb8 ? Unorm8 : Unorm8_8, SwizzleXY01, Groups);
    Require(AgcDriver::Graphics::StorageTexture::FindPending(address, bytes) != nullptr, what + ": the image_store results are not pending in the storage image");
    Run(device, code, block.Data(), format, swizzle, Groups);
    Verify(dmask, format, swizzle, what);
    AgcDriver::Graphics::StorageTexture::FlushPending(address, BlockBytes, nullptr, "test");
    device.WaitIdle();
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& what) {
    Fill(Srgb8_8);
    for (const bool useCache : {false, true, true}) {
        std::string refusal;
        try {
            Run(device, code, Texels.data(), Srgb8_8, SwizzleXY01, 1, useCache);
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find("samples or gathers an sRGB image the device cannot sample") != std::string::npos, what + " of an 8_8_SRGB image read through its UNORM view was not refused: " + refusal);
    }
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_SRGB_SHADER_DECODE", "1");
#else
        setenv("APS5_SRGB_SHADER_DECODE", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckLoad(*device, LoadXyzw, 0xfu, Srgb8_8, SwizzleXY01, "8_8_SRGB image_load dmask:0xf X Y 0 1");
        CheckLoad(*device, LoadXyzw, 0xfu, Srgb8_8, SwizzleYX10, "8_8_SRGB image_load dmask:0xf Y X 1 0");
        CheckLoad(*device, LoadXy, 0x3u, Srgb8_8, SwizzleXY01, "8_8_SRGB image_load dmask:0x3 X Y 0 1");
        CheckLoad(*device, LoadXyzw, 0xfu, Srgb8, SwizzleXXX1, "8_SRGB image_load dmask:0xf X X X 1");
        GuestBlock block;
        CheckStoredLoad(*device, block, LoadXy, 0x3u, Srgb8_8, SwizzleXY01, "8_8_SRGB image_load dmask:0x3 X Y 0 1 of 8_8_UNORM image_stores");
        CheckStoredLoad(*device, block, LoadXyzw, 0xfu, Srgb8_8, SwizzleYX10, "8_8_SRGB image_load dmask:0xf Y X 1 0 of 8_8_UNORM image_stores");
        CheckStoredLoad(*device, block, LoadXyzw, 0xfu, Srgb8, SwizzleXXX1, "8_SRGB image_load dmask:0xf X X X 1 of 8_UNORM image_stores");
        RequireRefused(*device, SampleLz, "image_sample_lz");
        RequireRefused(*device, Gather4Lz, "image_gather4_lz");
        std::puts("image sRGB load tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
