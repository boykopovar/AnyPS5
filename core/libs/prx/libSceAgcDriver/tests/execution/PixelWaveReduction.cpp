#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height> Results{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

struct Reduction {
    const char* name;
    std::uint32_t identity;
    std::uint32_t vector;
    std::uint32_t scalar;
    std::function<std::uint32_t(std::uint32_t, std::uint32_t)> combine;
};

std::uint32_t Key(std::uint32_t pixel) {
    return (pixel * 0x9e3779b1u) >> 22u;
}

std::vector<std::uint32_t> PixelCode(const Reduction& reduction) {
    const std::uint32_t dpp = reduction.vector | 0x001a1afau;
    return {
        0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, 0x000000c0, 0x7e120506, 0xd569000b, 0x0001ff06,
        0x9e3779b1, 0x2c161696, 0xbe98047e, 0x879a187e, 0xbeea287e, 0xd501000d, reduction.identity == 0u ? 0x006a1680u : 0x006a16c1u,
        dpp, 0xff01110d, dpp, 0xff01120d, dpp, 0xff01140d, dpp, 0xff01180d, 0xd778100c, 0x0305830d,
        reduction.vector | 0x001a190du, 0xbefe046a, 0xd760000a, 0x00013f0d, 0xd760000b, 0x00017f0d, reduction.scalar | 0x000c0b0au,
        0x8f099009, 0x880c090c, 0x7e0a020c, 0xe0702000, 0x80010506, 0x7e0e02f2, 0xf800180f, 0x07070707,
        0xbf810000,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::vector<std::array<float, 4>> Triangles() {
    std::vector<std::array<float, 4>> vertices;
    constexpr std::uint32_t columns = 6;
    constexpr std::uint32_t rows = 4;
    for (std::uint32_t cell = 0; cell < columns * rows; ++cell) {
        const float width = 2.0f / columns;
        const float height = 2.0f / rows;
        const float x = -1.0f + width * static_cast<float>(cell % columns);
        const float y = -1.0f + height * static_cast<float>(cell / columns);
        const float skew = 0.07f * static_cast<float>(cell % 5u);
        vertices.push_back({x + (0.05f + skew) * width, y + 0.05f * height, 0.5f, 1.0f});
        vertices.push_back({x + 0.93f * width, y + (0.11f + skew) * height, 0.5f, 1.0f});
        vertices.push_back({x + (0.37f - skew * 0.5f) * width, y + 0.91f * height, 0.5f, 1.0f});
    }
    return vertices;
}

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, const ShaderRecompiler::SpirvTarget& target) {
    static const auto triangles = Triangles();
    Pixels.fill(std::byte{0});
    Results.fill(Sentinel);

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto results = BufferDescriptor(Results.data(), 4u, static_cast<std::uint32_t>(Results.size()));
    std::copy(results.begin(), results.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
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

std::uint32_t Check(const Reduction& reduction, std::uint32_t subgroupSize) {
    const auto what = std::string("pixel wave reduction ") + reduction.name;
    std::map<std::uint32_t, std::vector<std::uint32_t>> waves;
    for (std::uint32_t pixel = 0; pixel < Results.size(); ++pixel) {
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) != 255u) {
            Require(Results[pixel] == Sentinel, what + ": uncovered pixel " + std::to_string(pixel) + " stored " + std::to_string(Results[pixel]));
            continue;
        }
        Require(Results[pixel] != Sentinel, what + ": covered pixel " + std::to_string(pixel) + " stored nothing");
        waves[Results[pixel] >> 16u].push_back(pixel);
    }
    std::uint32_t partial = 0;
    std::size_t covered = 0;
    for (const auto& [first, pixels] : waves) {
        Require(std::find(pixels.begin(), pixels.end(), first) != pixels.end(), what + ": the first lane of the wave of pixel " + std::to_string(pixels.front()) + ", pixel " + std::to_string(first) + ", is not a covered pixel of that wave");
        std::uint32_t expected = reduction.identity;
        for (const auto pixel : pixels) expected = reduction.combine(expected, Key(pixel));
        for (const auto pixel : pixels) {
            Require((Results[pixel] & 0xffffu) == expected, what + ": pixel " + std::to_string(pixel) + " of a wave of " + std::to_string(pixels.size()) + " covered pixels read " + std::to_string(Results[pixel] & 0xffffu) + ", the wave holds " + std::to_string(expected));
        }
        if (pixels.size() < subgroupSize) ++partial;
        covered += pixels.size();
    }
    Require(covered > 1000, what + ": the triangles covered only " + std::to_string(covered) + " pixels");
    Require(partial != 0u, what + ": no wave had fewer covered pixels than the host subgroup");
    return partial;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!TargetHasCapability(target, spv::CapabilityGroupNonUniformArithmetic)) {
            std::puts("skipped, the device has no subgroup arithmetic");
            return VulkanTestSkipped;
        }
        if (target.subgroupSize > 64u) {
            std::printf("skipped, the %u-lane subgroup is wider than the wave, so the half-wave scan throws\n", target.subgroupSize);
            return VulkanTestSkipped;
        }
        std::vector<ShaderRecompiler::SpirvTarget> layouts;
        if (target.subgroupSize >= 32u) layouts.push_back(target);
        else std::printf("the device's own layout is skipped, v_permlanex16_b32 reads lanes 16-31, outside the %u-lane subgroup\n", target.subgroupSize);
        if (target.subgroupSize < 64u) {
            auto wide = target;
            wide.subgroupSize = 64u;
            layouts.push_back(wide);
        }
        const std::array<Reduction, 5> reductions{{
            {"UMin", 0xffffffffu, 0x26000000u, 0x83800000u, [](std::uint32_t left, std::uint32_t right) { return std::min(left, right); }},
            {"UMax", 0u, 0x28000000u, 0x84800000u, [](std::uint32_t left, std::uint32_t right) { return std::max(left, right); }},
            {"IAdd", 0u, 0x4a000000u, 0x80000000u, [](std::uint32_t left, std::uint32_t right) { return left + right; }},
            {"AND", 0xffffffffu, 0x36000000u, 0x87000000u, [](std::uint32_t left, std::uint32_t right) { return left & right; }},
            {"OR", 0u, 0x38000000u, 0x88000000u, [](std::uint32_t left, std::uint32_t right) { return left | right; }},
        }};
        for (const auto& layout : layouts) {
            for (const auto& reduction : reductions) {
                const auto code = PixelCode(reduction);
                std::uint32_t partial = 0;
                for (std::uint32_t pass = 0; pass < 4; ++pass) {
                    Draw(*device, code, layout);
                    partial += Check(reduction, target.subgroupSize);
                }
                std::printf("%s, laid out for %u lanes on %u-lane subgroups: %u partial waves over 4 draws\n", reduction.name, layout.subgroupSize, target.subgroupSize, partial);
            }
        }
        std::puts("pixel wave reduction tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
