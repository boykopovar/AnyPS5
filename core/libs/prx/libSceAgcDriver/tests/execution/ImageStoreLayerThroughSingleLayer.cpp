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
constexpr std::uint32_t Type1D = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::size_t TexelBytes = 4096;
constexpr std::size_t BlockBytes = 65536;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};

alignas(256) constexpr std::array<std::uint32_t, 17> ArrayStoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0281, 0x7e400280, 0xf0201f28, 0x0001141e,
    0x7e3e0280, 0x7e4002c1, 0xf0201528, 0x0001141e, 0x7e400281, 0xf0201528, 0x0001141e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> CubeStoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0281, 0x7e400280, 0xf0201f18, 0x0001141e,
    0x7e3e0280, 0x7e4002c1, 0xf0201518, 0x0001141e, 0x7e400281, 0xf0201518, 0x0001141e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> LineArrayStoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f20, 0x0001141e, 0x7e280317,
    0x7e3e02c1, 0xf0201520, 0x0001141e, 0x7e3e0281, 0xf0201520, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 15> ArrayAtomicCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0x7e400315, 0xf0443128,
    0x0001141e, 0xbf8c3f70, 0xe070100c, 0x80001401, 0xbf8c3f70, 0xbf810000, 0xbf810000,
};

constexpr std::uint32_t Format32UInt = 20u;
constexpr std::array<std::uint32_t, 4> AtomicLayers{0u, 1u, 2u, 0xffffffffu};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store layer through a single layer: cannot allocate the guest block");
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
};

constexpr std::array<Format, 2> Formats{{
    {"32_SINT", 21u, 1u},
    {"32_32_32_32_SINT", 76u, 4u},
}};

struct Case {
    const char* name;
    std::span<const std::uint32_t> code;
    std::uint32_t type;
    std::uint32_t height;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Value(std::uint32_t tid, std::uint32_t index) {
    return (tid * 4u + index + 1u) * 0x9e3779b1u;
}

AgcDriver::Graphics::TileMipLayout Mip(const Format& format, const Case& test) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, Width, test.height, 1u).at(0);
}

std::uint64_t TexelOffset(const Format& format, const Case& test, std::uint32_t x, std::uint32_t y) {
    const auto mip = Mip(format, test);
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * format.components * 4u;
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> bytes(TexelBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] = static_cast<std::uint8_t>(0x80u + 37u * index + 11u * (index >> 4u));
    return bytes;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, const Case& test) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((test.height - 1u) << 14u),
        IdentitySwizzle | (test.type << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Case& test, std::uint32_t format) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < 4u; ++index) Input[tid * 4u + index] = Value(tid, index);
    }
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = TextureDescriptor(texels, format, test);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(test.code.data()), std::as_bytes(test.code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(test.code.data()), test.code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(test.code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void CheckTexels(const std::uint8_t* texels, const Format& format, const Case& test) {
    const auto initial = Initial();
    const auto what = std::string(test.name) + " to a " + format.name + " T#";
    const auto stored = test.height - 1u;
    for (std::uint32_t x = 0; x < Width; ++x) {
        const auto offset = TexelOffset(format, test, x, stored);
        for (std::uint32_t component = 0; component < format.components; ++component) {
            std::uint32_t actual = 0;
            std::memcpy(&actual, texels + offset + component * 4u, 4u);
            const auto wanted = Value(x, component);
            Require(actual == wanted, what + ": texel " + std::to_string(x) + "," + std::to_string(stored) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected the layer 0 store " + Hex(wanted) + " and the stores to layers 0xffffffff and 1 dropped");
        }
        if (stored != 0u) {
            const auto row = TexelOffset(format, test, x, 0u);
            const auto bytes = format.components * 4u;
            Require(std::equal(texels + row, texels + row + bytes, initial.begin() + static_cast<std::ptrdiff_t>(row)), what + ": texel " + std::to_string(x) + ",0 changed, but only stores to layers 0xffffffff and 1 addressed it");
        }
    }
    const auto end = TexelOffset(format, test, 0u, test.height);
    Require(std::equal(texels + end, texels + TexelBytes, initial.begin() + static_cast<std::ptrdiff_t>(end)), what + ": bytes past the texture changed");
}

void CheckAtomics(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    const Case test{"image_atomic_add 2d_array through a 2D T#", ArrayAtomicCode, Type2D, 1u};
    const Format format{"32_UINT", Format32UInt, 1u};
    std::vector<std::uint32_t> initial(Width);
    for (std::uint32_t x = 0; x < Width; ++x) initial[x] = 1000u + x;
    std::copy(reinterpret_cast<const std::uint8_t*>(initial.data()), reinterpret_cast<const std::uint8_t*>(initial.data() + Width), texels + TexelOffset(format, test, 0u, 0u));
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = 5u + tid;
        Input[tid * 4u + 1u] = AtomicLayers[tid % AtomicLayers.size()];
        Input[tid * 4u + 2u] = 0u;
        Input[tid * 4u + 3u] = 0xdeadbeefu;
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = TextureDescriptor(texels, format.format, test);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(test.code.data()), std::as_bytes(test.code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(test.code.data()), test.code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(test.code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto layer = AtomicLayers[tid % AtomicLayers.size()];
        std::uint32_t stored = 0;
        std::memcpy(&stored, texels + TexelOffset(format, test, tid, 0u), 4u);
        const auto wantedStored = layer == 0u ? initial[tid] + 5u + tid : initial[tid];
        const auto wantedReturned = layer == 0u ? initial[tid] : 0u;
        Require(stored == wantedStored, std::string(test.name) + ": texel " + std::to_string(tid) + " at layer " + Hex(layer) + " is " + Hex(stored) + ", expected " + Hex(wantedStored));
        Require(Input[tid * 4u + 3u] == wantedReturned, std::string(test.name) + ": lane " + std::to_string(tid) + " at layer " + Hex(layer) + " returned " + Hex(Input[tid * 4u + 3u]) + ", expected " + Hex(wantedReturned));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        const std::array<Case, 3> cases{{
            {"image_store 2d_array through a 2D T#", ArrayStoreCode, Type2D, 2u},
            {"image_store cube through a 2D T#", CubeStoreCode, Type2D, 2u},
            {"image_store 1d_array through a 1D T#", LineArrayStoreCode, Type1D, 1u},
        }};
        for (const auto& test : cases) {
            for (const auto& format : Formats) {
                Dispatch(*device, texels, test, format.format);
                CheckTexels(texels, format, test);
            }
        }
        CheckAtomics(*device, texels);
        std::puts("image store layer through single layer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
