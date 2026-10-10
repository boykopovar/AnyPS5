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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t ReversedSwizzle = 0x977u;
constexpr std::size_t TexelBytes = 8192;
constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 24> StoreCode{
    0x4a500081, 0xd5690014, 0x0201ff28, 0x9e3779b1, 0x4a2a28ff, 0x7f4a7c15, 0x4a2c28ff, 0xfe94f82a,
    0x4a2e28ff, 0x7ddf743f, 0x7e3c0300, 0x7e3e0280, 0xf0281f08, 0x0001141e, 0x7e3e0281, 0xf0281508,
    0x0001141e, 0x7e3e0282, 0xf0281a08, 0x0001141e, 0x7e3e0283, 0xf0281808, 0x0001141e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> MipCode{
    0x4a500081, 0xd5690014, 0x0201ff28, 0x9e3779b1, 0x4a2a28ff, 0x7f4a7c15, 0x4a2c28ff, 0xfe94f82a,
    0x4a2e28ff, 0x7ddf743f, 0x2c3c0081, 0x36400081, 0x34424081, 0x4c3e4284, 0xf02c1b08, 0x0001141e,
    0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store packed: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
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

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t bytes;
};

constexpr std::array<Format, 10> Formats{{
    {"8_UINT", 5u, 1u},
    {"16_UINT", 11u, 2u},
    {"8_8_UINT", 18u, 2u},
    {"32_SINT", 21u, 4u},
    {"32_FLOAT", 22u, 4u},
    {"16_16_UINT", 27u, 4u},
    {"8_8_8_8_UINT", 60u, 4u},
    {"32_32_UINT", 62u, 8u},
    {"16_16_16_16_UINT", 69u, 8u},
    {"32_32_32_32_FLOAT", 77u, 16u},
}};

std::uint32_t Levels(bool mip) {
    return mip ? 2u : 1u;
}

std::vector<AgcDriver::Graphics::TileMipLayout> Mips(const Format& format, bool mip) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, Width, Height, Levels(mip));
}

std::uint64_t TexelOffset(const AgcDriver::Graphics::TileMipLayout& mip, const Format& format, std::uint32_t x, std::uint32_t y) {
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * format.bytes;
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> bytes(TexelBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(0x80u + 37u * index + 11u * (index >> 4u));
    }
    return bytes;
}

std::uint32_t Data(std::uint32_t tid, std::uint32_t index) {
    return (tid + 1u) * 0x9e3779b1u + index * 0x7f4a7c15u;
}

void Store(std::vector<std::uint8_t>& bytes, const AgcDriver::Graphics::TileMipLayout& mip, const Format& format, std::uint32_t x, std::uint32_t y, std::uint32_t dmask, std::uint32_t tid) {
    std::array<std::uint32_t, 4> words{};
    std::uint32_t next = 0;
    for (std::uint32_t word = 0; word < 4u; ++word) {
        if (((dmask >> word) & 1u) != 0u) {
            words[word] = Data(tid, next++);
        }
    }
    std::memcpy(bytes.data() + TexelOffset(mip, format, x, y), words.data(), format.bytes);
}

std::vector<std::uint8_t> Expected(const Format& format, bool mip) {
    const auto mips = Mips(format, mip);
    auto bytes = Initial();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        if (mip) {
            const auto level = tid & 1u;
            Store(bytes, mips[level], format, tid >> 1u, 4u - 2u * level, 0xbu, tid);
            continue;
        }
        constexpr std::array<std::uint32_t, 4> Dmasks{0xfu, 0x5u, 0xau, 0x8u};
        for (std::uint32_t row = 0; row < Dmasks.size(); ++row) {
            Store(bytes, mips[0], format, tid, row, Dmasks[row], tid);
        }
    }
    return bytes;
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle, std::uint32_t levels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        swizzle | ((levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (levels - 1u) << 4u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::span<const std::uint32_t> code, const Format& format, std::uint32_t swizzle, bool mip) {
    const auto mips = Mips(format, mip);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= TexelBytes, std::string("image store packed: the mip chain of ") + format.name + " does not fit the texel storage");
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, format.format, swizzle, Levels(mip));
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    const auto result = ShaderRecompiler::Recompile(request);
    static std::map<std::uintptr_t, ShaderRecompiler::CompiledShaderArtifact> artifacts;
    auto& first = artifacts[reinterpret_cast<std::uintptr_t>(code.data())];
    if (first.variantId == 0u) first = result;
    else Require(result.cacheHit && result.variantId == first.variantId, "runtime storage image changed the compiled artifact");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void Check(const std::uint8_t* texels, const Format& format, bool mip, const std::string& what) {
    const auto expected = Expected(format, mip);
    const auto mips = Mips(format, mip);
    for (std::uint32_t level = 0; level < mips.size(); ++level) {
        for (std::uint32_t y = 0; y < mips[level].height; ++y) {
            for (std::uint32_t x = 0; x < mips[level].width; ++x) {
                const auto offset = TexelOffset(mips[level], format, x, y);
                const auto chunk = std::min<std::uint32_t>(format.bytes, 4u);
                for (std::uint32_t word = 0; word < format.bytes / chunk; ++word) {
                    std::uint32_t actual = 0;
                    std::uint32_t wanted = 0;
                    std::memcpy(&actual, texels + offset + word * chunk, chunk);
                    std::memcpy(&wanted, expected.data() + offset + word * chunk, chunk);
                    if (actual != wanted) {
                        char message[200];
                        std::snprintf(message, sizeof(message), "%s %s: level %u texel (%u, %u) dword %u is 0x%08x, expected 0x%08x", what.c_str(), format.name, level, x, y, word, actual, wanted);
                        Testing::Fail(message);
                    }
                }
            }
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, const std::string& reason) {
    std::string refusal;
    try {
        Run(device, texels, StoreCode, format, IdentitySwizzle, false);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(reason) != std::string::npos, std::string("image_store_pck of ") + format.name + " was not refused: " + refusal);
}


const Testing::Case storePackedFormats{"ImageStorePacked_EachUintAndFloatFormat_WritesMaskedComponents", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock block;
    for (const auto& format : Formats) {
        Run(device, block.Data(), StoreCode, format, IdentitySwizzle, false);
        Check(block.Data(), format, false, "image_store_pck");
    }
}};

const Testing::Case storePackedSwizzled{"ImageStorePacked_SwizzledDescriptor_WritesUnswizzledComponents", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock block;
    Run(device, block.Data(), StoreCode, Formats[6], ReversedSwizzle, false);
    Check(block.Data(), Formats[6], false, "image_store_pck with a swizzled descriptor");
}};

const Testing::Case storeMipPacked{"ImageStorePacked_MipStore_WritesSelectedLevel", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock block;
    for (const auto& format : {Formats[6], Formats[9]}) {
        Run(device, block.Data(), MipCode, format, IdentitySwizzle, true);
        Check(block.Data(), format, true, "image_store_mip_pck");
    }
}};

const Testing::Case unormRefused{"ImageStorePacked_UnormFormat_IsRefused", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock block;
    RequireRefused(device, block.Data(), {"8_8_8_8_UNORM", 56u, 4u}, "not reproducible");
}};

const Testing::Case threeComponentRefused{"ImageStorePacked_ThreeComponentFormat_IsRefused", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock block;
    RequireRefused(device, block.Data(), {"32_32_32_FLOAT", 74u, 12u}, "does not write");
}};

} // namespace
