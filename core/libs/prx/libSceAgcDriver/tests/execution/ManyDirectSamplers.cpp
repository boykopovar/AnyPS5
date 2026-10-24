#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Samplers = 17;
constexpr std::uint32_t Last = Samplers - 1u;
constexpr std::uint32_t SamplerWord = 32;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format8888Srgb = 130;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ForceDegamma = 1u << 20u;
constexpr std::uint32_t Elements = Last * 2u + 1u;
constexpr std::uint64_t FloatElements = (std::uint64_t{1} << Elements) - 1u;
constexpr std::uint64_t IntegerElements = (std::uint64_t{1} << (Last * 2u)) - 1u;
constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> OpaqueBlackBorder{0x1b6u, (4u * 256u) << 12u, 0u, 1u << 30u};
constexpr std::array<std::uint32_t, 4> OpaqueWhiteBorder{0x1b6u, (4u * 256u) << 12u, 0u, 2u << 30u};

alignas(256) std::array<float, Width * Height> FloatTexels{};
alignas(256) std::array<std::uint32_t, Width * Height> ColorTexels{};
alignas(256) std::array<std::uint32_t, Width * Height> UintTexels{};
alignas(256) std::array<float, Lanes * 16u> Output{};
alignas(256) std::array<std::uint32_t, 128> Srt{};

std::vector<std::uint32_t> SamplerCode(bool lastOutside) {
    std::vector<std::uint32_t> code{0xf4080700u, 0xfa000020u, 0xf4080900u, 0xfa000040u, 0xf4080a00u, 0xfa000050u};
    code.insert(code.end(), {0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0x061402f4u, 0x7e160302u, 0x7e220280u});
    for (std::uint32_t sampler = 0; sampler < Samplers; ++sampler) {
        code.insert(code.end(), {0xf4080800u, 0xfa000000u | ((SamplerWord + sampler * 4u) * 4u)});
        if (sampler == Last) {
            code.insert(code.end(), {0xf09c8f08u, lastOutside ? 0x01090c0au : 0x01090c01u});
            continue;
        }
        code.insert(code.end(), {0xf09c8f08u, 0x01090401u, sampler == 0u ? 0x7e100304u : 0x06100908u, 0xf09c8f08u, 0x010a0401u, 0x4a222304u});
    }
    code.insert(code.end(), {0x7e240308u, 0x7e120202u, 0x34201286u, 0x4a202100u, 0x34202084u, 0xe0781000u, 0x80070c10u, 0xe0741800u, 0x80071110u, 0xbf810000u});
    return code;
}

const std::vector<std::uint32_t> InsideCode = SamplerCode(false);
const std::vector<std::uint32_t> OutsideCode = SamplerCode(true);

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t format, std::uint32_t bcSwizzle = 0u) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (bcSwizzle << 25u) | (Type2D << 28u)};
}

struct Outcome {
    std::vector<float> lanes;
    std::vector<std::uint32_t> integerSums;
    std::vector<float> floatSums;
    std::vector<std::array<std::uint32_t, 4>> samplers;
    std::vector<std::uint64_t> imageSamplers;
};

Outcome Run(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, const std::array<std::uint32_t, 4>& image, const std::array<std::uint32_t, 4>& last) {
    Srt = {};
    const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    const auto integerImage = Image(UintTexels.data(), Format32UInt);
    std::copy(image.begin(), image.end(), Srt.begin() + 16);
    std::copy(integerImage.begin(), integerImage.end(), Srt.begin() + 20);
    for (std::uint32_t sampler = 0; sampler < Samplers; ++sampler) {
        const auto& words = sampler == Last ? last : PointClamp;
        std::copy(words.begin(), words.end(), Srt.begin() + SamplerWord + sampler * 4u);
    }
    const auto rootAddress = reinterpret_cast<std::uintptr_t>(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u)};
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Outcome outcome;
    for (const auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers) {
            for (std::size_t word = 0; word + 4u <= binding.guestDescriptor.size(); word += 4u) outcome.samplers.push_back({binding.guestDescriptor[word], binding.guestDescriptor[word + 1u], binding.guestDescriptor[word + 2u], binding.guestDescriptor[word + 3u]});
        }
        if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) outcome.imageSamplers.insert(outcome.imageSamplers.end(), binding.imageSamplers.begin(), binding.imageSamplers.end());
    }
    Output.fill(-1.0f);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(span.data()));
    device.WaitIdle();
    outcome.lanes.assign(Output.begin(), Output.begin() + Lanes * 4u);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        outcome.integerSums.push_back(std::bit_cast<std::uint32_t>(Output[512u + lane * 4u]));
        outcome.floatSums.push_back(Output[513u + lane * 4u]);
    }
    return outcome;
}

std::string Refusal(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, const std::array<std::uint32_t, 4>& image, const std::array<std::uint32_t, 4>& last) {
    try {
        static_cast<void>(Run(device, code, image, last));
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

float Component(const Outcome& outcome, std::uint32_t lane, std::uint32_t component) {
    return outcome.lanes.at(lane * 4u + component);
}

float FirstSamples(float texel) {
    float sum = texel;
    for (std::uint32_t sampler = 1; sampler < Last; ++sampler) sum += texel;
    return sum;
}

void Fill() {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            FloatTexels[y * Width + x] = static_cast<float>(x) + 100.0f * static_cast<float>(y) + 0.25f;
            ColorTexels[y * Width + x] = 0x1000u + x * 3u + 0x100u * y;
            UintTexels[y * Width + x] = 0x2000u + x * 5u + 0x100u * y;
        }
    }
}

