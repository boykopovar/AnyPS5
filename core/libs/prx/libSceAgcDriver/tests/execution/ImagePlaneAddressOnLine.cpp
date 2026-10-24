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
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Mips = 5;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1D = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SampleResult = 8;
constexpr std::uint32_t LoadResult = 12;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Line{};
alignas(256) std::array<std::uint8_t, 16384> Plane{};
alignas(256) std::array<std::uint8_t, 16384> MippedLine{};

alignas(256) constexpr std::array<std::uint32_t, 36> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf09c0f08, 0x00610c02,
    0xf0001f08, 0x00011005, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038,
    0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> ExplicitLodCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xbf8c3f70, 0xf0900f08, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> ArrayExplicitLodCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xbf8c3f70, 0xf0900f28, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> ArrayLevelZeroCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xbf8c3f70, 0xf09c0f28, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> LineLodCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301008, 0x80000301, 0xbf8c3f70, 0xf0900f00, 0x00610c02, 0xbf8c3f70,
    0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

struct SamplerCase {
    const char* name;
    std::uint32_t clamp;
    std::uint32_t filter;
    std::uint32_t mipFilter;
};

constexpr std::uint32_t ClampWrap = 0;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t ClampBorder = 6;
constexpr std::uint32_t FilterPoint = 0;
constexpr std::uint32_t FilterLinear = 1;
constexpr std::uint32_t MipPoint = 1;
constexpr std::uint32_t MipLinear = 2;

constexpr std::array<SamplerCase, 5> SamplerCases{{
    {"point filtering, point mips, clamp to edge", ClampEdge, FilterPoint, MipPoint},
    {"point filtering, point mips, wrap", ClampWrap, FilterPoint, MipPoint},
    {"linear filtering, linear mips, clamp to edge", ClampEdge, FilterLinear, MipLinear},
    {"linear filtering, linear mips, wrap", ClampWrap, FilterLinear, MipLinear},
    {"linear filtering, linear mips, clamp to border", ClampBorder, FilterLinear, MipLinear},
}};

constexpr std::array<float, 8> LodU{0.03125f, 0.33125f, 0.96875f, 0.0f, 1.0f, -0.2f, 1.3f, 0.6f};
constexpr std::array<float, 8> Lods{0.0f, 1.0f, 2.0f, 1.25f, 2.75f, 3.6f, 6.0f, -1.0f};

constexpr std::array<float, 5> Slices{0.0f, 1.0f, 2.7f, -1.0f, 5.0f};
constexpr std::array<float, 8> LineV{0.0f, 0.5f, 1.0f, 1.5f, -0.25f, 3.0f, 0.25f, 0.75f};
constexpr std::array<std::uint32_t, 8> LineY{0u, 1u, 2u, 3u, 0xffffffffu, 7u, 0u, 5u};

std::uint32_t TexelOf(std::uint32_t tid) {
    return (tid * 7u + 3u) % Size;
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void FillInput(bool line) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = TexelOf(tid);
        const auto u = Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(Size));
        const auto v = line ? Bits(LineV[tid % LineV.size()]) : Bits((static_cast<float>((tid * 5u + 1u) % Size) + 0.5f) / static_cast<float>(Size));
        const auto y = line ? LineY[tid % LineY.size()] : (tid * 5u + 1u) % Size;
        const std::array<std::uint32_t, 6> words{u, v, 0u, x, y, 0u};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

