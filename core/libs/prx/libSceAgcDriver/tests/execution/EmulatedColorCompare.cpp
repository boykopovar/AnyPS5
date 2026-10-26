#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 2;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format8UNorm = 1;
constexpr std::uint32_t Format8_8UNorm = 14;
constexpr std::uint32_t Format16UNorm = 7;
constexpr std::uint32_t Format1616UNorm = 23;
constexpr std::uint32_t Format9995Float = 132;
constexpr std::uint32_t Format8Srgb = 128;
constexpr std::uint32_t Format8_8Srgb = 129;
constexpr std::uint32_t Format8888Srgb = 130;
constexpr std::uint32_t Format8888UInt = 60;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t LessEqual = 3;
constexpr std::uint32_t GreaterEqual = 6;
constexpr std::uint32_t ClampWrap = 0;
constexpr std::uint32_t ClampMirror = 1;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t ClampHalfBorder = 4;
constexpr std::uint32_t ClampBorder = 6;
constexpr std::uint32_t BorderBlack = 0;
constexpr std::uint32_t BorderWhite = 2;
constexpr std::uint32_t BorderTable = 3;
constexpr std::uint32_t FilterPoint = 0;
constexpr std::uint32_t FilterBilinear = 1;
constexpr std::uint32_t FilterAnisoPoint = 2;
constexpr std::uint32_t FilterAnisoBilinear = 3;
constexpr std::uint32_t ReductionMin = 1;
constexpr std::array<std::uint8_t, 4> Red{0, 64, 128, 255};

alignas(256) std::array<float, Threads * 3> Input{};
alignas(256) std::array<float, Threads * 4> CubeInput{};
alignas(256) std::array<float, Threads * 4> Output{};
std::uint32_t Swizzle = 0xfacu;
alignas(256) std::array<std::uint32_t, Threads * 3> Extra{};
alignas(4096) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0bc0108, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> ArrayCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0bc0128, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> CubeCode{
    0x16140090, 0xe0381000, 0x8000010a, 0xbf8c3f70, 0xf0bc0118, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> OffsetCode{
    0x1614008c, 0xe03c1000, 0x8000030a, 0xe03c1000, 0x8005060a, 0xbf8c3f70, 0x7e020306, 0x7e040307,
    0x7e0c0308, 0xf0f80108, 0x00820901, 0xbf8c3f70, 0x34160082, 0xe0701000, 0x8001090b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> QuadCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0x7e0802f3, 0x7e0a02f3, 0x7e0c02f3, 0x7e0e02f3, 0xbf8c3f70, 0xf0bc0f08, 0x00820401, 0xbf8c3f70, 0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> PairCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0x7e0802f3, 0x7e0a02f3, 0x7e0c02f3, 0x7e0e02f3, 0xbf8c3f70, 0xf0bc0608, 0x00820401, 0xbf8c3f70, 0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

struct Sampler {
    std::uint32_t clamp;
    std::uint32_t filter;
    std::uint32_t border = BorderBlack;
    std::uint32_t reduction = 0;
    std::uint32_t lodBias = 0;
    bool disableCubeWrap = false;
    std::uint32_t function = LessEqual;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, bool cube = false) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        Swizzle | ((cube ? TypeCube : Type2D) << 28u),
        cube ? 5u : 0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(const Sampler& sampler) {
    return {
        sampler.clamp | (sampler.clamp << 3u) | (ClampEdge << 6u) | (sampler.function << 12u) | (sampler.reduction << 29u) | (sampler.disableCubeWrap ? (1u << 28u) : 0u),
        0u,
        sampler.lodBias | (sampler.filter << 20u) | (sampler.filter << 22u),
        sampler.border << 30u,
    };
}

void FillTexels() {
    Texels.fill(0);
    const auto descriptor = TextureDescriptor(Format1616UNorm);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    const auto& mip = surface.mips.at(0);
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            auto* texel = Texels.data() + mip.tiledOffset + y * mip.pitchBytes + x * 4u;
            texel[0] = Red[y * Side + x];
            texel[1] = Red[y * Side + x];
            texel[2] = 0x22;
            texel[3] = 0xff;
        }
    }
}