void CheckPairing(AgcDriver::VulkanDevice& device) {
    const auto outcome = Run(device, InsideCode, Image(FloatTexels.data(), Format32Float), OpaqueWhiteBorder);
    Require(outcome.samplers.size() == Elements, "many direct samplers: the sampler binding holds " + std::to_string(outcome.samplers.size()) + " elements, expected " + std::to_string(Elements));
    const auto& words = outcome.samplers[Elements - 1u];
    Require(words == OpaqueWhiteBorder, "many direct samplers: sampler element " + std::to_string(Elements - 1u) + " does not hold the last S#");
    std::vector<std::uint64_t> expected{FloatElements, IntegerElements};
    auto paired = outcome.imageSamplers;
    std::sort(paired.begin(), paired.end());
    std::sort(expected.begin(), expected.end());
    Require(paired == expected, "many direct samplers: the float image is not paired with sampler elements 0 to " + std::to_string(Elements - 1u) + " and the integer image with 0 to " + std::to_string(Elements - 2u));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Require(outcome.integerSums[lane] == Last * UintTexels[lane % Width], "many direct samplers: lane " + std::to_string(lane) + " summed " + std::to_string(outcome.integerSums[lane]) + " from the integer image through the 16 point copies, expected " + std::to_string(Last * UintTexels[lane % Width]));
    std::printf("many direct samplers: %u samplers bound as %zu sampler elements; the float image is paired with elements 0 to %u, the last sampler's at element %u, and the integer image with 0 to %u through the point copies\n", Samplers, outcome.samplers.size(), Elements - 1u, Elements - 1u, Elements - 2u);
}

void CheckBorder(AgcDriver::VulkanDevice& device) {
    const auto image = Image(FloatTexels.data(), Format32Float);
    const auto border = Run(device, OutsideCode, image, OpaqueWhiteBorder);
    const auto clamped = Run(device, OutsideCode, image, PointClamp);
    const auto edge = FloatTexels[Width - 1u];
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto expected = FirstSamples(FloatTexels[lane % Width]);
        Require(std::bit_cast<std::uint32_t>(border.floatSums[lane]) == std::bit_cast<std::uint32_t>(expected), "many direct samplers: lane " + std::to_string(lane) + " summed " + std::to_string(border.floatSums[lane]) + " through samplers 0 to 15, expected " + std::to_string(expected));
        Require(Component(border, lane, 0) == 1.0f && Component(border, lane, 3) == 1.0f, "many direct samplers: lane " + std::to_string(lane) + " read " + std::to_string(Component(border, lane, 0)) + ", alpha " + std::to_string(Component(border, lane, 3)) + " outside the image through sampler 16, expected its opaque white border");
        Require(Component(clamped, lane, 0) == edge, "many direct samplers: the clamped control of lane " + std::to_string(lane) + " read " + std::to_string(Component(clamped, lane, 0)) + ", expected the edge texel " + std::to_string(edge));
    }
    std::puts("many direct samplers: sampler 16, at sampler element 32, reads its opaque white border outside the image; a clamping S# there reads the edge texel");
}

void CheckDegamma(AgcDriver::VulkanDevice& device) {
    const auto image = Image(ColorTexels.data(), Format8888UNorm);
    auto forced = PointClamp;
    forced[0] |= ForceDegamma;
    const auto last = Run(device, InsideCode, image, forced);
    const auto srgb = Run(device, InsideCode, Image(ColorTexels.data(), Format8888Srgb), PointClamp);
    const auto plain = Run(device, InsideCode, image, PointClamp);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            Require(std::bit_cast<std::uint32_t>(Component(last, lane, component)) == std::bit_cast<std::uint32_t>(Component(srgb, lane, component)), "many direct samplers: lane " + std::to_string(lane) + " component " + std::to_string(component) + " sampled " + std::to_string(Component(last, lane, component)) + " with FORCE_DEGAMMA on sampler 16, " + std::to_string(Component(srgb, lane, component)) + " from the same texels as 8_8_8_8_SRGB");
        }
        Require(std::bit_cast<std::uint32_t>(last.floatSums[lane]) == std::bit_cast<std::uint32_t>(srgb.floatSums[lane]), "many direct samplers: lane " + std::to_string(lane) + " summed " + std::to_string(last.floatSums[lane]) + " with FORCE_DEGAMMA on sampler 16, " + std::to_string(srgb.floatSums[lane]) + " from the same texels as 8_8_8_8_SRGB");
        Require(Component(last, lane, 1) < Component(plain, lane, 1), "many direct samplers: lane " + std::to_string(lane) + " green " + std::to_string(Component(last, lane, 1)) + " with FORCE_DEGAMMA on sampler 16 is not decoded below " + std::to_string(Component(plain, lane, 1)));
    }
    std::puts("many direct samplers: FORCE_DEGAMMA on sampler 16, at sampler element 32, decodes the paired 8_8_8_8_UNORM image as 8_8_8_8_SRGB");
}

void CheckBorderSwizzle(AgcDriver::VulkanDevice& device) {
    const auto alphaImage = Image(FloatTexels.data(), Format32Float, 1u);
    const auto control = Refusal(device, InsideCode, alphaImage, PointClamp);
    Require(control.empty(), "many direct samplers: an alpha-moving BC swizzle without an opaque black border was refused: " + control);
    const auto refusal = Refusal(device, InsideCode, alphaImage, OpaqueBlackBorder);
    Require(refusal.find("moves the alpha channel is sampled through an opaque black border") != std::string::npos, "many direct samplers: an alpha-moving BC swizzle paired with an opaque black border on sampler 16 was not refused: " + refusal);
    std::puts("many direct samplers: an alpha-moving BC swizzle paired with an opaque black border on sampler 16 is refused");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Fill();
        CheckPairing(*device);
        CheckBorder(*device);
        CheckDegamma(*device);
        CheckBorderSwizzle(*device);
        std::puts("many direct samplers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
