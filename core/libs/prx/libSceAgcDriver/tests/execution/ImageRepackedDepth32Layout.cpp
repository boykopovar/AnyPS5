#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Result = 8;
constexpr std::uint32_t Width = 68;
constexpr std::uint32_t Height = 36;
constexpr std::uint32_t Levels = 3;
constexpr std::uint32_t Layers = 2;
constexpr std::uint32_t FormatDepth32 = 146;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 262144> TexelsA{};
alignas(256) std::array<std::uint8_t, 262144> TexelsB{};

constexpr std::uint32_t Prologue[] = {0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c, 0x80000501, 0xbf8c3f70};
constexpr std::uint32_t Epilogue[] = {0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000};

constexpr std::array<std::uint32_t, 22> Program(std::uint32_t first, std::uint32_t second) {
    std::array<std::uint32_t, 22> code{};
    std::size_t index = 0;
    for (const auto word : Prologue) code[index++] = word;
    code[index++] = first;
    code[index++] = second;
    for (const auto word : Epilogue) code[index++] = word;
    return code;
}

alignas(256) constexpr auto SampleLod2D = Program(0xf0900f08, 0x00610c02);
alignas(256) constexpr auto Load2DArray = Program(0xf0001f28, 0x00010c02);

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t MipWidth(std::uint32_t level) {
    return std::max(1u, Width >> level);
}

std::uint32_t MipHeight(std::uint32_t level) {
    return std::max(1u, Height >> level);
}

std::uint32_t Encode(std::uint32_t layer, std::uint32_t level, std::uint32_t y, std::uint32_t x) {
    return x + y * 128u + level * 8192u + layer * 32768u;
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t type, std::uint32_t layers) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (FormatDepth32 << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (TileLinear << 20u) | (type << 28u),
        layers - 1u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
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
}

template<typename TInput>
void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, TInput input) {
    Buffer.fill(Untouched);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> words = input(tid);
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    Dispatch(device, code, userData);
}

std::uint32_t ResultWord(std::uint32_t tid, std::uint32_t component) {
    return Buffer[tid * Words + Result + component];
}

const std::vector<std::pair<std::uint32_t, std::uint32_t>>& Positions(std::uint32_t level) {
    static const std::vector<std::pair<std::uint32_t, std::uint32_t>> positions[Levels] = {
        {{0u, 0u}, {1u, 0u}, {0u, 1u}, {67u, 0u}, {0u, 35u}, {67u, 35u}, {34u, 18u}, {1u, 35u}},
        {{0u, 0u}, {1u, 0u}, {0u, 1u}, {33u, 0u}, {0u, 17u}, {33u, 17u}, {17u, 9u}, {1u, 17u}},
        {{0u, 0u}, {1u, 0u}, {0u, 1u}, {16u, 0u}, {0u, 8u}, {16u, 8u}, {8u, 4u}, {1u, 8u}},
    };
    return positions[level];
}

void Fill(std::array<std::uint8_t, 262144>& texels, std::uint32_t type, std::uint32_t layers, const char* name) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels.data(), type, layers)));
    Require(geometry.guestBytes <= texels.size() && geometry.mips.size() == Levels, std::string(name) + ": the surface does not fit the texel storage");
    texels.fill(0xeeu);
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        for (std::uint32_t level = 0; level < Levels; ++level) {
            const auto& mip = geometry.mips[level];
            Require(mip.width == MipWidth(level) && mip.height == MipHeight(level), std::string(name) + ": level " + std::to_string(level) + " dimensions are not the halved chain");
            Require(mip.pitchBytes > mip.width * 8u, std::string(name) + ": level " + std::to_string(level) + " has no pitch padding, so the test cannot distinguish padded from packed layouts");
            const auto base = geometry.GuestOffset(layer, mip);
            for (std::uint32_t y = 0; y < mip.height; ++y) {
                for (std::uint32_t x = 0; x < mip.width; ++x) {
                    const float depth = static_cast<float>(Encode(layer, level, y, x)) / 65536.0f;
                    const std::uint32_t stencil = 0x5au;
                    std::memcpy(&texels[base + y * mip.pitchBytes + x * 8u], &depth, 4u);
                    std::memcpy(&texels[base + y * mip.pitchBytes + x * 8u + 4u], &stencil, 4u);
                }
            }
        }
    }
}

