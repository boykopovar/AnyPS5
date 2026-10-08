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
#include <cstring>
#include <iostream>
#include <limits>
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
constexpr std::uint32_t Format8888UInt = 60;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Format16161616Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Layers = 4;
constexpr std::uint32_t LessEqual = 3;
constexpr std::uint32_t Greater = 4;
constexpr std::uint32_t NotEqual = 5;
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
constexpr std::uint32_t FilterAnisoBilinear = 3;
constexpr std::uint32_t ReductionMin = 1;
constexpr std::array<std::array<std::uint8_t, 4>, Layers> Red{{{0, 64, 128, 255}, {230, 240, 250, 255}, {5, 15, 25, 35}, {200, 220, 210, 245}}};
constexpr std::array<float, 4> HalfRed{0.25f, std::numeric_limits<float>::quiet_NaN(), 0.5f, 0.0f};
constexpr std::array<std::uint16_t, 4> HalfRedBits{0x3400u, 0x7e00u, 0x3800u, 0x0000u};

alignas(256) std::array<float, Threads * 3> Input{};
alignas(256) std::array<float, Threads * 4> InputArray{};
alignas(256) std::array<float, Threads * 3> InputNaN{};
alignas(256) std::array<float, Threads> Output{};
alignas(256) std::array<std::uint32_t, Threads * 3> Extra{};
alignas(4096) std::array<std::uint8_t, 65536> Texels{};
alignas(4096) std::array<std::uint8_t, 4096> HalfTexels{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0bc0108, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> ArrayCode{
    0x16140090, 0xe0381000, 0x8000010a, 0xbf8c3f70, 0xf0bc0128, 0x00820501, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001050b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> OffsetCode{
    0x1614008c, 0xe03c1000, 0x8000030a, 0xe03c1000, 0x8005060a, 0xbf8c3f70, 0x7e020306, 0x7e040307,
    0x7e0c0308, 0xf0f80108, 0x00820901, 0xbf8c3f70, 0x34160082, 0xe0701000, 0x8001090b, 0xbf810000,
};

struct Sampler {
    std::uint32_t clamp;
    std::uint32_t filter;
    std::uint32_t border = BorderBlack;
    std::uint32_t reduction = 0;
    std::uint32_t lodBias = 0;
    std::uint32_t compare = LessEqual;
    bool truncCoord = false;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, bool array = false) {
    const void* base = format == Format16161616Float ? static_cast<const void*>(HalfTexels.data()) : static_cast<const void*>(Texels.data());
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | ((array ? Type2DArray : Type2D) << 28u),
        array ? Layers - 1u : 0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(const Sampler& sampler) {
    return {
        sampler.clamp | (sampler.clamp << 3u) | (ClampEdge << 6u) | (sampler.compare << 12u) | (sampler.truncCoord ? 1u << 27u : 0u) | (sampler.reduction << 29u),
        0u,
        sampler.lodBias | (sampler.filter << 20u) | (sampler.filter << 22u),
        sampler.border << 30u,
    };
}

void FillTexels() {
    Texels.fill(0);
    const auto descriptor = TextureDescriptor(Format8888UNorm, true);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    const auto& mip = surface.mips.at(0);
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t y = 0; y < Side; ++y) {
            for (std::uint32_t x = 0; x < Side; ++x) {
                const auto offset = surface.GuestLayerOffset(layer) + mip.tiledOffset + y * mip.pitchBytes + x * 4u;
                Require(offset + 4u <= Texels.size(), "the array test surface does not fit its buffer");
                auto* texel = Texels.data() + offset;
                texel[0] = Red[layer][y * Side + x];
                texel[1] = 0x11;
                texel[2] = 0x22;
                texel[3] = 0xff;
            }
        }
    }
    HalfTexels.fill(0);
    const auto half = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(Format16161616Float)));
    const auto& halfMip = half.mips.at(0);
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            const auto offset = halfMip.tiledOffset + y * halfMip.pitchBytes + x * 8u;
            Require(offset + 8u <= HalfTexels.size(), "the half float test surface does not fit its buffer");
            const std::array<std::uint16_t, 4> channels{HalfRedBits[y * Side + x], 0u, 0u, 0x3c00u};
            std::memcpy(HalfTexels.data() + offset, channels.data(), sizeof(channels));
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

float LayerCoordinate(std::uint32_t tid) {
    constexpr std::array<float, 12> values{-0.4f, 0.0f, 0.49999997f, 0.5f, 0.99f, 1.0f, 1.5f, 2.5f, 2.49f, 3.5f, 3.7f, 7.0f};
    return values[tid % values.size()];
}

void FillInputArray() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < 3u; ++i) InputArray[tid * 4u + i] = Input[tid * 3u + i];
        InputArray[tid * 4u + 3u] = LayerCoordinate(tid);
    }
}

