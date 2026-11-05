#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Results = 17;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Levels = 2;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 55> Code{
    0x34060087, 0x7e3c0300, 0x7e3e0280, 0x36420087, 0x7e440280, 0xd5480023, 0x02050700, 0xf0081f08,
    0x00010a1e, 0xf00c1f08, 0x00010e1e, 0xf0081a08, 0x0001121e, 0xf00c1408, 0x0001141e, 0xf0101f08,
    0x00011521, 0xf0141308, 0x00011921, 0xbf8c3f70, 0xe0701000, 0x80000a03, 0xe0701004, 0x80000b03,
    0xe0701008, 0x80000c03, 0xe070100c, 0x80000d03, 0xe0701010, 0x80000e03, 0xe0701014, 0x80000f03,
    0xe0701018, 0x80001003, 0xe070101c, 0x80001103, 0xe0701020, 0x80001203, 0xe0701024, 0x80001303,
    0xe0701028, 0x80001403, 0xe070102c, 0x80001503, 0xe0701030, 0x80001603, 0xe0701034, 0x80001703,
    0xe0701038, 0x80001803, 0xe070103c, 0x80001903, 0xe0701040, 0x80001a03, 0xbf810000,
};

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t bytes;
    bool floating;
};

constexpr std::array<Format, 11> Formats{{
    {"8_UNORM", 1u, 1u, false},
    {"16_UNORM", 7u, 2u, false},
    {"16_SINT", 12u, 2u, false},
    {"8_8_UNORM", 14u, 2u, false},
    {"32_FLOAT", 22u, 4u, true},
    {"16_16_SINT", 28u, 4u, false},
    {"8_8_8_8_UNORM", 56u, 4u, false},
    {"8_8_8_8_SINT", 61u, 4u, false},
    {"32_32_UINT", 62u, 8u, false},
    {"16_16_16_16_UNORM", 65u, 8u, false},
    {"32_32_32_32_FLOAT", 77u, 16u, true},
}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<AgcDriver::Graphics::TileMipLayout> Mips(const Format& format) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, Width, 1u, Levels);
}

std::uint8_t* Texel(const AgcDriver::Graphics::TileMipLayout& mip, const Format& format, std::uint32_t x) {
    return &Texels[mip.tiledOffset + static_cast<std::uint64_t>(x) * format.bytes];
}

void FillTexture(const Format& format) {
    const auto mips = Mips(format);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), std::string("image load packed: the mip chain of ") + format.name + " does not fit the texel storage");
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        for (std::uint32_t x = 0; x < mips[level].width; ++x) {
            auto* texel = Texel(mips[level], format, x);
            for (std::uint32_t byte = 0; byte < format.bytes; ++byte) {
                texel[byte] = static_cast<std::uint8_t>(0x80u + 37u * (x * format.bytes + byte) + 7u * x + 101u * level);
            }
            if (format.floating) {
                for (std::uint32_t word = 0; word < format.bytes / 4u; ++word) {
                    std::uint32_t bits;
                    std::memcpy(&bits, texel + word * 4u, 4u);
                    bits = (bits & 0x807fffffu) | ((0x60u + ((bits >> 23u) & 0x3fu)) << 23u);
                    std::memcpy(texel + word * 4u, &bits, 4u);
                }
            }
        }
    }
}

std::array<std::uint32_t, 4> RawTexel(const Format& format, std::uint32_t level, std::uint32_t x, bool sign) {
    const auto mips = Mips(format);
    std::array<std::uint32_t, 4> words{};
    std::memcpy(words.data(), Texel(mips[level], format, x), format.bytes);
    if (sign && format.bytes < 4u) {
        const auto shift = 32u - format.bytes * 8u;
        words[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(words[0] << shift) >> shift);
    }
    return words;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        swizzle | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t swizzle) {
    Buffer.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data(), format, swizzle);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    const auto result = ShaderRecompiler::Recompile(request);
    static ShaderRecompiler::CompiledShaderArtifact first;
    if (first.variantId == 0u) first = result;
    else Require(result.cacheHit && result.variantId == first.variantId, "runtime image format changed the compiled artifact");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Format& format) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto plain = RawTexel(format, 0u, tid, false);
        const auto sign = RawTexel(format, 0u, tid, true);
        const auto level = (tid >> 3u) & 1u;
        const auto mipPlain = RawTexel(format, level, tid & 7u, false);
        const auto mipSign = RawTexel(format, level, tid & 7u, true);
        const std::array<std::uint32_t, Results> expected{
            plain[0], plain[1], plain[2], plain[3],
            sign[0], sign[1], sign[2], sign[3],
            plain[1], plain[3],
            sign[2],
            mipPlain[0], mipPlain[1], mipPlain[2], mipPlain[3],
            mipSign[0], mipSign[1],
        };
        constexpr std::array<const char*, Results> names{
            "image_load_pck dmask:0xf [0]", "image_load_pck dmask:0xf [1]", "image_load_pck dmask:0xf [2]", "image_load_pck dmask:0xf [3]",
            "image_load_pck_sgn dmask:0xf [0]", "image_load_pck_sgn dmask:0xf [1]", "image_load_pck_sgn dmask:0xf [2]", "image_load_pck_sgn dmask:0xf [3]",
            "image_load_pck dmask:0xa [0]", "image_load_pck dmask:0xa [1]",
            "image_load_pck_sgn dmask:0x4",
            "image_load_mip_pck dmask:0xf [0]", "image_load_mip_pck dmask:0xf [1]", "image_load_mip_pck dmask:0xf [2]", "image_load_mip_pck dmask:0xf [3]",
            "image_load_mip_pck_sgn dmask:0x3 [0]", "image_load_mip_pck_sgn dmask:0x3 [1]",
        };
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Buffer[tid * Words + index];
            Require(actual == expected[index], std::string(format.name) + " " + names[index] + ": thread " + std::to_string(tid) + " is " + Hex(actual) + ", expected " + Hex(expected[index]));
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t swizzle, const std::string& reason, const std::string& what) {
    std::string refusal;
    try {
        Run(device, format, swizzle);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(reason) != std::string::npos, "image_load_pck of " + what + " was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto& format : Formats) {
            FillTexture(format);
            Run(*device, format.format, IdentitySwizzle);
            Check(format);
        }
        FillTexture(Formats[6]);
        RequireRefused(*device, 57u, IdentitySwizzle, "not recoverable", "8_8_8_8_SNORM");
        RequireRefused(*device, 56u, 0xf2eu, "identity swizzle", "a swizzled texture");
        std::puts("image load packed tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
