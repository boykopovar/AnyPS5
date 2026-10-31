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
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t ResultWord = 8;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t Gradients = 4;
constexpr std::uint32_t Clamps = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 61> Code{
    0x34020087, 0xe0301000, 0x80000501, 0xe0301004, 0x80000601, 0xe0301008, 0x80000701, 0xe030100c,
    0x80000801, 0xe0301010, 0x80000901, 0xe0301014, 0x80000a01, 0xe0301018, 0x80000b01, 0xbf8c3f70,
    0xf08c0f08, 0x00610c05, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01,
    0xe070102c, 0x80000f01, 0xf0840f08, 0x00610c09, 0xe0701030, 0x80000c01, 0xe0701034, 0x80000d01,
    0xe0701038, 0x80000e01, 0xe070103c, 0x80000f01, 0x7e0802ff, 0x00000201, 0xf0cc0f08, 0x00610c04,
    0xe0701040, 0x80000c01, 0xe0701044, 0x80000d01, 0xe0701048, 0x80000e01, 0xe070104c, 0x80000f01,
    0x7e0802ff, 0x00003e3f, 0xf0cc0f08, 0x00610c04, 0xe0701050, 0x80000c01, 0xe0701054, 0x80000d01,
    0xe0701058, 0x80000e01, 0xe070105c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr auto ExplicitGradientCode = [] {
    static_assert(Code[26] == 0xf0840f08u && Code[36] == 0x7e0802ffu);
    std::array<std::uint32_t, Code.size() - 10u> code{};
    std::copy_n(Code.begin(), 26u, code.begin());
    std::copy(Code.begin() + 36u, Code.end(), code.begin() + 26u);
    return code;
}();

constexpr auto BiasCode(std::uint32_t bias) {
    static_assert(Code[26] == 0xf0840f08u && Code[27] == 0x00610c09u);
    std::array<std::uint32_t, Code.size() + 5u> code{};
    std::copy_n(Code.begin(), 26u, code.begin());
    code[26] = 0x7e2002ffu;
    code[27] = bias;
    code[28] = 0x7e220309u;
    code[29] = 0x7e24030au;
    code[30] = 0x7e26030bu;
    std::copy(Code.begin() + 26u, Code.end(), code.begin() + 31u);
    code[31] = 0xf0980f08u;
    code[32] = 0x00610c10u;
    return code;
}

alignas(256) constexpr auto ZeroBiasCode = BiasCode(0u);
alignas(256) constexpr auto PositiveBiasCode = BiasCode(0x3f800000u);
alignas(256) constexpr auto NegativeBiasCode = BiasCode(0xbf800000u);

struct Sample {
    std::uint32_t level;
    std::uint32_t gradient;
    std::uint32_t clamp;
    std::uint32_t x;
    std::uint32_t y;
};

Sample SampleOf(std::uint32_t tid, bool affine = false) {
    const auto quad = tid / 4u;
    const auto x = affine ? (quad * 3u) % 8u + (tid & 1u) * 4u : (quad * 7u + 3u) % Size;
    const auto y = affine ? (quad * 5u) % 8u + ((tid >> 1u) & 1u) * 4u : (quad * 5u + 1u) % Size;
    return {tid % Levels, (tid / Levels) % Gradients, (tid / 3u) % Clamps, x, y};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void FillInput(bool affine) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid, affine);
        auto* words = &Buffer[tid * Words];
        for (std::uint32_t gradient = 0; gradient < Gradients; ++gradient) {
            words[gradient] = Bits(gradient == sample.gradient ? std::exp2(static_cast<float>(sample.level) - 0.25f) / static_cast<float>(Size) : 0.0f);
        }
        words[4] = Bits((static_cast<float>(sample.x) + 0.5f) / static_cast<float>(Size));
        words[5] = Bits((static_cast<float>(sample.y) + 0.5f) / static_cast<float>(Size));
        words[6] = Bits(static_cast<float>(sample.clamp));
    }
}

void FillTexture() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Size, Size, Levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "image sample LOD clamp: the mip chain does not fit the texel storage");
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = mips[level];
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                auto* texel = &Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = static_cast<std::uint8_t>(level);
                texel[1] = static_cast<std::uint8_t>(x);
                texel[2] = static_cast<std::uint8_t>(y);
                texel[3] = 255u;
            }
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

