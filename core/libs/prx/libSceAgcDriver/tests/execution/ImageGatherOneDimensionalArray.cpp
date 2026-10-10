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
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 8;
constexpr std::uint32_t Layers = 3;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1DArray = 12;
constexpr std::uint32_t ClampWrap = 0;
constexpr std::uint32_t ClampEdge = 2;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 8192> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> Gather1DArray{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0120, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 10> Gather1DArrayOffset{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf15c0120, 0x00820602, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};

struct Variant {
    std::span<const std::uint32_t> code;
    bool offset;
    std::uint32_t clamp;
    const char* name;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (Type1DArray << 28u),
        Layers - 1u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t clamp) {
    return {clamp | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
}

std::uint8_t Red(std::uint32_t layer, std::uint32_t x) {
    return static_cast<std::uint8_t>((layer * Width + x) * 10u + 5u);
}

void FillTexels() {
    Texels.fill(0);
    const auto descriptor = TextureDescriptor();
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    const auto& mip = surface.mips.at(0);
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto* texel = Texels.data() + surface.GuestLayerOffset(layer) + mip.tiledOffset + x * 4u;
            texel[0] = Red(layer, x);
            texel[1] = 0x11;
            texel[2] = 0x22;
            texel[3] = 0xff;
        }
    }
}

void FillInput() {
    constexpr std::array<float, 8> layers{0.0f, 1.0f, 2.0f, 0.4f, 1.6f, 2.4f, -1.0f, 5.0f};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto offset = static_cast<std::int32_t>(tid % 11u) - 5;
        Input[tid * 4u + 0u] = (static_cast<std::uint32_t>(offset) & 0x3fu) | (tid % 3u == 0u ? 0x3f00u : 0u);
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(-0.3f + 0.05f * static_cast<float>(tid));
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(layers[tid % layers.size()]);
        Input[tid * 4u + 3u] = 0u;
    }
}

std::uint32_t Texel(int x, std::uint32_t layer, std::uint32_t clamp) {
    const int size = static_cast<int>(Width);
    const int address = clamp == ClampEdge ? std::clamp(x, 0, size - 1) : ((x % size) + size) % size;
    return Red(layer, static_cast<std::uint32_t>(address));
}

std::array<std::uint32_t, 4> Expected(std::uint32_t tid, const Variant& variant) {
    const float u = std::bit_cast<float>(Input[tid * 4u + 1u]);
    int left = static_cast<int>(std::floor(u * static_cast<float>(Width) - 0.5f));
    if (variant.offset) {
        const auto field = Input[tid * 4u] & 0x3fu;
        left += static_cast<int>(field ^ 0x20u) - 0x20;
    }
    const float rounded = std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 2u]));
    const auto layer = static_cast<std::uint32_t>(std::clamp(rounded, 0.0f, static_cast<float>(Layers - 1u)));
    const auto first = Texel(left, layer, variant.clamp);
    const auto second = Texel(left + 1, layer, variant.clamp);
    return {first, second, second, first};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const Variant& variant) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor();
    const auto sampler = SamplerDescriptor(variant.clamp);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(variant.code.data()), std::as_bytes(variant.code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(variant.code.data()), variant.code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const Variant& variant) {
    Output.fill(-1.0f);
    const auto result = Compile(device, variant);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(variant.code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Expected(tid, variant);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float actual = Output[tid * 4u + component];
            const float scaled = actual * 255.0f;
            Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && static_cast<std::uint32_t>(std::lround(scaled)) == expected[component], std::string(variant.name) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
        }
    }
}

void Prepare() {
    FillTexels();
    FillInput();
}

const Testing::Case clampEdge{"ImageGatherOneDimensionalArray_GatherClampToEdge_ReturnsNeighborTexels", [] {
    auto& device = SharedVulkanTestDevice();
    Prepare();
    Run(device, {Gather1DArray, false, ClampEdge, "1D array gather, clamp to edge"});
}};

const Testing::Case wrap{"ImageGatherOneDimensionalArray_GatherWrap_ReturnsWrappedTexels", [] {
    auto& device = SharedVulkanTestDevice();
    Prepare();
    Run(device, {Gather1DArray, false, ClampWrap, "1D array gather, wrap"});
}};

const Testing::Case offsetClampEdge{"ImageGatherOneDimensionalArray_GatherOffsetClampToEdge_ReturnsOffsetTexels", [] {
    auto& device = SharedVulkanTestDevice();
    Prepare();
    Run(device, {Gather1DArrayOffset, true, ClampEdge, "1D array gather with offsets, clamp to edge"});
}};

const Testing::Case offsetWrap{"ImageGatherOneDimensionalArray_GatherOffsetWrap_ReturnsWrappedOffsetTexels", [] {
    auto& device = SharedVulkanTestDevice();
    Prepare();
    Run(device, {Gather1DArrayOffset, true, ClampWrap, "1D array gather with offsets, wrap"});
}};

} // namespace
