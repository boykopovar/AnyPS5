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
#include <utility>
#include <vector>

namespace {

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 16;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format8UNorm = 1;
constexpr std::uint32_t Format88UNorm = 14;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 4> Prologue{0x34020084, 0xe0381000, 0x80000401, 0xbf8c3f70};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        IdentitySwizzle | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {(2u << 3u) | (2u << 6u), 0u, 0u, 0u};
}

void FillTexels(std::uint32_t format, std::uint32_t bytes) {
    Texels.fill(0);
    const auto descriptor = TextureDescriptor(format);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    const auto& mip = surface.mips.at(0);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto* texel = Texels.data() + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * bytes;
            for (std::uint32_t c = 0; c < bytes; ++c) {
                texel[c] = static_cast<std::uint8_t>(((y * Width + x) * 4u + c + 1u) & 0xffu);
            }
        }
    }
}

void FillInput() {
    constexpr std::array<std::array<float, 2>, 16> edges{{
        {0.03125f, 0.0f}, {0.09375f, 0.25f}, {0.53125f, 0.5f}, {0.96875f, 0.75f},
        {-0.25f, -0.3f}, {1.5f, 0.99f}, {0.4f, 1.0f}, {0.2f, 2.0f},
        {0.3f, 0.6f}, {0.7f, 0.1f}, {0.0f, -1.0f}, {1.0f, 0.5f},
        {0.0625f, 0.0f}, {0.15f, 0.375f}, {0.85f, 0.875f}, {-3.0f, 0.2f},
    }};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        float u = static_cast<float>(tid % Width) / static_cast<float>(Width);
        float v = static_cast<float>(tid % Height) / static_cast<float>(Height);
        if (tid >= Width) {
            u = edges[tid - Width][0];
            v = edges[tid - Width][1];
            if (tid % 2u == 1u && tid < Width + 4u) u = std::nextafter(u, -1.0f);
        }
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(u);
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(v);
        Input[tid * 4u + 2u] = 0u;
        Input[tid * 4u + 3u] = 0u;
    }
}

std::uint8_t TexelByte(std::int32_t x, std::int32_t y, std::uint32_t c) {
    const auto cx = static_cast<std::uint32_t>(std::clamp(x, 0, static_cast<int>(Width) - 1));
    const auto cy = static_cast<std::uint32_t>(std::max(y, 0));
    if (cy >= Height) return 0u;
    return static_cast<std::uint8_t>(((cy * Width + cx) * 4u + c + 1u) & 0xffu);
}

std::array<std::uint32_t, 4> Expected(std::uint32_t tid, std::uint32_t dmask, std::uint32_t bytes) {
    const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
    const float v = std::bit_cast<float>(Input[tid * 4u + 1u]);
    const auto anchor = static_cast<std::int32_t>(std::floor(u * static_cast<float>(Width) - 0.5f));
    const auto row = static_cast<std::int32_t>(std::floor(v * static_cast<float>(Height)));
    std::uint32_t stream[4] = {};
    if (bytes <= 2u) {
        for (std::uint32_t element = 0; element < 8u; ++element) {
            for (std::uint32_t c = 0; c < bytes; ++c) {
                const auto bit = (element * bytes + c) * 8u;
                stream[bit / 32u] |= static_cast<std::uint32_t>(TexelByte(anchor + static_cast<std::int32_t>(element) - 3, row, c)) << (bit % 32u);
            }
        }
    }
    std::array<std::uint32_t, 4> selected{};
    std::uint32_t next = 0;
    for (std::uint32_t bit = 0; bit < 4u; ++bit) {
        if (((dmask >> bit) & 1u) != 0u) selected[next++] = stream[bit];
    }
    return selected;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> program, std::uint32_t format) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(format);
    const auto sampler = SamplerDescriptor();
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(program.data()), std::as_bytes(std::span<const std::uint32_t>(program))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(program.data()), program, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void RunGather8h(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t bytes, std::uint32_t dmask, bool tfe) {
    std::vector<std::uint32_t> program(Prologue.begin(), Prologue.end());
    program.insert(program.end(), {0x7e1002ffu, 0xdeadbeefu, 0x7e1202ffu, 0xdeadbeefu, 0x7e1402ffu, 0xdeadbeefu, 0x7e1602ffu, 0xdeadbeefu});
    program.push_back(0xf18c0008u | (dmask << 8u) | (tfe ? 0x00010000u : 0u));
    program.insert(program.end(), {0x00820804u, 0xbf8c3f70u, 0xe0781000u, 0x80010801u, 0xbf810000u});
    Output.fill(0xabababab);
    const auto result = Compile(device, program, format);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(program.data()));
    device.WaitIdle();
    const auto written = static_cast<std::uint32_t>(std::popcount(dmask));
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Expected(tid, dmask, bytes);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const std::uint32_t actual = Output[tid * 4u + component];
            const std::uint32_t wanted = component < written ? expected[component] : 0xdeadbeefu;
            Require(actual == wanted, std::string("packed eight-texel gather: format ") + std::to_string(format) + " dmask " + std::to_string(dmask) + (tfe ? " tfe" : "") + " thread " + std::to_string(tid) + " register " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(wanted));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        for (const auto [format, bytes] : {std::pair{Format8UNorm, 1u}, std::pair{Format88UNorm, 2u}, std::pair{Format8888UNorm, 4u}}) {
            FillTexels(format, bytes);
            for (const std::uint32_t dmask : {0xfu, 0x1u, 0x3u, 0x5u, 0xau, 0x8u, 0x2u}) RunGather8h(*device, format, bytes, dmask, false);
            RunGather8h(*device, format, bytes, 0x3u, true);
        }
        std::puts("image packed eight-texel gather tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
