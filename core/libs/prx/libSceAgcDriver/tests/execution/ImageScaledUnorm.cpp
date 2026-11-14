#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t LoadWidth = 2048;
constexpr std::uint32_t LoadGroups = LoadWidth / Threads;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t SwizzleXYZ1 = 0x3acu;
constexpr std::uint32_t SwizzleZYX1 = 0x32eu;
constexpr std::uint32_t SwizzleX011 = 0x244u;
constexpr std::uint32_t SwizzleX001 = 0x204u;
constexpr std::uint32_t SwizzleWZYX = 0x977u;
constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> BilinearSampler{0x00000092u, 0x00fff000u, 0x00500000u, 0u};
constexpr std::uint32_t SampleStep = 0x3a200000u;
constexpr std::uint32_t SampleStart = 0x39c00000u;

alignas(256) std::array<std::uint32_t, LoadWidth * 4> Output{};

constexpr std::array<std::uint32_t, 15> LoadCode(std::uint32_t dmask) {
    return {
        0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x7e3e0280u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
        0x7e1a0280u, 0xf0001008u | (dmask << 8u), 0x00010a1eu, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 9> SamplerCode(std::uint32_t word0) {
    return {
        0x34060084u, 0x7e280280u, 0x7e2a0280u, word0, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
        0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 13> SampleAtCode(std::uint32_t step, std::uint32_t start) {
    return {
        0x34060084u, 0x7e280d00u, 0x102828ffu, step, 0x062828ffu, start, 0x7e2a0280u, 0xf09c0f08u,
        0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
    };
}

constexpr std::uint32_t GatherLz(std::uint32_t dmask) {
    return 0xf0000008u | (0x47u << 18u) | (dmask << 8u);
}

alignas(256) constexpr auto LoadXyzw = LoadCode(0xfu);
alignas(256) constexpr auto SampleAt = SampleAtCode(SampleStep, SampleStart);
alignas(256) constexpr auto GatherX = SamplerCode(GatherLz(0x1u));
alignas(256) constexpr auto GatherW = SamplerCode(GatherLz(0x8u));

struct ScaledFormat {
    std::uint32_t code;
    std::uint32_t components;
    std::uint32_t bits;
    std::size_t offset;
    const char* name;
};

constexpr std::array<ScaledFormat, 6> Formats{{
    {3u, 1u, 8u, 0u, "8_USCALED"},
    {9u, 1u, 16u, 2048u, "16_USCALED"},
    {16u, 2u, 8u, 6144u, "8_8_USCALED"},
    {25u, 2u, 16u, 10240u, "16_16_USCALED"},
    {58u, 4u, 8u, 18432u, "8_8_8_8_USCALED"},
    {67u, 4u, 16u, 26624u, "16_16_16_16_USCALED"},
}};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image scaled unorm: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle, std::uint32_t width) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        (width - 1u) >> 2u,
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::uint32_t ChannelBits(const ScaledFormat& format, std::uint32_t index, std::uint32_t channel) {
    const auto maximum = (1u << format.bits) - 1u;
    if (index == 2u) return 0u;
    if (index == 3u) return maximum;
    return ((index * 2654435761u + 0x9e3779b9u) >> (channel * 5u + 3u)) & maximum;
}

void FillTexels(std::uint8_t* block, const ScaledFormat& format) {
    const auto bytes = format.bits / 8u;
    for (std::uint32_t index = 0; index < LoadWidth; ++index) {
        for (std::uint32_t channel = 0; channel < format.components; ++channel) {
            const auto value = ChannelBits(format, index, channel);
            std::memcpy(block + format.offset + (index * format.components + channel) * bytes, &value, bytes);
        }
    }
}

double Selected(const ScaledFormat& format, std::uint32_t index, std::uint32_t swizzle, std::uint32_t component) {
    const auto selector = (swizzle >> (component * 3u)) & 7u;
    if (selector == 0u) return 0.0;
    if (selector == 1u) return 1.0;
    const auto channel = selector - 4u;
    if (channel >= format.components) return channel == 3u ? 1.0 : 0.0;
    return static_cast<double>(ChannelBits(format, index, channel));
}

std::string Describe(const ScaledFormat& format, std::uint32_t swizzle) {
    char text[64];
    std::snprintf(text, sizeof(text), "%s with DST_SEL 0x%03x", format.name, swizzle);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& sampler, std::uint32_t groups) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

float Result(std::uint32_t index) {
    return std::bit_cast<float>(Output[index]);
}

void CheckLoad(AgcDriver::VulkanDevice& device, std::uint8_t* block, const ScaledFormat& format, std::uint32_t swizzle) {
    Run(device, LoadXyzw, TextureDescriptor(block + format.offset, format.code, swizzle, LoadWidth), {}, LoadGroups);
    for (std::uint32_t index = 0; index < LoadWidth; ++index) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto expected = Selected(format, index, swizzle, component);
            const auto actual = static_cast<double>(Result(index * 4u + component));
            Require(actual == expected, "image_load of " + Describe(format, swizzle) + ": texel " + std::to_string(index) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void CheckSample(AgcDriver::VulkanDevice& device, std::uint8_t* block, const ScaledFormat& format, std::uint32_t swizzle, bool linear) {
    Run(device, SampleAt, TextureDescriptor(block + format.offset, format.code, swizzle, LoadWidth), linear ? BilinearSampler : PointSampler, 1);
    const auto tolerance = linear ? 1.0e-5 * static_cast<double>((1u << format.bits) - 1u) : 0.0;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = (static_cast<double>(tid) * static_cast<double>(std::bit_cast<float>(SampleStep)) + static_cast<double>(std::bit_cast<float>(SampleStart))) * LoadWidth;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            double expected = 0.0;
            if (linear) {
                const auto centre = x - 0.5;
                const auto first = static_cast<std::int64_t>(std::floor(centre));
                const auto weight = std::nearbyint((centre - std::floor(centre)) * 256.0) / 256.0;
                const auto left = Selected(format, static_cast<std::uint32_t>(std::clamp<std::int64_t>(first, 0, LoadWidth - 1)), swizzle, component);
                const auto right = Selected(format, static_cast<std::uint32_t>(std::clamp<std::int64_t>(first + 1, 0, LoadWidth - 1)), swizzle, component);
                expected = left + (right - left) * weight;
            } else {
                expected = Selected(format, static_cast<std::uint32_t>(std::floor(x)), swizzle, component);
            }
            const auto actual = static_cast<double>(Result(tid * 4u + component));
            Require(std::fabs(actual - expected) <= tolerance, std::string(linear ? "bilinear" : "point") + " image_sample_lz of " + Describe(format, swizzle) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void CheckGather(AgcDriver::VulkanDevice& device, std::uint8_t* block, const ScaledFormat& format, std::uint32_t swizzle, std::uint32_t component) {
    Run(device, component == 0u ? std::span<const std::uint32_t>(GatherX) : std::span<const std::uint32_t>(GatherW), TextureDescriptor(block + format.offset, format.code, swizzle, LoadWidth), PointSampler, 1);
    const auto expected = Selected(format, 0u, swizzle, component);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t lane = 0; lane < 4u; ++lane) {
            const auto actual = static_cast<double>(Result(tid * 4u + lane));
            Require(actual == expected, "image_gather4_lz component " + std::to_string(component) + " of " + Describe(format, swizzle) + ": thread " + std::to_string(tid) + " texel " + std::to_string(lane) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::uint8_t* block, const ScaledFormat& format, std::uint32_t swizzle) {
    std::string refusal;
    try {
        Run(device, LoadXyzw, TextureDescriptor(block + format.offset, format.code, swizzle, LoadWidth), {}, 1);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("selects the alpha a scaled image format does not have") != std::string::npos, "image_load of " + Describe(format, swizzle) + " was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        for (const auto& format : Formats) FillTexels(block.Data(), format);
        for (const auto& format : Formats) {
            for (const auto swizzle : {SwizzleXYZ1, SwizzleZYX1, SwizzleX011, SwizzleX001}) CheckLoad(*device, block.Data(), format, swizzle);
            CheckSample(*device, block.Data(), format, SwizzleXYZ1, false);
            CheckSample(*device, block.Data(), format, SwizzleX001, false);
            CheckSample(*device, block.Data(), format, SwizzleZYX1, true);
            CheckGather(*device, block.Data(), format, SwizzleX001, 3u);
            CheckGather(*device, block.Data(), format, SwizzleXYZ1, 0u);
            if (format.components == 4u) {
                CheckLoad(*device, block.Data(), format, SwizzleXYZW);
                CheckLoad(*device, block.Data(), format, SwizzleWZYX);
                CheckSample(*device, block.Data(), format, SwizzleXYZW, false);
                CheckGather(*device, block.Data(), format, SwizzleXYZW, 3u);
            } else {
                RequireRefused(*device, block.Data(), format, SwizzleXYZW);
            }
        }
        std::puts("image scaled unorm tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
