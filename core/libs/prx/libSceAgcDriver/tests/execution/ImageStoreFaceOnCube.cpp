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
constexpr std::uint32_t TypePlane = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::size_t TexelBytes = 32768;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t FaceBytes = 2048;
constexpr std::size_t RowBytes = 256;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};

alignas(256) constexpr std::array<std::uint32_t, 9> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x363c0087, 0x2c3e0083, 0xf0201f08, 0x0001141e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> StoreCodeDmask1{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x363c0087, 0x2c3e0083, 0xf0200108, 0x0001141e, 0xbf810000,
};

constexpr std::array<std::uint8_t, Threads> Unorm8888Bytes{
    0x0, 0x4, 0x8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c,
    0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c,
    0x80, 0x83, 0x87, 0x8b, 0x8f, 0x93, 0x97, 0x9b, 0x9f, 0xa3, 0xa7, 0xab, 0xaf, 0xb3, 0xb7, 0xbb,
    0xbf, 0xc3, 0xc7, 0xcb, 0xcf, 0xd3, 0xd7, 0xdb, 0xdf, 0xe3, 0xe7, 0xeb, 0xef, 0xf3, 0xf7, 0xfb,
};

constexpr std::array<std::uint8_t, Threads> Snorm8888Bytes{
    0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e,
    0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e,
    0x40, 0x41, 0x43, 0x45, 0x47, 0x49, 0x4b, 0x4d, 0x4f, 0x51, 0x53, 0x55, 0x57, 0x59, 0x5b, 0x5d,
    0x5f, 0x61, 0x63, 0x65, 0x67, 0x69, 0x6b, 0x6d, 0x6f, 0x71, 0x73, 0x75, 0x77, 0x79, 0x7b, 0x7d,
};

constexpr std::array<std::uint16_t, Threads> Half{
    0x0, 0x2400, 0x2800, 0x2a00, 0x2c00, 0x2d00, 0x2e00, 0x2f00, 0x3000, 0x3080, 0x3100, 0x3180, 0x3200, 0x3280, 0x3300, 0x3380,
    0x3400, 0x3440, 0x3480, 0x34c0, 0x3500, 0x3540, 0x3580, 0x35c0, 0x3600, 0x3640, 0x3680, 0x36c0, 0x3700, 0x3740, 0x3780, 0x37c0,
    0x3800, 0x3820, 0x3840, 0x3860, 0x3880, 0x38a0, 0x38c0, 0x38e0, 0x3900, 0x3920, 0x3940, 0x3960, 0x3980, 0x39a0, 0x39c0, 0x39e0,
    0x3a00, 0x3a20, 0x3a40, 0x3a60, 0x3a80, 0x3aa0, 0x3ac0, 0x3ae0, 0x3b00, 0x3b20, 0x3b40, 0x3b60, 0x3b80, 0x3ba0, 0x3bc0, 0x3be0,
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

enum class InputKind {
    Raw,
    Measured,
};

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t texelBytes;
    InputKind input;
    bool control;
};

constexpr std::array<Format, 7> Formats{{
    {"32_UINT", 20u, 4u, InputKind::Raw, false},
    {"32_FLOAT", 22u, 4u, InputKind::Raw, false},
    {"8_8_8_8_UNORM", 56u, 4u, InputKind::Measured, true},
    {"8_8_8_8_SNORM", 57u, 4u, InputKind::Measured, true},
    {"32_SINT", 21u, 4u, InputKind::Measured, true},
    {"16_16_FLOAT", 29u, 4u, InputKind::Measured, true},
    {"16_16_16_16_FLOAT", 71u, 8u, InputKind::Measured, true},
}};

constexpr std::array<Format, 2> Refused{{
    {"8_8_8_8_SINT", 61u, 4u, InputKind::Measured, false},
    {"16_FLOAT", 13u, 4u, InputKind::Measured, false},
}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Value(std::uint32_t tid) {
    return 0x3f000000u + tid * 0x1357u;
}

std::uint32_t LaneInput(const Format& format, std::uint32_t tid) {
    if (format.input == InputKind::Raw) return Value(tid);
    if (format.format == 21u) return std::bit_cast<std::uint32_t>(static_cast<float>(tid));
    return std::bit_cast<std::uint32_t>(static_cast<float>(tid) / 64.0f);
}

std::array<std::uint32_t, 2> TexelWords(const Format& format, std::uint32_t tid) {
    if (format.input == InputKind::Raw) return {Value(tid), 0u};
    switch (format.format) {
    case 56u: return {Unorm8888Bytes[tid], 0u};
    case 57u: return {Snorm8888Bytes[tid], 0u};
    case 21u: return {std::bit_cast<std::uint32_t>(static_cast<float>(tid)), 0u};
    case 29u: return {Half[tid], 0u};
    case 71u: return {Half[tid], 0u};
    default: return {0u, 0u};
    }
}

std::array<std::uint32_t, 8> Descriptor(const void* data, std::uint32_t format, std::uint32_t type, std::uint32_t baseArray) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    const bool cube = type == TypeCube;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        IdentitySwizzle | (type << 28u),
        cube ? (Faces - 1u) | (baseArray << 16u) : 0u,
        0u,
        0u,
        0u,
    };
}

