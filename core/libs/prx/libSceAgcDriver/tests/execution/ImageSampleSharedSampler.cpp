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
#include <cstring>
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

alignas(256) constexpr auto ProductCode = [] {
    std::array<std::uint32_t, Code.size() + 4u> code{};
    std::copy_n(Code.begin(), 10u, code.begin());
    for (std::uint32_t component = 0u; component < 4u; ++component) {
        const auto reg = 14u + component;
        code[10u + component] = 0x10000000u | (reg << 17u) | (reg << 9u) | 0x100u | reg;
    }
    std::copy(Code.begin() + 10u, Code.end(), code.begin() + 14u);
    return code;
}();

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
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
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

ShaderRecompiler::RecompileResult Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t format, bool useCache) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto integers = TextureDescriptor(Integers.data(), Format32SInt, SwizzleX111);
    const auto colors = TextureDescriptor(Colors.data(), format, IdentitySwizzle);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(integers.begin(), integers.end(), userData.begin() + 4);
    std::copy(LinearSampler.begin(), LinearSampler.end(), userData.begin() + 12);
    std::copy(colors.begin(), colors.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
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
    return result;
}

void Check(AgcDriver::VulkanDevice& device) {
    Fill();
    Run(device, Code, Format8888UNorm, false);
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

void CheckNormalizedProducts(AgcDriver::VulkanDevice& device) {
    constexpr auto FloatFormat = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32_32_32_32Float);
    constexpr auto SignedFormat = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format8_8_8_8SNorm);
    const std::array<std::uint32_t, 4> floating{0x7f812345u, 0xffc54321u, 0x80000000u, 0x7f800000u};
    ShaderRecompiler::RecompileResult first;
    ShaderRecompiler::RecompileResult sampledFirst;
    for (const auto format : {Format8888UNorm, SignedFormat, FloatFormat, Format8888UNorm}) {
        const auto pitch = PitchBytes(format, Colors.size());
        for (std::uint32_t y = 0u; y < Size; ++y) {
            for (std::uint32_t x = 0u; x < Size; ++x) {
                if (format == FloatFormat) std::memcpy(Colors.data() + y * pitch + x * 16u, floating.data(), sizeof(floating));
                else {
                    const std::array<std::uint8_t, 4> texel = format == SignedFormat ? std::array<std::uint8_t, 4>{128u, 255u, 0u, 127u} : std::array<std::uint8_t, 4>{0u, 255u, 128u, 64u};
                    std::memcpy(Colors.data() + y * pitch + x * 4u, texel.data(), texel.size());
                }
            }
        }
        const auto sample = Run(device, Code, format, true);
        if (sampledFirst.spirv.empty()) sampledFirst = sample;
        else Require(sampledFirst.spirv.ContentHash() == sample.spirv.ContentHash() && sampledFirst.Identity() == sample.Identity(), "format transition split a sampled shader with no bounded NaN query");
        std::array<std::uint32_t, 4> sampled{};
        std::copy_n(Output.begin() + 4u, 4u, sampled.begin());
        const auto result = Run(device, ProductCode, format, true);
        for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
            for (std::uint32_t component = 0u; component < 4u; ++component) {
                const auto bits = sampled[component];
                const auto value = std::bit_cast<float>(bits);
                const auto expected = (bits & 0x7fffffffu) > 0x7f800000u ? bits : std::bit_cast<std::uint32_t>(value * value);
                Require(Output[lane * 8u + 4u + component] == expected, "sampled product changed value or NaN payload across a format transition");
            }
        }
        if (first.spirv.empty()) first = result;
        else if (format == Format8888UNorm) Require(first.spirv.ContentHash() == result.spirv.ContentHash() && result.cacheHit, "sampled format transition missed the original cached normalized variant");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device);
        CheckNormalizedProducts(*device);
        std::puts("image sample shared sampler tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
