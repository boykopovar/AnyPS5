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

constexpr std::uint32_t Threads = 128;
constexpr std::uint32_t Words = 16;
constexpr std::uint32_t ResultWord = 4;
constexpr std::uint32_t Results = 12;
constexpr std::int32_t Width = 13;
constexpr std::int32_t Height = 7;
constexpr std::uint32_t StorageLevels = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Type1D = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t OneZeroOneZero = 0x041u;
constexpr std::uint8_t OtherLevelTexel = 0x40u;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> SingleLevel{};
alignas(256) std::array<std::uint8_t, 16384> MultiLevel{};

alignas(256) constexpr std::array<std::uint32_t, 40> Code{
    0x34020086, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xbf8c3f70, 0x7e0802ff, 0x402ccccd,
    0xf09c0f08, 0x00610802, 0xf0900f08, 0x00610c02, 0xf0800f08, 0x00611002, 0xbf8c3f70, 0xe0701010,
    0x80000801, 0xe0701014, 0x80000901, 0xe0701018, 0x80000a01, 0xe070101c, 0x80000b01, 0xe0701020,
    0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030,
    0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> LevelZeroCode{
    0x34020086, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xbf8c3f70, 0xf09c0f08, 0x00610802,
    0xbf8c3f70, 0xe0701010, 0x80000801, 0xe0701014, 0x80000901, 0xe0701018, 0x80000a01, 0xe070101c,
    0x80000b01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> GradientCode{
    0x34020086, 0xe0301000, 0x80000601, 0xe0301004, 0x80000701, 0xbf8c3f70, 0x7e0402f6, 0x7e060280,
    0x7e080280, 0x7e0a02f6, 0xf0880f08, 0x00610802, 0xbf8c3f70, 0xe0701010, 0x80000801, 0xe0701014,
    0x80000901, 0xe0701018, 0x80000a01, 0xe070101c, 0x80000b01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 45> OffsetCode{
    0x34020086, 0xe0301008, 0x80000201, 0xe0301000, 0x80000301, 0xe0301004, 0x80000401, 0xbf8c3f70,
    0x7e0a02ff, 0x402ccccd, 0x7e2802ff, 0x0000023d, 0xf0dc0f08, 0x00610802, 0xf0d00f08, 0x00610c02,
    0xf0dc0f0a, 0x00611014, 0x00000403, 0xbf8c3f70, 0xe0701010, 0x80000801, 0xe0701014, 0x80000901,
    0xe0701018, 0x80000a01, 0xe070101c, 0x80000b01, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01,
    0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101,
    0xe0701038, 0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

enum class Offset { None, Lane, Constant };
constexpr std::int32_t ConstantOffsetU = -3;
constexpr std::int32_t ConstantOffsetV = 2;

struct Program {
    std::span<const std::uint32_t> code;
    std::array<const char*, 3> instructions;
    std::array<Offset, 3> offsets;
    std::size_t count = 3;
};

const Program Plain{Code, {"image_sample_lz", "image_sample_l 2.7", "image_sample"}, {Offset::None, Offset::None, Offset::None}};
const Program LevelZero{LevelZeroCode, {"image_sample_lz", "", ""}, {Offset::None, Offset::None, Offset::None}, 1};
const Program Gradient{GradientCode, {"image_sample_d (4, 0, 0, 4)", "", ""}, {Offset::None, Offset::None, Offset::None}, 1};
const Program Offsets{OffsetCode, {"image_sample_lz_o", "image_sample_l_o 2.7", "image_sample_lz_o (-3, 2) NSA"}, {Offset::Lane, Offset::Lane, Offset::Constant}};

alignas(256) constexpr std::array<std::uint32_t, 34> ConstantCode{
    0x34020086, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xbf8c3f70, 0xf09c0608, 0x00610802,
    0xf11c0208, 0x00610a02, 0xf11c0408, 0x00610e02, 0xbf8c3f70, 0xe0701010, 0x80000801, 0xe0701014,
    0x80000901, 0xe0701018, 0x80000a01, 0xe070101c, 0x80000b01, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034,
    0x80001101, 0xbf810000,
};

struct SamplerCase {
    const char* name;
    std::array<std::uint32_t, 4> words;
    bool linear;
    bool border;
    bool atBaseLevel;
};

constexpr std::array<SamplerCase, 5> Samplers{{
    {"bilinear clamp-to-last-texel, point mips, MAX_LOD 0xfff", {0x00008092u, 0x00fff000u, 0x05500000u, 0u}, true, false, true},
    {"point clamp-to-last-texel, point mips", {0x00008092u, 0x00fff000u, 0x04000000u, 0u}, false, false, true},
    {"bilinear clamp-to-last-texel, linear mips, MIN_LOD 1, LOD bias 1.5", {0x00008092u, 0x00fff100u, 0x09500180u, 0u}, true, false, false},
    {"bilinear clamp-to-border, transparent black", {0x000080b6u, 0x00fff000u, 0x05500000u, 0u}, true, true, true},
    {"point clamp-to-border, no mips, transparent black", {0x000080b6u, 0u, 0u, 0u}, false, true, true},
}};

struct Coordinate {
    float u;
    float v;
    std::int32_t du = 0;
    std::int32_t dv = 0;
};

std::int32_t LevelWidth(std::uint32_t level) {
    return std::max(Width >> level, 1);
}

std::int32_t LevelHeight(std::uint32_t level) {
    return std::max(Height >> level, 1);
}

std::vector<Coordinate> Coordinates(std::uint32_t level) {
    const auto width = LevelWidth(level);
    const auto height = LevelHeight(level);
    std::vector<float> us;
    for (std::int32_t k = 0; k < width; ++k) {
        for (const float fraction : {0.5f, 0.0f, 0.25f, 0.75f}) us.push_back(static_cast<float>(k) + fraction);
    }
    for (const float edge : {-1.0f, -0.5f, static_cast<float>(width), static_cast<float>(width + 1), static_cast<float>(width) - 0.25f}) us.push_back(edge);
    constexpr std::array<float, 3> fractions{0.5f, 0.0f, 0.75f};
    std::vector<Coordinate> result;
    for (std::size_t index = 0; index < us.size(); ++index) {
        for (std::size_t fraction = 0; fraction < fractions.size(); ++fraction) {
            result.push_back({us[index], static_cast<float>((index * fractions.size() + fraction) % height) + fractions[fraction]});
        }
    }
    for (std::int32_t x = 0; x < width; ++x) {
        const float v = static_cast<float>(x % height) + 0.5f;
        result.push_back({static_cast<float>(x) - 1.0f, v});
        result.push_back({static_cast<float>(x) + 2.0f, v});
    }
    return result;
}

std::vector<Coordinate> OffsetCoordinates() {
    constexpr std::array<std::int32_t, 7> us{-32, -5, -1, 0, 1, 3, 31};
    constexpr std::array<std::int32_t, 6> vs{-2, -1, 0, 2, -32, 31};
    auto result = Coordinates(0u);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index].du = us[index % us.size()];
        result[index].dv = vs[index % vs.size()];
    }
    return result;
}

std::array<double, 4> TexelOf(std::uint32_t level, std::int32_t x, std::int32_t y) {
    if (level == 1u) return {240.0 * ((x + y + 1) & 1), 32.0 * y + 48.0, 16.0 * x + 40.0, 200.0};
    return {240.0 * ((x + y) & 1), 16.0 * x + 16.0, 32.0 * y + 32.0, 240.0};
}

void FillTexture(std::span<std::uint8_t> texels, std::uint32_t levels) {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Width, Height, levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= texels.size(), "image sample unnormalized: the mip chain does not fit the texel storage");
    std::fill(texels.begin(), texels.end(), std::uint8_t{0xeeu});
    for (std::uint32_t level = 0; level < levels; ++level) {
        const auto& mip = mips[level];
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                auto* texel = &texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                const auto value = TexelOf(level, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
                for (std::uint32_t component = 0; component < 4u; ++component) {
                    texel[component] = level <= 1u ? static_cast<std::uint8_t>(value[component]) : OtherLevelTexel;
                }
            }
        }
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t lastLevel, std::uint32_t maxMip) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | ((static_cast<std::uint32_t>(Width - 1) & 3u) << 30u),
        (static_cast<std::uint32_t>(Width - 1) >> 2u) | (static_cast<std::uint32_t>(Height - 1) << 14u),
        0xfacu | (lastLevel << 16u) | (Type2D << 28u),
        0u,
        maxMip << 4u,
        0u,
        0u,
    };
}