AgcDriver::Graphics::SurfaceGeometry Geometry(const void* data, std::uint32_t format, std::uint32_t type) {
    return AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(Descriptor(data, format, type, 0u)));
}

std::uint64_t TexelOffset(const AgcDriver::Graphics::SurfaceGeometry& geometry, std::uint32_t texelBytes, std::uint32_t face, std::uint32_t x, std::uint32_t y) {
    const auto& mip = geometry.mips.at(0);
    return geometry.GuestLayerOffset(face) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * texelBytes;
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

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t type, std::uint32_t baseArray) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid * 4u] = LaneInput(format, tid);
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = Descriptor(texels, format.format, type, baseArray);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code = format.input == InputKind::Raw ? std::span<const std::uint32_t>(StoreCode) : std::span<const std::uint32_t>(StoreCodeDmask1);
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

void CheckTexels(const std::uint8_t* texels, const Format& format, std::uint32_t type, std::uint32_t baseArray) {
    const bool cube = type == TypeCube;
    const auto geometry = Geometry(texels, format.format, type);
    const auto expectedLayers = cube ? Faces : 1u;
    const auto kind = cube ? " cube T# with BASE_ARRAY " + std::to_string(baseArray) : std::string(" plane T#");
    const auto label = std::string("image_store 2d through a ") + format.name + kind;
    Require(geometry.layers == expectedLayers && geometry.guestBytes <= TexelBytes, "image store face on a cube: the " + std::string(cube ? "cube" : "plane") + " has " + std::to_string(geometry.layers) + " layers in " + std::to_string(geometry.guestBytes) + " bytes, expected " + std::to_string(expectedLayers) + " in at most " + std::to_string(TexelBytes));
    const auto initial = Initial();
    std::vector<bool> written(TexelBytes, false);
    const auto words = format.texelBytes / 4u;
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto offset = TexelOffset(geometry, format.texelBytes, baseArray, x, y);
            if (format.input == InputKind::Measured) {
                const auto measured = static_cast<std::uint64_t>(baseArray) * FaceBytes + y * RowBytes + x * format.texelBytes;
                Require(offset == measured, label + ": the geometry puts texel (" + std::to_string(x) + ", " + std::to_string(y) + ") at byte " + std::to_string(offset) + ", the measured layout has it at " + std::to_string(measured));
            }
            const auto wanted = TexelWords(format, y * Size + x);
            for (std::uint32_t word = 0; word < words; ++word) {
                std::uint32_t actual = 0;
                std::memcpy(&actual, texels + offset + word * 4u, 4u);
                Require(actual == wanted[word], label + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") word " + std::to_string(word) + " of face " + std::to_string(baseArray) + " is " + Hex(actual) + ", expected " + Hex(wanted[word]));
            }
            std::fill(written.begin() + static_cast<std::ptrdiff_t>(offset), written.begin() + static_cast<std::ptrdiff_t>(offset + format.texelBytes), true);
        }
    }
    for (std::size_t index = 0; index < TexelBytes; ++index) {
        Require(written[index] || texels[index] == initial[index], label + ": byte " + std::to_string(index) + " outside face " + std::to_string(baseArray) + " changed");
    }
}

bool Row(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t type, std::uint32_t baseArray) {
    const auto label = std::string(format.name) + (type == TypeCube ? " cube BASE_ARRAY " + std::to_string(baseArray) : std::string(" plane control"));
    try {
        Dispatch(device, texels, format, type, baseArray);
        CheckTexels(texels, format, type, baseArray);
    } catch (const std::exception& error) {
        std::cout << "row " << label << ": FAIL: " << error.what() << '\n';
        return false;
    }
    std::cout << "row " << label << ": ok\n";
    return true;
}

bool RefusedRow(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format) {
    const auto label = std::string(format.name) + " cube refused";
    try {
        Dispatch(device, texels, format, TypeCube, 0u);
    } catch (const std::exception& error) {
        const std::string message = error.what();
        const bool same = message.find("incompatible with the static runtime image interface") != std::string::npos;
        std::cout << "row " << label << ": " << (same ? "ok: " : "FAIL: unexpected message: ") << message << '\n';
        return same;
    }
    std::cout << "row " << label << ": FAIL: the store through a cube T# was accepted\n";
    return false;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        bool passed = true;
        for (const auto& format : Formats) {
            for (const auto baseArray : {0u, 6u}) passed &= Row(*device, texels, format, TypeCube, baseArray);
            if (format.control) passed &= Row(*device, texels, format, TypePlane, 0u);
        }
        for (const auto& format : Refused) passed &= RefusedRow(*device, texels, format);
        if (!passed) return 1;
        std::puts("image store face on cube tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
