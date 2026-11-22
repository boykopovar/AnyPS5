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
#include <vector>

namespace {

using namespace ShaderRecompiler;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Extent = 16;
alignas(256) std::array<std::uint32_t, Extent * Extent * 4> pixels{};
alignas(256) std::array<std::uint32_t, Extent * Extent * 4> otherPixels{};
constexpr std::array<std::array<float, 4>, 3> positions{{{-1, -1, 0, 1}, {3, -1, 0, 1}, {-1, 3, 0, 1}}};
constexpr std::array<std::uint32_t, 6> vertexCode{0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};

void CheckPackedRtzExport(AgcDriver::VulkanDevice& device, const RecompileResult& vertex, AgcDriver::Graphics::State state, const AgcDriver::Pm4::DrawParameters& draw, bool predicated) {
    constexpr std::array<std::array<std::uint32_t, 6>, 8> cases{{
        {0x38800000u, 0xb8801fffu, 0xc77fffffu, 0x477fffffu, 0x84000400u, 0x7bfffbffu},
        {0x00000000u, 0x80000000u, 0x337fffffu, 0xb3800000u, 0x80000000u, 0x80010000u},
        {0x3f801fffu, 0xbf801fffu, 0x3f802000u, 0xbf802001u, 0xbc003c00u, 0xbc013c01u},
        {0x387fffffu, 0xb87fffffu, 0x38800000u, 0xb8800000u, 0x83ff03ffu, 0x84000400u},
        {0x47800000u, 0xc7800000u, 0x7f7fffffu, 0xff7fffffu, 0xfbff7bffu, 0xfbff7bffu},
        {0x7f800000u, 0xff800000u, 0x7f800001u, 0xffd23456u, 0xfc007c00u, 0xfe917e00u},
        {0x477fdfffu, 0x477fe000u, 0x3f7fffffu, 0x3f800001u, 0x7bff7bfeu, 0x3c003bffu},
        {0x7fa56789u, 0xff800001u, 0x00000001u, 0x807fffffu, 0xfe007f2bu, 0x80000000u},
    }};
    alignas(256) std::array<std::array<std::uint32_t, 8>, Extent> inputs{};
    for (std::size_t x = 0; x < inputs.size(); ++x) {
        std::ranges::copy(cases[x % cases.size()], inputs[x].begin());
        if (predicated && x >= cases.size()) std::fill_n(inputs[x].begin(), 4, 0x44000fffu);
    }
    const auto address = reinterpret_cast<std::uintptr_t>(inputs.data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), sizeof(inputs), 0x31016facu};
    std::vector<std::uint32_t> code{
        0x7e000f00u, 0x34020085u, 0xe0381000u, 0x80000401u, 0xe0341010u, 0x80000801u, 0xbf8c3f70u,
        0x5e140b04u, 0x5e160f06u,
    };
    if (predicated) code.insert(code.end(), {0x7d880088u, 0x02141508u, 0x02161709u});
    code.insert(code.end(), {0xf800140fu, 0x00000b0au, 0xf8001c3fu, 0x00000908u, 0xbf810000u});
    state.colors[0].componentMapping = state.colors[1].componentMapping = 0xe4u;
    const AgcDriver::Registers registers{{0x1b6, 0x8000}, {0x1b3, 0x100u}, {0x1b4, 0x100u}, {0x203, 0}, {0x1c5, 0x4004u}};
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(registers, AgcDriver::Graphics::ExportMappings(state));
    RecompileRequest request{{ShaderStage::Fragment, 0x30000, code, 0, {}}, {32, 0, descriptor, {}, pixel, {}, {}}, device.Target(), PixelPushLayout(0u, device.Target())};
    request.useCache = false;
    request.context.floatMode = ShaderFloatMode{0xc0u, true, false, false};
    const auto fragment = Recompile(request);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, &fragment, PixelPushOffset(0u, device.Target())}}};
    pixels.fill(0xdeadbeefu);
    otherPixels.fill(0xbaadf00du);
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    Require(pixels[0] == 0x38800000u && pixels[2] == 0xc77fe000u && pixels[4] == 0 && pixels[8] == 0x3f800000u, "compressed export did not draw the expected input columns");
    Require(pixels == otherPixels, "RTZ color export differs from an independently supplied pair of half words");
}

void Run(AgcDriver::VulkanDevice& device, bool integer) {
    const auto address = reinterpret_cast<std::uintptr_t>(positions.data());
    const std::array<std::uint32_t, 4> users{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (16u << 16u), 3, 0x01016facu};
    RecompileRequest vertexRequest{{ShaderStage::Vertex, 0x10000, vertexCode, 0, {}}, {32, 0, users, {}, {}, ShaderVertexStageInfo{}, {}}, device.Target(), {0, 0, 0, 128}};
    const auto vertex = Recompile(vertexRequest);
    const std::array<std::uint32_t, 4> values = integer ? std::array<std::uint32_t, 4>{11, 22, 33, 44} : std::array<std::uint32_t, 4>{0, 0x3e800000u, 0x3f000000u, 0x3f800000u};
    const std::array<std::uint32_t, 13> code{0x7e0002ffu, values[0], 0x7e0202ffu, values[1], 0x7e0402ffu, values[2], 0x7e0602ffu, values[3], 0xf800100fu, 0x03020100u, 0xf800183fu, 0x03020100u, 0xbf810000u};
    AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, integer ? 0x7007u : 0x9009u}};
    RecompileRequest request{{ShaderStage::Fragment, 0x20000, code, 0, {}}, {32, 0, {}, {}, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), {}, {}}, device.Target(), PixelPushLayout(0u, device.Target())};
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
            if (device.Target().registrationModules) {
                Require(materialized->PipelineVariantId() == first->PipelineVariantId(), "registration modules: the export mapping specialized the fragment module");
                continue;
            }
            for (std::size_t cursor = 5; cursor < materialized->spirv.size();) {
                const auto count = materialized->spirv[cursor] >> 16u;
                Require(count != 0u && count <= materialized->spirv.size() - cursor, "invalid specialized fragment instruction");
                Require((materialized->spirv[cursor] & 0xffffu) != static_cast<std::uint32_t>(spv::OpVectorExtractDynamic), "fragment export retained a dynamic channel selection");
                cursor += count;
            }
        }
        request.useCache = true;
        if (integer) continue;
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, pixel.get(), PixelPushOffset(0u, device.Target())}}};
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
    if (!integer) {
        for (const bool predicated : {false, true}) CheckPackedRtzExport(device, vertex, state, draw, predicated);
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
