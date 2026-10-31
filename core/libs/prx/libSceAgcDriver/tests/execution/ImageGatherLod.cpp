#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static bool ImageViewMinLod(const VulkanDevice& device) { return device.graphicsContext().imageViewMinLod; }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format8888UInt = 60;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ClampWrap = 0;
constexpr std::uint32_t ClampMirror = 1;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t ClampHalfBorder = 4;
constexpr std::uint32_t ClampBorder = 6;
constexpr std::uint32_t FilterPoint = 0;
constexpr std::uint32_t FilterBilinear = 1;
constexpr std::uint32_t MipNone = 0;
constexpr std::uint32_t MipPoint = 1;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 65536> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> Gather2DLod{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf1100108, 0x00820602, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};

constexpr std::array<float, Threads> Lods{
    0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 0.49f, 0.5f,
    0.51f, 1.49f, 1.5f, 2.5f, -0.7f, -3.0f, 4294967296.0f, std::numeric_limits<float>::infinity(),
    std::numeric_limits<float>::quiet_NaN(), 1.25f, 2.75f, 3.2f, 0.25f, 1.75f, -0.4f, 2.0f,
    3.5f, 0.75f, 1.0f, 2.25f, 0.0f, 3.0f, 1.5f, 2.6f,
};

struct Variant {
    std::uint32_t format;
    std::uint32_t clampX;
    std::uint32_t clampY;
    std::uint32_t xyFilter;
    std::uint32_t mipFilter;
    std::uint32_t minLodRaw;
    std::uint32_t maxLodRaw;
    std::int32_t biasRaw;
    std::uint32_t textureMinLodRaw;
    const char* name;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, std::uint32_t minLodRaw = 0u) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (minLodRaw << 8u) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(const Variant& variant) {
    return {
        variant.clampX | (variant.clampY << 3u) | (ClampEdge << 6u),
        variant.minLodRaw | (variant.maxLodRaw << 12u),
        (static_cast<std::uint32_t>(variant.biasRaw) & 0x3fffu) | (variant.xyFilter << 20u) | (variant.xyFilter << 22u) | (variant.mipFilter << 26u),
        0u,
    };
}

std::uint8_t Red(std::uint32_t level, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint8_t>(level * 61u + x * 13u + y * 7u + 3u);
}

void FillTexels() {
    Texels.fill(0);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(Format8888UNorm)));
    Require(surface.mips.size() == Levels, "the test surface has " + std::to_string(surface.mips.size()) + " mips, expected " + std::to_string(Levels));
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = surface.mips.at(level);
        const auto extent = Size >> level;
        for (std::uint32_t y = 0; y < extent; ++y) {
            for (std::uint32_t x = 0; x < extent; ++x) {
                const auto offset = mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u;
                Require(offset + 4u <= Texels.size(), "the test surface does not fit its buffer");
                auto* texel = Texels.data() + offset;
                texel[0] = Red(level, x, y);
                texel[1] = 0x11;
                texel[2] = 0x22;
                texel[3] = 0xff;
            }
        }
    }
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(-0.37f + 0.077f * static_cast<float>(tid));
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(1.31f - 0.053f * static_cast<float>(tid));
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(Lods[tid]);
        Input[tid * 4u + 3u] = 0u;
    }
}

int Address(int coordinate, int extent, std::uint32_t clamp) {
    switch (clamp) {
        case ClampWrap: return ((coordinate % extent) + extent) % extent;
        case ClampMirror: {
            const int folded = ((coordinate % (2 * extent)) + 2 * extent) % (2 * extent);
            return folded < extent ? folded : 2 * extent - 1 - folded;
        }
        case ClampEdge: return std::clamp(coordinate, 0, extent - 1);
        default: return std::min(coordinate < 0 ? -1 - coordinate : coordinate, extent - 1);
    }
}

bool Fallback(const Variant& variant) {
    const auto border = [](std::uint32_t clamp) { return clamp == ClampHalfBorder || clamp == ClampBorder; };
    return border(variant.clampX) || border(variant.clampY);
}

std::uint32_t Level(float lod, const Variant& variant) {
    if (Fallback(variant)) return 0u;
    if (variant.mipFilter == MipNone) return std::min(variant.textureMinLodRaw / 256u, Levels - 1u);
    const auto clamp = [](float value, float low, float high) { return std::isnan(value) ? low : std::min(std::max(value, low), high); };
    const float biased = lod + static_cast<float>(variant.biasRaw) / 256.0f;
    const float sampled = std::min(std::isnan(biased) ? static_cast<float>(variant.minLodRaw) / 256.0f : std::max(biased, static_cast<float>(variant.minLodRaw) / 256.0f), static_cast<float>(variant.maxLodRaw) / 256.0f);
    const float limited = clamp(std::max(sampled, static_cast<float>(variant.textureMinLodRaw) / 256.0f), 0.0f, static_cast<float>(Levels - 1u));
    return static_cast<std::uint32_t>(std::clamp(std::ceil(limited + 0.5f) - 1.0f, 0.0f, static_cast<float>(Levels - 1u)));
}

