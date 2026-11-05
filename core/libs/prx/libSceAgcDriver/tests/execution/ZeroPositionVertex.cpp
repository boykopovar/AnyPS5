#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
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
alignas(256) std::array<std::byte, Extent * Extent * 4u> pixels{};
alignas(256) std::array<std::byte, 512> vertices{};
constexpr std::array<std::uint32_t, 11> vertexCode{0xf4080100u, 0xfa000000u, 0x4a0a0a02u, 0x4a0a0b08u, 0x4a0a0a03u, 0xe00c2000u, 0x80010005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> pixelCode{0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

using Position = std::array<float, 4>;

struct Counts {
    unsigned total = 0;
    unsigned upper = 0;
    std::array<std::array<unsigned, 16>, 16> map{};
};

Counts RunCase(AgcDriver::VulkanDevice& device, const char* name, std::uint32_t records, const std::vector<Position>& positions, std::size_t stored, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
    vertices.fill(std::byte{0});
    constexpr std::uint32_t stride = 16u;
    for (std::size_t index = 0; index < stored; ++index) std::memcpy(vertices.data() + index * stride, positions[index].data(), 16u);
    std::array<std::uint32_t, 4> descriptor{};
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(descriptor.data());
    std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 0u, 0u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{tableAddress, std::as_bytes(std::span(descriptor))}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.fetchEmbedded = true;
    vertexInfo.resourcesNum = 1u;
    vertexInfo.resourcesDst[0] = {0, 4, 0, 0};
    ShaderRecompiler::RecompileRequest request{{ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}}, {32u, 0u, userData, std::nullopt, std::nullopt, vertexInfo, memory}, device.Target(), {0u, 0u, 0u, 96u}};
    const auto address = reinterpret_cast<std::uintptr_t>(vertices.data());
    descriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), records, 0x01000facu | (77u << 12u)};
    request.context.vertex->resources[0].fields = descriptor;
    const auto vertex = ShaderRecompiler::Recompile(request);
    const auto pushBytes = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
    pixelInfo.wave32 = true;
    pixelInfo.targetOutputMode[0] = 9u;
    pixelInfo.targetExportMapping.fill(0xe4u);
    ShaderRecompiler::RecompileRequest pixelRequest{{ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}}, {32u, 0u, {}, std::nullopt, pixelInfo, std::nullopt, {}}, device.Target(), PixelPushLayout(pushBytes, device.Target())};
    const auto pixel = ShaderRecompiler::Recompile(pixelRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(pushBytes, device.Target())}}};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Extent, Extent};
    state.topology = topology;
    state.viewport = {0.0f, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0.0f, 1.0f};
    state.scissor = {{0, 0}, {Extent, Extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    AgcDriver::Pm4::DrawParameters draw{0u, static_cast<std::uint32_t>(positions.size()), 0u, 1u, 0u, false};
    draw.firstInstance = 0u;
    pixels.fill(std::byte{0});
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    Counts counts;
    for (std::uint32_t y = 0; y < Extent; ++y) {
        for (std::uint32_t x = 0; x < Extent; ++x) {
            const auto index = (y * Extent + x) * 4u;
            if (pixels[index] == std::byte{0} && pixels[index + 3u] == std::byte{0}) continue;
            ++counts.total;
            const float ndcX = (static_cast<float>(x) + 0.5f) / Extent * 2.0f - 1.0f;
            const float ndcY = 1.0f - (static_cast<float>(y) + 0.5f) / Extent * 2.0f;
            if (ndcX + ndcY > -1.0f) ++counts.upper;
            ++counts.map[y / 4u][x / 4u];
        }
    }
    std::cout << "case " << name << ": records=" << records << " total=" << counts.total << " outsideFirstTriangle(x+y>-1)=" << counts.upper << "\n";
    for (const auto& row : counts.map) {
        std::string line;
        for (const auto count : row) line += count == 0u ? '.' : count == 16u ? '#' : '+';
        std::cout << "  " << line << "\n";
    }
    return counts;
}

void Run(AgcDriver::VulkanDevice& device) {
    std::cout << "device: " << device.DeviceName() << "\n";
    const Position v0{-1.0f, -1.0f, 0.0f, 1.0f};
    const Position v1{0.0f, -1.0f, 0.0f, 1.0f};
    const Position v2{-1.0f, 0.0f, 0.0f, 1.0f};
    const auto make = [&](const Position& last) { return std::vector<Position>{v0, v1, v2, last}; };
    const auto a = RunCase(device, "A out-of-range vertex 3", 3u, make({0.0f, 0.0f, 0.0f, 0.0f}), 3u);
    const auto b = RunCase(device, "B in-range (0,0,0,0)", 4u, make({0.0f, 0.0f, 0.0f, 0.0f}), 4u);
    const auto c = RunCase(device, "C repeat of v1", 4u, make(v1), 4u);
    const auto d = RunCase(device, "D (0,0,0,1) at the centre", 4u, make({0.0f, 0.0f, 0.0f, 1.0f}), 4u);
    const auto e = RunCase(device, "E (1,1,0,1) real triangle", 4u, make({1.0f, 1.0f, 0.0f, 1.0f}), 4u);
    const Position zero{0.0f, 0.0f, 0.0f, 0.0f};
    const auto f = RunCase(device, "F negative zeros", 4u, make({-0.0f, 0.0f, -0.0f, -0.0f}), 4u);
    const auto g = RunCase(device, "G triangle list, second triangle has (0,0,0,0)", 6u, {v0, v1, v2, v1, v2, zero}, 6u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const auto h = RunCase(device, "H strip with two (0,0,0,0) vertices", 5u, {v0, v1, v2, zero, zero}, 5u);
    const auto p = RunCase(device, "P point list, (-0.5,-0.5,0,1) and (0,0,0,0)", 2u, {{-0.5f, -0.5f, 0.0f, 1.0f}, zero}, 2u, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    Require(e.upper > 0u, "probe cannot see the second triangle");
    Require(c.total > 0u && c.upper == 0u, "the first triangle is not drawn alone");
    Require(a.upper == 0u && a.total == c.total, "a vertex fetched past num_records must clip the primitive away");
    Require(b.upper == 0u && b.total == c.total, "a stored (0,0,0,0) vertex must clip the primitive away");
    Require(f.upper == 0u && f.total == c.total, "a stored (-0,0,-0,-0) vertex must clip the primitive away");
    Require(g.upper == 0u && g.total == c.total, "a triangle list with a (0,0,0,0) vertex must keep only the first triangle");
    Require(h.upper == 0u && h.total == c.total, "a strip with two (0,0,0,0) vertices must keep only the first triangle");
    Require(d.upper == 496u, "a (0,0,0,1) vertex must not be clipped away");
    Require(p.total == 1u, "a point at (0,0,0,0) must not be drawn");
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
