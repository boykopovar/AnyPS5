#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Factor = 0x00a5c3e1u;
constexpr std::uint32_t Counter = 0x1234u;
constexpr std::uint32_t Slots = 64;
constexpr std::uint32_t CounterOffset = Slots * 16u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height * Results> Outputs{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

std::vector<std::uint32_t> PixelCode() {
    std::vector<std::uint32_t> code{0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, Width};
    const auto vop2 = [&](std::uint32_t opcode, std::uint32_t destination, std::uint32_t source, std::uint32_t vector) {
        code.push_back(opcode | (destination << 17u) | (vector << 9u) | source);
    };
    const auto ds = [&](std::uint32_t opcode, std::uint32_t offset, std::uint32_t address, std::uint32_t data0, std::uint32_t data1, std::uint32_t destination) {
        code.push_back(0xd8000000u | (opcode << 18u) | offset);
        code.push_back(address | (data0 << 8u) | (data1 << 16u) | (destination << 24u));
    };
    vop2(0x36000000, 8, 0xff, 6);
    code.push_back(Slots - 1u);
    vop2(0x34000000, 8, 0x84, 8);
    code.push_back(0x7e126f06);
    vop2(0x16000000, 10, 0xff, 6);
    code.push_back(Factor);
    code.insert(code.end(), {0x7e1c02c1, 0x7e2002ff, 0xffff0000, 0x7e240280, 0x7e2602ff, Counter, 0xbefc03ff, 0x0000ffff});
    for (std::uint32_t offset = 0; offset < 16; offset += 4) ds(0x0d, offset, 8, 9, 0, 0);
    ds(0x1e, 0, 8, 10, 0, 0);
    ds(0x1e, 2, 8, 6, 0, 0);
    ds(0xa1, 6, 8, 10, 0, 0);
    ds(0x1f, 8, 8, 6, 0, 0);
    ds(0xa0, 11, 8, 10, 0, 0);
    ds(0x20, 12, 8, 10, 0, 12);
    ds(0x23, 12, 8, 14, 0, 13);
    ds(0x2c, 12, 8, 16, 6, 15);
    ds(0x0d, CounterOffset, 18, 19, 0, 0);
    ds(0x3e, CounterOffset, 0, 0, 0, 17);
    code.push_back(0xbf8cc07f);
    for (std::uint32_t dword = 0; dword < 4; ++dword) ds(0x36, dword * 4u, 8, 0, 0, 20 + dword);
    code.push_back(0xbf8cc07f);
    const std::array<std::uint32_t, Results> stored{20, 21, 22, 23, 12, 13, 15, 17};
    for (std::uint32_t slot = 0; slot < Results; ++slot) {
        code.push_back(0xe0702000 | (slot * 4u));
        code.push_back(0x80010006 | (stored[slot] << 8u));
    }
    code.insert(code.end(), {0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000});
    return code;
}

std::array<std::uint32_t, Results> Expected(std::uint32_t pixel) {
    const auto initial = ~pixel;
    const auto product = static_cast<std::uint32_t>(std::uint64_t{Factor} * pixel);
    const auto added = initial + product;
    const auto incremented = added == 0xffffffffu ? 0u : added + 1u;
    const auto masked = (incremented & 0x0000ffffu) | pixel;
    return {
        (initial & 0xff00ff00u) | (product & 0xffu) | ((pixel & 0xffu) << 16u),
        (initial & 0x0000ffffu) | (product & 0xffff0000u),
        (initial & 0x00ff0000u) | (pixel & 0xffffu) | (((product >> 16u) & 0xffu) << 24u),
        masked,
        initial,
        added,
        incremented,
        Counter,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

constexpr std::array<std::array<float, 4>, 3> Triangles{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    const auto& triangles = Triangles;
    const auto pixelCode = PixelCode();
    Pixels.fill(std::byte{0});
    Outputs.fill(Sentinel);
    const auto target = device.Target();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto results = BufferDescriptor(Outputs.data(), Results * 4u, Width * Height);
    std::copy(results.begin(), results.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(std::span(pixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
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
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(std::uint32_t waveSize) {
    static constexpr std::array<const char*, Results> names{"ds_write_b8 dword", "ds_write_b16_d16_hi dword", "ds_write_b16 and ds_write_b8_d16_hi dword", "atomic dword", "ds_add_rtn_u32", "ds_inc_rtn_u32", "ds_mskor_rtn_b32", "ds_append"};
    const auto what = "pixel LDS read-modify-write wave" + std::to_string(waveSize);
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            Require(Pixels[pixel * 4u + channel] == std::byte{255}, what + ": missing color at pixel " + std::to_string(pixel));
        }
        const auto expected = Expected(pixel);
        for (std::uint32_t slot = 0; slot < Results; ++slot) {
            const auto actual = Outputs[pixel * Results + slot];
            Require(actual == expected[slot], what + ": pixel " + std::to_string(pixel) + " " + names[slot] + " expected " + std::to_string(expected[slot]) + " got " + std::to_string(actual));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto waveSize : {64u, 32u}) {
            std::fprintf(stderr, "pixel LDS read-modify-write wave%u\n", waveSize);
            for (std::uint32_t pass = 0; pass < 2; ++pass) {
                Draw(*device, waveSize);
                Check(waveSize);
            }
        }
        std::puts("pixel LDS read-modify-write tests passed: 4 draws, 2048 pixels, 16384 results");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
