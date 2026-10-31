#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Invocations = 384;
constexpr std::uint32_t Words = 8;
constexpr std::uint32_t Size = 64;
constexpr std::uint32_t Mips = 7;
constexpr float CompareReference = 50.0f;
alignas(256) std::array<std::uint32_t, Invocations * Words> Input{};
alignas(256) std::array<std::uint32_t, Invocations * Words> Output{};
alignas(256) std::array<std::uint32_t, 16384> Texels{};
alignas(256) std::array<std::uint32_t, 16384> LineTexels{};

alignas(256) constexpr std::array<std::uint32_t, 44> Code{
    0x34020085, 0x8f1a1819, 0x4a02021a, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008,
    0x80000401, 0xe030100c, 0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70,
    0xf0800108, 0x00820803, 0xf0940108, 0x00820902, 0x7e040306, 0xf0940108, 0x00820a02, 0xf09c0108,
    0x00820b03, 0xf0900108, 0x00820c03, 0x7e040307, 0xf0a00108, 0x00a20d02, 0xbf8c3f70, 0xe0701000,
    0x80010801, 0xe0701004, 0x80010901, 0xe0701008, 0x80010a01, 0xe070100c, 0x80010b01, 0xe0701010,
    0x80010c01, 0xe0701014, 0x80010d01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> CompareCode{
    0x34020085, 0x8f1a1819, 0x4a02021a, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe0301014,
    0x80000201, 0xbf8c3f70, 0xf0a00108, 0x00a20d02, 0xbf8c3f70, 0xe0701014, 0x80010d01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 23> MaskedQuadCode{
    0x34020085, 0x8f1a1819, 0x4a02021a, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xbf8c3f70,
    0x7e1002ff, 0xdeadbeef, 0xbefe03ff, 0x55555555, 0xbeff03ff, 0x55555555, 0x7e060280, 0xf0800108,
    0x00820803, 0xbf8c3f70, 0xbefe03c1, 0xbeff03c1, 0xe0701000, 0x80010801, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(bool line = false) {
    const auto address = reinterpret_cast<std::uintptr_t>(line ? LineTexels.data() : Texels.data());
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (22u << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | (line ? 0u : (Size - 1u) << 14u),
        0xfacu | ((Mips - 1u) << 16u) | ((line ? 8u : 9u) << 28u),
        0u,
        (Mips - 1u) << 4u,
        0u,
        0u,
    };
}

float LevelMark(std::uint32_t level) {
    return static_cast<float>(10u + 40u * level);
}

void FillTexture(bool line = false) {
    auto& texels = line ? LineTexels : Texels;
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(line)));
    Require(geometry.guestBytes <= sizeof(texels), "compute implicit LOD: the texture does not fit");
    Require(geometry.mips.size() == Mips, "compute implicit LOD: the texture does not have seven mips");
    texels.fill(0xdeadbeefu);
    for (std::uint32_t level = 0; level < Mips; ++level) {
        const auto& mip = geometry.mips[level];
        for (std::uint32_t y = 0; y < mip.height; ++y) for (std::uint32_t x = 0; x < mip.width; ++x) {
            texels[(mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes) / 4u + x] = std::bit_cast<std::uint32_t>(LevelMark(level));
        }
    }
}

std::uint32_t ImplicitLevel(std::uint32_t tid) {
    return ((tid / 32u) % 3u) * 2u;
}

void FillInput() {
    Input.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Invocations; ++tid) {
        const auto quad = (tid / 4u) % 8u;
        const float x = static_cast<float>(tid & 1u);
        const float y = static_cast<float>((tid >> 1u) & 1u);
        const float major = (quad & 2u) != 0u ? y : x;
        const float minor = (quad & 2u) != 0u ? x : y;
        const float step = static_cast<float>(1u << ImplicitLevel(tid)) / static_cast<float>(Size);
        const float u = static_cast<float>(quad * 2u + 1u) / static_cast<float>(Size) + step * ((quad & 1u) != 0u ? minor * 0.25f : major);
        const float v = static_cast<float>((tid / 32u) * 2u + 1u) / static_cast<float>(Size) + step * ((quad & 1u) != 0u ? major : minor * 0.25f);
        const std::array<float, 6> values{1.0f, u, v, static_cast<float>((tid / 4u) % Mips), -1.0f, CompareReference};
        for (std::uint32_t index = 0; index < values.size(); ++index) Input[tid * Words + index] = std::bit_cast<std::uint32_t>(values[index]);
    }
}

