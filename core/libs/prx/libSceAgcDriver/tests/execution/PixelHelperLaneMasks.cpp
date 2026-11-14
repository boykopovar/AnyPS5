#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> MaskPixelCode64{
    0xbefe0a7e, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0xd4040014, 0x0001e702, 0xd4040016, 0x0001e703, 0x87981614, 0xd5010002, 0x006204f6,
    0xd5010003, 0x006206f6, 0x10040410, 0x10060611, 0xf0800f08, 0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> MaskPixelCode32{
    0xbefe0a7e, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0xd4040014, 0x0001e702, 0xd4040016, 0x0001e703, 0x87181614, 0xd5010002, 0x006204f6,
    0xd5010003, 0x006206f6, 0x10040410, 0x10060611, 0xf0800f08, 0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> CopyPixelCode64{
    0xbefe04c1, 0x7e0402f6, 0x7e0602f6, 0xbe94047e, 0xbefe0414, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0x10040410, 0x10060611, 0xf0800f08,
    0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> CopyPixelCode32{
    0xbefe03c1, 0x7e0402f6, 0x7e0602f6, 0xbe94037e, 0xbefe0314, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0x10040410, 0x10060611, 0xf0800f08,
    0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> BranchPixelCode64{
    0xbefe0a7e, 0x7e0402f6, 0x7e0602f6, 0x7c0200f3, 0xbe94246a, 0x88fe147e, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0x10040410, 0x10060611,
    0xf0800f08, 0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> BranchPixelCode32{
    0xbefe0a7e, 0x7e0402f6, 0x7e0602f6, 0x7c0200f3, 0xbe943c6a, 0x887e147e, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0x10040410, 0x10060611,
    0xf0800f08, 0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

struct Program {
    const char* name;
    std::span<const std::uint32_t> wave64;
    std::span<const std::uint32_t> wave32;
};

constexpr std::array<Program, 3> Programs{{
    {"compare masks combined by a scalar AND", MaskPixelCode64, MaskPixelCode32},
    {"EXEC copied to an SGPR and back without s_wqm", CopyPixelCode64, CopyPixelCode32},
    {"EXEC saved by s_and_saveexec and restored by s_or", BranchPixelCode64, BranchPixelCode32},
}};

std::vector<std::array<float, 4>> Triangles() {
    std::vector<std::array<float, 4>> vertices;
    for (std::uint32_t cell = 0; cell < 24; ++cell) {
        const float x = -1.0f + (2.0f / 6.0f) * static_cast<float>(cell % 6);
        const float y = -1.0f + 0.5f * static_cast<float>(cell / 6);
        const float skew = 0.07f * static_cast<float>(cell % 5);
        vertices.push_back({x + (0.05f + skew) / 3.0f, y + 0.025f, 0.5f, 1.0f});
        vertices.push_back({x + 0.31f, y + (0.11f + skew) * 0.5f, 0.5f, 1.0f});
        vertices.push_back({x + (0.37f - skew * 0.5f) / 3.0f, y + 0.455f, 0.5f, 1.0f});
    }
    return vertices;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32) & 0xffffu) | (16u << 16), count, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Texels.data());
    return {static_cast<std::uint32_t>(address >> 8), static_cast<std::uint32_t>((address >> 40) & 0xffu) | (56u << 20) | (3u << 30),
            ((Size - 1u) >> 2) | ((Size - 1u) << 14), 0xfacu | ((Levels - 1u) << 16) | (9u << 28), 0u, (Levels - 1u) << 4, 0u, 0u};
}

void FillTexture() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor()));
    Require(geometry.guestBytes <= Texels.size(), "helper lane mask texture does not fit");
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = geometry.mips[level];
        for (std::uint32_t y = 0; y < (Size >> level); ++y) for (std::uint32_t x = 0; x < (Size >> level); ++x) {
            auto* texel = &Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
            texel[0] = static_cast<std::uint8_t>(10u + 40u * level);
            texel[1] = 77;
            texel[2] = 99;
            texel[3] = 255;
        }
    }
}