using Samples = std::vector<std::array<std::uint32_t, Results>>;

Samples Run(AgcDriver::VulkanDevice& device, const Program& program, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& sampler, const std::vector<Coordinate>& coordinates) {
    Samples samples;
    for (std::size_t first = 0; first < coordinates.size(); first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto coordinate = first + lane < coordinates.size() ? coordinates[first + lane] : Coordinate{0.5f, 0.5f};
            Buffer[lane * Words] = std::bit_cast<std::uint32_t>(coordinate.u);
            Buffer[lane * Words + 1u] = std::bit_cast<std::uint32_t>(coordinate.v);
            Buffer[lane * Words + 2u] = 0xffffc0c0u | (static_cast<std::uint32_t>(coordinate.du) & 0x3fu) | ((static_cast<std::uint32_t>(coordinate.dv) & 0x3fu) << 8u);
        }
        std::vector<std::uint32_t> userData(16, 0u);
        const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
        std::copy(buffer.begin(), buffer.end(), userData.begin());
        std::copy(texture.begin(), texture.end(), userData.begin() + 4);
        std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
        const auto code = program.code;
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
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
        for (std::uint32_t lane = 0; lane < Threads && first + lane < coordinates.size(); ++lane) {
            std::array<std::uint32_t, Results> values{};
            std::copy_n(&Buffer[lane * Words + ResultWord], Results, values.begin());
            samples.push_back(values);
        }
    }
    return samples;
}

