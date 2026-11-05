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
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Size = 8;
constexpr std::uint32_t Levels = 4;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SwizzleX111 = 0x24cu;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t SampleX = 3;
constexpr std::uint32_t SampleY = 5;
constexpr float Tolerance = 1.0f / 64.0f;
constexpr std::array<std::uint32_t, 4> AnisoSampler{0x00000092u | (4u << 9u), 0x00fff000u, (3u << 20u) | (3u << 22u), 0u};
constexpr std::uint32_t AnisoOverride = 1u << 29u;

alignas(256) std::array<std::uint32_t, Threads * 8> Output{};
using Storage = std::array<std::uint8_t, 16384>;
alignas(256) Storage SingleIntegers{};
alignas(256) Storage SingleColors{};
alignas(256) Storage MipIntegers{};
alignas(256) Storage MipColors{};

alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34060085u, 0x7e2802ffu, 0x3ef00000u, 0x7e2a02ffu, 0x3f300000u, 0xf09c0f08u, 0x00610a14u, 0xf09c0f08u,
    0x00640e14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xe0781010u, 0x80000e03u, 0xbf810000u, 0xbf810000u,
};

struct Levelled {
    std::uint32_t lastLevel;
    std::uint32_t maxMip;
};

constexpr Levelled SingleLevel{0, 0};
constexpr Levelled Mipmapped{Levels - 1u, Levels - 1u};
constexpr Levelled ClampedToOneLevel{Levels - 1u, 0};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

AgcDriver::Graphics::TileMipLayout BaseLevel(std::uint32_t format, std::uint32_t levels) {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format, Size, Size, levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Storage{}.size(), "image sample aniso override: a texture does not fit its storage");
    return mips[0];
}

std::int32_t Integer(std::uint32_t x, std::uint32_t y) {
    return -static_cast<std::int32_t>(100u * x + 7u * y + 1u);
}

void Fill(Storage& integers, Storage& colors, std::uint32_t levels) {
    const auto integerLevel = BaseLevel(Format32SInt, levels);
    const auto colorLevel = BaseLevel(Format8888UNorm, levels);
    integers.fill(0x5au);
    colors.fill(0x5au);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto bits = static_cast<std::uint32_t>(Integer(x, y));
            auto* integer = &integers[integerLevel.linearOffset + static_cast<std::size_t>(y) * integerLevel.pitchBytes + x * 4u];
            for (std::uint32_t byte = 0; byte < 4u; ++byte) integer[byte] = static_cast<std::uint8_t>(bits >> (byte * 8u));
            auto* color = &colors[colorLevel.linearOffset + static_cast<std::size_t>(y) * colorLevel.pitchBytes + x * 4u];
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

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels, std::uint32_t format, std::uint32_t swizzle, Levelled levels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        swizzle | (levels.lastLevel << 16u) | (Type2D << 28u),
        0u,
        levels.maxMip << 4u,
        0u,
        0u,
    };
}

std::optional<std::string> Run(AgcDriver::VulkanDevice& device, Levelled first, Levelled second, std::uint32_t samplerFlags) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto integers = TextureDescriptor(first.maxMip == 0 ? SingleIntegers.data() : MipIntegers.data(), Format32SInt, SwizzleX111, first);
    const auto colors = TextureDescriptor(second.maxMip == 0 ? SingleColors.data() : MipColors.data(), Format8888UNorm, IdentitySwizzle, second);
    auto sampler = AnisoSampler;
    sampler[2] |= samplerFlags;
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(integers.begin(), integers.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
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
    try {
        const auto result = ShaderRecompiler::Recompile(request);
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
    } catch (const std::runtime_error& error) {
        return std::string(error.what());
    }
    return std::nullopt;
}

void CheckSamples(const std::string& what) {
    const std::array<std::uint32_t, 4> integer{static_cast<std::uint32_t>(Integer(SampleX, SampleY)), 1u, 1u, 1u};
    const std::array<float, 4> color{0.25f, 0.0f, 0.0f, 1.0f};
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto actual = Output[lane * 8u + component];
            Require(actual == integer[component], what + ": 32_SINT lane " + std::to_string(lane) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(integer[component]));
            const auto bits = Output[lane * 8u + 4u + component];
            Require(std::fabs(std::bit_cast<float>(bits) - color[component]) <= Tolerance, what + ": 8_8_8_8_UNORM lane " + std::to_string(lane) + " component " + std::to_string(component) + " is " + Hex(bits) + ", expected " + std::to_string(color[component]));
        }
    }
}

void ExpectRuns(AgcDriver::VulkanDevice& device, const std::string& what, Levelled first, Levelled second, std::uint32_t samplerFlags) {
    const auto error = Run(device, first, second, samplerFlags);
    Require(!error.has_value(), what + " was refused: " + error.value_or(""));
    CheckSamples(what);
}

void ExpectMixedRefusal(AgcDriver::VulkanDevice& device, const std::string& what, Levelled first, Levelled second) {
    const auto error = Run(device, first, second, AnisoOverride);
    Require(error.has_value() && error->find("both single-level and mipmapped") != std::string::npos, what + " was not refused as a mixed ANISO_OVERRIDE pairing" + (error.has_value() ? ": " + *error : std::string()));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Fill(SingleIntegers, SingleColors, 1u);
        Fill(MipIntegers, MipColors, Levels);
        ExpectRuns(*device, "an ANISO_OVERRIDE sampler shared by two single-level images", SingleLevel, SingleLevel, AnisoOverride);
        ExpectRuns(*device, "an ANISO_OVERRIDE sampler shared by two mipmapped images", Mipmapped, Mipmapped, AnisoOverride);
        ExpectRuns(*device, "an ANISO_OVERRIDE sampler shared by a mipmapped image and one whose LAST_LEVEL is clamped to MAX_MIP 0", ClampedToOneLevel, Mipmapped, AnisoOverride);
        ExpectRuns(*device, "an anisotropic sampler without ANISO_OVERRIDE shared by a single-level and a mipmapped image", SingleLevel, Mipmapped, 0u);
        ExpectMixedRefusal(*device, "an ANISO_OVERRIDE sampler shared by a single-level and a mipmapped image", SingleLevel, Mipmapped);
        ExpectMixedRefusal(*device, "an ANISO_OVERRIDE sampler shared by a single-level image and one whose LAST_LEVEL is clamped to MAX_MIP 0", SingleLevel, ClampedToOneLevel);
        std::puts("image sample aniso override tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