void FillCubeTexels() {
    Texels.fill(0);
    const auto descriptor = TextureDescriptor(Format1616UNorm, true);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(surface.layers == 6u && surface.guestBytes <= Texels.size(), "cube descriptor did not expose six faces within texture storage");
    const auto& mip = surface.mips.at(0);
    for (std::uint32_t face = 0; face < 6u; ++face) {
        for (std::uint32_t y = 0; y < Side; ++y) {
            for (std::uint32_t x = 0; x < Side; ++x) {
                auto* texel = Texels.data() + surface.GuestLayerOffset(face) + mip.tiledOffset + y * mip.pitchBytes + x * 4u;
                texel[0] = static_cast<std::uint8_t>(face == 0u ? 255u : 0u);
                texel[1] = texel[0];
                texel[3] = 0xffu;
            }
        }
    }
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 3u + 0u] = -0.6f + 0.07f * static_cast<float>(tid);
        Input[tid * 3u + 1u] = 1.55f - 0.065f * static_cast<float>(tid);
        Input[tid * 3u + 2u] = -0.25f + 0.05f * static_cast<float>((tid * 7u) % 32u);
    }
    Input[0] = 0.0f; Input[1] = 0.0f; Input[2] = 0.0f;
    Input[3] = 0.999f; Input[4] = 0.999f; Input[5] = 1.2f;
    Input[6] = 0.6f; Input[7] = -0.1f; Input[8] = 0.5f;
    Input[9] = 0.3f; Input[10] = 0.5f; Input[11] = 1.1f;
    Input[12] = 0.9f; Input[13] = 1.05f; Input[14] = -0.05f;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = static_cast<std::int32_t>(tid % 7u) - 3;
        const auto y = static_cast<std::int32_t>((tid / 7u) % 5u) - 2;
        Extra[tid * 3u + 0u] = (static_cast<std::uint32_t>(x) & 0x3fu) | ((static_cast<std::uint32_t>(y) & 0x3fu) << 8u) | (tid % 3u == 0u ? 0x3f0000u : 0u);
        Extra[tid * 3u + 1u] = std::bit_cast<std::uint32_t>(-2.5f + 0.25f * static_cast<float>(tid % 21u));
        Extra[tid * 3u + 2u] = std::bit_cast<std::uint32_t>(0.5f * static_cast<float>(tid % 5u));
    }
    Extra[0] = 0x00001f20u;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, std::span<const std::uint32_t> code = Code, bool useCache = false, bool nativeSampleOffsets = true, bool cube = false) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = cube ? BufferDescriptor(CubeInput.data(), static_cast<std::uint32_t>(sizeof(CubeInput))) : BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(format, cube);
    const auto samplerWords = SamplerDescriptor(sampler);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(samplerWords.begin(), samplerWords.end(), userData.begin() + 16);
    const auto extra = BufferDescriptor(Extra.data(), static_cast<std::uint32_t>(sizeof(Extra)));
    std::copy(extra.begin(), extra.end(), userData.begin() + 20);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = useCache;
    request.target.nonConstantImageOffsets = request.target.nonConstantImageOffsets && nativeSampleOffsets;
    return ShaderRecompiler::Recompile(request);
}

std::uint64_t Run(AgcDriver::VulkanDevice& device, const Sampler& sampler, const char* name, bool offsets = false, bool useCache = false, bool array = false, std::uint64_t expectedVariant = 0) {
    Output.fill(-1.0f);
    const std::span<const std::uint32_t> code = offsets ? std::span<const std::uint32_t>(OffsetCode) : array ? std::span<const std::uint32_t>(ArrayCode) : std::span<const std::uint32_t>(Code);
    const auto result = Compile(device, Format1616UNorm, sampler, code, useCache);
    if (expectedVariant != 0) Require(result.cacheHit && result.variantId == expectedVariant, std::string(name) + " did not reuse the compiled comparison artifact");
    for (const auto& binding : result.bindings) {
        Require(binding.role != ShaderRecompiler::DescriptorRole::GuestSamplers, "emulated comparison retained a sampler binding");
        Require(std::all_of(binding.imageSamplers.begin(), binding.imageSamplers.end(), [](auto mask) { return mask == 0u; }), "emulated comparison retained an unused sampler association");
    }
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Require(Output[tid] == 0.0f, std::string(name) + ": thread " + std::to_string(tid) + " compared to " + std::to_string(Output[tid]) + ", expected 0");
    }
    return result.variantId;
}

constexpr std::uint32_t Select(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return x | (y << 3u) | (z << 6u) | (w << 9u);
}