std::array<double, 4> Fetch(const SamplerCase& sampler, std::uint32_t level, std::int32_t x, std::int32_t y) {
    const auto width = LevelWidth(level);
    const auto height = LevelHeight(level);
    if (sampler.border && (x < 0 || x >= width || y < 0 || y >= height)) return {0.0, 0.0, 0.0, 0.0};
    return TexelOf(level, std::clamp(x, 0, width - 1), std::clamp(y, 0, height - 1));
}

std::array<double, 4> Reference(const SamplerCase& sampler, std::uint32_t level, const Coordinate& coordinate) {
    if (!sampler.linear) return Fetch(sampler, level, static_cast<std::int32_t>(std::floor(coordinate.u)), static_cast<std::int32_t>(std::floor(coordinate.v)));
    const double u = static_cast<double>(coordinate.u) - 0.5;
    const double v = static_cast<double>(coordinate.v) - 0.5;
    const auto i = static_cast<std::int32_t>(std::floor(u));
    const auto j = static_cast<std::int32_t>(std::floor(v));
    const double alpha = u - i;
    const double beta = v - j;
    std::array<double, 4> result{};
    for (std::uint32_t component = 0; component < 4u; ++component) {
        result[component] = (1.0 - alpha) * (1.0 - beta) * Fetch(sampler, level, i, j)[component] + alpha * (1.0 - beta) * Fetch(sampler, level, i + 1, j)[component]
            + (1.0 - alpha) * beta * Fetch(sampler, level, i, j + 1)[component] + alpha * beta * Fetch(sampler, level, i + 1, j + 1)[component];
    }
    return result;
}

