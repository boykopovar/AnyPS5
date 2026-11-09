#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t MipSize = 16;
constexpr std::uint32_t MipLevels = 5;
constexpr std::uint32_t CubeSize = 8;
constexpr std::uint32_t Faces = 6;
constexpr std::uint32_t RepackedFormat = 43;
constexpr std::uint32_t NativeFormat = 36;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t Result = 8;
constexpr std::array<std::uint32_t, 4> PointMipSampler{0u, 0xfffu << 12u, 1u << 26u, 0u};
constexpr std::array<std::uint32_t, 4> BilinearSampler{0u, 0xfffu << 12u, (1u << 20u) | (1u << 22u), 0u};
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 65536> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 30> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xbf8c3f70, 0xf09c0108, 0x00610c02, 0xf0900108, 0x00610d02,
    0xf0800108, 0x00610e02, 0xf0001108, 0x00010f05, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

std::uint32_t GuestWord(std::uint32_t seed) {
    seed ^= seed >> 16u;
    seed *= 0x7feb352du;
    seed ^= seed >> 15u;
    seed *= 0x846ca68bu;
    return seed ^ (seed >> 16u);
}

float SmallFloat(std::uint32_t bits, std::uint32_t mantissaBits) {
    const auto exponent = bits >> mantissaBits;
    const auto mantissa = bits & ((1u << mantissaBits) - 1u);
    const auto scale = static_cast<float>(1u << mantissaBits);
    if (exponent == 31u) return mantissa == 0u ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
    if (exponent == 0u) return std::ldexp(static_cast<float>(mantissa) / scale, -14);
    return std::ldexp(1.0f + static_cast<float>(mantissa) / scale, static_cast<int>(exponent) - 15);
}

float GuestComponent(std::uint32_t word, std::uint32_t component) {
    if (component == 0u) return SmallFloat(word & 0x3ffu, 5u);
    if (component == 1u) return SmallFloat((word >> 10u) & 0x7ffu, 6u);
    return SmallFloat(word >> 21u, 6u);
}

bool Same(float actual, float expected) {
    return (std::isnan(actual) && std::isnan(expected)) || std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected);
}

std::uint32_t Swizzle(std::uint32_t component) {
    return (4u + component) | (5u << 3u) | (6u << 6u) | (7u << 9u);
}

std::array<std::uint32_t, 8> Descriptor(std::uint32_t format, std::uint32_t size, std::uint32_t type, std::uint32_t swizzle, std::uint32_t lastLevel, std::uint32_t lastArray) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((size - 1u) & 3u) << 30u),
        ((size - 1u) >> 2u) | ((size - 1u) << 14u),
        swizzle | (lastLevel << 16u) | (type << 28u),
        lastArray,
        lastLevel << 4u,
        0u,
        0u,
    };
}

template <typename TWord>
void Fill(const std::array<std::uint32_t, 8>& descriptor, TWord word) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(geometry.guestBytes <= Texels.size(), "image repacked float: the texture does not fit the texel storage");
    std::fill(Texels.begin(), Texels.end(), 0u);
    for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) {
        for (std::uint32_t level = 0; level < geometry.mips.size(); ++level) {
            const auto& mip = geometry.mips[level];
            for (std::uint32_t y = 0; y < mip.height; ++y) {
                for (std::uint32_t x = 0; x < mip.width; ++x) {
                    const std::uint32_t value = word(layer, level, x, y);
                    std::memcpy(&Texels[geometry.GuestLayerOffset(layer) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u], &value, 4u);
                }
            }
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& sampler) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Input(std::uint32_t size, float base, std::uint32_t lod) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = tid % size;
        const auto y = (tid * 3u) % size;
        const float u = base + (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
        const float v = base + (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
        const std::array<std::uint32_t, 5> words{std::bit_cast<std::uint32_t>(u), std::bit_cast<std::uint32_t>(v), std::bit_cast<std::uint32_t>(static_cast<float>(lod)), x, y};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

std::uint32_t MipWord(std::uint32_t, std::uint32_t level, std::uint32_t x, std::uint32_t y) {
    return GuestWord(level * 4096u + y * 64u + x);
}

void CheckMipLevels(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t lod = 1; lod < MipLevels; ++lod) {
        const auto size = MipSize >> lod;
        for (std::uint32_t component = 0; component < 3u; ++component) {
            const auto descriptor = Descriptor(RepackedFormat, MipSize, Type2D, Swizzle(component), MipLevels - 1u, 0u);
            Fill(descriptor, MipWord);
            Input(size, 0.0f, lod);
            Run(device, descriptor, PointMipSampler);
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                const auto x = tid % size;
                const auto y = (tid * 3u) % size;
                const float sampled = std::bit_cast<float>(Buffer[tid * Words + Result + 1u]);
                const float expected = GuestComponent(MipWord(0u, lod, x, y), component);
                Require(Same(sampled, expected), "image repacked float: image_sample_l at LOD " + std::to_string(lod) + ", thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(sampled) + ", expected " + std::to_string(expected));
                const auto loaded = std::bit_cast<float>(Buffer[tid * Words + Result + 3u]);
                Require(Same(loaded, GuestComponent(MipWord(0u, 0u, x, y), component)), "image repacked float: image_load of mip 0, thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(loaded));
            }
        }
    }
}

std::uint32_t CubeWord(std::uint32_t face, std::uint32_t, std::uint32_t x, std::uint32_t y) {
    const auto exponent = 12u + (face + x + y) % 6u;
    return ((exponent << 5u) | ((x * 7u + y) & 31u)) | (((exponent + 1u) << 6u | ((y * 5u + face) & 63u)) << 10u) | (((exponent + 2u) << 6u | ((x * 3u + face * 11u) & 63u)) << 21u);
}

void CheckCubeFace(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t component = 0; component < 3u; ++component) {
        const auto repacked = Descriptor(RepackedFormat, CubeSize, TypeCube, Swizzle(component), 0u, Faces - 1u);
        Fill(repacked, CubeWord);
        Input(CubeSize, 1.0f, 0u);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) Buffer[tid * Words] = std::bit_cast<std::uint32_t>(std::bit_cast<float>(Buffer[tid * Words]) + 0.3f / CubeSize);
        const auto input = Buffer;
        Run(device, repacked, BilinearSampler);
        const auto repackedResults = Buffer;
        constexpr std::array<std::uint32_t, 3> nativeChannel{2u, 1u, 0u};
        const auto native = Descriptor(NativeFormat, CubeSize, TypeCube, Swizzle(nativeChannel[component]), 0u, Faces - 1u);
        Fill(native, [](std::uint32_t face, std::uint32_t level, std::uint32_t x, std::uint32_t y) { return AgcDriver::Graphics::RepackTextureWord(CubeWord(face, level, x, y)); });
        Buffer = input;
        Run(device, native, BilinearSampler);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t op = 0; op < 4u; ++op) {
                const float actual = std::bit_cast<float>(repackedResults[tid * Words + Result + op]);
                const float expected = std::bit_cast<float>(Buffer[tid * Words + Result + op]);
                Require(Same(actual, expected), "image repacked float: cube face sample " + std::to_string(op) + ", thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", the native B10G11R11 cube gives " + std::to_string(expected));
            }
            const float face = std::bit_cast<float>(repackedResults[tid * Words + Result + 3u]);
            Require(Same(face, GuestComponent(CubeWord(0u, 0u, tid % CubeSize, (tid * 3u) % CubeSize), component)), "image repacked float: image_load of cube face 0, thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(face));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckMipLevels(*device);
        CheckCubeFace(*device);
        std::puts("image repacked float tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