void FillMaskedQuadInput() {
    Input.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Invocations; ++tid) {
        const float u = static_cast<float>((tid & 1u) == 0u ? 2u : 4u) / static_cast<float>(Size);
        Input[tid * Words + 1u] = std::bit_cast<std::uint32_t>(u);
        Input[tid * Words + 2u] = std::bit_cast<std::uint32_t>(0.25f);
    }
}

std::uint32_t LineImplicitLevel(std::uint32_t tid) {
    const auto pattern = (tid / 32u) % 4u;
    return pattern == 0u ? 0u : (pattern - 1u) * 2u;
}

void FillLineInput() {
    Input.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Invocations; ++tid) {
        const auto quad = tid / 4u;
        const float x = static_cast<float>(tid & 1u);
        const float y = static_cast<float>((tid >> 1u) & 1u);
        const auto level = LineImplicitLevel(tid);
        const float step = (tid / 32u) % 4u == 0u ? 0.0f : static_cast<float>(1u << level) / static_cast<float>(Size);
        const float u = 0.25f + step * ((quad & 1u) != 0u ? y : x);
        const float v = 0.5f + static_cast<float>(16u << (quad % 3u)) * ((quad & 1u) != 0u ? x : y);
        const float reference = level == 0u ? CompareReference : LevelMark(level) + ((quad & 2u) != 0u ? 20.0f : -20.0f);
        Input[tid * Words + 1u] = std::bit_cast<std::uint32_t>(u);
        Input[tid * Words + 2u] = std::bit_cast<std::uint32_t>(v);
        Input[tid * Words + 5u] = std::bit_cast<std::uint32_t>(reference);
    }
}

void Check(const std::string& run) {
    constexpr std::array<const char*, 6> names{"image_sample", "image_sample_b +1", "image_sample_b -1", "image_sample_lz", "image_sample_l", "image_sample_c"};
    for (std::uint32_t result = 0; result < names.size(); ++result) for (std::uint32_t tid = 0; tid < Invocations; ++tid) {
        const auto level = ImplicitLevel(tid);
        const std::array<std::uint32_t, 6> levels{level, level + 1u, level == 0u ? 0u : level - 1u, 0u, (tid / 4u) % Mips, level};
        const auto expected = result == 5u ? (CompareReference < LevelMark(level) ? 1.0f : 0.0f) : LevelMark(levels[result]);
        const auto actual = std::bit_cast<float>(Output[tid * Words + result]);
        Require(actual == expected, "compute implicit LOD " + run + ": invocation " + std::to_string(tid) + " " + names[result] + " is " + std::to_string(actual) + ", expected " + std::to_string(expected) + " at mip " + std::to_string(levels[result]));
    }
}

void CheckLine(const std::string& run) {
    for (std::uint32_t tid = 0; tid < Invocations; ++tid) {
        const auto level = LineImplicitLevel(tid);
        const auto reference = std::bit_cast<float>(Input[tid * Words + 5u]);
        const auto expected = reference < LevelMark(level) ? 1.0f : 0.0f;
        const auto actual = std::bit_cast<float>(Output[tid * Words + 5u]);
        Require(actual == expected, "compute implicit LOD " + run + ": invocation " + std::to_string(tid) + " DIM2D comparison through a 1D descriptor is " + std::to_string(actual) + ", expected " + std::to_string(expected) + " at mip " + std::to_string(level));
    }
}