std::array<std::uint32_t, 8> LineDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Line.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        (Size - 1u) >> 2u,
        0xfacu | (Type1D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 8> MippedLineDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(MippedLine.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        (Size - 1u) >> 2u,
        0xfacu | ((Mips - 1u) << 16u) | (Type1D << 28u),
        0u,
        (Mips - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 8> PlaneDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Plane.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void FillTexture(std::span<std::uint8_t> storage, const std::array<std::uint32_t, 8>& descriptor, std::uint32_t rows) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(geometry.guestBytes <= storage.size(), "image plane address on a line: the texture does not fit the texel storage");
    const auto& mip = geometry.mips.at(0);
    std::fill(storage.begin(), storage.end(), 0xeeu);
    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            auto* texel = &storage[geometry.GuestLayerOffset(0) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
            texel[0] = 77u;
            texel[1] = static_cast<std::uint8_t>(x);
            texel[2] = static_cast<std::uint8_t>(y);
            texel[3] = 255u;
        }
    }
}

std::array<std::uint32_t, 4> MipTexel(std::uint32_t level, std::uint32_t x) {
    return {10u + 40u * level, x * 12u + 3u, 200u - 30u * level, 255u - 7u * x - 11u * level};
}

void FillMippedLine() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(MippedLineDescriptor()));
    Require(geometry.guestBytes <= MippedLine.size(), "image plane address on a line: the mipped texture does not fit the texel storage");
    Require(geometry.mips.size() == Mips, "image plane address on a line: the mipped texture has " + std::to_string(geometry.mips.size()) + " mips");
    std::fill(MippedLine.begin(), MippedLine.end(), 0xeeu);
    for (std::uint32_t level = 0; level < Mips; ++level) {
        const auto& mip = geometry.mips[level];
        for (std::uint32_t x = 0; x < (Size >> level); ++x) {
            const auto value = MipTexel(level, x);
            auto* texel = &MippedLine[geometry.GuestLayerOffset(0) + mip.tiledOffset + x * 4u];
            for (std::uint32_t component = 0; component < 4u; ++component) texel[component] = static_cast<std::uint8_t>(value[component]);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

std::array<std::uint32_t, 4> SamplerDescriptor(const SamplerCase& sampler) {
    return {sampler.clamp | (sampler.clamp << 3u) | (sampler.clamp << 6u), 0xfffu << 12u, (sampler.filter << 20u) | (sampler.filter << 22u) | (sampler.mipFilter << 26u), 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& sampler = SamplerDescriptor()) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Expect(std::uint32_t tid, std::uint32_t result, const std::array<std::uint32_t, 4>& expected, const std::string& name) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float value = std::bit_cast<float>(Buffer[tid * Words + result + component]) * 255.0f;
        Require(std::lround(value) == static_cast<long>(expected[component]), name + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
    }
}

void CheckLine() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> texel{77u, TexelOf(tid), 0u, 255u};
        Expect(tid, SampleResult, texel, "image_sample_lz 2d on a 1D texture with v " + std::to_string(LineV[tid % LineV.size()]));
        Expect(tid, LoadResult, texel, "image_load 2d on a 1D texture with y " + std::to_string(static_cast<std::int32_t>(LineY[tid % LineY.size()])));
    }
}

void CheckPlane() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> texel{77u, TexelOf(tid), (tid * 5u + 1u) % Size, 255u};
        Expect(tid, SampleResult, texel, "image_sample_lz 2d on a 2D texture");
        Expect(tid, LoadResult, texel, "image_load 2d on a 2D texture");
    }
}

float LaneU(std::uint32_t tid) {
    return LodU[(tid + tid / 8u) % LodU.size()];
}

float LaneLod(std::uint32_t tid) {
    return Lods[tid / 4u];
}

void FillLodInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> words{Bits(LaneU(tid)), Bits(LineV[(tid / 2u) % LineV.size()]), Bits(LaneLod(tid)), 0u};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

void FillArrayInput(bool levelZero) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lod = levelZero ? 0.0f : LaneLod(tid);
        const std::array<std::uint32_t, 4> words{Bits(LaneU(tid)), Bits(LineV[(tid / 2u) % LineV.size()]), Bits(Slices[tid % Slices.size()]), Bits(lod)};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

void FillLineInput(bool levelZero) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lod = levelZero ? 0.0f : LaneLod(tid);
        const std::array<std::uint32_t, 4> words{Bits(LaneU(tid)), 0u, Bits(lod), 0u};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

std::array<std::uint32_t, Threads * 4u> SampleResults() {
    std::array<std::uint32_t, Threads * 4u> results{};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy_n(&Buffer[tid * Words + SampleResult], 4u, &results[tid * 4u]);
    return results;
}

std::uint32_t PointLevel(float lod) {
    return static_cast<std::uint32_t>(std::clamp(std::lround(lod), 0L, static_cast<long>(Mips - 1u)));
}