float NaNReference(std::uint32_t tid) {
    constexpr std::array<float, 4> references{0.25f, std::numeric_limits<float>::quiet_NaN(), 0.5f, -0.0f};
    return references[(tid / 4u) % references.size()];
}

void FillInputNaN() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto texel = tid % 4u;
        InputNaN[tid * 3u + 0u] = NaNReference(tid);
        InputNaN[tid * 3u + 1u] = (static_cast<float>(texel % Side) + 0.5f) / static_cast<float>(Side);
        InputNaN[tid * 3u + 2u] = (static_cast<float>(texel / Side) + 0.5f) / static_cast<float>(Side);
    }
}

float Passes(float reference, float red, std::uint32_t compare) {
    switch (compare) {
        case LessEqual: return reference <= red ? 1.0f : 0.0f;
        case Greater: return reference > red ? 1.0f : 0.0f;
        case NotEqual: return reference != red ? 1.0f : 0.0f;
        default: throw std::runtime_error("the test reference does not model this compare function");
    }
}

std::uint32_t ExpectedLayer(std::uint32_t tid) {
    const auto rounded = static_cast<int>(std::nearbyint(LayerCoordinate(tid)));
    return static_cast<std::uint32_t>(std::clamp(rounded, 0, static_cast<int>(Layers) - 1));
}

std::int32_t OffsetComponent(std::uint32_t tid, std::uint32_t component) {
    const auto field = (Extra[tid * 3u] >> (component * 8u)) & 0x3fu;
    return static_cast<std::int32_t>(field ^ 0x20u) - 0x20;
}

float ReferenceTexel(int x, int y, float reference, const Sampler& sampler, std::uint32_t layer) {
    const std::uint32_t clamp = sampler.clamp;
    const int size = static_cast<int>(Side);
    if (clamp == ClampBorder && (x < 0 || x >= size || y < 0 || y >= size)) {
        const float border = sampler.border == BorderWhite ? 1.0f : 0.0f;
        return Passes(reference, border, sampler.compare);
    }
    const auto address = [&](int value) {
        if (clamp == ClampEdge) return std::clamp(value, 0, static_cast<int>(Side) - 1);
        const int size = static_cast<int>(Side);
        return ((value % size) + size) % size;
    };
    const float red = static_cast<float>(Red[layer][address(y) * Side + address(x)]) / 255.0f;
    return Passes(reference, red, sampler.compare);
}

float Expected(std::uint32_t tid, const Sampler& sampler, bool offsets, bool array) {
    const float u = Input[tid * 3u + 1u] * static_cast<float>(Side);
    const float v = Input[tid * 3u + 2u] * static_cast<float>(Side);
    const float reference = std::clamp(Input[tid * 3u + 0u], 0.0f, 1.0f);
    const int offsetX = offsets ? OffsetComponent(tid, 0u) : 0;
    const int offsetY = offsets ? OffsetComponent(tid, 1u) : 0;
    const std::uint32_t layer = array ? ExpectedLayer(tid) : 0u;
    if (sampler.filter == FilterPoint) return ReferenceTexel(static_cast<int>(std::floor(u)) + offsetX, static_cast<int>(std::floor(v)) + offsetY, reference, sampler, layer);
    const float cu = u - 0.5f;
    const float cv = v - 0.5f;
    const int x = static_cast<int>(std::floor(cu)) + offsetX;
    const int y = static_cast<int>(std::floor(cv)) + offsetY;
    const float a = cu - std::floor(cu);
    const float b = cv - std::floor(cv);
    const float top = ReferenceTexel(x, y, reference, sampler, layer) * (1.0f - a) + ReferenceTexel(x + 1, y, reference, sampler, layer) * a;
    const float bottom = ReferenceTexel(x, y + 1, reference, sampler, layer) * (1.0f - a) + ReferenceTexel(x + 1, y + 1, reference, sampler, layer) * a;
    return top * (1.0f - b) + bottom * b;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, std::span<const std::uint32_t> code = Code, std::span<const float> inputs = Input, bool array = false) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = BufferDescriptor(inputs.data(), static_cast<std::uint32_t>(inputs.size_bytes()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(format, array);
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
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const Sampler& sampler, const char* name, bool offsets = false, bool array = false) {
    Output.fill(-1.0f);
    const std::span<const std::uint32_t> code = array ? std::span<const std::uint32_t>(ArrayCode) : offsets ? std::span<const std::uint32_t>(OffsetCode) : std::span<const std::uint32_t>(Code);
    const auto result = Compile(device, Format8888UNorm, sampler, code, array ? std::span<const float>(InputArray) : std::span<const float>(Input), array);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float expected = Expected(tid, sampler, offsets, array);
        const float tolerance = sampler.filter == FilterPoint ? 0.0f : 1e-4f;
        Require(std::fabs(Output[tid] - expected) <= tolerance, std::string(name) + ": thread " + std::to_string(tid) + " compared to " + std::to_string(Output[tid]) + ", expected " + std::to_string(expected));
    }
}

