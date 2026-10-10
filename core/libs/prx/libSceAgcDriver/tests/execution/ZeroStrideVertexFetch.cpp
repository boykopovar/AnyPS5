#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Vertices = 3;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Vertices * 4> Output{};

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<float, 4> Element{0.25f, -2.0f, 7.5f, 1.0f};

struct FetchCase {
    std::uint32_t select;
    std::uint32_t records;
    bool inRange;
    const char* what;
};

std::array<std::uint32_t, 4> VertexBufferDescriptor(const FetchCase& value) {
    const auto address = reinterpret_cast<std::uintptr_t>(Element.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), value.records, 0x0104dfacu | (value.select << 28u)};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

void Draw(AgcDriver::VulkanDevice& device, const FetchCase& value) {
    Pixels.fill(std::byte{0x40});
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = VertexBufferDescriptor(value);
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertexRequest);
    Testing::Require(vertexResult.vertexAttributes.size() == 1u, std::string(value.what) + ": the fetch did not become a vertex attribute");
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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

void Check(const FetchCase& value) {
    for (std::uint32_t vertex = 0; vertex < Vertices; ++vertex) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto expected = value.inRange ? std::bit_cast<std::uint32_t>(Element[component]) : 0u;
            const auto actual = Output[vertex * 4u + component];
            Testing::Require(actual == expected, std::string(value.what) + ": vertex " + std::to_string(vertex) + " v" + std::to_string(8u + component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void DrawAndCheck(const FetchCase& value) {
    auto& device = SharedVulkanTestDevice();
    Draw(device, value);
    Check(value);
}

const Testing::Case oneRecordSelect2{"ZeroStrideVertexFetch_OobSelect2OneRecord_FetchesElement", [] {
    DrawAndCheck({2u, 1u, true, "OOB_SELECT 2, one record"});
}};

const Testing::Case noRecordsSelect2{"ZeroStrideVertexFetch_OobSelect2NoRecords_FetchesZero", [] {
    DrawAndCheck({2u, 0u, false, "OOB_SELECT 2, no records"});
}};

const Testing::Case wholeElementSelect3{"ZeroStrideVertexFetch_OobSelect3WholeElement_FetchesElement", [] {
    DrawAndCheck({3u, 16u, true, "OOB_SELECT 3, whole element in range"});
}};

const Testing::Case noRecordsSelect3{"ZeroStrideVertexFetch_OobSelect3NoRecords_FetchesZero", [] {
    DrawAndCheck({3u, 0u, false, "OOB_SELECT 3, no records"});
}};

const Testing::Case wholeElementSelect0{"ZeroStrideVertexFetch_OobSelect0WholeElement_FetchesElement", [] {
    DrawAndCheck({0u, 16u, true, "OOB_SELECT 0, whole element in range"});
}};

} // namespace
