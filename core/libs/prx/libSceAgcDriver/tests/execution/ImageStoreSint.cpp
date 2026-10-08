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
constexpr std::uint32_t TitleSwizzle = 0x24cu;
constexpr std::size_t TexelBytes = 8192;
constexpr std::size_t BlockBytes = 65536;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e,
    0x7e3e0281, 0xf0201508, 0x0001141e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> TitleCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0202108, 0x0001141e,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> LoadCode{
    0x34020084, 0x7e3c0300, 0x7e3e0280, 0xf0001f08, 0x0001141e, 0xbf8c3f70, 0xe0781000, 0x80001401,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 5> D16Code{0x7e3c0300, 0x7e3e0280, 0xf0201108, 0x8001141e, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 5> PackedCode{0x7e3c0300, 0x7e3e0280, 0xf0281108, 0x0001141e, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 7> AtomicD16Code{0x7e3c0300, 0x7e3e0280, 0xf0441108, 0x0001141e, 0xf0201108, 0x8001141e, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 9> PackedWideCode{0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0281308, 0x0001141e, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 5> AtomicCode{0x7e3c0300, 0x7e3e0280, 0xf0441108, 0x0001141e, 0xbf810000};

constexpr std::array<std::uint32_t, 24> Edges{
    0x00000000u, 0x00000001u, 0xffffffffu, 0x0000007fu, 0x00000080u, 0xffffff80u, 0xffffff7fu, 0x000000ffu,
    0x00000100u, 0x00007fffu, 0x00008000u, 0xffff8000u, 0xffff7fffu, 0x0000ffffu, 0x00010000u, 0xffff0000u,
    0x7fffffffu, 0x80000000u, 0x12345678u, 0xedcba988u, 0x00007ffeu, 0xffff8001u, 0x0000007eu, 0xffffff81u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store sint: cannot allocate the guest block");
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

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t components;
    std::uint32_t bits;
};

constexpr std::array<Format, 8> Formats{{
    {"16_SINT", 12u, 1u, 16u},
    {"8_8_SINT", 19u, 2u, 8u},
    {"32_SINT", 21u, 1u, 32u},
    {"16_16_SINT", 28u, 2u, 16u},
    {"8_8_8_8_SINT", 61u, 4u, 8u},
    {"32_32_SINT", 63u, 2u, 32u},
    {"16_16_16_16_SINT", 70u, 4u, 16u},
    {"32_32_32_32_SINT", 76u, 4u, 32u},
}};

struct Write {
    std::uint32_t row;
    std::uint32_t dmask;
};

constexpr std::array<Write, 2> StoreWrites{{{0u, 0xfu}, {1u, 0x5u}}};
constexpr std::array<Write, 1> TitleWrites{{{0u, 0x1u}}};
constexpr std::array<Write, 1> PackedWideWrites{{{0u, 0x3u}}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Value(std::uint32_t tid, std::uint32_t index) {
    if (tid < Edges.size()) return Edges[(tid + 7u * index) % Edges.size()];
    const auto mixed = (tid * 4u + index + 1u) * 0x9e3779b1u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(mixed) >> ((tid * 4u + index) % 25u));
}

std::uint32_t Saturate(std::uint32_t value, std::uint32_t bits) {
    if (bits == 32u) return value;
    const auto high = (std::int64_t{1} << (bits - 1u)) - 1;
    const auto clamped = std::clamp<std::int64_t>(static_cast<std::int32_t>(value), -high - 1, high);
    return static_cast<std::uint32_t>(clamped) & ((1u << bits) - 1u);
}

std::uint32_t SignExtend(std::uint32_t value, std::uint32_t bits) {
    if (bits == 32u) return value;
    const auto shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}

std::uint32_t SwizzleOf(const Format& format) {
    return format.components == 2u ? TwoChannelSwizzle : IdentitySwizzle;
}

std::uint32_t TexelBytesOf(const Format& format) {
    return format.components * format.bits / 8u;
}

AgcDriver::Graphics::TileMipLayout Mip(const Format& format) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, Width, Height, 1u).at(0);
}

std::uint64_t TexelOffset(const Format& format, std::uint32_t x, std::uint32_t y) {
    const auto mip = Mip(format);
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * TexelBytesOf(format);
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> bytes(TexelBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(0x80u + 37u * index + 11u * (index >> 4u));
    }
    return bytes;
}

std::uint32_t StoredComponent(const Format& format, std::uint32_t tid, std::uint32_t dmask, std::uint32_t component) {
    if (((dmask >> component) & 1u) == 0u) return 0u;
    std::uint32_t index = 0;
    for (std::uint32_t lower = 0; lower < component; ++lower) index += (dmask >> lower) & 1u;
    return Saturate(Value(tid, index), format.bits);
}

std::vector<std::uint8_t> Expected(const Format& format, std::span<const Write> writes) {
    auto bytes = Initial();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (const auto& write : writes) {
            auto* texel = bytes.data() + TexelOffset(format, tid, write.row);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                const auto value = StoredComponent(format, tid, write.dmask, component);
                std::memcpy(texel + component * format.bits / 8u, &value, format.bits / 8u);
            }
        }
    }
    return bytes;
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
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::span<const std::uint32_t> code, std::uint32_t format, std::uint32_t swizzle, const std::array<std::uint32_t, 4>& buffer) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, format, swizzle);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
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
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::span<const std::uint32_t> code, std::uint32_t format, std::uint32_t swizzle) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < 4u; ++index) Input[tid * 4u + index] = Value(tid, index);
    }
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    Dispatch(device, texels, code, format, swizzle, BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)));
}

