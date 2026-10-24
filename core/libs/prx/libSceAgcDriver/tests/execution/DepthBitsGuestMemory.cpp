#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 192;
constexpr std::uint32_t Texels = Width * Height;
constexpr std::uint32_t Format16UInt = 11;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Swizzle64KbZX = 24;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t Unwritten = 0xa5a5a5a5u;
constexpr std::size_t SlotBytes = 256u << 10u;
constexpr std::size_t BlockBytes = 1u << 20u;

static_assert(Width == 256u, "the row shader scales the row by 256 texels");

alignas(256) constexpr std::array<std::uint32_t, 10> RowCode{
    0x7e020210u, 0x34040288u, 0x4a040500u, 0x34040482u, 0xf0001108u, 0x00010300u, 0xbf8c3f70u, 0xe0701000u,
    0x80000302u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 16> Edges32{
    0x00000000u, 0x00000001u, 0x12345678u, 0x7f7fffffu, 0x3f800000u, 0x80000000u, 0xbf800000u, 0x00800000u,
    0x007fffffu, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800001u, 0xffffffffu, 0x7fbfffffu,
};

constexpr std::array<std::uint32_t, 8> Edges16{
    0x0000u, 0x0001u, 0x1234u, 0x7fffu, 0x8000u, 0x8001u, 0xfffeu, 0xffffu,
};

alignas(4096) std::array<std::uint8_t, 3u * SlotBytes + 65536u> UntrackedStorage{};
alignas(256) std::array<std::uint32_t, Texels> UntrackedOutput{};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "depth bits guest memory: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
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

    std::uint8_t* Slots() { return block; }
    std::span<std::uint32_t> Output() { return {reinterpret_cast<std::uint32_t*>(block + 3u * SlotBytes), Texels}; }

private:
    std::uint8_t* block = nullptr;
};

struct Memory {
    const char* name;
    std::uint8_t* slots;
    std::span<std::uint32_t> output;
};

struct Case {
    const char* name;
    std::uint32_t format;
    std::uint32_t elementBytes;
    std::uint32_t slot;
};

constexpr std::array<Case, 3> Cases{{
    {"32_UINT", Format32UInt, 4u, 0u},
    {"32_SINT", Format32SInt, 4u, 1u},
    {"16_UINT", Format16UInt, 2u, 2u},
}};

static_assert(Texels * 4u <= SlotBytes && 4u * SlotBytes <= BlockBytes, "the slots and the output must fit the guest block");

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Mix(std::uint32_t x, std::uint32_t y) {
    auto hash = (x * 0x9e3779b1u) ^ (y * 0x85ebca77u);
    hash ^= hash >> 15u;
    hash *= 0x2c1b3c6du;
    hash ^= hash >> 12u;
    return hash;
}

std::uint32_t Pattern(const Case& test, std::uint32_t x, std::uint32_t y) {
    const bool edge = (x & 15u) == (y & 15u);
    if (test.elementBytes == 2u) return edge ? Edges16[(x / 16u + y) % Edges16.size()] : Mix(x, y) & 0xffffu;
    return edge ? Edges32[(x / 16u + y) % Edges32.size()] : Mix(x, y);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        IdentitySwizzle | (Swizzle64KbZX << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::uint64_t EquationOffset(const TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16; ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12) & (mask & 0xfff000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

std::uint8_t* SlotOf(const Memory& memory, const Case& test) {
    return memory.slots + test.slot * SlotBytes;
}

void Fill(const Memory& memory, const Case& test) {
    auto* texels = SlotOf(memory, test);
    const auto descriptor = DecodeTextureResource(TextureDescriptor(texels, test.format));
    Require(descriptor.baseAddress == reinterpret_cast<std::uintptr_t>(texels) && descriptor.pipeBankXor == 0u, std::string(test.name) + ": the texels are not 64 KiB aligned");
    const auto geometry = DescribeSurface(descriptor);
    Require(geometry.guestBytes <= SlotBytes, std::string(test.name) + ": the surface does not fit its slot");
    const auto* equation = FindTextureSwizzleEquation(XorSwizzleMode(descriptor.tileMode), test.elementBytes);
    Require(equation != nullptr, std::string(test.name) + ": the SW_64KB_Z_X equation is missing");
    const auto& mip = geometry.mips.at(0);
    Require(!mip.tail, std::string(test.name) + ": the base level is a tail mip");
    const auto layout = ThinBlockLayout(descriptor.tileMode, test.elementBytes);
    std::fill(texels, texels + SlotBytes, std::uint8_t{0xee});
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = mip.tiledOffset + (static_cast<std::uint64_t>(y / layout[2]) * mip.blocksPerRow + x / layout[1]) * layout[0] + EquationOffset(*equation, x, y);
            Require(offset + test.elementBytes <= geometry.guestBytes, std::string(test.name) + ": a texel lies outside the surface");
            const auto value = Pattern(test, x, y);
            std::memcpy(texels + offset, &value, test.elementBytes);
        }
    }
}

void Read(AgcDriver::VulkanDevice& device, const Memory& memory, const Case& test) {
    std::fill(memory.output.begin(), memory.output.end(), Unwritten);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(memory.output.data(), static_cast<std::uint32_t>(memory.output.size_bytes()));
    const auto texture = TextureDescriptor(SlotOf(memory, test), test.format);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(RowCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Width, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, regions},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, Height, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Memory& memory, const Case& test) {
    std::uint32_t wrong = 0;
    std::string first;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = Pattern(test, x, y);
            const auto actual = memory.output[y * Width + x];
            if (actual == expected) continue;
            if (wrong == 0) first = "(" + std::to_string(x) + ", " + std::to_string(y) + ") read " + Hex(actual) + ", expected " + Hex(expected);
            ++wrong;
        }
    }
    Require(wrong == 0, std::string(test.name) + " SW_64KB_Z_X depth bits from " + memory.name + ": " + std::to_string(wrong) + " of " + std::to_string(Texels) + " texels differ, first " + first);
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto* untracked = UntrackedStorage.data() + ((0u - reinterpret_cast<std::uintptr_t>(UntrackedStorage.data())) & 0xffffu);
        const std::array<Memory, 2> memories{{
            {"untracked memory", untracked, UntrackedOutput},
            {"a guest allocation", block.Slots(), block.Output()},
        }};
        for (const auto& memory : memories) {
            for (const auto& test : Cases) {
                Fill(memory, test);
                Read(*device, memory, test);
                Check(memory, test);
            }
        }
        std::puts("depth bits guest memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
