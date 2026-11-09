#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t TwoChannelSwizzle = 0x22cu;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TexelBytes = 8192;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) constexpr std::array<std::uint32_t, 12> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e,
    0x7e3e0281, 0xf0201508, 0x0001141e, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 12> StoreD16Code{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x8001141e,
    0x7e3e0281, 0xf0201508, 0x8001141e, 0xbf810000,
};

struct Format {
    const char* name;
    std::uint32_t value;
    std::uint32_t components;
};

constexpr std::array<Format, 3> Formats{{
    {"16_FLOAT", 13u, 1u},
    {"16_16_FLOAT", 29u, 2u},
    {"16_16_16_16_FLOAT", 71u, 4u},
}};

struct Write {
    std::uint32_t row;
    std::uint32_t dmask;
};

constexpr std::array<Write, 2> Writes{{{0u, 0xfu}, {1u, 0x5u}}};
constexpr std::array<std::uint32_t, 24> EdgeBits{
    0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3f801000u, 0x3f803000u,
    0x3f800fffu, 0x3f801001u, 0x477fe000u, 0x477ff000u, 0x47800000u, 0xc7800000u,
    0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0x38800000u, 0x33800000u,
    0x33000000u, 0x000116c2u, 0x3eaaaaabu, 0x40490fdbu, 0x00000001u, 0x80000001u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store float rounding: cannot allocate the guest block");
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

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::uint16_t HalfTowardZero(std::uint32_t bits) {
    const auto sign = static_cast<std::uint16_t>((bits >> 16u) & 0x8000u);
    const auto exponent = (bits >> 23u) & 0xffu;
    const auto mantissa = bits & 0x7fffffu;
    if (exponent == 0xffu) {
        auto payload = static_cast<std::uint16_t>(mantissa >> 13u);
        if (mantissa != 0u && payload == 0u) payload = 1u;
        return static_cast<std::uint16_t>(sign | 0x7c00u | payload);
    }
    if (exponent == 0u) return sign;
    const int unbiased = static_cast<int>(exponent) - 127;
    if (unbiased > 15) return static_cast<std::uint16_t>(sign | 0x7bffu);
    if (unbiased < -14) {
        const int shift = -14 - unbiased + 13;
        if (shift >= 32) return sign;
        const auto significand = 0x800000u | mantissa;
        return static_cast<std::uint16_t>(sign | (significand >> static_cast<unsigned>(shift)));
    }
    return static_cast<std::uint16_t>(sign | (static_cast<std::uint16_t>(unbiased + 15) << 10u) | (mantissa >> 13u));
}

std::uint32_t Value(std::uint32_t tid, std::uint32_t component) {
    const auto slot = tid * 4u + component;
    if (slot < EdgeBits.size()) return EdgeBits[slot];
    const auto seed = (slot + 1u) * 0x9e3779b1u;
    const auto mixed = seed ^ (seed >> 16u);
    return (mixed & 0x807fffffu) | (((mixed >> 23u) % 31u + 110u) << 23u);
}

std::uint64_t TexelOffset(const Format& format, std::uint32_t x, std::uint32_t y) {
    const auto mip = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.value, Width, Height, 1u).at(0);
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * format.components * sizeof(std::uint16_t);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, const Format& format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format.value << 20u) | (((Width - 1u) & 3u) << 30u),
            ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), (format.components == 2u ? TwoChannelSwizzle : IdentitySwizzle) | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

std::uint16_t Half(std::uint32_t tid, std::uint32_t component) {
    return HalfTowardZero(Value(tid, component));
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, bool d16) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            Input[tid * 4u + component] = d16 ? component < 2u ? static_cast<std::uint32_t>(Half(tid, component * 2u)) | (static_cast<std::uint32_t>(Half(tid, component * 2u + 1u)) << 16u) : 0u : Value(tid, component);
        }
    }
    const auto& storeCode = d16 ? StoreD16Code : StoreCode;
    std::fill(texels, texels + TexelBytes, 0x5au);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * sizeof(Input[0])));
    const auto texture = TextureDescriptor(texels, format);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(storeCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(storeCode.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(storeCode.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(storeCode.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

std::uint16_t ExpectedComponent(std::uint32_t tid, std::uint32_t dmask, std::uint32_t component) {
    if (((dmask >> component) & 1u) == 0u) return 0u;
    std::uint32_t source = 0u;
    for (std::uint32_t lower = 0u; lower < component; ++lower) source += (dmask >> lower) & 1u;
    return Half(tid, source);
}

void Check(const std::uint8_t* texels, const Format& format, bool d16) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(format, x, y);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                std::uint16_t actual = 0u;
                std::memcpy(&actual, texels + offset + component * sizeof(actual), sizeof(actual));
                const auto dmask = y == Writes[0].row ? Writes[0].dmask : y == Writes[1].row ? Writes[1].dmask : 0u;
                const auto expected = dmask != 0u ? ExpectedComponent(x, dmask, component) : 0x5a5au;
                Require(actual == expected, std::string(d16 ? "D16 image_store of " : "image_store of ") + format.name + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        for (const bool d16 : {false, true}) {
            for (const auto& format : Formats) {
                Store(*device, block.Data(), format, d16);
                Check(block.Data(), format, d16);
            }
        }
        std::puts("image store float rounding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