void CheckTexels(const std::uint8_t* texels, const Format& format, std::span<const Write> writes, const std::string& what) {
    const auto expected = Expected(format, writes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(format, x, y);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                std::uint32_t actual = 0;
                std::uint32_t wanted = 0;
                std::memcpy(&actual, texels + offset + component * format.bits / 8u, format.bits / 8u);
                std::memcpy(&wanted, expected.data() + offset + component * format.bits / 8u, format.bits / 8u);
                Require(actual == wanted, what + " " + format.name + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(wanted));
            }
        }
    }
}

void CheckLoad(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format) {
    Output.fill(0xdeadbeefu);
    Dispatch(device, texels, LoadCode, format.format, SwizzleOf(format), BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u)));
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t component = 0; component < format.components; ++component) {
            const auto actual = Output[tid * 4u + component];
            const auto wanted = SignExtend(StoredComponent(format, tid, 0xfu, component), format.bits);
            Require(actual == wanted, std::string("image_load after image_store of ") + format.name + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(wanted));
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::span<const std::uint32_t> code, std::uint32_t format, const std::string& reason, const std::string& what) {
    std::string refusal;
    try {
        Store(device, texels, code, format, IdentitySwizzle);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(reason) != std::string::npos, what + " was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        for (const auto& format : Formats) {
            Store(*device, texels, StoreCode, format.format, SwizzleOf(format));
            CheckTexels(texels, format, StoreWrites, "image_store");
            CheckLoad(*device, texels, format);
        }
        Store(*device, texels, TitleCode, Formats[0].format, TitleSwizzle);
        CheckTexels(texels, Formats[0], TitleWrites, "image_store dmask:0x1 glc with DST_SEL X,1,1,1");
        RequireRefused(*device, texels, D16Code, 12u, "stores 16-bit data to an image of a SINT format", "image_store d16 of 16_SINT");
        RequireRefused(*device, texels, D16Code, 21u, "stores 16-bit data to an image of a SINT format", "image_store d16 of 32_SINT");
        RequireRefused(*device, texels, PackedCode, 12u, "storage image descriptor uses an unsupported format", "image_store_pck of 16_SINT");
        RequireRefused(*device, texels, AtomicCode, 12u, "atomic image descriptor uses an unsupported format 12", "image_atomic_add of 16_SINT");
        RequireRefused(*device, texels, StoreCode, 73u, "storage image descriptor uses an unsupported format", "image_store of 32_32_32_SINT");
        for (const auto index : {2u, 5u, 7u}) {
            Store(*device, texels, PackedWideCode, Formats[index].format, SwizzleOf(Formats[index]));
            CheckTexels(texels, Formats[index], PackedWideWrites, "image_store_pck dmask:0x3");
        }
        RequireRefused(*device, texels, AtomicD16Code, 21u, "stores 16-bit data to an image of a SINT format", "image_store d16 of an atomically updated 32_SINT image");
        std::puts("image store sint tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