void RunChannels(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::array<std::uint32_t, 4> channels, std::uint32_t registers, std::uint32_t firstChannel, const char* name) {
    const Sampler sampler{ClampEdge, FilterBilinear};
    Swizzle = Select(channels[0], channels[1], channels[2], channels[3]);
    Output.fill(-2.0f);
    const auto result = Compile(device, Format1616UNorm, sampler, code);
    Swizzle = 0xfacu;
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t reg = 0; reg < 4u; ++reg) {
            float expected = -1.0f;
            if (reg < registers) {
                const auto select = channels[firstChannel + reg];
                expected = select == 0u ? 0.0f : select == 1u ? 1.0f : 0.0f;
            }
            const float actual = Output[tid * 4u + reg];
            Require(std::fabs(actual - expected) <= 1e-4f, std::string(name) + ": thread " + std::to_string(tid) + " register " + std::to_string(reg) + " returned " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void RunCube(AgcDriver::VulkanDevice& device, const Sampler& sampler, const char* name) {
    FillCubeTexels();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        CubeInput[tid * 4u + 0u] = 0.5f;
        CubeInput[tid * 4u + 1u] = 1.0f;
        CubeInput[tid * 4u + 2u] = 0.0f;
        CubeInput[tid * 4u + 3u] = 0.0f;
    }
    Output.fill(-1.0f);
    const auto result = Compile(device, Format1616UNorm, sampler, CubeCode, false, true, true);
    for (const auto& binding : result.bindings) {
        Require(binding.role != ShaderRecompiler::DescriptorRole::GuestSamplers, "cube emulated comparison retained a sampler binding");
        Require(std::all_of(binding.imageSamplers.begin(), binding.imageSamplers.end(), [](auto mask) { return mask == 0u; }), "cube emulated comparison retained an unused sampler association");
    }
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CubeCode.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Require(Output[tid] == 0.0f, std::string(name) + ": +X cube face comparison returned " + std::to_string(Output[tid]) + ", expected 0");
    }
}

bool BindsDepthCompare(const ShaderRecompiler::RecompileResult& result) {
    return std::any_of(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) {
        return std::any_of(binding.imageDepthCompare.begin(), binding.imageDepthCompare.end(), [](bool compare) { return compare; });
    });
}

void RunZeroCube(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, const char* name) {
    FillCubeTexels();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        CubeInput[tid * 4u + 0u] = 0.5f;
        CubeInput[tid * 4u + 1u] = 1.0f;
        CubeInput[tid * 4u + 2u] = 0.0f;
        CubeInput[tid * 4u + 3u] = 0.0f;
    }
    Output.fill(-1.0f);
    const auto result = Compile(device, format, sampler, CubeCode, false, true, true);
    Require(!BindsDepthCompare(result), std::string(name) + " kept a depth-compare binding");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CubeCode.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Require(Output[tid] == 0.0f, std::string(name) + ": thread " + std::to_string(tid) + " compared to " + std::to_string(Output[tid]) + ", expected 0");
    }
}

void RunZero(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, const char* name, bool useCache, std::uint64_t expectedVariant) {
    Output.fill(-1.0f);
    const auto result = Compile(device, format, sampler, Code, useCache);
    if (useCache) Require(result.cacheHit && result.variantId == expectedVariant, std::string(name) + " did not reuse the compiled comparison artifact");
    Require(!BindsDepthCompare(result), std::string(name) + " kept a depth-compare binding");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Require(Output[tid] == 0.0f, std::string(name) + ": thread " + std::to_string(tid) + " compared to " + std::to_string(Output[tid]) + ", expected 0");
    }
}

