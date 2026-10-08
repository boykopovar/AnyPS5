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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Size = 8;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SwizzleX111 = 0x24cu;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t SampleX = 3;
constexpr std::uint32_t SampleY = 5;
constexpr float Tolerance = 1.0f / 64.0f;
constexpr std::array<std::uint32_t, 4> LinearSampler{0x00000092u, 0x00fff000u, 0x00500000u, 0u};

alignas(256) std::array<std::uint32_t, Threads * 8> Output{};
alignas(256) std::array<std::uint8_t, 4096> Integers{};
alignas(256) std::array<std::uint8_t, 4096> Colors{};

alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34060085u, 0x7e2802ffu, 0x3ef00000u, 0x7e2a02ffu, 0x3f300000u, 0xf09c0f08u, 0x00610a14u, 0xf09c0f08u,
    0x00640e14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xe0781010u, 0x80000e03u, 0xbf810000u, 0xbf810000u,
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t PitchBytes(std::uint32_t format, std::size_t storage) {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format, Size, Size, 1u);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= storage, "image sample shared sampler: a texture does not fit its storage");
    return mips[0].pitchBytes;
}

std::int32_t Integer(std::uint32_t x, std::uint32_t y) {
    return -static_cast<std::int32_t>(100u * x + 7u * y + 1u);
}

void Fill() {
    const auto integerPitch = PitchBytes(Format32SInt, Integers.size());
    const auto colorPitch = PitchBytes(Format8888UNorm, Colors.size());
    Integers.fill(0x5au);
    Colors.fill(0x5au);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto bits = static_cast<std::uint32_t>(Integer(x, y));
            auto* integer = &Integers[static_cast<std::size_t>(y) * integerPitch + x * 4u];
            for (std::uint32_t byte = 0; byte < 4u; ++byte) integer[byte] = static_cast<std::uint8_t>(bits >> (byte * 8u));
            auto* color = &Colors[static_cast<std::size_t>(y) * colorPitch + x * 4u];
            color[0] = x > SampleX ? 255u : 0u;
            color[1] = 0u;
            color[2] = 0u;
            color[3] = 255u;
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels, std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto integers = TextureDescriptor(Integers.data(), Format32SInt, SwizzleX111);
    const auto colors = TextureDescriptor(Colors.data(), Format8888UNorm, IdentitySwizzle);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(integers.begin(), integers.end(), userData.begin() + 4);
    std::copy(LinearSampler.begin(), LinearSampler.end(), userData.begin() + 12);
    std::copy(colors.begin(), colors.end(), userData.begin() + 16);
    const std::span<const std::uint32_t> code(Code);
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
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(AgcDriver::VulkanDevice& device) {
    Fill();
    Run(device);
    const std::array<std::uint32_t, 4> integer{static_cast<std::uint32_t>(Integer(SampleX, SampleY)), 1u, 1u, 1u};
    const std::array<float, 4> color{0.25f, 0.0f, 0.0f, 1.0f};
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto actual = Output[lane * 8u + component];
            Require(actual == integer[component], "32_SINT image_sample_lz through a bilinear sampler shared with 8_8_8_8_UNORM: lane " + std::to_string(lane) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(integer[component]));
        }
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto bits = Output[lane * 8u + 4u + component];
            const auto actual = std::bit_cast<float>(bits);
            Require(std::fabs(actual - color[component]) <= Tolerance, "8_8_8_8_UNORM image_sample_lz through a bilinear sampler shared with 32_SINT: lane " + std::to_string(lane) + " component " + std::to_string(component) + " is " + Hex(bits) + ", expected " + std::to_string(color[component]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device);
        std::puts("image sample shared sampler tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