void Check(const Program& program, const SamplerCase& sampler, std::uint32_t level, const char* image, const std::vector<Coordinate>& coordinates, const Samples& samples) {
    Require(samples.size() == coordinates.size(), "image sample unnormalized: a dispatch lost samples");
    const double tolerance = sampler.linear ? 0.5 : 0.25;
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
        const auto& coordinate = coordinates[index];
        for (std::uint32_t instruction = 0; instruction < program.count; ++instruction) {
            const auto offset = program.offsets[instruction];
            const std::int32_t du = offset == Offset::Lane ? coordinate.du : offset == Offset::Constant ? ConstantOffsetU : 0;
            const std::int32_t dv = offset == Offset::Lane ? coordinate.dv : offset == Offset::Constant ? ConstantOffsetV : 0;
            const auto expected = Reference(sampler, level, {coordinate.u + static_cast<float>(du), coordinate.v + static_cast<float>(dv)});
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const double value = static_cast<double>(std::bit_cast<float>(samples[index][instruction * 4u + component])) * 255.0;
                Require(std::fabs(value - expected[component]) <= tolerance, std::string(program.instructions[instruction]) + ", " + sampler.name + ", " + image + ": (" + std::to_string(coordinate.u) + ", " + std::to_string(coordinate.v) + ") offset (" + std::to_string(du) + ", " + std::to_string(dv) + ") component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

void ExpectFailure(AgcDriver::VulkanDevice& device, const Program& program, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& sampler, std::string_view reason, const char* what) {
    const std::vector<Coordinate> one{{0.5f, 0.5f}};
    try {
        static_cast<void>(Run(device, program, texture, sampler, one));
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string(what) + ": unexpected error: " + error.what());
        return;
    }
    throw std::runtime_error(std::string(what) + " was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture(SingleLevel, 1);
        FillTexture(MultiLevel, StorageLevels);
        const auto coordinates = Coordinates(0u);
        const auto levelOneCoordinates = Coordinates(1u);
        const auto single = TextureDescriptor(SingleLevel.data(), 0u, 0u);
        const auto multi = TextureDescriptor(MultiLevel.data(), 0u, StorageLevels - 1u);
        const auto chain = TextureDescriptor(MultiLevel.data(), StorageLevels - 1u, StorageLevels - 1u);
        auto fromLevelOne = chain;
        fromLevelOne[3] |= 1u << 12u;
        const bool driverSamplesUnnormalizedAboveMipZero = device->DeviceName().find("llvmpipe") == std::string::npos;
        const auto offsetCoordinates = OffsetCoordinates();
        for (const auto& sampler : Samplers) {
            const auto singleSamples = Run(*device, Plain, single, sampler.words, coordinates);
            Check(Plain, sampler, 0u, "1-level image", coordinates, singleSamples);
            const auto multiSamples = Run(*device, Plain, multi, sampler.words, coordinates);
            Check(Plain, sampler, 0u, "first level of a 4-level image", coordinates, multiSamples);
            Require(multiSamples == singleSamples, std::string(sampler.name) + ": the first level of a 4-level image does not sample like a 1-level image");
            Check(Offsets, sampler, 0u, "1-level image", offsetCoordinates, Run(*device, Offsets, single, sampler.words, offsetCoordinates));
            Check(Gradient, sampler, 0u, "1-level image", coordinates, Run(*device, Gradient, single, sampler.words, coordinates));
            Check(Gradient, sampler, 0u, "first level of a 4-level image", coordinates, Run(*device, Gradient, multi, sampler.words, coordinates));
            if (!sampler.atBaseLevel) continue;
            const auto levelZeroSingle = Run(*device, LevelZero, single, sampler.words, coordinates);
            const auto levelZeroChain = Run(*device, LevelZero, chain, sampler.words, coordinates);
            Check(LevelZero, sampler, 0u, "4-level view", coordinates, levelZeroChain);
            Require(levelZeroChain == levelZeroSingle, std::string(sampler.name) + ": a 4-level view read by image_sample_lz does not sample its base level like a 1-level image");
            if (driverSamplesUnnormalizedAboveMipZero) Check(LevelZero, sampler, 1u, "3-level view starting at mip 1", levelOneCoordinates, Run(*device, LevelZero, fromLevelOne, sampler.words, levelOneCoordinates));
        }
        ExpectFailure(*device, Plain, TextureDescriptor(MultiLevel.data(), 2u, 2u), Samplers[0].words, "single-level", "a 3-level view read by a selected LOD");
        ExpectFailure(*device, Plain, chain, Samplers[0].words, "single-level", "a 4-level view read by image_sample_l and image_sample");
        ExpectFailure(*device, Plain, fromLevelOne, Samplers[0].words, "single-level", "a view starting at mip 1 read by image_sample_l and image_sample");
        ExpectFailure(*device, Offsets, chain, Samplers[0].words, "single-level", "a 4-level view read with texel offsets");
        ExpectFailure(*device, Gradient, chain, Samplers[0].words, "single-level", "a 4-level view read by image_sample_d");
        ExpectFailure(*device, LevelZero, chain, Samplers[2].words, "single-level", "a 4-level view through an S# with a minimum LOD and a LOD bias");
        const auto magFilterReference = Run(*device, Plain, single, {0x00008092u, 0x00fff000u, 0x05500000u, 0u}, coordinates);
        const auto unequalFilterSamples = Run(*device, Plain, single, {0x00008092u, 0x00fff000u, 0x05100000u, 0u}, coordinates);
        Require(unequalFilterSamples == magFilterReference, "unnormalized sampler with unequal filters does not sample with its magnification filter");
        const auto clampModeZeroSamples = Run(*device, Plain, single, {0x00008090u, 0x00fff000u, 0x05500000u, 0u}, coordinates);
        Require(clampModeZeroSamples == magFilterReference, "unnormalized sampler with clamp mode 0 does not sample with clamp mode 2");
        for (const std::size_t index : {std::size_t{0}, std::size_t{1}, std::size_t{4}}) {
            auto truncated = Samplers[index].words;
            truncated[0] |= 1u << 27u;
            const auto truncatedSamples = Run(*device, Plain, single, truncated, coordinates);
            Check(Plain, Samplers[index], 0u, "1-level image through an S# with TRUNC_COORD", coordinates, truncatedSamples);
            Require(truncatedSamples == Run(*device, Plain, single, Samplers[index].words, coordinates), std::string(Samplers[index].name) + ": TRUNC_COORD changed the samples");
        }
        const auto constantView = [&](std::uint32_t type, std::uint32_t format, std::uint32_t swizzle) {
            auto descriptor = TextureDescriptor(SingleLevel.data(), 0u, 0u);
            descriptor[1] = (descriptor[1] & ~(0x1ffu << 20u)) | (format << 20u);
            descriptor[3] = swizzle | (type << 28u);
            return descriptor;
        };
        for (const auto& sampler : Samplers) {
            for (const auto& sample : Run(*device, Plain, constantView(Type3D, Format8888UNorm, OneZeroOneZero), sampler.words, coordinates)) {
                for (std::uint32_t index = 0; index < Results; ++index) {
                    const std::uint32_t expected = index % 2u == 0u ? 0x3f800000u : 0u;
                    Require(sample[index] == expected, std::string(sampler.name) + ": a 3D view whose channels select (1, 0, 1, 0) returned " + Hex(sample[index]) + " for result " + std::to_string(index));
                }
            }
        }
        struct ConstantCase {
            const char* name;
            std::array<std::uint32_t, 8> texture;
            std::uint32_t one;
        };
        const std::array<ConstantCase, 4> constants{{
            {"a 3D UNORM view selecting (1, 0, 1, 0)", constantView(Type3D, Format8888UNorm, OneZeroOneZero), 0x3f800000u},
            {"a 32_UINT view selecting (1, 0, 1, 0)", constantView(Type2D, Format32UInt, OneZeroOneZero), 1u},
            {"a 32_SINT view selecting (1, 0, 1, 0)", constantView(Type2D, Format32SInt, OneZeroOneZero), 1u},
            {"a 1D view selecting (0, 0, 0, 0)", constantView(Type1D, Format8888UNorm, 0u), 0u},
        }};
        const std::array<const char*, 3> constantInstructions{"image_sample_lz dmask 0x6", "image_gather4_lz dmask 0x2", "image_gather4_lz dmask 0x4"};
        const Program constantProgram{ConstantCode, constantInstructions, Plain.offsets};
        for (const auto& constant : constants) {
            auto normalized = Samplers[0].words;
            normalized[0] &= ~0x8000u;
            for (const auto& sample : Run(*device, constantProgram, constant.texture, normalized, coordinates)) {
                const std::array<std::uint32_t, 10> expected{0u, constant.one, 0u, 0u, 0u, 0u, constant.one, constant.one, constant.one, constant.one};
                for (std::uint32_t index = 0; index < expected.size(); ++index) {
                    const auto instruction = constantInstructions[index < 2u ? 0u : index < 6u ? 1u : 2u];
                    Require(sample[index] == expected[index], std::string(instruction) + ", " + constant.name + ": result " + std::to_string(index) + " is " + Hex(sample[index]) + ", expected " + Hex(expected[index]));
                }
            }
        }
        std::puts("image sample unnormalized tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
