#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 8;
constexpr std::uint32_t Layers = 3;
constexpr std::uint32_t BaseArray = 8100;
constexpr std::uint32_t LastArray = BaseArray + Layers - 1u;
constexpr std::uint32_t MaxGuestLayers = 8192;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1DArray = 12;
constexpr std::uint32_t ClampEdge = 2;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 8> Output{};
constexpr std::size_t TexelBytes = 4u << 20u;
constexpr std::size_t TexelAlignment = 65536;
std::array<std::uint8_t, TexelBytes + TexelAlignment - 1> TexelStorage{};
const std::span<std::uint8_t, TexelBytes> Texels{reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(TexelStorage.data()) + TexelAlignment - 1) & ~(TexelAlignment - 1)), TexelBytes};

constexpr std::size_t ClearKeyBytes = TexelBytes / 256u;
alignas(256) std::array<std::uint8_t, ClearKeyBytes> ClearKeys{};

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf09c0f20, 0x00820602, 0xf0001f20, 0x00020a04,
    0x34020085, 0xbf8c3f70, 0xe0781000, 0x80010601, 0xe0781010, 0x80010a01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleOnlyCode{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf09c0f20, 0x00820602, 0x34020085, 0xbf8c3f70,
    0xe0781000, 0x80010601, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t baseArray = BaseArray, std::uint32_t lastArray = LastArray, bool dccClear = false) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    const auto meta = dccClear ? static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ClearKeys.data())) >> 8u : 0u;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (Type1DArray << 28u),
        lastArray | (baseArray << 16u), 0u,
        dccClear ? (1u << 20u) | (1u << 21u) | (static_cast<std::uint32_t>(meta & 0xffu) << 24u) : 0u,
        static_cast<std::uint32_t>(meta >> 8u),
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
}

std::array<std::uint8_t, 4> Texel(std::uint32_t x, std::uint32_t guestLayer) {
    return {static_cast<std::uint8_t>((guestLayer * Width + x) * 9u + 3u), static_cast<std::uint8_t>(x * 30u + guestLayer % 7u), static_cast<std::uint8_t>(guestLayer), 0xff};
}

void FillTexels() {
    std::ranges::fill(Texels, std::uint8_t{0});
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor()));
    const auto& mip = surface.mips.at(0);
    Require(surface.layers == LastArray + 1u && surface.guestBytes <= Texels.size(), "the test array does not fit its storage");
    for (std::uint32_t layer = 0; layer <= LastArray; ++layer) {
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

void Expect(std::uint32_t tid, const char* name, const float* actual, const std::array<std::uint8_t, 4>& expected) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float scaled = actual[component] * 255.0f;
        Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && std::lround(scaled) == expected[component], std::string(name) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t baseArray = BaseArray, std::uint32_t lastArray = LastArray, bool dccClear = false) {
    Output.fill(-1.0f);
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(baseArray, lastArray, dccClear);
    const auto sampler = SamplerDescriptor();
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::span<const std::uint32_t> code = dccClear ? std::span<const std::uint32_t>(SampleOnlyCode) : std::span<const std::uint32_t>(Code);
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
    const auto clearsBefore = AgcDriver::Graphics::SampledTexturesCleared();
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    if (dccClear) {
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(texture);
        Require(AgcDriver::Graphics::TextureClearKeys(resource, AgcDriver::Graphics::DescribeSurface(resource).guestBytes) == DccKeys::Clear0000, "the 1D array fixture does not expose a DCC zero clear");
        if (std::getenv("APS5_NO_TEXTURE_CACHE") == nullptr) Require(AgcDriver::Graphics::SampledTexturesCleared() == clearsBefore + 1u, "the over-limit DCC row window was not materialized by a GPU clear");
    }
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
        const float layer = std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(lastArray - baseArray));
        const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(u * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
        if (dccClear) {
            Expect(tid, "image_sample_lz of a DCC-cleared row window", &Output[tid * 8u], {0, 0, 0, 0});
        } else {
            Expect(tid, "image_sample_lz on a window of a 1D array past maxImageArrayLayers", &Output[tid * 8u], Texel(x, baseArray + static_cast<std::uint32_t>(layer)));
            Expect(tid, "image_load on a window of a 1D array past maxImageArrayLayers", &Output[tid * 8u + 4u], Texel(Input[tid * 4u + 2u], baseArray + Input[tid * 4u + 3u]));
        }
    }
}

}

int main() {
    try {
        Require(reinterpret_cast<std::uintptr_t>(Texels.data()) % TexelAlignment == 0, "the test array is not 64 KiB aligned");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexels();
        FillInput();
        Run(*device);
        const auto& limits = device->graphicsContext().limits;
        const auto limit = limits.maxImageArrayLayers;
        Require(limit != 0u, "the device reports no maxImageArrayLayers");
        if (limit >= MaxGuestLayers - 1u) {
            std::fprintf(stderr, "skipped: maxImageArrayLayers %u leaves no encodable over-limit array index below %u\n", limit, MaxGuestLayers);
            return VulkanTestSkipped;
        }
        if (limit + 1u > limits.maxImageDimension2D / 2u) {
            std::fprintf(stderr, "skipped: maxImageArrayLayers %u needs %u physical rows but maxImageDimension2D %u permits %u\n", limit, 2u * (limit + 1u), limits.maxImageDimension2D, limits.maxImageDimension2D / 2u);
            return VulkanTestSkipped;
        }
        const auto clearResource = AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(1u, limit + 1u, true));
        const auto clearBytes = AgcDriver::Graphics::DescribeSurface(clearResource).guestBytes;
        Require(clearBytes <= Texels.size() && AgcDriver::Graphics::DccKeyBytes(clearBytes) <= ClearKeys.size(), "the over-limit clear fixture exceeds its guest texel or DCC-key storage");
        Run(*device, 1u, limit + 1u, true);
        try {
            Run(*device, 0);
        } catch (const std::exception& error) {
            if (std::string(error.what()).find("an image view of more array slices than maxImageArrayLayers") == std::string::npos) throw;
        }
        std::puts("a window of a 1D array past maxImageArrayLayers was sampled and loaded from its viewed layers");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