std::uint32_t PointTexel(float u, std::uint32_t level, std::uint32_t clamp) {
    const auto width = static_cast<std::int64_t>(Size >> level);
    const auto texel = static_cast<std::int64_t>(std::floor(u * static_cast<float>(width)));
    if (clamp == ClampWrap) return static_cast<std::uint32_t>((texel % width + width) % width);
    return static_cast<std::uint32_t>(std::clamp<std::int64_t>(texel, 0, width - 1));
}

void CheckExplicitLod(AgcDriver::VulkanDevice& device) {
    FillMippedLine();
    for (const auto& sampler : SamplerCases) {
        const auto samplerWords = SamplerDescriptor(sampler);
        FillLodInput();
        Run(device, LineLodCode, MippedLineDescriptor(), samplerWords);
        const auto reference = SampleResults();
        FillLodInput();
        Run(device, ExplicitLodCode, MippedLineDescriptor(), samplerWords);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto u = LaneU(tid);
            const auto lod = LaneLod(tid);
            const auto what = std::string("image_sample_l 2d on a 1D texture with ") + sampler.name + ", u " + std::to_string(u) + ", v " + std::to_string(LineV[(tid / 2u) % LineV.size()]) + ", LOD " + std::to_string(lod);
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto value = Buffer[tid * Words + SampleResult + component];
                const auto expected = reference[tid * 4u + component];
                Require(value == expected, what + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " has bits " + std::to_string(value) + ", image_sample_l 1d returned bits " + std::to_string(expected));
            }
            if (sampler.filter == FilterPoint) {
                const auto level = PointLevel(lod);
                Expect(tid, SampleResult, MipTexel(level, PointTexel(u, level, sampler.clamp)), what);
            } else if (sampler.clamp != ClampBorder) {
                const auto mark = std::bit_cast<float>(Buffer[tid * Words + SampleResult]) * 255.0f;
                const auto expected = 10.0f + 40.0f * std::clamp(lod, 0.0f, static_cast<float>(Mips - 1u));
                Require(std::fabs(mark - expected) <= 1.0f, what + ": thread " + std::to_string(tid) + " blends the levels to " + std::to_string(mark) + ", expected " + std::to_string(expected));
            }
        }
    }
}

void CheckArray(AgcDriver::VulkanDevice& device) {
    FillMippedLine();
    for (const bool levelZero : {false, true}) {
        for (const auto& sampler : SamplerCases) {
            const auto samplerWords = SamplerDescriptor(sampler);
            FillLineInput(levelZero);
            Run(device, LineLodCode, MippedLineDescriptor(), samplerWords);
            const auto reference = SampleResults();
            FillArrayInput(levelZero);
            Run(device, levelZero ? std::span<const std::uint32_t>(ArrayLevelZeroCode) : std::span<const std::uint32_t>(ArrayExplicitLodCode), MippedLineDescriptor(), samplerWords);
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                const auto what = std::string(levelZero ? "image_sample_lz" : "image_sample_l") + " 2d array on a 1D texture with " + sampler.name + ", u " + std::to_string(LaneU(tid)) + ", v " + std::to_string(LineV[(tid / 2u) % LineV.size()]) + ", slice " + std::to_string(Slices[tid % Slices.size()]);
                for (std::uint32_t component = 0; component < 4u; ++component) {
                    const auto value = Buffer[tid * Words + SampleResult + component];
                    const auto expected = reference[tid * 4u + component];
                    Require(value == expected, what + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " has bits " + std::to_string(value) + ", image_sample_l 1d returned bits " + std::to_string(expected));
                }
            }
        }
    }
}
}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture(Line, LineDescriptor(), 1u);
        FillInput(true);
        Run(*device, Code, LineDescriptor());
        CheckLine();
        FillTexture(Plane, PlaneDescriptor(), Size);
        FillInput(false);
        Run(*device, Code, PlaneDescriptor());
        CheckPlane();
        CheckExplicitLod(*device);
        CheckArray(*device);
        std::puts("image plane address on line tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
