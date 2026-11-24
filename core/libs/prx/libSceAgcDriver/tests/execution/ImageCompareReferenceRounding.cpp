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
constexpr std::uint32_t Format16UNorm = 7;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ReferenceWord = 6;
constexpr std::uint32_t Result = 8;
constexpr std::uint32_t CompareLess = 1;
constexpr std::uint32_t CompareEqual = 2;

alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(4096) std::array<std::uint8_t, 32768> UnormTexels{};
alignas(4096) std::array<std::uint8_t, 32768> FloatTexels{};

alignas(256) constexpr std::array<std::uint32_t, 17> Code2D_Lz{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf0bc0108, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code2D_ImplicitLod{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf0a00108, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

struct Case {
    const char* name;
    double delta;
};

constexpr std::array<Case, 13> UnormCases{{
    {"k - 0.45", -0.45},
    {"k + 0.51", 0.51},
    {"k + 0.9", 0.9},
    {"k - 0.9", -0.9},
    {"k - 0.5", -0.5},
    {"k - 0.499", -0.499},
    {"k - 0.4", -0.4},
    {"k + 0.4", 0.4},
    {"k + 0.5", 0.5},
    {"k + 0.501", 0.501},
    {"k + 0.56", 0.56},
    {"k + 0.6", 0.6},
    {"k + 1.5", 1.5},
}};

constexpr std::array<Case, 7> FloatCases{{
    {"exact k", 0.0},
    {"k - 0.4", -0.4},
    {"k - 0.01", -0.01},
    {"k + 0.01", 0.01},
    {"k + 0.4", 0.4},
    {"k + 0.5", 0.5},
    {"k + 1.0", 1.0},
}};

std::uint32_t TexelOf(std::uint32_t tid) {
    return tid % Size;
}

std::uint32_t CodeOf(std::uint32_t tid) {
    return (TexelOf(tid) & 1u) != 0u ? 32768u : 1u;
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t NearestCode(float reference) {
    const auto product = std::clamp(reference * 65535.0f, 0.0f, 65535.0f);
    return static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(product)));
}

std::array<std::uint32_t, 8> PlaneDescriptor(void* data, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void FillUnorm() {
    const auto descriptor = PlaneDescriptor(UnormTexels.data(), Format16UNorm);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(geometry.guestBytes <= UnormTexels.size(), "16_UNORM texture does not fit texel storage");
    std::fill(UnormTexels.begin(), UnormTexels.end(), 0xeeu);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto value = static_cast<std::uint16_t>((x & 1u) != 0u ? 32768u : 1u);
            auto* texel = &UnormTexels[geometry.GuestLayerOffset(0) + geometry.mips.at(0).tiledOffset + static_cast<std::uint64_t>(y) * geometry.mips.at(0).pitchBytes + x * 2u];
            texel[0] = static_cast<std::uint8_t>(value);
            texel[1] = static_cast<std::uint8_t>(value >> 8u);
        }
    }
}

void FillFloat() {
    const auto descriptor = PlaneDescriptor(FloatTexels.data(), Format32Float);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(geometry.guestBytes <= FloatTexels.size(), "32_FLOAT texture does not fit texel storage");
    std::fill(FloatTexels.begin(), FloatTexels.end(), 0xeeu);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto value = static_cast<float>((x & 1u) != 0u ? 32768u : 1u);
            auto* texel = &FloatTexels[geometry.GuestLayerOffset(0) + geometry.mips.at(0).tiledOffset + static_cast<std::uint64_t>(y) * geometry.mips.at(0).pitchBytes + x * 4u];
            std::copy_n(reinterpret_cast<const std::uint8_t*>(&value), 4u, texel);
        }
    }
}

void FillInput(double delta, bool unorm) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = TexelOf(tid);
        const auto k = static_cast<double>(CodeOf(tid));
        const auto reference = static_cast<float>(unorm ? (k + delta) / 65535.0 : k + delta);
        Buffer[tid * Words] = Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(Size));
        Buffer[tid * Words + 1u] = Bits(0.5f / static_cast<float>(Size));
        Buffer[tid * Words + ReferenceWord] = Bits(reference);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t compare) {
    return {compare << 12u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& textureDesc, std::uint32_t compare) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto sampler = SamplerDescriptor(compare);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(textureDesc.begin(), textureDesc.end(), userData.begin() + 4);
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

void Check(const std::string& op, bool unorm, bool equal) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto k = CodeOf(tid);
        const auto reference = std::bit_cast<float>(Buffer[tid * Words + ReferenceWord]);
        const auto code = unorm ? NearestCode(reference) : static_cast<std::uint32_t>(reference);
        const bool expected = unorm ? (equal ? code == k : code < k) : (equal ? reference == static_cast<float>(k) : reference < static_cast<float>(k));
        const float value = std::bit_cast<float>(Buffer[tid * Words + Result]);
        const float want = expected ? 1.0f : 0.0f;
        Require(value == want, op + ": thread " + std::to_string(tid) + " (code " + std::to_string(k) + ", reference " + std::to_string(reference) + ") is " + std::to_string(value) + ", expected " + std::to_string(want));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillUnorm();
        FillFloat();
        const auto unorm = PlaneDescriptor(UnormTexels.data(), Format16UNorm);
        const auto floatTexture = PlaneDescriptor(FloatTexels.data(), Format32Float);

        for (const auto& entry : UnormCases) {
            for (const auto& code : {Code2D_Lz, Code2D_ImplicitLod}) {
                FillInput(entry.delta, true);
                Run(*device, code, unorm, CompareEqual);
                Check(std::string("16_UNORM EQUAL ") + entry.name, true, true);
                FillInput(entry.delta, true);
                Run(*device, code, unorm, CompareLess);
                Check(std::string("16_UNORM LESS ") + entry.name, true, false);
            }
        }

        for (const auto& entry : FloatCases) {
            FillInput(entry.delta, false);
            Run(*device, Code2D_Lz, floatTexture, CompareEqual);
            Check(std::string("32_FLOAT EQUAL ") + entry.name, false, true);
            FillInput(entry.delta, false);
            Run(*device, Code2D_Lz, floatTexture, CompareLess);
            Check(std::string("32_FLOAT LESS ") + entry.name, false, false);
        }

        std::puts("16_UNORM and 32_FLOAT comparison reference tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
