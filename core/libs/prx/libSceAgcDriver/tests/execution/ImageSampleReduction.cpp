#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Size = 4;
constexpr std::uint32_t Levels = 2;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ReductionMin = 1;
constexpr std::uint32_t ReductionMax = 2;
constexpr std::uint32_t FilterPoint = 0;
constexpr std::uint32_t FilterBilinear = 1;
constexpr std::uint32_t MipNone = 0;
constexpr std::uint32_t MipPoint = 1;
constexpr std::uint32_t MipLinear = 2;
constexpr float Margin = 0.05f;
constexpr std::array<float, Size * Size> Level0{17, 11, 23, 14, 20, 25, 12, 19, 13, 22, 16, 24, 21, 10, 18, 15};
constexpr std::array<float, (Size / 2) * (Size / 2)> Level1{5, 30, 16.5f, 9};

alignas(256) std::array<float, Threads * 3> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 4096> Texels{};
alignas(4096) std::array<std::uint8_t, 4096> Colors{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0900108, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> GatherCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf11c0108, 0x00820401, 0xbf8c3f70,
    0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 19> LoadedCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0x36180083, 0x7e1a0280, 0xbf8c3f70, 0xf0900108, 0x00820401, 0xf0000108, 0x0005050c,
    0xbf8c3f70, 0x34160082, 0xe0701000, 0x8001040b, 0xe0701080, 0x8001050b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> SampledCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0900108, 0x00820401, 0xf0900108, 0x00850501, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xe0701080, 0x8001050b, 0xbf810000, 0xbf810000, 0xbf810000,
};

struct Sampler {
    std::uint32_t reduction;
    std::uint32_t filter;
    std::uint32_t mip;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, const void* texels = Texels.data(), std::uint32_t levels = Levels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(const Sampler& sampler) {
    return {0x92u | (sampler.reduction << 29u), 0x00fff000u, (sampler.filter << 20u) | (sampler.filter << 22u) | (1u << 24u) | (sampler.mip << 26u), 0u};
}

void FillTexels() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format32Float, Size, Size, Levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "image sample reduction: the mip chain does not fit the texel storage");
    Texels.fill(0);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = mips[level];
        const auto& values = level == 0 ? std::span<const float>(Level0) : std::span<const float>(Level1);
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                const auto bits = std::bit_cast<std::uint32_t>(values[y * mip.width + x]);
                auto* texel = &Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                for (std::uint32_t byte = 0; byte < 4u; ++byte) texel[byte] = static_cast<std::uint8_t>(bits >> (byte * 8u));
            }
        }
    }
}

std::uint8_t Red(int x, int y) {
    const auto cx = static_cast<std::uint32_t>(std::clamp(x, 0, static_cast<int>(Size) - 1));
    const auto cy = static_cast<std::uint32_t>(std::clamp(y, 0, static_cast<int>(Size) - 1));
    return static_cast<std::uint8_t>(17u * ((cy * 7u + cx * 5u) % 16u));
}

void FillColors() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Size, Size, 1);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Colors.size(), "image sample reduction: the 8_8_8_8 texture does not fit its storage");
    Colors.fill(0);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            auto* texel = &Colors[mips[0].tiledOffset + static_cast<std::uint64_t>(y) * mips[0].pitchBytes + x * 4u];
            texel[0] = Red(static_cast<int>(x), static_cast<int>(y));
            texel[3] = 255;
        }
    }
}

bool Safe(float coordinate) {
    for (std::uint32_t size = Size; size >= Size / 2; size /= 2) {
        for (const float offset : {0.0f, -0.5f}) {
            const float scaled = coordinate * static_cast<float>(size) + offset;
            const float fraction = scaled - std::floor(scaled);
            if (fraction < Margin || fraction > 1.0f - Margin) return false;
        }
    }
    return true;
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float u = (static_cast<float>((tid * 5u + 1u) % 12u) - 1.7f) / 8.0f;
        const float v = (static_cast<float>((tid * 7u + 3u) % 12u) - 1.7f) / 8.0f;
        Require(Safe(u) && Safe(v), "image sample reduction: thread " + std::to_string(tid) + " samples too close to a texel boundary");
        Input[tid * 3u + 0u] = u;
        Input[tid * 3u + 1u] = v;
        Input[tid * 3u + 2u] = tid % 2u == 0u ? 0.25f : 0.75f;
    }
}