void Draw(AgcDriver::VulkanDevice& device, const Program& program, std::uint32_t wave, std::uint32_t level, bool emulate) {
    auto target = device.Target();
    if (emulate) target.fragmentShaderBarycentricEnabled = false;
    static const auto triangles = Triangles();
    Pixels.fill(std::byte{0});
    const auto vertexDescriptor = BufferDescriptor(triangles.data(), static_cast<std::uint32_t>(triangles.size()));
    const std::vector<std::uint32_t> vertexUserData(vertexDescriptor.begin(), vertexDescriptor.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {wave, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory}, target, {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = wave == 32;
    pixel.interpolatorCount = 1;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(18);
    const auto texture = TextureDescriptor();
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
    const std::array<std::uint32_t, 4> sampler{0u, 0xfffu << 12, 1u << 26, 0u};
    std::copy(sampler.begin(), sampler.end(), pixelUserData.begin() + 12);
    pixelUserData[16] = std::bit_cast<std::uint32_t>(std::exp2(static_cast<float>(level) - 0.25f) * Width / (2.0f * Size));
    pixelUserData[17] = std::bit_cast<std::uint32_t>(std::exp2(static_cast<float>(level) - 0.25f) * Height / (2.0f * Size));
    const auto code = wave == 64u ? program.wave64 : program.wave32;
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {wave, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory}, target, PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    std::vector<AgcDriver::Graphics::CompiledShader> shaders{{ShaderStage::Vertex, &vertexResult, 0}};
    ShaderRecompiler::RecompileResult geometry;
    if (pixelResult.barycentricEmulation.active) {
        geometry = ShaderRecompiler::BuildBarycentricGeometryShader(vertexResult, pixelResult, target, device.GeometryLimits());
        shaders.push_back({ShaderStage::Geometry, &geometry, 0});
    }
    shaders.push_back({ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)});
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, wave, wave, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    device.Draw(state, {0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false}, shaders);
    device.WaitIdle();
    const auto covered = [](std::uint32_t x, std::uint32_t y) { return Pixels[(y * Width + x) * 4 + 3] != std::byte{0}; };
    std::uint32_t inside = 0;
    std::uint32_t edge = 0;
    std::uint32_t bad = 0;
    std::uint32_t badEdge = 0;
    for (std::uint32_t y = 0; y < Height; ++y) for (std::uint32_t x = 0; x < Width; ++x) {
        if (!covered(x, y)) continue;
        const auto qx = x & ~1u;
        const auto qy = y & ~1u;
        const bool partial = !covered(qx, qy) || !covered(qx + 1u, qy) || !covered(qx, qy + 1u) || !covered(qx + 1u, qy + 1u);
        const bool wrong = std::to_integer<unsigned>(Pixels[(y * Width + x) * 4]) != 10u + 40u * level;
        ++(partial ? edge : inside);
        bad += wrong;
        badEdge += wrong && partial;
    }
    const auto what = std::string(program.name) + ", emulate" + std::to_string(emulate) + " wave" + std::to_string(wave) + " mip" + std::to_string(level);
    std::printf("%s: inside=%u edge=%u incorrect=%u incorrect_edge=%u\n", what.c_str(), inside, edge, bad, badEdge);
    Require(inside > 1000 && edge > 100, what + ": the triangles covered too few pixels");
    Require(bad == 0, what + ": implicit derivatives selected the wrong mip at " + std::to_string(bad) + " pixels, " + std::to_string(badEdge) + " of them in partly covered quads");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->GeometryLimits()) {
            std::puts("skipped, the device lacks geometryShader");
            return VulkanTestSkipped;
        }
        FillTexture();
        for (const auto& program : Programs) for (const auto emulate : {false, true}) for (const auto wave : {64u, 32u}) for (const auto level : {0u, 1u, 2u}) Draw(*device, program, wave, level, emulate);
        std::puts("pixel helper lane mask tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
