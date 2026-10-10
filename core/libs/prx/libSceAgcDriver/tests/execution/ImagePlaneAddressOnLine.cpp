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
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1D = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SampleResult = 8;
constexpr std::uint32_t LoadResult = 12;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Line{};
alignas(256) std::array<std::uint8_t, 16384> Plane{};

alignas(256) constexpr std::array<std::uint32_t, 36> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf09c0f08, 0x00610c02,
    0xf0001f08, 0x00011005, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038,
    0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> ExplicitLodCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xbf8c3f70, 0xf0900f08, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xbf810000,
};

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

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
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

void RequireRefused(AgcDriver::VulkanDevice& device) {
    std::string refusal;
    try {
        Run(device, ExplicitLodCode, LineDescriptor());
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("incompatible with the static runtime image interface") != std::string::npos, "image_sample_l 2d on a 1D texture was not refused: " + refusal);
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
        RequireRefused(*device);
        std::puts("image plane address on line tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