void Reject(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, std::string_view reason, std::span<const std::uint32_t> code = Code, bool nativeSampleOffsets = true, bool cube = false) {
    try {
        static_cast<void>(Compile(device, format, sampler, code, false, nativeSampleOffsets, cube));
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected rejection: ") + error.what());
        return;
    }
    Require(false, std::string("expected rejection: ") + std::string(reason));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool lodClamp = TargetHasCapability(device->Target(), spv::CapabilityMinLod);
        const auto* forced = std::getenv("APS5_SRGB_SHADER_DECODE");
        if (forced != nullptr && std::string_view(forced) == "1") Require((device->Target().srgbDecodeFormats & 3u) == 3u, "forced R8 and RG8 sRGB shader decoding is unavailable");
        FillTexels();
        FillInput();
        RunCube(*device, {ClampEdge, FilterPoint}, "point cube, clamp to edge");
        RunCube(*device, {ClampEdge, FilterBilinear}, "bilinear cube without DISABLE_CUBE_WRAP");
        for (std::uint32_t function = 0; function < 8u; ++function) {
            const auto name = "bilinear 8_8_8_8 srgb cube function " + std::to_string(function);
            RunZeroCube(*device, Format8888Srgb, {ClampEdge, FilterBilinear, BorderBlack, 0u, 0u, false, function}, name.c_str());
        }
        RunZeroCube(*device, Format8Srgb, {ClampEdge, FilterBilinear}, "bilinear 8 srgb cube without DISABLE_CUBE_WRAP");
        RunZeroCube(*device, Format8_8Srgb, {ClampEdge, FilterBilinear}, "bilinear 8_8 srgb cube without DISABLE_CUBE_WRAP");
        RunCube(*device, {ClampEdge, FilterBilinear, BorderBlack, 0u, 0u, true}, "bilinear cube with DISABLE_CUBE_WRAP");
        Reject(*device, Format9995Float, {ClampEdge, FilterPoint}, "implemented only for floating-point, UNORM and SNORM formats");
        for (std::uint32_t function = 0; function < 8u; ++function) {
            const auto name = "bilinear 8_8_8_8 unorm cube function " + std::to_string(function);
            RunZeroCube(*device, Format8888UNorm, {ClampEdge, FilterBilinear, BorderBlack, 0u, 0u, false, function}, name.c_str());
        }
        RunZeroCube(*device, Format8888UNorm, {ClampEdge, FilterAnisoBilinear, BorderBlack, 0u, 0u, false, GreaterEqual}, "anisotropic bilinear 8_8_8_8 unorm cube without DISABLE_CUBE_WRAP");
        RunZero(*device, Format1616UNorm, {ClampEdge, FilterAnisoBilinear}, "anisotropic bilinear 16_16 unorm", false, 0u);
        RunZero(*device, Format1616UNorm, {ClampEdge, FilterAnisoPoint}, "anisotropic point 16_16 unorm", false, 0u);
        FillTexels();
        Require(BindsDepthCompare(Compile(*device, Format16UNorm, {ClampEdge, FilterPoint})), "an R16 unorm texture left the native comparison path");
        Require(BindsDepthCompare(Compile(*device, Format32Float, {ClampEdge, FilterBilinear})), "an R32 float texture left the native comparison path");
        Require(!BindsDepthCompare(Compile(*device, Format8888UNorm, {ClampEdge, FilterBilinear})), "a color texture kept a depth-compare binding");
        for (const auto format : {1u, 2u, 8u, 13u, 14u, 15u, 23u, 24u, 29u, 36u, 50u, 51u, 56u, 57u, 65u, 66u, 71u, 77u}) {
            if (format == 51u && !device->SupportsImageFormat(VK_FORMAT_A2B10G10R10_SNORM_PACK32, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
                std::puts("measured GFX10 color format 51 returns zero: skipped, Vulkan device does not support the image format");
                continue;
            }
            const auto name = "measured GFX10 color format " + std::to_string(format) + " returns zero";
            RunZero(*device, format, {ClampEdge, FilterPoint}, name.c_str(), false, 0u);
        }
        for (std::uint32_t function = 0; function < 8u; ++function) {
            const auto name = "16_16 unorm compare function " + std::to_string(function) + " returns zero";
            RunZero(*device, Format1616UNorm, {ClampEdge, FilterPoint, BorderBlack, 0u, 0u, false, function}, name.c_str(), false, 0u);
        }
        for (const bool useCache : {false, true}) {
            const auto variant = useCache ? Run(*device, {ClampEdge, FilterPoint}, "cached UNORM before sRGB", false, true) : 0u;
            if (useCache) Require(variant != 0, "the cached comparison has no compiled artifact identity");
            for (const auto format : {Format8Srgb, Format8_8Srgb, Format8888Srgb}) {
                for (std::uint32_t function = 0; function < 8u; ++function) {
                    for (const auto filter : {FilterPoint, FilterBilinear}) {
                        const auto name = std::string(useCache ? "cached" : "uncached") + " sRGB format " + std::to_string(format) + " function " + std::to_string(function) + (filter == FilterPoint ? " point" : " bilinear");
                        RunZero(*device, format, {ClampEdge, filter, BorderBlack, 0u, 0u, false, function}, name.c_str(), useCache, variant);
                    }
                }
            }
            if (useCache) Run(*device, {ClampEdge, FilterPoint}, "cached UNORM after sRGB", false, true, false, variant);
        }
        for (const auto format : {Format8UNorm, Format8_8UNorm, Format8888UNorm}) {
            for (std::uint32_t function = 0; function < 8u; ++function) {
                for (const auto filter : {FilterPoint, FilterBilinear, FilterAnisoBilinear}) {
                    if (format != Format8888UNorm && function != LessEqual) continue;
                    const auto name = "8-bit unorm format " + std::to_string(format) + " function " + std::to_string(function) + " filter " + std::to_string(filter);
                    RunZero(*device, format, {ClampEdge, filter, BorderBlack, 0u, 0u, false, function}, name.c_str(), false, 0u);
                }
            }
        }
        RunZero(*device, Format1616UNorm, {ClampEdge, FilterPoint}, "16_16 unorm comparison returns zero", false, 0u);
        Run(*device, {ClampEdge, FilterPoint}, "point, clamp to edge");
        Run(*device, {ClampWrap, FilterPoint}, "point, wrap");
        if (lodClamp) Run(*device, {ClampWrap, FilterPoint}, "point, wrap with negative offsets", true);
        Run(*device, {ClampEdge, FilterBilinear}, "bilinear, 2D array instruction on a 2D texture", false, false, true);
        Run(*device, {ClampEdge, FilterBilinear}, "bilinear, clamp to edge");
        Run(*device, {ClampWrap, FilterBilinear}, "bilinear, wrap");
        Run(*device, {ClampBorder, FilterPoint, BorderWhite}, "point, white border");
        Run(*device, {ClampBorder, FilterPoint, BorderBlack}, "point, black border");
        Run(*device, {ClampBorder, FilterBilinear, BorderWhite}, "bilinear, white border");
        Run(*device, {ClampBorder, FilterBilinear, BorderBlack}, "bilinear, black border");
        Run(*device, {ClampEdge, FilterPoint, BorderBlack, 0u, 0x0140u}, "point, clamp to edge, sampler LOD bias");
        if (lodClamp) {
            Run(*device, {ClampEdge, FilterPoint}, "point, clamp to edge, offsets, bias and LOD clamp", true);
            Run(*device, {ClampWrap, FilterBilinear}, "bilinear, wrap, offsets, bias and LOD clamp", true);
            Run(*device, {ClampBorder, FilterBilinear, BorderWhite, 0u, 0x3f00u}, "bilinear, white border, offsets, bias and LOD clamp", true);
        }
        for (unsigned iteration = 0; iteration < 2; ++iteration) {
            Run(*device, {ClampEdge, FilterPoint}, "cached point comparison", false, true);
            if (lodClamp) Run(*device, {ClampWrap, FilterBilinear}, "cached bilinear comparison with offsets", true, true);
        }
        Reject(*device, Format32Float, {ClampEdge, FilterPoint}, "native comparison with a nonconstant texel offset requires", OffsetCode, false);
        RunChannels(*device, QuadCode, {4, 5, 6, 7}, 4, 0, "view R,G,B,A");
        RunChannels(*device, QuadCode, {0, 0, 0, 0}, 4, 0, "view 0,0,0,0");
        RunChannels(*device, QuadCode, {1, 1, 1, 1}, 4, 0, "view 1,1,1,1");
        RunChannels(*device, QuadCode, {0, 4, 1, 5}, 4, 0, "view 0,R,1,G");
        RunChannels(*device, QuadCode, {4, 0, 0, 1}, 4, 0, "view R,0,0,1");
        RunChannels(*device, QuadCode, {5, 4, 5, 6}, 4, 0, "view G,R,B,A");
        RunChannels(*device, QuadCode, {7, 4, 4, 4}, 4, 0, "view A,R,R,R");
        RunChannels(*device, PairCode, {0, 4, 4, 4}, 2, 1, "view 0,R,R,R, dmask 6");
        Swizzle = Select(4, 2, 4, 4);
        Reject(*device, Format8888UNorm, {ClampEdge, FilterPoint}, "every view channel is a constant or a color channel");
        Swizzle = Select(5, 5, 5, 5);
        Reject(*device, Format8888UNorm, {ClampEdge, FilterPoint}, "a view channel selects red");
        Swizzle = 0xfacu;
        Reject(*device, Format8888UInt, {ClampEdge, FilterPoint}, "unsupported format");
        Reject(*device, Format8888UNorm, {ClampMirror, FilterPoint}, "wrap, clamp-to-edge or clamp-to-border");
        Reject(*device, Format8888UNorm, {ClampHalfBorder, FilterPoint}, "wrap, clamp-to-edge or clamp-to-border");
        Reject(*device, Format8888UNorm, {ClampBorder, FilterPoint, BorderTable}, "border color table");
        Reject(*device, Format8888UNorm, {ClampEdge, FilterBilinear, BorderBlack, ReductionMin}, "min or max reduction");
        std::puts(lodClamp ? "emulated color compare tests passed" : "emulated color compare tests passed, the offset and LOD clamp cases were skipped without shaderResourceMinLod");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