std::array<std::uint32_t, 4> Expected(std::uint32_t tid, const Variant& variant) {
    const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
    const float v = std::bit_cast<float>(Input[tid * 4u + 1u]);
    const auto level = Level(std::bit_cast<float>(Input[tid * 4u + 2u]), variant);
    const int extent = static_cast<int>(Size >> level);
    const int left = static_cast<int>(std::floor(u * static_cast<float>(extent) - 0.5f));
    const int top = static_cast<int>(std::floor(v * static_cast<float>(extent) - 0.5f));
    const auto texel = [&](int x, int y) {
        if (Fallback(variant) && (x < 0 || y < 0 || x >= extent || y >= extent)) return 0u;
        return static_cast<std::uint32_t>(Red(level, static_cast<std::uint32_t>(Address(x, extent, variant.clampX)), static_cast<std::uint32_t>(Address(y, extent, variant.clampY))));
    };
    return {texel(left, top + 1), texel(left + 1, top + 1), texel(left + 1, top), texel(left, top)};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const Variant& variant) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(variant.format, variant.textureMinLodRaw);
    const auto sampler = SamplerDescriptor(variant);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::span<const std::uint32_t> code(Gather2DLod);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const Variant& variant) {
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, variant);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Gather2DLod.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Expected(tid, variant);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto word = Output[tid * 4u + component];
            std::uint32_t actual = word;
            if (variant.format == Format8888UNorm) {
                const float scaled = std::bit_cast<float>(word) * 255.0f;
                Require(std::fabs(scaled - std::round(scaled)) < 1e-3f, std::string(variant.name) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, not a whole step");
                actual = static_cast<std::uint32_t>(std::lround(scaled));
            }
            Require(actual == expected[component], std::string(variant.name) + ": thread " + std::to_string(tid) + " (LOD " + std::to_string(Lods[tid]) + ", level " + std::to_string(Level(Lods[tid], variant)) + ") component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[component]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexels();
        FillInput();
        constexpr std::uint32_t AllLevels = 15u << 8u;
        Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipPoint, 0u, AllLevels, 0, 0u, "clamp to edge, point"});
        Run(*device, {Format8888UNorm, ClampWrap, ClampWrap, FilterPoint, MipPoint, 0u, AllLevels, 0, 0u, "wrap, point"});
        Run(*device, {Format8888UNorm, ClampMirror, ClampMirror, FilterPoint, MipPoint, 0u, AllLevels, 0, 0u, "mirror, point"});
        Run(*device, {Format8888UNorm, ClampEdge, ClampWrap, FilterBilinear, MipPoint, 0u, AllLevels, 0, 0u, "bilinear x/y"});
        Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipPoint, 320u, 704u, 0, 0u, "MIN_LOD 1.25, MAX_LOD 2.75"});
        Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipPoint, 512u, 512u, 0, 0u, "MIN_LOD = MAX_LOD = 2"});
        Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipPoint, 0u, AllLevels, -192, 0u, "LOD_BIAS -0.75"});
        Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipNone, 0u, AllLevels, 0, 0u, "MIP_FILTER none"});
        Run(*device, {Format8888UInt, ClampEdge, ClampWrap, FilterBilinear, MipPoint, 0u, AllLevels, 0, 0u, "8_8_8_8 UINT with a bilinear sampler"});
        if (AgcDriver::VulkanDeviceTestAccess::ImageViewMinLod(*device)) {
            Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipPoint, 0u, AllLevels, 0, 640u, "T# MIN_LOD 2.5"});
            Run(*device, {Format8888UNorm, ClampEdge, ClampEdge, FilterPoint, MipNone, 0u, AllLevels, 0, 640u, "MIP_FILTER none, T# MIN_LOD 2.5"});
        } else {
            std::puts("T# MIN_LOD cases skipped, the device has no VK_EXT_image_view_min_lod");
        }
        Run(*device, {Format8888UNorm, ClampBorder, ClampHalfBorder, FilterPoint, MipPoint, 0u, AllLevels, 0, 0u, "border modes 6 and 4: base-level gather"});
        std::puts("image 2D explicit-LOD gather tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
