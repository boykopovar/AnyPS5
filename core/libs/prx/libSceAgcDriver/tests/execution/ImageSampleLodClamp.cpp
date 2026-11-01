#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderInfo.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "PipelineSpecialization.hpp"
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
#include <set>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static bool ImageViewMinLod(const VulkanDevice& device) { return device.graphicsContext().imageViewMinLod; }
};

}

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

alignas(256) constexpr std::array<std::uint32_t, 87> Code{
    0x34020087, 0xe0301000, 0x80000501, 0xe0301004, 0x80000601, 0xe0301008, 0x80000701, 0xe030100c,
    0x80000801, 0xe0301010, 0x80000901, 0xe0301014, 0x80000a01, 0xe0301018, 0x80000b01, 0xe0301010,
    0x80001001, 0xe0301014, 0x80001101, 0xe030101c, 0x80001201, 0xbf8c3f70,
    0xf08c0f08, 0x00610c05, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01,
    0xe070102c, 0x80000f01, 0xf0840f08, 0x00610c09, 0xe0701030, 0x80000c01, 0xe0701034, 0x80000d01,
    0xe0701038, 0x80000e01, 0xe070103c, 0x80000f01, 0x7e0802ff, 0x00000201, 0xf0cc0f08, 0x00610c04,
    0xe0701040, 0x80000c01, 0xe0701044, 0x80000d01, 0xe0701048, 0x80000e01, 0xe070104c, 0x80000f01,
    0x7e0802ff, 0x00003e3f, 0xf0cc0f08, 0x00610c04, 0xe0701050, 0x80000c01, 0xe0701054, 0x80000d01,
    0xe0701058, 0x80000e01, 0xe070105c, 0x80000f01, 0xf0900f08, 0x00610c10, 0xe0701060, 0x80000c01,
    0xe0701064, 0x80000d01, 0xe0701068, 0x80000e01, 0xe070106c, 0x80000f01, 0xf09c0f08, 0x00610c09,
    0xe0701070, 0x80000c01, 0xe0701074, 0x80000d01, 0xe0701078, 0x80000e01, 0xe070107c, 0x80000f01,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 27> GradientCode{
    0x34020087, 0xe0301000, 0x80000501, 0xe0301004, 0x80000601, 0xe0301008, 0x80000701, 0xe030100c,
    0x80000801, 0xe0301010, 0x80000901, 0xe0301014, 0x80000a01, 0xe0301018, 0x80000b01, 0xbf8c3f70,
    0xf08c0f08, 0x00610c05, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01,
    0xe070102c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 55> PairedCode{
    0x34020087u, 0xe0301000u, 0x80000501u, 0xe0301004u, 0x80000601u, 0xe0301008u, 0x80000701u, 0xe030100cu,
    0x80000801u, 0xe0301010u, 0x80000901u, 0xe0301014u, 0x80000a01u, 0xbf8c3f70u, 0xf0880f08u, 0x00610c05u,
    0xe0701020u, 0x80000c01u, 0xe0701024u, 0x80000d01u, 0xe0701028u, 0x80000e01u, 0xe070102cu, 0x80000f01u,
    0xf09c0f08u, 0x00810c09u, 0xe0701030u, 0x80000c01u, 0xe0701034u, 0x80000d01u, 0xe0701038u, 0x80000e01u,
    0xe070103cu, 0x80000f01u, 0xf0880f08u, 0x00810c05u, 0xe0701040u, 0x80000c01u, 0xe0701044u, 0x80000d01u,
    0xe0701048u, 0x80000e01u, 0xe070104cu, 0x80000f01u, 0xf09c0f08u, 0x00850c09u, 0xe0701050u, 0x80000c01u,
    0xe0701054u, 0x80000d01u, 0xe0701058u, 0x80000e01u, 0xe070105cu, 0x80000f01u, 0xbf810000u,
};

struct Case {
    std::uint32_t minLod;
    std::uint32_t baseLevel;
    std::uint32_t maxLod = 0xfffu;
    std::uint32_t mipFilter = 1u;
    std::int32_t bias = 0;
    std::uint32_t filters = 0;
};

struct Sample {
    std::uint32_t level;
    std::uint32_t gradient;
    std::uint32_t clamp;
    std::uint32_t x;
    std::uint32_t y;
    std::int32_t lod;
};

Sample SampleOf(std::uint32_t tid) {
    return {tid % Levels, (tid / Levels) % Gradients, (tid / 3u) % Clamps, (tid * 7u + 3u) % Size, (tid * 5u + 1u) % Size, static_cast<std::int32_t>((tid / 4u) % Levels) - 1};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        auto* words = &Buffer[tid * Words];
        for (std::uint32_t gradient = 0; gradient < Gradients; ++gradient) {
            words[gradient] = Bits(gradient == sample.gradient ? std::exp2(static_cast<float>(sample.level) - 0.25f) / static_cast<float>(Size) : 0.0f);
        }
        words[4] = Bits((static_cast<float>(sample.x) + 0.5f) / static_cast<float>(Size));
        words[5] = Bits((static_cast<float>(sample.y) + 0.5f) / static_cast<float>(Size));
        words[6] = Bits(static_cast<float>(sample.clamp));
        words[7] = Bits(static_cast<float>(sample.lod));
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

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t minLod, std::uint32_t baseLevel, std::uint32_t lastLevel = Levels - 1u) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (minLod << 8u) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (baseLevel << 12u) | (lastLevel << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(const Case& item) {
    return {0u, item.maxLod << 12u, (static_cast<std::uint32_t>(item.bias) & 0x3fffu) | item.filters | (item.mipFilter << 26u), 0u};
}

ShaderRecompiler::RecompileRequest Request(std::span<const std::uint32_t> code, const ShaderRecompiler::SpirvTarget& target, const std::vector<std::uint32_t>& userData, std::span<const ShaderRecompiler::MemoryRegion> memory, bool cached) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = cached;
    return request;
}

std::vector<std::uint32_t> UserData(const Case& item) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data(), item.minLod, item.baseLevel);
    const auto sampler = SamplerDescriptor(item);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    return userData;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const Case& item, bool cached) {
    const auto userData = UserData(item);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto result = ShaderRecompiler::Recompile(Request(code, device.Target(), userData, memory, cached));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Wrap(std::uint32_t coordinate, std::int32_t offset, std::uint32_t size) {
    const auto extent = static_cast<std::int32_t>(size);
    return static_cast<std::uint32_t>(((static_cast<std::int32_t>(coordinate) + offset) % extent + extent) % extent);
}

std::uint32_t Level(float lambda, float floor, float maxLod, std::uint32_t last, std::uint32_t baseLevel) {
    const auto lod = std::min(std::max({lambda, floor, 0.0f}), maxLod);
    Require(std::abs(lod - std::floor(lod) - 0.5f) > 1.0f / 512.0f, "image sample LOD clamp: an expected LOD of " + std::to_string(lod) + " sits on a nearest-mip rounding tie, which Vulkan leaves to the implementation");
    return std::min(static_cast<std::uint32_t>(std::ceil(lod + 0.5f)) - 1u, last) + baseLevel;
}

std::string Describe(const Case& item) {
    return "T# MIN_LOD " + std::to_string(item.minLod) + " over BASE_LEVEL " + std::to_string(item.baseLevel) + " with S# MAX_LOD " + std::to_string(item.maxLod) + ", mip filter " + std::to_string(item.mipFilter) + " and LOD bias " + std::to_string(item.bias);
}

void Check(const Case& item, std::size_t results) {
    constexpr std::array<const char*, 6> names{"image_sample_d_cl", "image_sample_cl", "image_sample_d_cl_o (1, 2)", "image_sample_d_cl_o (-1, -2)", "image_sample_l", "image_sample_lz"};
    constexpr std::array<std::array<std::int32_t, 2>, 6> offsets{{{0, 0}, {0, 0}, {1, 2}, {-1, -2}, {0, 0}, {0, 0}}};
    const std::uint32_t last = Levels - 1u - item.baseLevel;
    const auto maxLod = static_cast<float>(item.maxLod) / 256.0f;
    const auto view = item.minLod > item.baseLevel * 256u ? static_cast<float>(std::min(item.minLod - item.baseLevel * 256u, last * 256u)) / 256.0f : 0.0f;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto gradient = static_cast<float>(sample.level) - 0.25f - static_cast<float>(item.baseLevel) + static_cast<float>(item.bias) / 256.0f;
        const auto floor = std::max(static_cast<float>(sample.clamp), view);
        const auto sampled = Level(gradient, floor, maxLod, last, item.baseLevel);
        const auto explicitLevel = [&](float lod, float clamp) {
            const auto clamped = view > 0.0f ? std::max({lod, clamp, view}) : std::max(lod, clamp);
            return Level(clamped + static_cast<float>(item.bias) / 256.0f, 0.0f, maxLod, last, item.baseLevel);
        };
        const auto none = -std::numeric_limits<float>::infinity();
        const auto levelOf = [&](std::size_t index) {
            switch (index) {
                case 1: return explicitLevel(0.0f, static_cast<float>(sample.clamp));
                case 4: return explicitLevel(static_cast<float>(sample.lod), none);
                case 5: return explicitLevel(0.0f, none);
                default: return sampled;
            }
        };
        for (std::size_t index = 0; index < results; ++index) {
            const std::uint32_t level = levelOf(index);
            const std::uint32_t size = Size >> level;
            const std::array<std::uint32_t, 4> expected{level, Wrap(sample.x >> level, offsets[index][0], size), Wrap(sample.y >> level, offsets[index][1], size), 255u};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const float value = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u + component]) * 255.0f;
                Require(std::lround(value) == static_cast<long>(expected[component]), std::string(names[index]) + " with " + Describe(item) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

void RunAndCheck(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::size_t results, const Case& item, bool cached) {
    FillInput();
    Run(device, code, item, cached);
    Check(item, results);
}

struct Pairing {
    std::uint32_t minLod;
    std::uint32_t baseLevel;
    std::array<std::uint32_t, 4> held;
    std::uint32_t secondMinLod = 0;
    std::uint32_t secondLastLevel = 0;
};

std::string Describe(const Pairing& item) {
    return "T# MIN_LOD " + std::to_string(item.minLod) + " over BASE_LEVEL " + std::to_string(item.baseLevel) + " with a second S# of words 1 " + std::to_string(item.held[1]) + " and 2 " + std::to_string(item.held[2]) + ", and a second T# of MIN_LOD " + std::to_string(item.secondMinLod) + " and LAST_LEVEL " + std::to_string(item.secondLastLevel);
}

void RunPairing(AgcDriver::VulkanDevice& device, const Pairing& item) {
    FillInput();
    std::vector<std::uint32_t> userData(28, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data(), item.minLod, item.baseLevel);
    const auto sampler = SamplerDescriptor(Case{0u, 0u});
    const auto second = TextureDescriptor(Texels.data(), item.secondMinLod, 0u, item.secondLastLevel);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    std::copy(item.held.begin(), item.held.end(), userData.begin() + 16);
    std::copy(second.begin(), second.end(), userData.begin() + 20);
    const std::span<const std::uint32_t> code(PairedCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto result = ShaderRecompiler::Recompile(Request(code, device.Target(), userData, memory, false));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

float Bilinear(std::uint32_t coordinate, std::uint32_t size) {
    const auto texel = (static_cast<float>(coordinate) + 0.5f) / static_cast<float>(Size) * static_cast<float>(size) - 0.5f;
    const auto low = std::floor(texel);
    const auto weight = texel - low;
    const auto first = Wrap(0u, static_cast<std::int32_t>(low), size);
    const auto next = Wrap(0u, static_cast<std::int32_t>(low) + 1, size);
    return (1.0f - weight) * static_cast<float>(first) + weight * static_cast<float>(next);
}

void CheckPairing(const Pairing& item) {
    const std::uint32_t last = Levels - 1u - item.baseLevel;
    const auto view = item.minLod > item.baseLevel * 256u ? static_cast<float>(std::min(item.minLod - item.baseLevel * 256u, last * 256u)) / 256.0f : 0.0f;
    const auto secondView = item.secondLastLevel == 0u ? 0.0f : static_cast<float>(std::min(item.secondMinLod, item.secondLastLevel * 256u)) / 256.0f;
    const bool magnifyLinear = ((item.held[2] >> 20u) & 1u) != 0u;
    const bool mipLinear = ((item.held[2] >> 26u) & 3u) == 2u;
    constexpr std::array<const char*, 4> names{"image_sample_d through the mipmapped S#", "image_sample_lz through the S# that holds the LOD at 0", "image_sample_d through the S# that holds the LOD at 0", "image_sample_lz of the second T# through the S# that holds the LOD at 0"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto gradient = static_cast<float>(sample.level) - 0.25f - static_cast<float>(item.baseLevel);
        const std::array<std::uint32_t, 4> levels{Level(gradient, view, 4095.0f / 256.0f, last, item.baseLevel), Level(view, 0.0f, static_cast<float>(last), last, item.baseLevel), Level(view, 0.0f, static_cast<float>(last), last, item.baseLevel), Level(secondView, 0.0f, static_cast<float>(item.secondLastLevel), item.secondLastLevel, 0u)};
        for (std::size_t index = 0; index < levels.size(); ++index) {
            const auto level = levels[index];
            const auto size = Size >> level;
            const auto read = [&](std::uint32_t component) { return std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u + component]) * 255.0f; };
            const auto fail = [&](std::uint32_t component, float expected) { return std::string(names[index]) + " with " + Describe(item) + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(read(component)) + ", expected " + std::to_string(expected); };
            if (index != 0u && index != 3u && mipLinear) {
                const auto blended = static_cast<float>(item.baseLevel) + view;
                const auto lower = static_cast<std::uint32_t>(std::floor(blended));
                const auto weight = blended - static_cast<float>(lower);
                const std::array<float, 3> expected{blended, (1.0f - weight) * static_cast<float>(sample.x >> lower) + weight * static_cast<float>(sample.x >> (lower + 1u)), (1.0f - weight) * static_cast<float>(sample.y >> lower) + weight * static_cast<float>(sample.y >> (lower + 1u))};
                for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::abs(read(component) - expected[component]) < 0.05f, fail(component, expected[component]));
                continue;
            }
            const bool filtered = index != 0u && magnifyLinear;
            const std::array<float, 4> expected{static_cast<float>(level), filtered ? Bilinear(sample.x, size) : static_cast<float>(sample.x >> level), filtered ? Bilinear(sample.y, size) : static_cast<float>(sample.y >> level), 255.0f};
            for (std::uint32_t component = 0; component < 4u; ++component) Require(std::abs(read(component) - expected[component]) < (filtered ? 0.05f : 0.5f), fail(component, expected[component]));
        }
    }
}

void RequireRejected(AgcDriver::VulkanDevice& device, const Pairing& item, const std::string& reason) {
    try {
        RunPairing(device, item);
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, Describe(item) + " was rejected with \"" + error.what() + "\", expected \"" + reason + "\"");
        return;
    }
    Require(false, Describe(item) + " ran in the shader-clamp lane, which cannot apply its clamp as the view clamp does");
}