void CheckSampledLayout(AgcDriver::VulkanDevice& device) {
    Fill(TexelsA, Type2D, 1u, "sampled layout");
    std::vector<std::array<std::uint32_t, 4>> probes;
    for (std::uint32_t level = 0; level < Levels; ++level) {
        for (const auto& position : Positions(level)) probes.push_back({level, position.first, position.second, 0u});
    }
    Run(device, SampleLod2D, TextureDescriptor(TexelsA.data(), Type2D, 1u), [&](std::uint32_t tid) {
        if (tid >= probes.size()) return std::array<std::uint32_t, 4>{0u, 0u, 0u, 0u};
        const auto& probe = probes[tid];
        const float xn = (static_cast<float>(probe[1]) + 0.5f) / static_cast<float>(MipWidth(probe[0]));
        const float yn = (static_cast<float>(probe[2]) + 0.5f) / static_cast<float>(MipHeight(probe[0]));
        return std::array<std::uint32_t, 4>{Bits(xn), Bits(yn), Bits(static_cast<float>(probe[0])), 0u};
    });
    for (std::uint32_t tid = 0; tid < probes.size() && tid < Threads; ++tid) {
        const auto& probe = probes[tid];
        const float expected = static_cast<float>(Encode(0u, probe[0], probe[2], probe[1])) / 65536.0f;
        const auto expectedBits = std::bit_cast<std::uint32_t>(expected);
        Require(ResultWord(tid, 0u) == expectedBits, "image_sample_l of a padded multi-level linear X24_8_32 texture: level " + std::to_string(probe[0]) + " position " + std::to_string(probe[1]) + "x" + std::to_string(probe[2]) + " reads " + std::to_string(std::bit_cast<float>(ResultWord(tid, 0u))) + ", expected " + std::to_string(expected));
    }
    std::cout << "Sampled multi-row padded multi-level linear X24_8_32 layout probes passed (" << probes.size() << " positions)\n";
}

void CheckLayeredLayout(AgcDriver::VulkanDevice& device) {
    Fill(TexelsB, Type2DArray, Layers, "layered layout");
    std::vector<std::array<std::uint32_t, 4>> probes;
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (const auto& position : Positions(0u)) probes.push_back({position.first, position.second, layer, 0u});
    }
    Run(device, Load2DArray, TextureDescriptor(TexelsB.data(), Type2DArray, Layers), [&](std::uint32_t tid) {
        if (tid >= probes.size()) return std::array<std::uint32_t, 4>{0u, 0u, 0u, 0u};
        return probes[tid];
    });
    for (std::uint32_t tid = 0; tid < probes.size() && tid < Threads; ++tid) {
        const auto& probe = probes[tid];
        const float expected = static_cast<float>(Encode(probe[2], 0u, probe[1], probe[0])) / 65536.0f;
        const auto expectedBits = std::bit_cast<std::uint32_t>(expected);
        Require(ResultWord(tid, 0u) == expectedBits, "image_load of a padded multi-layer linear X24_8_32 array: layer " + std::to_string(probe[2]) + " position " + std::to_string(probe[0]) + "x" + std::to_string(probe[1]) + " of mip 0 reads " + std::to_string(std::bit_cast<float>(ResultWord(tid, 0u))) + ", expected " + std::to_string(expected));
    }
    std::cout << "Layered multi-row padded linear X24_8_32 array layout probes passed (" << probes.size() << " positions)\n";
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckSampledLayout(*device);
        CheckLayeredLayout(*device);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
    std::cout << "Repacked depth32 layout tests passed\n";
    return 0;
}
