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

constexpr std::uint32_t Size = 8;
constexpr std::uint32_t Threads = Size * Size;
constexpr std::uint32_t Faces = 12;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::size_t TexelBytes = 32768;
constexpr std::size_t BlockBytes = 65536;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};

alignas(256) constexpr std::array<std::uint32_t, 9> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x363c0087, 0x2c3e0083, 0xf0201f08, 0x0001141e, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store face on a cube: cannot allocate the guest block");
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
};

constexpr std::array<Format, 2> Formats{{
    {"32_UINT", 20u},
    {"32_FLOAT", 22u},
}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Value(std::uint32_t tid) {
    return 0x3f000000u + tid * 0x1357u;
}

std::array<std::uint32_t, 8> CubeDescriptor(const void* data, std::uint32_t format, std::uint32_t baseArray) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        IdentitySwizzle | (TypeCube << 28u),
        (Faces - 1u) | (baseArray << 16u),
        0u,
        0u,
        0u,
    };
}

AgcDriver::Graphics::SurfaceGeometry Geometry(const void* data, std::uint32_t format) {
    return AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(CubeDescriptor(data, format, 0u)));
}

std::uint64_t TexelOffset(const AgcDriver::Graphics::SurfaceGeometry& geometry, std::uint32_t face, std::uint32_t x, std::uint32_t y) {
    const auto& mip = geometry.mips.at(0);
    return geometry.GuestLayerOffset(face) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u;
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

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t baseArray) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid * 4u] = Value(tid);
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = CubeDescriptor(texels, format.format, baseArray);
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
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void CheckTexels(const std::uint8_t* texels, const Format& format, std::uint32_t baseArray) {
    const auto geometry = Geometry(texels, format.format);
    Require(geometry.layers == Faces && geometry.guestBytes <= TexelBytes, "image store face on a cube: the cube has " + std::to_string(geometry.layers) + " layers in " + std::to_string(geometry.guestBytes) + " bytes, expected " + std::to_string(Faces) + " in at most " + std::to_string(TexelBytes));
    const auto initial = Initial();
    std::vector<bool> written(TexelBytes, false);
    const auto label = std::string("image_store 2d through a ") + format.name + " cube T# with BASE_ARRAY " + std::to_string(baseArray);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto offset = TexelOffset(geometry, baseArray, x, y);
            std::uint32_t actual = 0;
            std::memcpy(&actual, texels + offset, 4u);
            const auto wanted = Value(y * Size + x);
            Require(actual == wanted, label + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") of face " + std::to_string(baseArray) + " is " + Hex(actual) + ", expected " + Hex(wanted));
            std::fill(written.begin() + static_cast<std::ptrdiff_t>(offset), written.begin() + static_cast<std::ptrdiff_t>(offset + 4u), true);
        }
    }
    for (std::size_t index = 0; index < TexelBytes; ++index) {
        Require(written[index] || texels[index] == initial[index], label + ": byte " + std::to_string(index) + " outside face " + std::to_string(baseArray) + " changed");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        for (const auto& format : Formats) {
            for (const auto baseArray : {0u, 6u}) {
                Dispatch(*device, texels, format, baseArray);
                CheckTexels(texels, format, baseArray);
            }
        }
        std::puts("image store face on cube tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
