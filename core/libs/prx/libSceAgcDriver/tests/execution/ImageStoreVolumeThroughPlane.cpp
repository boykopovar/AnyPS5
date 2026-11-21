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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Size = 8;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::size_t TexelBytes = 16384;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0xf0201110, 0x00011714, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store volume through plane: cannot allocate the guest block");
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

std::uint32_t Value(std::uint32_t tid) {
    return 0x3f800000u + (tid / Size) * Size + tid % Size;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> PlaneDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        IdentitySwizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::size_t bytes, const std::array<std::uint32_t, Threads>& depths) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> words{tid % Size, tid / Size, depths[tid], Value(tid)};
        std::copy(words.begin(), words.end(), Input.begin() + tid * 4u);
    }
    std::fill(texels, texels + TexelBytes, Fill);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = PlaneDescriptor(texels);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(StoreCode);
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
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), bytes, nullptr, "test");
    device.WaitIdle();
}

void Check(const char* leg, const std::uint8_t* texels, const std::array<std::uint32_t, Threads>& depths) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(PlaneDescriptor(texels)));
    const auto& mip = geometry.mips.at(0);
    const std::string prefix = std::string("image_store 3d through a 2D T#, leg ") + leg + ": ";
    const std::uint32_t fill = 0x01010101u * Fill;
    std::vector<bool> covered(TexelBytes, false);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = tid % Size;
        const auto y = tid / Size;
        const auto offset = mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u;
        std::uint32_t actual = 0;
        std::memcpy(&actual, texels + offset, 4u);
        const auto expected = depths[tid] == 0u ? Value(tid) : fill;
        Require(actual == expected, prefix + "texel (" + std::to_string(x) + ", " + std::to_string(y) + ") with z " + std::to_string(depths[tid]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        for (std::uint32_t byte = 0; byte < 4u; ++byte) covered[offset + byte] = true;
    }
    for (std::size_t index = 0; index < TexelBytes; ++index) {
        Require(covered[index] || texels[index] == Fill, prefix + "byte " + std::to_string(index) + " outside the face is " + Hex(texels[index]) + ", expected " + Hex(Fill));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(PlaneDescriptor(texels)));
        Require(geometry.guestBytes <= TexelBytes, "image store volume through plane: the plane does not fit the texel storage");
        std::array<std::uint32_t, Threads> depths{};
        Dispatch(*device, texels, geometry.guestBytes, depths);
        Check("A (all z 0)", texels, depths);
        depths.fill(1u);
        Dispatch(*device, texels, geometry.guestBytes, depths);
        Check("B (all z 1)", texels, depths);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) depths[tid] = tid & 1u;
        Dispatch(*device, texels, geometry.guestBytes, depths);
        Check("C (z is lane & 1)", texels, depths);
        std::puts("image store volume through plane tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