void RunNaN(AgcDriver::VulkanDevice& device, std::uint32_t compare, const char* name) {
    Output.fill(-1.0f);
    const Sampler sampler{ClampEdge, FilterPoint, BorderBlack, 0u, 0u, compare};
    const auto result = Compile(device, Format16161616Float, sampler, Code, InputNaN);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float expected = Passes(NaNReference(tid), HalfRed[tid % 4u], compare);
        Require(Output[tid] == expected, std::string(name) + ": thread " + std::to_string(tid) + " compared to " + std::to_string(Output[tid]) + ", expected " + std::to_string(expected));
    }
}

bool BindsDepthCompare(const ShaderRecompiler::RecompileResult& result) {
    return std::any_of(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) {
        return std::any_of(binding.imageDepthCompare.begin(), binding.imageDepthCompare.end(), [](bool compare) { return compare; });
    });
}

void Reject(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, std::string_view reason) {
    try {
        static_cast<void>(Compile(device, format, sampler));
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
        FillTexels();
        FillInput();
        FillInputArray();
        FillInputNaN();
        Require(BindsDepthCompare(Compile(*device, Format32Float, {ClampEdge, FilterBilinear})), "an R32 float texture left the native comparison path");
        Require(!BindsDepthCompare(Compile(*device, Format8888UNorm, {ClampEdge, FilterBilinear})), "a color texture kept a depth-compare binding");
        Run(*device, {ClampEdge, FilterPoint}, "point, clamp to edge");
        Run(*device, {ClampEdge, FilterBilinear}, "bilinear, clamp to edge");
        Run(*device, {ClampWrap, FilterBilinear}, "bilinear, wrap");
        Run(*device, {ClampBorder, FilterPoint, BorderWhite}, "point, white border");
        Run(*device, {ClampBorder, FilterPoint, BorderBlack}, "point, black border");
        Run(*device, {ClampBorder, FilterBilinear, BorderWhite}, "bilinear, white border");
        Run(*device, {ClampBorder, FilterBilinear, BorderBlack}, "bilinear, black border");
        Run(*device, {ClampEdge, FilterPoint, BorderBlack, 0u, 0x0140u}, "point, clamp to edge, sampler LOD bias");
        Run(*device, {ClampEdge, FilterPoint}, "point, clamp to edge, offsets, bias and LOD clamp", true);
        Run(*device, {ClampWrap, FilterBilinear}, "bilinear, wrap, offsets, bias and LOD clamp", true);
        Run(*device, {ClampBorder, FilterBilinear, BorderWhite, 0u, 0x3f00u}, "bilinear, white border, offsets, bias and LOD clamp", true);
        Run(*device, {ClampEdge, FilterPoint, BorderBlack, 0u, 0u, Greater}, "point, GREATER");
        Run(*device, {ClampBorder, FilterBilinear, BorderWhite, 0u, 0u, Greater}, "bilinear, white border, GREATER");
        Run(*device, {ClampEdge, FilterPoint}, "2D array, point, clamp to edge", false, true);
        Run(*device, {ClampEdge, FilterBilinear}, "2D array, bilinear, clamp to edge", false, true);
        Run(*device, {ClampWrap, FilterBilinear}, "2D array, bilinear, wrap", false, true);
        Run(*device, {ClampBorder, FilterBilinear, BorderWhite}, "2D array, bilinear, white border", false, true);
        Run(*device, {ClampEdge, FilterPoint, BorderBlack, 0u, 0u, Greater}, "2D array, point, GREATER", false, true);
        RunNaN(*device, NotEqual, "half float, NOT_EQUAL with NaN and signed zero");
        RunNaN(*device, LessEqual, "half float, LESSEQUAL with NaN and signed zero");
        Reject(*device, Format8888UInt, {ClampEdge, FilterPoint}, "unsupported format");
        Reject(*device, Format8888UNorm, {ClampMirror, FilterPoint}, "wrap, clamp-to-edge or clamp-to-border");
        Reject(*device, Format8888UNorm, {ClampHalfBorder, FilterPoint}, "wrap, clamp-to-edge or clamp-to-border");
        Reject(*device, Format8888UNorm, {ClampBorder, FilterPoint, BorderTable}, "border color table");
        Reject(*device, Format8888UNorm, {ClampEdge, FilterAnisoBilinear}, "point or bilinear");
        Reject(*device, Format8888UNorm, {ClampEdge, FilterBilinear, BorderBlack, ReductionMin}, "min or max reduction");
        Reject(*device, Format8888UNorm, {ClampEdge, FilterPoint, BorderBlack, 0u, 0u, LessEqual, true}, "TRUNC_COORD");
        std::puts("emulated color compare tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
