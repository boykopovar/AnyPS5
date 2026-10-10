#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Vertices = 189;
constexpr std::uint32_t Threads = 192;
constexpr std::uint32_t Keys = 6;
constexpr std::uint32_t KeyStride = 440;
constexpr std::uint32_t PassLimit = 128;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
alignas(256) std::array<std::uint32_t, Threads> LaneKeys{};
alignas(256) std::array<std::uint32_t, Keys * KeyStride / 4> Table{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 68> VertexCode{
    0x8700ff03, 0x000000ff, 0x81ea00c0, 0x90fe6ac1, 0xe0302000, 0x80020c05, 0x7e3c0305, 0x36080a81,
    0x7e080d04, 0x7e2a02c1, 0xbea4040c, 0xbea6040e, 0xbea80380, 0xbf8c3f70, 0xbe96047e, 0xbe88047e,
    0xbeb0047e, 0x7c0808f9, 0x068698f2, 0xbf880023, 0x80288128, 0xbf0aff28, 0x00000080, 0xbf84001f,
    0xbeea1416, 0xd7600020, 0x0000d50c, 0x7d8418f9, 0x06869a20, 0xd5010004, 0x00690283, 0x7da40881,
    0xbf88000b, 0x7e0a0280, 0x87ea7e18, 0x8afe6a7e, 0x8ab06a30, 0xbf880006, 0x7e080283, 0xb82001b8,
    0xf4200792, 0x40000000, 0xbf8cc07f, 0x7e2a021e, 0xbefe0430, 0x7da40883, 0xbf880008, 0x7e0a0281,
    0x8a961a16, 0xbf128016, 0xbf800000, 0x85ea807e, 0x8ab06a30, 0x8afe6a7e, 0xbf82ffdc, 0xbefe0408,
    0x7e2c0228, 0x7e2e030c, 0x7e300305, 0xe0782000, 0x8004151e, 0x7e000280, 0x7e020280, 0x7e040280,
    0x7e0602f2, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 59> ComputeCode{
    0xbefe04c1, 0xe0302000, 0x80020c00, 0x7e3c0300, 0x36080081, 0x7e080d04, 0x7e2a02c1, 0xbea4040c,
    0xbea6040e, 0xbea80380, 0xbf8c3f70, 0xbe96047e, 0xbe88047e, 0xbeb0047e, 0x7c0808f9, 0x068698f2,
    0xbf880023, 0x80288128, 0xbf0aff28, 0x00000080, 0xbf84001f, 0xbeea1416, 0xd7600020, 0x0000d50c,
    0x7d8418f9, 0x06869a20, 0xd5010004, 0x00690283, 0x7da40881, 0xbf88000b, 0x7e0a0280, 0x87ea7e18,
    0x8afe6a7e, 0x8ab06a30, 0xbf880006, 0x7e080283, 0xb82001b8, 0xf4200792, 0x40000000, 0xbf8cc07f,
    0x7e2a021e, 0xbefe0430, 0x7da40883, 0xbf880008, 0x7e0a0281, 0x8a961a16, 0xbf128016, 0xbf800000,
    0x85ea807e, 0x8ab06a30, 0x8afe6a7e, 0xbf82ffdc, 0xbefe0408, 0x7e2c0228, 0x7e2e030c, 0x7e300305,
    0xe0782000, 0x8004151e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::uint32_t Entry(std::uint32_t key) {
    return 0x5a000000u + key * 0x00010101u;
}

std::vector<std::uint32_t> Prepare(std::uint32_t lanes) {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) LaneKeys[lane] = (lane * 5u + lane / 7u) % Keys;
    Table.fill(0u);
    for (std::uint32_t key = 0; key < Keys; ++key) Table[key * KeyStride / 4u] = Entry(key);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    const auto keys = BufferDescriptor(LaneKeys.data(), 4u, lanes);
    const auto table = BufferDescriptor(Table.data(), 0u, static_cast<std::uint32_t>(Table.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), 16u, lanes);
    std::copy(keys.begin(), keys.end(), userData.begin());
    std::copy(table.begin(), table.end(), userData.begin() + 4);
    std::copy(output.begin(), output.end(), userData.begin() + 8);
    return userData;
}

void Draw(AgcDriver::VulkanDevice& device) {
    const auto vertexUserData = Prepare(Vertices);
    Pixels.fill(std::byte{0});
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 8, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX);
    pixel.posX = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        device.Target(),
        {0, 0, vertexPush, 128 - vertexPush}
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, vertexPush}
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
    const AgcDriver::Pm4::DrawParameters draw{0, Vertices, 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Dispatch(AgcDriver::VulkanDevice& device) {
    const auto userData = Prepare(Threads);
    const std::span<const std::uint32_t> code(ComputeCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 8, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const std::string& name, std::uint32_t lanes) {
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        const auto what = "exec host lanes: " + name + " " + std::to_string(lane);
        const auto key = LaneKeys[lane];
        const bool inside = (lane & 1u) == 0u;
        const auto* result = &Output[lane * 4u];
        Require(result[2] == key, what + " read key " + std::to_string(result[2]) + ", expected " + std::to_string(key));
        Require(result[1] >= 1u && result[1] <= Keys, what + " ran " + std::to_string(result[1]) + " passes of the waterfall loop over EXEC copied by s_mov_b64, expected 1 to " + std::to_string(Keys) + " (the loop stops at " + std::to_string(PassLimit) + ")");
        Require(result[3] == (inside ? 0u : 1u), what + " left the loop with v5 " + std::to_string(result[3]) + ", expected " + std::to_string(inside ? 0u : 1u));
        const auto expected = inside ? 0xffffffffu : Entry(key);
        Require(result[0] == expected, what + " loaded " + std::to_string(result[0]) + " through its scalarized key, expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Dispatch(*device);
        Check("compute lane", Threads);
        if (device->Target().subgroupSize < 32u) {
            std::printf("vertex draw skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
        } else if ((device->SubgroupStages() & VK_SHADER_STAGE_VERTEX_BIT) == 0u) {
            std::puts("vertex draw skipped, the device has no subgroup operations in vertex shaders");
        } else {
            Draw(*device);
            Check("vertex", Vertices);
        }
        std::puts("exec host lane tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