void CheckMaskedQuad(const std::string& run) {
    for (std::uint32_t tid = 0; tid < Invocations; ++tid) {
        const auto expected = (tid & 1u) == 0u ? std::bit_cast<std::uint32_t>(LevelMark(2u)) : 0xdeadbeefu;
        Require(Output[tid * Words] == expected, "compute implicit LOD " + run + ": invocation " + std::to_string(tid) + ((tid & 1u) == 0u ? " did not sample mip 2 from its inactive neighbor's preserved coordinate" : " overwrote its inactive output sentinel"));
        for (std::uint32_t word = 1u; word < Words; ++word) Require(Output[tid * Words + word] == 0xdeadbeefu, "compute implicit LOD " + run + ": a masked-quad store overwrote another output word");
    }
}

std::vector<std::uint32_t> UserData(std::uint32_t groupThreads, bool line = false) {
    std::vector<std::uint32_t> userData(25, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(line);
    const std::array<std::uint32_t, 4> sampler{0u, 0xfffu << 12u, 1u << 26u, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 20);
    userData[20] = 1u << 12u;
    userData[24] = std::countr_zero(groupThreads * Words * 4u);
    return userData;
}

bool Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t groupThreads, const ShaderRecompiler::SpirvTarget& target, bool zeroUnusedDims = false, bool line = false, bool maskedQuad = false) {
    if (maskedQuad) std::printf("compute implicit LOD masked quad coordinate: wave%u subgroup%u threads%u spanned requested %u\n", waveSize, target.subgroupSize, groupThreads, static_cast<unsigned>(target.spannedWaves));
    Output.fill(0xdeadbeefu);
    const auto userData = UserData(groupThreads, line);
    const auto code = maskedQuad ? std::span<const std::uint32_t>(MaskedQuadCode) : line ? std::span<const std::uint32_t>(CompareCode) : std::span<const std::uint32_t>(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto unused = zeroUnusedDims ? 0u : 1u;
    const ShaderRecompiler::ShaderComputeStageInfo compute{{groupThreads, unused, unused}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    ShaderRecompiler::RecompileResult compiled;
    try {
        compiled = ShaderRecompiler::Recompile(request);
    } catch (const std::exception& error) {
        const std::string refusal = error.what();
        const bool derivatives = TargetHasCapability(target, spv::CapabilityComputeDerivativeGroupLinearKHR) && std::ranges::find(target.supportedExtensions, "SPV_KHR_compute_shader_derivatives") != target.supportedExtensions.end();
        if (!derivatives && refusal.find("ComputeDerivativeGroupLinearKHR") != std::string::npos) {
            std::printf("skipped, compute derivatives are unavailable: %s\n", error.what());
            return false;
        }
        throw;
    }
    if (!maskedQuad) Require(std::ranges::any_of(compiled.bindings, [](const auto& binding) {
        return binding.kind == ShaderRecompiler::DescriptorKind::Sampler && std::ranges::find(binding.samplerDepthCompare, true) != binding.samplerDepthCompare.end();
    }), "compute implicit LOD: R32_FLOAT comparison did not select a native comparison sampler");
    const bool dualHalfFallback = waveSize == 64u && target.subgroupSize == 32u && target.spannedWaves && groupThreads == 64u;
    if (dualHalfFallback) Require(!compiled.fullSubgroups, "compute implicit LOD: spanned wave64 did not fall back to dual halves");
    if (zeroUnusedDims || dualHalfFallback) {
        const auto words = compiled.spirv.Words();
        bool normalized = false;
        for (std::size_t cursor = 5; cursor < words.size(); cursor += words[cursor] >> 16u) {
            if ((words[cursor] & 0xffffu) == spv::OpExecutionMode && (words[cursor] >> 16u) == 6u && words[cursor + 2u] == spv::ExecutionModeLocalSize) {
                normalized = words[cursor + 3u] == (dualHalfFallback ? 32u : groupThreads) && words[cursor + 4u] == 1u && words[cursor + 5u] == 1u;
            }
        }
        Require(normalized, dualHalfFallback ? "compute implicit LOD: dual-half wave64 fallback did not use LocalSize 32 1 1" : "compute implicit LOD: zero unused dimensions did not normalize to a four-thread workgroup");
    }
    device.Dispatch(compiled, Invocations / groupThreads, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto run = "wave" + std::to_string(waveSize) + " subgroup" + std::to_string(target.subgroupSize) + " threads" + std::to_string(groupThreads) + (zeroUnusedDims ? " zero unused dimensions" : "") + (target.spannedWaves ? " spanned requested" : "") + (line ? " native DIM2D comparison on 1D" : "") + (maskedQuad ? " masked quad coordinate" : "");
    if (maskedQuad) CheckMaskedQuad(run);
    else if (line) CheckLine(run);
    else Check(run);
    std::printf("compute implicit LOD %s passed\n", run.c_str());
    return true;
}

void CheckRefusals(const ShaderRecompiler::SpirvTarget& target) {
    auto userData = UserData(128);
    const std::span<const std::uint32_t> code(CompareCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{128, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto refused = [&](const char* name, std::string_view expected) {
        std::string refusal;
        try {
            static_cast<void>(ShaderRecompiler::Recompile(request));
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find(expected) != std::string::npos, std::string("compute implicit LOD ") + name + " was not refused as expected: " + refusal);
        std::printf("compute implicit LOD %s refused as expected\n", name);
    };
    std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
    std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityComputeDerivativeGroupLinearKHR));
    request.target.supportedCapabilities = capabilities;
    refused("missing capability", "require enabled ComputeDerivativeGroupLinearKHR and SPV_KHR_compute_shader_derivatives");
    request.target = target;
    std::vector<std::string_view> extensions(target.supportedExtensions.begin(), target.supportedExtensions.end());
    std::erase(extensions, std::string_view("SPV_KHR_compute_shader_derivatives"));
    request.target.supportedExtensions = extensions;
    refused("missing extension", "require enabled ComputeDerivativeGroupLinearKHR and SPV_KHR_compute_shader_derivatives");
}

}

int main(int argc, char** argv) {
    try {
        const bool maskedQuad = argc == 2 && std::string_view(argv[1]) == "--masked-quad-coordinate";
        Require(argc == 1 || maskedQuad, "compute implicit LOD: unexpected argument");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
        const auto wave32 = device->ComputeTarget(32);
        const auto wave64 = device->ComputeTarget(64);
        if (maskedQuad) {
            FillMaskedQuadInput();
            if (!Run(*device, 32, 128, wave32, false, false, true)) return VulkanTestSkipped;
            if (!Run(*device, 64, 128, wave64, false, false, true)) return VulkanTestSkipped;
            if (wave32.subgroupSize == 32u) {
                if (wave64.subgroupSize != 32u && !Run(*device, 64, 128, wave32, false, false, true)) return VulkanTestSkipped;
                auto requested = wave32;
                requested.spannedWaves = true;
                if (!Run(*device, 64, 64, requested, false, false, true)) return VulkanTestSkipped;
            } else {
                std::puts("skipped masked-quad wave64 dual-half cases, a 32-lane compute subgroup is unavailable");
                return VulkanTestSkipped;
            }
            return 0;
        }
        FillInput();
        if (!Run(*device, 32, 128, wave32)) return VulkanTestSkipped;
        if (!Run(*device, 64, 128, wave64)) return VulkanTestSkipped;
        if (wave32.subgroupSize == 32u) {
            if (wave64.subgroupSize != 32u && !Run(*device, 64, 128, wave32)) return VulkanTestSkipped;
            auto requested = wave32;
            requested.spannedWaves = true;
            if (!Run(*device, 64, 64, requested)) return VulkanTestSkipped;
        } else {
            std::puts("skipped wave64 spanned-request case, a 32-lane compute subgroup is unavailable");
        }
        if (!Run(*device, 32, 4, wave32, true)) return VulkanTestSkipped;
        CheckRefusals(wave32);
        FillTexture(true);
        FillLineInput();
        if (!Run(*device, 32, 128, wave32, false, true)) return VulkanTestSkipped;
        if (!Run(*device, 64, 128, wave64, false, true)) return VulkanTestSkipped;
        if (wave32.subgroupSize == 32u) {
            auto requested = wave32;
            requested.spannedWaves = true;
            if (!Run(*device, 64, 64, requested, false, true)) return VulkanTestSkipped;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