float Texel(std::uint32_t level, int x, int y) {
    const int size = static_cast<int>(Size >> level);
    const auto cx = static_cast<std::uint32_t>(std::clamp(x, 0, size - 1));
    const auto cy = static_cast<std::uint32_t>(std::clamp(y, 0, size - 1));
    return level == 0 ? Level0[cy * Size + cx] : Level1[cy * (Size / 2) + cx];
}

float Expected(std::uint32_t tid, const Sampler& sampler) {
    const float u = Input[tid * 3u + 0u];
    const float v = Input[tid * 3u + 1u];
    const float lod = Input[tid * 3u + 2u];
    std::vector<float> footprint;
    const auto gather = [&](std::uint32_t level) {
        const float size = static_cast<float>(Size >> level);
        if (sampler.filter == FilterPoint) {
            footprint.push_back(Texel(level, static_cast<int>(std::floor(u * size)), static_cast<int>(std::floor(v * size))));
            return;
        }
        const int x = static_cast<int>(std::floor(u * size - 0.5f));
        const int y = static_cast<int>(std::floor(v * size - 0.5f));
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) footprint.push_back(Texel(level, x + dx, y + dy));
        }
    };
    gather(sampler.mip == MipPoint && lod > 0.5f ? 1u : 0u);
    return sampler.reduction == ReductionMin ? *std::min_element(footprint.begin(), footprint.end()) : *std::max_element(footprint.begin(), footprint.end());
}

std::array<float, 4> ExpectedGather(std::uint32_t tid) {
    const int x = static_cast<int>(std::floor(Input[tid * 3u + 0u] * static_cast<float>(Size) - 0.5f));
    const int y = static_cast<int>(std::floor(Input[tid * 3u + 1u] * static_cast<float>(Size) - 0.5f));
    return {Texel(0, x, y + 1), Texel(0, x + 1, y + 1), Texel(0, x + 1, y), Texel(0, x, y)};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::uint32_t format, const Sampler& sampler, std::span<const std::uint32_t> code = Code) {
    std::vector<std::uint32_t> userData(28, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(format);
    const auto samplerWords = SamplerDescriptor(sampler);
    const auto colors = TextureDescriptor(Format8888UNorm, Colors.data(), 1);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(samplerWords.begin(), samplerWords.end(), userData.begin() + 16);
    std::copy(colors.begin(), colors.end(), userData.begin() + 20);
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

void Run(AgcDriver::VulkanDevice& device, const Sampler& sampler, const char* name) {
    Output.fill(-1.0f);
    const auto result = Compile(device, Format32Float, sampler);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float expected = Expected(tid, sampler);
        Require(Output[tid] == expected, std::string(name) + ": thread " + std::to_string(tid) + " sampled " + std::to_string(Output[tid]) + ", expected " + std::to_string(expected));
    }
}

void Gather(AgcDriver::VulkanDevice& device, const Sampler& sampler, const char* name) {
    Output.fill(-1.0f);
    const auto result = Compile(device, Format32Float, sampler, GatherCode);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(GatherCode.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = ExpectedGather(tid);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float sampled = Output[tid * 4u + component];
            Require(sampled == expected[component], std::string(name) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " gathered " + std::to_string(sampled) + ", expected " + std::to_string(expected[component]));
        }
    }
}

std::uint32_t PairedSamplers(const ShaderRecompiler::RecompileResult& result, std::uint32_t format) {
    for (const auto& binding : result.bindings) {
        if (binding.kind != ShaderRecompiler::DescriptorKind::SampledImage) continue;
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            if (((binding.guestDescriptor.at(element * 8u + 1u) >> 20u) & 0x1ffu) == format) return binding.Usage().imageSamplers.at(element);
        }
    }
    Require(false, "image sample reduction: no sampled image of format " + std::to_string(format));
    return 0;
}