bool Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data());
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    ShaderRecompiler::RecompileResult result;
    try {
        result = ShaderRecompiler::Recompile(request);
    } catch (const std::exception& error) {
        const auto& target = device.Target();
        const bool derivatives = TargetHasCapability(target, spv::CapabilityComputeDerivativeGroupLinearKHR) && std::ranges::find(target.supportedExtensions, "SPV_KHR_compute_shader_derivatives") != target.supportedExtensions.end();
        if (!derivatives && std::string(error.what()).find("require enabled ComputeDerivativeGroupLinearKHR and SPV_KHR_compute_shader_derivatives") != std::string::npos) {
            std::printf("skipped image_sample_cl, compute derivatives are unavailable: %s\n", error.what());
            return false;
        }
        throw;
    }
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return true;
}

std::uint32_t Wrap(std::uint32_t coordinate, std::int32_t offset, std::uint32_t size) {
    const auto extent = static_cast<std::int32_t>(size);
    return static_cast<std::uint32_t>(((static_cast<std::int32_t>(coordinate) + offset) % extent + extent) % extent);
}

void Check(bool implicit, bool affine, std::int32_t bias = 0) {
    constexpr std::array<const char*, 4> names{"image_sample_d_cl", "image_sample_cl", "image_sample_d_cl_o (1, 2)", "image_sample_d_cl_o (-1, -2)"};
    constexpr std::array<std::array<std::int32_t, 2>, 4> offsets{{{0, 0}, {0, 0}, {1, 2}, {-1, -2}}};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid, affine);
        const std::uint32_t clamped = std::max(sample.level, sample.clamp);
        const auto implicitLevel = static_cast<std::uint32_t>(std::clamp((affine ? 2 : 0) + bias, 0, static_cast<std::int32_t>(Levels - 1u)));
        const std::array<std::uint32_t, 4> levels{clamped, std::max(implicitLevel, sample.clamp), clamped, clamped};
        for (std::uint32_t index = 0; index < levels.size(); ++index) {
            if (index == 1u && !implicit) continue;
            const std::uint32_t level = levels[index];
            const std::uint32_t size = Size >> level;
            const std::array<std::uint32_t, 4> expected{level, Wrap(sample.x >> level, offsets[index][0], size), Wrap(sample.y >> level, offsets[index][1], size), 255u};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const float value = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u + component]) * 255.0f;
                Require(std::lround(value) == static_cast<long>(expected[component]), std::string(names[index]) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::string argument = argc == 2 ? argv[1] : "";
        const bool zeroBias = argument == "--implicit-zero-bias" || argument == "--implicit-affine-zero-bias";
        const bool affine = argument == "--implicit-affine-quads" || argument == "--implicit-affine-zero-bias";
        Require(argc == 1 || (argc == 2 && (affine || zeroBias)), "image sample LOD clamp: unexpected argument");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityMinLod)) {
            std::puts("skipped, the device has no shaderResourceMinLod");
            return VulkanTestSkipped;
        }
        FillInput(affine);
        FillTexture();
        const bool implicit = Run(*device, zeroBias ? std::span<const std::uint32_t>(ZeroBiasCode) : std::span<const std::uint32_t>(Code));
        if (!implicit) {
            Require(Run(*device, ExplicitGradientCode), "image sample explicit-gradient LOD clamp unexpectedly requires compute derivatives");
        }
        Check(implicit, affine);
        if (!implicit) {
            std::puts("image sample explicit-gradient LOD clamp tests passed");
            return VulkanTestSkipped;
        }
        if (zeroBias && affine) {
            FillInput(true);
            Require(Run(*device, PositiveBiasCode), "image sample positive-bias LOD clamp unexpectedly requires compute derivatives");
            Check(true, true, 1);
            FillInput(true);
            Require(Run(*device, NegativeBiasCode), "image sample negative-bias LOD clamp unexpectedly requires compute derivatives");
            Check(true, true, -1);
        }
        std::puts("image sample LOD clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
