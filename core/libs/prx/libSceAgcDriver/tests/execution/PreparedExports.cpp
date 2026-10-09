#include <spirv/unified1/spirv.hpp>
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {

using namespace ShaderRecompiler;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Extent = 16;
alignas(256) std::array<std::uint32_t, Extent * Extent * 4> pixels{};
alignas(256) std::array<std::uint32_t, Extent * Extent * 4> otherPixels{};
constexpr std::array<std::array<float, 4>, 3> positions{{{-1, -1, 0, 1}, {3, -1, 0, 1}, {-1, 3, 0, 1}}};
constexpr std::array<std::uint32_t, 6> vertexCode{0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};

void Run(AgcDriver::VulkanDevice& device, bool integer) {
    const auto address = reinterpret_cast<std::uintptr_t>(positions.data());
    const std::array<std::uint32_t, 4> users{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (16u << 16u), 3, 0x01016facu};
    RecompileRequest vertexRequest{{ShaderStage::Vertex, 0x10000, vertexCode, 0, {}}, {32, 0, users, {}, {}, ShaderVertexStageInfo{}, {}}, device.Target(), {0, 0, 0, 128}};
    const auto vertex = Recompile(vertexRequest);
    const std::array<std::uint32_t, 4> values = integer ? std::array<std::uint32_t, 4>{11, 22, 33, 44} : std::array<std::uint32_t, 4>{0, 0x3e800000u, 0x3f000000u, 0x3f800000u};
    const std::array<std::uint32_t, 13> code{0x7e0002ffu, values[0], 0x7e0202ffu, values[1], 0x7e0402ffu, values[2], 0x7e0602ffu, values[3], 0xf800100fu, 0x03020100u, 0xf800183fu, 0x03020100u, 0xbf810000u};
    AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, integer ? 0x7007u : 0x9009u}};
    RecompileRequest request{{ShaderStage::Fragment, 0x20000, code, 0, {}}, {32, 0, {}, {}, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), {}, {}}, device.Target(), {0, 0, 0, 128}};
    const auto handle = PrepareShader(request);
    const auto& artifact = GetPreparedArtifact(*handle);
    std::uint32_t scalarType = 0;
    unsigned extracts = 0;
    const auto& words = artifact.spirv.Words();
    for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> 16u) {
        const auto opcode = words[offset] & 0xffffu;
        if (integer && opcode == spv::OpTypeInt && words[offset + 2] == 32 && words[offset + 3] == 0) scalarType = words[offset + 1];
        if (!integer && opcode == spv::OpTypeFloat && words[offset + 2] == 32) scalarType = words[offset + 1];
        if (opcode == spv::OpVectorExtractDynamic) {
            Require(scalarType != 0 && words[offset + 1] == scalarType, "runtime export mapping uses the wrong scalar type");
            ++extracts;
        }
    }
    Require(extracts == 8, "fragment did not map both MRT outputs at runtime");
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{0x20000, 0, 1, {code.begin(), code.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    request.shader.code = snapshot.code;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0, 32, 32, {}, {}};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, integer ? VK_FORMAT_R32G32B32A32_UINT : VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(pixels), 0};
    state.color.elementBytes = 16;
    auto other = state.color;
    other.address = reinterpret_cast<std::uintptr_t>(otherPixels.data());
    other.slot = 3;
    other.exportIndex = 3;
    state.colors = {state.color, other};
    state.hasColorTarget = true;
    state.renderExtent = {Extent, Extent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0, 1};
    state.scissor = {{0, 0}, {Extent, Extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends.resize(4);
    state.blends[0] = state.blends[3] = state.blend;
    const AgcDriver::Pm4::DrawParameters draw{0, 3, 0, 1, 0, false};
    std::shared_ptr<const RecompileResult> first;
    for (const auto mapping : {0xe4u, 0xe5u, 0xe6u, 0xe7u, 0x1bu, 0u, 0xe4u}) {
        state.colors[0].componentMapping = mapping;
        state.colors[1].componentMapping = mapping ^ 0xffu;
        request.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        SrtRuntime runtime{};
        const auto capture = invocation.Capture(runtime);
        const auto pixel = invocation.Materialize(*capture);
        Require(pixel->variantId == artifact.variantId && pixel->specialization.empty(), "export mapping did not materialize a prepared fragment module");
        if (!first) first = pixel;
        else if (mapping == 0xe4u) Require(pixel == first, "export mapping did not reuse the materialized result");
        else Require(pixel != first, "export mapping reused stale materialized data");
        for (const bool cached : {true, false}) {
            request.useCache = cached;
            const auto materialized = MaterializeShader(request, *capture, *handle);
            Require(materialized->spirv.data() == pixel->spirv.data() && materialized->PipelineVariantId() == pixel->PipelineVariantId(), "export mapping did not reuse its specialized module");
            for (std::size_t cursor = 5; cursor < materialized->spirv.size();) {
                const auto count = materialized->spirv[cursor] >> 16u;
                Require(count != 0u && count <= materialized->spirv.size() - cursor, "invalid specialized fragment instruction");
                Require((materialized->spirv[cursor] & 0xffffu) != static_cast<std::uint32_t>(spv::OpVectorExtractDynamic), "fragment export retained a dynamic channel selection");
                cursor += count;
            }
        }
        request.useCache = true;
        if (integer) continue;
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, pixel.get(), 0}}};
        pixels.fill(0xdeadbeef);
        otherPixels.fill(0xdeadbeef);
        device.Draw(state, draw, shaders);
        device.WaitIdle();
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const auto shift = (i % 4) * 2;
            Require(pixels[i] == values[(mapping >> shift) & 3u], "fragment exported the wrong channel");
            Require(otherPixels[i] == values[((mapping ^ 0xffu) >> shift) & 3u], "fragment used the wrong MRT mapping");
        }
    }
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, false);
        Run(*device, true);
        std::cout << "prepared fragment export mapping tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