float ExpectedColor(std::uint32_t tid, bool sampled) {
    if (!sampled) return static_cast<float>(Red(static_cast<int>(tid & 3u), 0)) / 255.0f;
    const int x = static_cast<int>(std::floor(Input[tid * 3u + 0u] * static_cast<float>(Size) - 0.5f));
    const int y = static_cast<int>(std::floor(Input[tid * 3u + 1u] * static_cast<float>(Size) - 0.5f));
    return static_cast<float>(std::min({Red(x, y), Red(x + 1, y), Red(x, y + 1), Red(x + 1, y + 1)})) / 255.0f;
}

void RunWithColors(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::span<const std::uint32_t> code, bool sampled, const char* name) {
    const Sampler sampler{ReductionMin, FilterBilinear, MipPoint};
    Output.fill(-1.0f);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float expected = Expected(tid, sampler);
        Require(Output[tid] == expected, std::string(name) + ": thread " + std::to_string(tid) + " sampled " + std::to_string(Output[tid]) + " from 32_FLOAT, expected " + std::to_string(expected));
        const float color = ExpectedColor(tid, sampled);
        Require(std::fabs(Output[Threads + tid] - color) <= 1.0f / 1024.0f, std::string(name) + ": thread " + std::to_string(tid) + " read " + std::to_string(Output[Threads + tid]) + " from 8_8_8_8, expected " + std::to_string(color));
    }
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
        FillColors();
        FillInput();
        const Sampler minBilinear{ReductionMin, FilterBilinear, MipPoint};
        const auto loaded = Compile(*device, Format32Float, minBilinear, LoadedCode);
        Require(PairedSamplers(loaded, Format32Float) == 1u && PairedSamplers(loaded, Format8888UNorm) == 0u, "image_load of 8_8_8_8 next to a min sampler: wrong image-sampler pairs");
        const auto sampled = Compile(*device, Format32Float, minBilinear, SampledCode);
        Require(PairedSamplers(sampled, Format32Float) == 1u && PairedSamplers(sampled, Format8888UNorm) == 1u, "8_8_8_8 sampled through a min sampler: wrong image-sampler pairs");
        static_cast<void>(Compile(*device, Format32SInt, {ReductionMin, FilterPoint, MipPoint}));
        Reject(*device, Format32SInt, {ReductionMin, FilterBilinear, MipPoint}, "needs point filtering");
        Reject(*device, Format32SInt, {ReductionMax, FilterPoint, MipLinear}, "needs point filtering");
        if (!device->SamplerFilterMinmax()) {
            std::puts("skipped, the device has no min/max sampler reduction with component mapping");
            return VulkanTestSkipped;
        }
        Run(*device, {ReductionMin, FilterBilinear, MipPoint}, "min, bilinear, point mip");
        Run(*device, {ReductionMax, FilterBilinear, MipPoint}, "max, bilinear, point mip");
        Run(*device, {ReductionMin, FilterPoint, MipPoint}, "min, point, point mip");
        Run(*device, {ReductionMax, FilterPoint, MipPoint}, "max, point, point mip");
        Run(*device, {ReductionMin, FilterBilinear, MipNone}, "min, bilinear, no mip");
        Run(*device, {ReductionMax, FilterPoint, MipNone}, "max, point, no mip");
        Gather(*device, {ReductionMin, FilterBilinear, MipPoint}, "gather4_lz, min");
        Gather(*device, {ReductionMax, FilterBilinear, MipPoint}, "gather4_lz, max");
        RunWithColors(*device, loaded, LoadedCode, false, "min 32_FLOAT, image_load 8_8_8_8");
        try {
            RunWithColors(*device, sampled, SampledCode, true, "min 32_FLOAT and 8_8_8_8");
            std::puts("the device filters 8_8_8_8 UNORM with min/max reduction");
        } catch (const std::exception& error) {
            Require(std::string_view(error.what()).find("format 37 does not support min/max filtering is sampled through a min or max reduction sampler") != std::string_view::npos, std::string("min 32_FLOAT and 8_8_8_8: unexpected error: ") + error.what());
        }
        std::puts("image sample reduction tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
