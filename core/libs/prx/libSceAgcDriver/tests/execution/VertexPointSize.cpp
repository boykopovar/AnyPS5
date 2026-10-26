#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Extent = 64u;
constexpr std::uint32_t MiscVector = 1u << 21u;
constexpr std::uint32_t VertexPointSize = 1u << 16u;
alignas(256) std::array<std::byte, Extent * Extent * 4u> pixels{};
alignas(256) std::array<std::byte, 512> vertices{};
constexpr std::array<std::uint32_t, 11> positionCode{0xf4080100u, 0xfa000000u, 0x4a0a0a02u, 0x4a0a0b08u, 0x4a0a0a03u, 0xe00c2000u, 0x80010005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> pixelCode{0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

std::vector<std::uint32_t> SizeExportCode(float size) {
    std::vector<std::uint32_t> code(positionCode.begin(), positionCode.begin() + 8);
    code.insert(code.end(), {0x7e0802ffu, std::bit_cast<std::uint32_t>(size), 0xf80000cfu, 0x03020100u, 0xf80008d1u, 0x00000004u, 0xbf810000u});
    return code;
}

struct PointSize {
    float size = 1.0f;
    float minimum = 0.0f;
    float maximum = 0.0f;
    bool fromVertex = false;
};

struct Square {
    unsigned pixels = 0;
    std::uint32_t left = Extent, top = Extent, right = 0, bottom = 0;
};

std::vector<Square> RunCase(AgcDriver::VulkanDevice& device, const char* name, std::span<const std::uint32_t> vertexCode, const PointSize& points, float offset) {
    const std::array<std::array<float, 2>, 4> centres{{{16.0f, 16.0f}, {48.0f, 16.0f}, {16.0f, 48.0f}, {48.0f, 48.0f}}};
    vertices.fill(std::byte{0});
    constexpr std::uint32_t stride = 16u;
    for (std::size_t index = 0; index < centres.size(); ++index) {
        const std::array<float, 4> position{(centres[index][0] + offset) / Extent * 2.0f - 1.0f, (centres[index][1] + offset) / Extent * 2.0f - 1.0f, 0.0f, 1.0f};
        std::memcpy(vertices.data() + index * stride, position.data(), sizeof(position));
    }
    std::array<std::uint32_t, 4> descriptor{};
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(descriptor.data());
    std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 0u, 0u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{tableAddress, std::as_bytes(std::span(descriptor))}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.fetchEmbedded = true;
    vertexInfo.resourcesNum = 1u;
    vertexInfo.resourcesDst[0] = {0, 4, 0, 0};
    vertexInfo.paClVsOutCntl = points.fromVertex ? MiscVector | VertexPointSize : 0u;
    vertexInfo.pointList = true;
    vertexInfo.pointSize = points.size;
    vertexInfo.pointSizeMin = points.minimum;
    vertexInfo.pointSizeMax = points.maximum;
    ShaderRecompiler::RecompileRequest request{{ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}}, {32u, 0u, userData, std::nullopt, std::nullopt, vertexInfo, memory}, device.Target(), {0u, 0u, 0u, 96u}};
    const auto address = reinterpret_cast<std::uintptr_t>(vertices.data());
    descriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), static_cast<std::uint32_t>(centres.size()), 0x01000facu | (77u << 12u)};
    request.context.vertex->resources[0].fields = descriptor;
    const auto vertex = ShaderRecompiler::Recompile(request);
    const auto pushBytes = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
    pixelInfo.wave32 = true;
    pixelInfo.targetOutputMode[0] = 9u;
    pixelInfo.targetExportMapping.fill(0xe4u);
    pixelInfo.rasterPrimitive = ShaderRecompiler::RasterPrimitive::Point;
    ShaderRecompiler::RecompileRequest pixelRequest{{ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}}, {32u, 0u, {}, std::nullopt, pixelInfo, std::nullopt, {}}, device.Target(), PixelPushLayout(pushBytes, device.Target())};
    const auto pixel = ShaderRecompiler::Recompile(pixelRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(pushBytes, device.Target())}}};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Extent, Extent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    state.paClVsOutCntl = vertexInfo.paClVsOutCntl;
    state.vertexPoints = true;
    state.pointSize = points.size;
    state.pointSizeMin = points.minimum;
    state.pointSizeMax = points.maximum;
    state.viewport = {0.0f, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0.0f, 1.0f};
    state.scissor = {{0, 0}, {Extent, Extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    AgcDriver::Pm4::DrawParameters draw{0u, static_cast<std::uint32_t>(centres.size()), 0u, 1u, 0u, false};
    draw.firstInstance = 0u;
    pixels.fill(std::byte{0});
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    std::vector<Square> squares(centres.size());
    for (std::uint32_t y = 0; y < Extent; ++y) {
        for (std::uint32_t x = 0; x < Extent; ++x) {
            const auto index = (y * Extent + x) * 4u;
            if (pixels[index] == std::byte{0} && pixels[index + 3u] == std::byte{0}) continue;
            auto& square = squares[(y >= Extent / 2u ? 2u : 0u) + (x >= Extent / 2u ? 1u : 0u)];
            ++square.pixels;
            square.left = std::min(square.left, x);
            square.top = std::min(square.top, y);
            square.right = std::max(square.right, x + 1u);
            square.bottom = std::max(square.bottom, y + 1u);
        }
    }
    std::cout << "case " << name << ":";
    for (const auto& square : squares) std::cout << " " << square.pixels << " [" << square.left << "," << square.top << ")-(" << square.right << "," << square.bottom << ")";
    std::cout << "\n";
    return squares;
}

void RequireSquares(const std::vector<Square>& squares, std::uint32_t side, const char* name) {
    for (const auto& square : squares) {
        Require(square.pixels == side * side && square.right - square.left == side && square.bottom - square.top == side, std::string(name) + ": a point is not a " + std::to_string(side) + "x" + std::to_string(side) + " square");
    }
}

void RequireAtMostOnePixel(const std::vector<Square>& squares, const char* name) {
    for (const auto& square : squares) Require(square.pixels <= 1u, std::string(name) + ": a point with no positive size covers more than one pixel");
}

void Run(AgcDriver::VulkanDevice& device) {
    std::cout << "device: " << device.DeviceName() << "\n";
    RequireSquares(RunCase(device, "register size 1", positionCode, {1.0f, 1.0f, 1.0f, false}, 0.5f), 1u, "register size 1");
    RequireSquares(RunCase(device, "register size 4", positionCode, {4.0f, 1.0f, 1.0f, false}, 0.0f), 4u, "register size 4");
    RequireSquares(RunCase(device, "register size 3 ignores the clamp", positionCode, {3.0f, 6.0f, 8.0f, false}, 0.5f), 3u, "register size 3 ignores the clamp");
    const auto two = SizeExportCode(2.0f);
    RequireSquares(RunCase(device, "vertex size 2 within [1, 8]", two, {1.0f, 1.0f, 8.0f, true}, 0.0f), 2u, "vertex size 2 within [1, 8]");
    const auto six = SizeExportCode(6.0f);
    RequireSquares(RunCase(device, "vertex size 6 clamped to 4", six, {1.0f, 0.5f, 4.0f, true}, 0.0f), 4u, "vertex size 6 clamped to 4");
    const auto one = SizeExportCode(1.0f);
    RequireSquares(RunCase(device, "vertex size 1 clamped to 5", one, {1.0f, 5.0f, 8.0f, true}, 0.5f), 5u, "vertex size 1 clamped to 5");
    RequireSquares(RunCase(device, "vertex size over a register size", six, {1.0f, 1.0f, 8.0f, true}, 0.0f), 6u, "vertex size over a register size");
    const auto zero = SizeExportCode(0.0f);
    RequireAtMostOnePixel(RunCase(device, "vertex size 0 within [0, 4]", zero, {0.0f, 0.0f, 4.0f, true}, 0.5f), "vertex size 0 within [0, 4]");
    const auto negative = SizeExportCode(-3.0f);
    RequireAtMostOnePixel(RunCase(device, "vertex size -3 within [0, 4]", negative, {1.0f, 0.0f, 4.0f, true}, 0.5f), "vertex size -3 within [0, 4]");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