void RequireRejected(AgcDriver::VulkanDevice& device, const Case& item, const std::string& reason, std::span<const std::uint32_t> code = Code) {
    FillInput();
    try {
        Run(device, code, item, false);
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, Describe(item) + " was rejected with \"" + error.what() + "\", expected \"" + reason + "\"");
        return;
    }
    Require(false, Describe(item) + " ran in the shader-clamp lane, which cannot apply its clamp as the view clamp does");
}

bool MinLodSpecId(std::uint32_t id) {
    return id >= ShaderRecompiler::PipelineSpecialization::MinLodBase && id < ShaderRecompiler::PipelineSpecialization::MinLodBase + ShaderRecompiler::ShaderInfo::MaxImages;
}

std::set<std::uint32_t> MinLodSpecIds(std::span<const std::uint32_t> spirv) {
    std::set<std::uint32_t> ids;
    for (std::size_t word = 5; word < spirv.size();) {
        const auto count = spirv[word] >> 16u;
        if (count == 0u) break;
        if ((spirv[word] & 0xffffu) == spv::OpDecorate && count >= 4u && spirv[word + 2u] == spv::DecorationSpecId) {
            if (MinLodSpecId(spirv[word + 3u])) ids.insert(spirv[word + 3u]);
        }
        word += count;
    }
    return ids;
}

