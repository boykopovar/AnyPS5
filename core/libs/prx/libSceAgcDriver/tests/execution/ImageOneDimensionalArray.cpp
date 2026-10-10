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
constexpr std::uint32_t ClampEdge = 2;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 8> Output{};
alignas(4096) std::array<std::uint8_t, 8192> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf09c0f20, 0x00820602, 0xf0001f20, 0x00020a04,
    0x34020085, 0xbf8c3f70, 0xe0781000, 0x80010601, 0xe0781010, 0x80010a01, 0xbf810000,
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

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
}

std::array<std::uint8_t, 4> Texel(std::uint32_t x, std::uint32_t layer) {
    return {static_cast<std::uint8_t>((layer * Width + x) * 9u + 3u), static_cast<std::uint8_t>(x * 30u), static_cast<std::uint8_t>(layer * 100u), 0xff};
}

void FillTexels() {
    Texels.fill(0);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor()));
    const auto& mip = surface.mips.at(0);
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto texel = Texel(x, layer);
            std::copy(texel.begin(), texel.end(), Texels.begin() + static_cast<std::ptrdiff_t>(surface.GuestLayerOffset(layer) + mip.tiledOffset + x * 4u));
        }
    }
}

void FillInput() {
    constexpr std::array<float, 8> layers{0.0f, 1.0f, 2.0f, 0.4f, 1.6f, 2.4f, -1.0f, 5.0f};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(-0.2f + 0.045f * static_cast<float>(tid));
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(layers[tid % layers.size()]);
        Input[tid * 4u + 2u] = tid % Width;
        Input[tid * 4u + 3u] = (tid / Width) % Layers;
    }
}

void RequireTexel(std::uint32_t tid, const char* name, const float* actual, const std::array<std::uint8_t, 4>& expected) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float scaled = actual[component] * 255.0f;
        Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && std::lround(scaled) == expected[component], std::string(name) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
    }
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(-1.0f);
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor();
    const auto sampler = SamplerDescriptor();
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::span<const std::uint32_t> code(Code);
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
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
        const float layer = std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(Layers - 1u));
        const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(u * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
        RequireTexel(tid, "image_sample_lz on a 1D array", &Output[tid * 8u], Texel(x, static_cast<std::uint32_t>(layer)));
        RequireTexel(tid, "image_load on a 1D array", &Output[tid * 8u + 4u], Texel(Input[tid * 4u + 2u], Input[tid * 4u + 3u]));
    }
}

const Testing::Case sampleAndLoad{"ImageOneDimensionalArray_SampleAndLoad_ReturnLayerTexels", [] {
    auto& device = SharedVulkanTestDevice();
    FillTexels();
    FillInput();
    Run(device);
}};

} // namespace
