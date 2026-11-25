#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Lanes = Width * Height;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t LaneBytes = 64;
constexpr std::uint32_t Fill = 0xaaaaaaaau;

struct Store {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t channels;
    std::uint32_t components;
    std::uint32_t offset;
};

constexpr std::array<Store, 8> Stores{{
    {5, 1, 1, 1, 1},
    {11, 2, 1, 1, 3},
    {60, 1, 4, 4, 6},
    {60, 1, 4, 3, 14},
    {11, 2, 1, 1, 19},
    {27, 2, 2, 2, 26},
    {69, 2, 4, 4, 33},
    {18, 1, 2, 2, 45},
}};

alignas(256) std::array<std::uint32_t, Lanes * Inputs> Input{};
alignas(256) std::array<std::uint8_t, Lanes * LaneBytes> Output{};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

std::vector<std::uint32_t> PixelProgram() {
    std::vector<std::uint32_t> code{0x7e3c0f00, 0x7e3e0f01, 0x34043e86, 0x3800051e, 0x34020084, 0x34060086};
    for (std::uint32_t component = 0; component < Inputs; ++component) code.insert(code.end(), {0xe0301000u | (component * 4u), 0x80000401u | (component << 8u)});
    code.push_back(0xbf8c3f70);
    code.insert(code.end(), {0x7e3c02ff, Fill});
    for (std::uint32_t offset = 0; offset < LaneBytes; offset += 4) code.insert(code.end(), {0xe0701000u | offset, 0x80011e03});
    code.push_back(0xbf8c3f70);
    for (const auto& store : Stores) {
        const std::uint32_t op = 3u + store.components;
        code.insert(code.end(), {0xe8001000u | (store.format << 19u) | (op << 16u) | store.offset, 0x80010403});
    }
    code.insert(code.end(), {0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000});
    return code;
}

std::uint32_t Value(std::uint32_t lane, std::uint32_t component) {
    const auto mixed = (lane + 1u) * 2654435761u + component * 40503u;
    return lane % 5u == 3u ? mixed | 0x10000u : mixed >> 3u & 0xffffu;
}

std::array<std::uint8_t, LaneBytes> Expected(std::uint32_t lane) {
    std::array<std::uint8_t, LaneBytes> bytes{};
    bytes.fill(static_cast<std::uint8_t>(Fill & 0xffu));
    for (const auto& store : Stores) {
        const std::uint32_t limit = store.width == 1u ? 0xffu : 0xffffu;
        for (std::uint32_t component = 0; component < store.channels; ++component) {
            const auto value = component < store.components ? std::min(Value(lane, component), limit) : 0u;
            for (std::uint32_t byte = 0; byte < store.width; ++byte) bytes[store.offset + component * store.width + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
        }
    }
    return bytes;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count, std::uint32_t word3) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, word3};
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    static const std::vector<std::array<float, 4>> triangles{
        {-1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
        {1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
    };
    const auto& target = device.Target();
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        for (std::uint32_t component = 0; component < Inputs; ++component) Input[lane * Inputs + component] = Value(lane, component);
    }
    Output.fill(0xdeu);
    Pixels.fill(std::byte{0});

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()), 0x01016facu);
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
    const auto input = BufferDescriptor(Input.data(), 0u, static_cast<std::uint32_t>(Input.size() * 4u), 0x31016facu);
    const auto output = BufferDescriptor(Output.data(), 0u, static_cast<std::uint32_t>(Output.size()), 0x31016facu);
    std::copy(input.begin(), input.end(), pixelUserData.begin());
    std::copy(output.begin(), output.end(), pixelUserData.begin() + 4);
    static const auto pixelCode = PixelProgram();
    const std::span<const std::uint32_t> pixelWords(pixelCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelWords.data()), std::as_bytes(pixelWords)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelWords.data()), pixelWords, 0, {}},
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
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto expected = Expected(lane);
        for (std::uint32_t byte = 0; byte < LaneBytes; ++byte) {
            const auto actual = Output[lane * LaneBytes + byte];
            Require(actual == expected[byte], "pixel formatted store unaligned: wave" + std::to_string(waveSize) + " pixel (" + std::to_string(lane % Width) + ", " + std::to_string(lane / Width) + ") byte " + std::to_string(byte) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[byte]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> waves{64u};
        if (device->Target().subgroupSize >= 32u) waves.push_back(32u);
        for (const auto waveSize : waves) {
            Draw(*device, waveSize);
            Check(waveSize);
        }
        std::puts("pixel formatted store unaligned tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
