#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
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
constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t Texels = Width * Height;
constexpr std::uint32_t Sint8 = 6;
constexpr std::uint32_t Unorm8 = 1;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SwizzleX111 = 0x24cu;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> LinearSampler{0x00000092u, 0x00fff000u, 0x00500000u, 0u};

alignas(256) std::array<std::uint32_t, Texels * 4> Output{};
alignas(256) std::array<std::uint8_t, 256 * Height> Texture{};

constexpr std::array<std::uint32_t, 17> TexelCode(std::uint32_t word0) {
    return {
        0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x2c3e3c87u, 0x363c3cffu, 0x0000007fu, 0x7e140280u,
        0x7e160280u, 0x7e180280u, 0x7e1a0280u, word0, 0x00010a1eu, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
        0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 11> SamplerCode(std::uint32_t word0, std::uint32_t u, std::uint32_t v) {
    return {
        0x34060084u, 0x7e2802ffu, u, 0x7e2a02ffu, v, word0, 0x00610a14u, 0xbf8c3f70u,
        0xe0781000u, 0x80000a03u, 0xbf810000u,
    };
}

alignas(256) constexpr auto LoadX = TexelCode(0xf0001108u);
alignas(256) constexpr auto LoadXyzw = TexelCode(0xf0001f08u);
alignas(256) constexpr auto StoreX = TexelCode(0xf0201108u);
alignas(256) constexpr auto SampleLz = SamplerCode(0xf09c0f08u, 0x3ce00000u, 0x3d300000u);
alignas(256) constexpr auto Gather4Lz = SamplerCode(0xf11c0108u, 0x3c000000u, 0x3c000000u);

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t PitchBytes() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Unorm8, Width, Height, 1u);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texture.size(), "image 8_SINT: the texture does not fit its storage");
    return mips[0].pitchBytes;
}

std::uint8_t& Texel(std::uint32_t x, std::uint32_t y) {
    static const auto pitch = PitchBytes();
    return Texture[static_cast<std::size_t>(y) * pitch + x];
}

std::uint32_t Signed(std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(Texel(x, y))));
}

void Fill() {
    Texture.fill(0x5au);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            Texel(x, y) = static_cast<std::uint8_t>(0x80u + 37u * x + 11u * y);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t records, std::uint32_t stride = 0u) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t format, const std::array<std::uint32_t, 4>& sampler, std::uint32_t groups) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto texture = TextureDescriptor(Texture.data(), format, Width, Height, SwizzleX111);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Expect(std::uint32_t slot, const std::array<std::uint32_t, 4>& expected, const std::string& what) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const auto actual = Output[slot * 4u + component];
        Require(actual == expected[component], what + ": result " + std::to_string(slot) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(expected[component]));
    }
}

void CheckLoads(AgcDriver::VulkanDevice& device) {
    Fill();
    Require(Texel(0, 0) == 0x80u && Signed(0, 0) == 0xffffff80u, "image 8_SINT: texel (0, 0) must hold 0x80");
    Run(device, LoadX, Sint8, PointSampler, Texels / Threads);
    for (std::uint32_t texel = 0; texel < Texels; ++texel) {
        Expect(texel, {Signed(texel % Width, texel / Width), 0u, 0u, 0u}, "8_SINT image_load dmask:0x1");
    }
    Run(device, LoadXyzw, Sint8, PointSampler, Texels / Threads);
    for (std::uint32_t texel = 0; texel < Texels; ++texel) {
        Expect(texel, {Signed(texel % Width, texel / Width), 1u, 1u, 1u}, "8_SINT image_load dmask:0xf X 1 1 1");
    }
}

void CheckSamples(AgcDriver::VulkanDevice& device) {
    Fill();
    Run(device, SampleLz, Sint8, PointSampler, 1);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Expect(lane, {Signed(3, 5), 1u, 1u, 1u}, "8_SINT image_sample_lz with point filtering");
    }
    Run(device, Gather4Lz, Sint8, LinearSampler, 1);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Expect(lane, {Signed(0, 1), Signed(1, 1), Signed(1, 0), Signed(0, 0)}, "8_SINT image_gather4_lz through a bilinear sampler");
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t format, const std::array<std::uint32_t, 4>& sampler, const std::string& reason, const std::string& what) {
    std::string refusal;
    try {
        Run(device, code, format, sampler, 1);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(reason) != std::string::npos, what + " was not refused: " + refusal);
}

struct alignas(4096) GuestTables {
    std::array<std::array<std::uint32_t, 8>, 2> heap{};
    std::array<std::array<std::uint32_t, 4>, 3> materials{};
    std::array<std::uint32_t, 4> output{};
    std::array<std::uint32_t, 16> srt{};
};

struct alignas(256) TableTexture {
    std::array<std::uint8_t, 4096> bytes{};
};

GuestTables Tables;
TableTexture TableTextures[2];

alignas(256) const std::array<std::uint32_t, 20> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u,
    0x7e200500u, 0x93109010u, 0xf4200406u, 0x20000004u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u,
    0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u,
};

void CheckTable(AgcDriver::VulkanDevice& device) {
    TableTextures[0].bytes.fill(0x80u);
    TableTextures[1].bytes.fill(0xffu);
    for (std::uint32_t entry = 0; entry < 2u; ++entry) Tables.heap[entry] = TextureDescriptor(TableTextures[entry].bytes.data(), Sint8, 4u, 4u, IdentitySwizzle);
    const auto heap = BufferDescriptor(Tables.heap.data(), 2u, 32u);
    const auto materials = BufferDescriptor(Tables.materials.data(), 3u, 16u);
    const auto output = BufferDescriptor(Tables.output.data(), 16u);
    std::copy(heap.begin(), heap.end(), Tables.srt.begin());
    std::copy(materials.begin(), materials.end(), Tables.srt.begin() + 8);
    std::copy(output.begin(), output.end(), Tables.srt.begin() + 12);
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Tables.srt.data()));
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::span<const std::uint32_t> code(TableCode);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
        {reinterpret_cast<std::uintptr_t>(&Tables), std::as_bytes(std::span<const GuestTables, 1>(&Tables, 1))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{8, 1, 1}, 0u, {false, false, false}, false, 1};
    for (const std::uint32_t key : {0u, 1u}) {
        Tables.materials = {{{0u, key, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 1u, 0u, 0u}}};
        Tables.output.fill(0xdeadbeefu);
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
        const auto expected = key == 0u ? 0xffffff80u : 0xffffffffu;
        Require(Tables.output[0] == expected, "8_SINT image table entry " + std::to_string(key) + ": image_sample_lz read " + Hex(Tables.output[0]) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckLoads(*device);
        CheckSamples(*device);
        CheckTable(*device);
        RequireRefused(*device, StoreX, Sint8, PointSampler, "storage image descriptor uses an unsupported format", "8_SINT image_store");
        std::puts("image 8_SINT tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
