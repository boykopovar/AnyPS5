#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 8;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1DArray = 12;
constexpr std::uint32_t LinearTile = 0;
constexpr std::uint32_t Z64KBXTile = 0x18;
constexpr std::size_t StorageBytes = 8u << 20u;
constexpr std::size_t StorageAlignment = 65536;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 8> Output{};
std::array<std::uint8_t, StorageBytes + StorageAlignment - 1> StorageRaw{};
const std::span<std::uint8_t> Storage{reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(StorageRaw.data()) + StorageAlignment - 1) & ~(StorageAlignment - 1)), StorageBytes};

alignas(256) constexpr std::array<std::uint32_t, 13> LoadCode{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf0001f20, 0x00020a04, 0x34020085,
    0xbf8c3f70, 0xe0781000, 0x80010601, 0xe0781010, 0x80010a01, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t layers, std::uint32_t tile) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Storage.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (tile << 20u) | (Type1DArray << 28u),
        (layers - 1u),
        0u,
        0u,
        0u,
    };
}

std::array<std::uint8_t, 4> Texel(std::uint32_t x, std::uint32_t layer) {
    return {static_cast<std::uint8_t>((layer * Width + x) * 9u + 3u), static_cast<std::uint8_t>(x * 30u + layer % 7u), static_cast<std::uint8_t>(layer), 0xff};
}

std::uint64_t EquationOffset(const AgcDriver::Graphics::TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t layer) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16u; ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12) & (mask & 0xfff000u)) ^ ((layer << 24u) & (mask & 0xff000000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

void FillTexels(std::uint32_t layers, std::uint32_t tile) {
    std::ranges::fill(Storage, std::uint8_t{0});
    const auto descriptor = AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(layers, tile));
    const auto surface = AgcDriver::Graphics::DescribeSurface(descriptor);
    const auto& mip = surface.mips.at(0);
    Require(surface.guestBytes <= Storage.size(), "the test array does not fit its storage");
    const auto* equation = tile == LinearTile ? nullptr : AgcDriver::Graphics::FindTextureSwizzleEquation(AgcDriver::Graphics::XorSwizzleMode(descriptor.tileMode), 4u);
    Require(tile == LinearTile || equation != nullptr, "the test array has no swizzle equation");
    std::array<std::uint32_t, 3> layout{};
    if (equation != nullptr) layout = AgcDriver::Graphics::ThinBlockLayout(descriptor.tileMode, 4u);
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto offset = surface.GuestLayerOffset(layer) + mip.tiledOffset;
            if (equation != nullptr) offset += (x / layout[1]) * layout[0] + EquationOffset(*equation, x, 0u, layer);
            else offset += x * 4u;
            const auto texel = Texel(x, layer);
            std::copy(texel.begin(), texel.end(), Storage.begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }
}

void FillInput(std::uint32_t layers, bool oversized) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = 0u;
        Input[tid * 4u + 1u] = 0u;
        Input[tid * 4u + 2u] = tid % Width;
        Input[tid * 4u + 3u] = oversized ? (tid % 4u == 0u ? layers - 1u : tid % 4u == 1u ? layers - 2u : tid) : tid % layers;
    }
}

void Expect(std::uint32_t tid, const float* actual, const std::array<std::uint8_t, 4>& expected) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float scaled = actual[component] * 255.0f;
        Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && std::lround(scaled) == expected[component], "image_load on a 1D array: thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t layers, std::uint32_t tile, bool useCache) {
    Output.fill(-1.0f);
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(layers, tile);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> code(LoadCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = useCache;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Expect(tid, &Output[tid * 8u + 4u], Texel(Input[tid * 4u + 2u], Input[tid * 4u + 3u]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto maxLayers = device->Target().maxImageArrayLayers;
        Require(maxLayers >= 8u && maxLayers < 8192u, "the device reports an unusable maxImageArrayLayers");
        const auto oversized = maxLayers + 1u;
        FillTexels(oversized, LinearTile);
        FillInput(oversized, true);
        Run(*device, oversized, LinearTile, true);
        Run(*device, oversized, LinearTile, true);
        FillTexels(8u, Z64KBXTile);
        FillInput(8u, false);
        Run(*device, 8u, Z64KBXTile, false);
        Run(*device, 8u, Z64KBXTile, true);
        std::puts("oversized linear and fitting tiled 1D arrays loaded correctly");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
