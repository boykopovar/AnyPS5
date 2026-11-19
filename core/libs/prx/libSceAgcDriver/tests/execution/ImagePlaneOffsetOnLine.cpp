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
constexpr std::uint32_t Width = 16;
constexpr std::uint32_t Levels = 2;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1D = 8;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t LevelZeroResult = 8;
constexpr std::uint32_t ExplicitLodResult = 12;
constexpr std::uint32_t MipLoadResult = 16;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(4096) std::array<std::uint8_t, 8192> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 48> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0xf0dc0f08, 0x00610c02, 0xf0d00f08, 0x00611002, 0xf0041f08, 0x00011406, 0xbf8c3f70, 0xe0701020,
    0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030,
    0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c, 0x80001301, 0xe0701040,
    0x80001401, 0xe0701044, 0x80001501, 0xe0701048, 0x80001601, 0xe070104c, 0x80001701, 0xbf810000,
};

constexpr std::array<float, 8> PlaneV{0.0f, 0.5f, 1.0f, 3.0f, -0.25f, 7.0f, 0.75f, 0.25f};

std::int32_t OffsetX(std::uint32_t tid) {
    return static_cast<std::int32_t>(tid % 16u) - 8;
}

std::int32_t OffsetY(std::uint32_t tid) {
    return static_cast<std::int32_t>((tid * 5u) % 16u) - 8;
}

float U(std::uint32_t tid) {
    return -0.3f + 0.05f * static_cast<float>(tid);
}

std::uint32_t Level(std::uint32_t tid) {
    return (tid / 3u) % Levels;
}

std::uint32_t LoadX(std::uint32_t tid) {
    return (tid * 7u) % 20u;
}

std::uint32_t LoadY(std::uint32_t tid) {
    constexpr std::array<std::uint32_t, 5> rows{0u, 1u, 2u, 5u, 0xffffffffu};
    return rows[tid % rows.size()];
}

std::uint32_t LoadMip(std::uint32_t tid) {
    return (tid / 2u) % 4u;
}

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto offset = (static_cast<std::uint32_t>(OffsetX(tid)) & 0x3fu) | ((static_cast<std::uint32_t>(OffsetY(tid)) & 0x3fu) << 8u);
        const std::array<std::uint32_t, 7> words{offset, std::bit_cast<std::uint32_t>(U(tid)), std::bit_cast<std::uint32_t>(PlaneV[tid % PlaneV.size()]), std::bit_cast<std::uint32_t>(static_cast<float>(Level(tid))), LoadX(tid), LoadY(tid), LoadMip(tid)};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

std::array<std::uint32_t, 8> LineDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | ((Levels - 1u) << 16u) | (Type1D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::uint8_t LevelMarker(std::uint32_t level) {
    return static_cast<std::uint8_t>(40u + level * 100u);
}

void FillTexels() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(LineDescriptor()));
    Require(geometry.guestBytes <= Texels.size(), "image plane offset on a line: the texture does not fit the texel storage");
    Require(geometry.mips.size() == Levels, "image plane offset on a line: the texture has the wrong level count");
    std::fill(Texels.begin(), Texels.end(), 0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = geometry.mips.at(level);
        for (std::uint32_t x = 0; x < (Width >> level); ++x) {
            auto* texel = &Texels[mip.tiledOffset + x * 4u];
            texel[0] = LevelMarker(level);
            texel[1] = static_cast<std::uint8_t>(x * 16u);
            texel[2] = 0u;
            texel[3] = 255u;
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = LineDescriptor();
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
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

void Expect(std::uint32_t tid, std::uint32_t result, std::uint32_t level, const std::string& name) {
    const auto width = static_cast<std::int32_t>(Width >> level);
    const auto x = std::clamp(static_cast<std::int32_t>(std::floor(U(tid) * static_cast<float>(width))) + OffsetX(tid), 0, width - 1);
    const std::array<std::uint32_t, 4> expected{LevelMarker(level), static_cast<std::uint32_t>(x) * 16u, 0u, 255u};
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float value = std::bit_cast<float>(Buffer[tid * Words + result + component]) * 255.0f;
        Require(std::lround(value) == static_cast<long>(expected[component]), name + ": thread " + std::to_string(tid) + " (u " + std::to_string(U(tid)) + ", offset " + std::to_string(OffsetX(tid)) + "/" + std::to_string(OffsetY(tid)) + ", v " + std::to_string(PlaneV[tid % PlaneV.size()]) + ") component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
    }
}

void ExpectMipLoad(std::uint32_t tid) {
    const auto mip = LoadMip(tid);
    const auto x = LoadX(tid);
    const bool inside = mip < Levels && x < (Width >> mip);
    const std::array<std::uint32_t, 4> expected = inside ? std::array<std::uint32_t, 4>{LevelMarker(mip), x * 16u, 0u, 255u} : std::array<std::uint32_t, 4>{};
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const float value = std::bit_cast<float>(Buffer[tid * Words + MipLoadResult + component]) * 255.0f;
        Require(std::lround(value) == static_cast<long>(expected[component]), "image_load_mip 2d on a 1D texture: thread " + std::to_string(tid) + " (x " + std::to_string(x) + ", y " + std::to_string(static_cast<std::int32_t>(LoadY(tid))) + ", mip " + std::to_string(mip) + ") component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexels();
        FillInput();
        Run(*device);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Expect(tid, LevelZeroResult, 0u, "image_sample_lz_o 2d on a 1D texture");
            Expect(tid, ExplicitLodResult, Level(tid), "image_sample_l_o 2d on a 1D texture at LOD " + std::to_string(Level(tid)));
            ExpectMipLoad(tid);
        }
        std::puts("image plane offset on line tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