void CheckSpirvLanes(const ShaderRecompiler::SpirvTarget& deviceTarget) {
    const Case item{0x200u, 0u};
    const auto userData = UserData(item);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    auto target = deviceTarget;
    target.shaderImageMinLod = false;
    const auto view = ShaderRecompiler::PrepareShader(Request(code, target, userData, memory, true));
    Require(MinLodSpecIds(ShaderRecompiler::GetPreparedArtifact(*view).spirv.Words()).empty(), "a target without the shader clamp declares minimum LOD specialization constants");
    target.shaderImageMinLod = true;
    const auto shader = ShaderRecompiler::PrepareShader(Request(code, target, userData, memory, true));
    Require(MinLodSpecIds(ShaderRecompiler::GetPreparedArtifact(*shader).spirv.Words()) == std::set<std::uint32_t>{ShaderRecompiler::PipelineSpecialization::MinLodBase}, "a target with the shader clamp does not declare exactly the image's minimum LOD specialization constant");
}

}

int main(int argc, char** argv) {
    try {
        const bool shaderLane = argc > 1 && std::strcmp(argv[1], "--shader-lane") == 0;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityMinLod)) {
            std::puts("skipped, the device has no shaderResourceMinLod");
            return VulkanTestSkipped;
        }
        const bool shaderClamp = device->Target().shaderImageMinLod;
        if (shaderLane && !shaderClamp) {
            std::puts("skipped, the device does not apply T# minimum LODs in the shader under APS5_NO_IMAGE_VIEW_MIN_LOD and APS5_NO_GPL");
            return VulkanTestSkipped;
        }
        if (!shaderClamp && !AgcDriver::VulkanDeviceTestAccess::ImageViewMinLod(*device)) {
            std::puts("skipped, the device has no VK_EXT_image_view_min_lod, and graphics pipeline libraries keep T# minimum LODs out of the shader");
            return VulkanTestSkipped;
        }
        CheckSpirvLanes(device->Target());
        FillTexture();
        for (const auto& item : {Case{0u, 0u}, Case{0x200u, 0u}, Case{0x200u, 1u}, Case{0x200u, 0u, 0x300u}}) RunAndCheck(*device, Code, 6u, item, false);
        if (!shaderClamp) RunAndCheck(*device, GradientCode, 1u, Case{0x100u, 1u, 0xfffu, 1u, -128}, false);
        if (!shaderClamp) RunAndCheck(*device, GradientCode, 1u, Case{0x200u, 0u, 0xfffu, 1u, -256}, false);
        if (!shaderClamp) RunAndCheck(*device, Code, 6u, Case{0u, 0u, 0xfffu, 1u, 256}, false);
        RunAndCheck(*device, Code, 6u, Case{0x100u, 1u, 0xfffu, 1u, -96}, false);
        for (const auto& item : {Case{0x200u, 0u}, Case{0u, 0u}, Case{0x300u, 0u}, Case{0x200u, 1u}, Case{0x100u, 0u}}) RunAndCheck(*device, Code, 6u, item, true);
        for (const auto& item : {Pairing{0x300u, 0u, {0u, 0u, 0u, 0u}}, Pairing{0x300u, 1u, {0u, 0u, 1u << 26u, 0u}}, Pairing{0x300u, 0u, {0u, 0u, (1u << 20u) | (1u << 22u), 0u}}, Pairing{0x300u, 0u, {0u, 0u, 0x3f00u, 0u}}, Pairing{0x300u, 0u, {0u, 0u, 0u, 0u}, 0x300u, 4u}}) {
            RunPairing(*device, item);
            CheckPairing(item);
        }
        if (shaderClamp) {
            for (const auto& item : {Case{0x1c0u, 0u}, Case{0x2c0u, 1u}}) RunAndCheck(*device, Code, 6u, item, false);
            RunAndCheck(*device, GradientCode, 1u, Case{0x1c0u, 0u}, false);
            for (const auto& item : {Case{0x1c0u, 0u}, Case{0u, 0u}, Case{0x2c0u, 1u}}) RunAndCheck(*device, Code, 6u, item, true);
            RequireRejected(*device, Case{0x200u, 0u, 0x100u}, "above the maximum LOD of 256/256 of S# 0, which is not implemented");
            RunPairing(*device, Pairing{0x140u, 0u, {0u, 0u, 2u << 26u, 0u}});
            CheckPairing(Pairing{0x140u, 0u, {0u, 0u, 2u << 26u, 0u}});
            RequireRejected(*device, Pairing{0x300u, 0u, {0u, 0u, 0u, 0u}, 0x100u, 4u}, "which holds the LOD at 0, also samples image 1 with another minimum LOD, which is not implemented");
            RequireRejected(*device, Case{0x200u, 0u, 0xfffu, 1u, -256}, "to an explicit LOD that S# 0 biases by -256/256, which is not implemented");
            RequireRejected(*device, Case{0x200u, 0u, 0xfffu, 1u, 64}, "to an explicit LOD that S# 0 biases by 64/256, which is not implemented");
            RequireRejected(*device, Case{0x200u, 0u, 0xfffu, 1u, 0, 1u << 20u}, "and S# 0 has different magnification and minification filters");
        }
        std::puts(shaderClamp ? "image sample LOD clamp tests passed in the shader-clamp lane" : "image sample LOD clamp tests passed in the image-view lane");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
