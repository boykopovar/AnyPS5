#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Recompiler.hpp"
#include "CacheKey.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include <spirv/unified1/spirv.hpp>
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

constexpr std::uint32_t Marker = 0x5a17c0deu;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Extent = 64u;
constexpr std::uint32_t Rows = 8u;
constexpr std::uint32_t Vertices = 3u;
constexpr std::uint32_t Components = 4u;
alignas(256) std::array<std::uint32_t, Rows * Vertices * Components> Output{};
alignas(256) std::array<std::uint32_t, 16> Table{};
alignas(256) std::array<std::byte, Extent * Extent * 4u> Pixels{};

constexpr std::array<std::uint32_t, 2> MaskedAdd{0x4a0814f9u, 0x84060608u};
constexpr std::array<std::uint32_t, 34> Body{
    0xf4100204u, 0xfa000000u, 0xbf8cc07fu,
    0x7e000304u, 0x7e020305u, 0x7e04020au, 0x7e0602ffu, 0x12345678u,
    0x360c0087u, 0x4a0e0d06u, 0x4a0c0d07u, 0x4a0c0b06u, 0x340e0c84u,
    0xe0781000u, 0x80030007u,
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u,
    0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u,
    0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u,
    0xbf810000u, 0xbf810000u, 0xbf810000u,
};
constexpr std::array<std::uint32_t, 5> PixelCode{
    0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u,
};

std::vector<std::uint32_t> Program(bool folded, bool rebased = false) {
    std::vector<std::uint32_t> code;
    if (rebased) code.push_back(0x4e10100au);
    if (folded) code.push_back(0x7e080308u);
    else code.insert(code.end(), MaskedAdd.begin(), MaskedAdd.end());
    code.insert(code.end(), Body.begin(), Body.end());
    return code;
}

void CheckEncoding() {
    const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(Program(false));
    const auto& add = decoded.instructions.at(0);
    Require(add.op == ShaderRecompiler::RdnaOpcode::VAddNcU32 && add.destination.reg == 4u &&
        add.source0.kind == ShaderRecompiler::RdnaOperandKind::VectorRegister && add.source0.reg == 8u && add.source0.sdwaSel == 6u &&
        add.source1.kind == ShaderRecompiler::RdnaOperandKind::ScalarRegister && add.source1.reg == 10u && add.source1.sdwaSel == 4u && !add.source1.sdwaSext,
        "the probe did not encode v_add_nc_u32_sdwa v4, v8, s10 src1_sel:WORD_0");
    const auto& overwrite = decoded.instructions.at(1);
    Require(overwrite.op == ShaderRecompiler::RdnaOpcode::SLoadDwordx16 && overwrite.destination.reg == 8u && overwrite.source0.reg == 8u,
        "the probe did not overwrite s10 in an immediate s_load_dwordx16 s[8:23], s[8:9], 0");
    Require(decoded.instructions.at(11).op == ShaderRecompiler::RdnaOpcode::VLshlrevB32,
        "the probe did not convert its output record index to a byte offset");
    const auto rebased = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(Program(false, true));
    Require(rebased.instructions.front().op == ShaderRecompiler::RdnaOpcode::VSubrevNcU32 &&
        rebased.instructions.front().destination.reg == 8u && rebased.instructions.front().source0.reg == 10u && rebased.instructions.front().source1.reg == 8u,
        "the arithmetic prototype did not subtract the supplied base from the initial instance index");
}

bool ReadTable(void*, std::uint64_t address, std::uint32_t* value) {
    const auto begin = reinterpret_cast<std::uintptr_t>(Table.data());
    if (address < begin || address - begin >= sizeof(Table) || (address - begin) % sizeof(std::uint32_t) != 0u) return false;
    *value = Table[(address - begin) / sizeof(std::uint32_t)];
    return true;
}

AgcDriver::Graphics::State State(std::uint32_t wave) {
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, wave, wave, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Extent, Extent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0.0f, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0.0f, 1.0f};
    state.scissor = {{0, 0}, {Extent, Extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    return state;
}

std::array<std::uint32_t, Output.size()> Draw(AgcDriver::VulkanDevice& device, std::uint32_t wave, std::uint32_t base, std::uint32_t instances, bool folded, bool rebased = false, bool dynamic = false) {
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(Table.data());
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    Table.fill(0u);
    Table[2] = Marker;
    const std::array<std::uint32_t, 4> descriptor{
        static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), sizeof(Output), 0x31016facu,
    };
    std::copy(descriptor.begin(), descriptor.end(), Table.begin() + 4);
    const std::array<std::uint32_t, 4> words{
        static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), dynamic ? 0xbadc0ffeu : folded ? 0u : base, 0u,
    };
    const auto code = Program(folded, rebased);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{tableAddress, std::as_bytes(std::span(Table))}}};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {wave, 8u, words, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        device.Target(), {0u, 0u, 0u, 96u},
    };
    if (dynamic) {
        request.graphics = ShaderRecompiler::GraphicsCompileContext{.firstUserSgpr = 8u, .instanceInputSgpr = 10};
        auto ordinary = request;
        ordinary.graphics->instanceInputSgpr = -1;
        auto routed = request;
        routed.graphics->instanceInputSgpr = 11;
        for (const auto build : {&ShaderRecompiler::RecompileCacheKey::Build, &ShaderRecompiler::RecompileCacheKey::BuildInterface}) {
            std::vector<std::uint64_t> key, normalKey, routedKey;
            build(request, key);
            build(ordinary, normalKey);
            build(routed, routedKey);
            Require(key != normalKey && key != routedKey, "the dynamic instance input and SGPR route did not enter the shader cache key");
        }
        Require(ShaderRecompiler::RecompileCacheKey::ContextHash(request) != ShaderRecompiler::RecompileCacheKey::ContextHash(ordinary) &&
            ShaderRecompiler::RecompileCacheKey::ContextHash(request) != ShaderRecompiler::RecompileCacheKey::ContextHash(routed), "the source memo did not distinguish the instance input route");
        const ShaderRecompiler::RequestSerializer serializer;
        const auto replay = serializer.Deserialize(serializer.Serialize(request));
        Require(replay.request.graphics && replay.request.graphics->instanceInputSgpr == 10, "the replay lost its dynamic instance input");
        if (wave == 32u && base == 0u) {
            const auto refuse = [](const ShaderRecompiler::RecompileRequest& invalid) {
                bool refused = false;
                try {
                    static_cast<void>(ShaderRecompiler::Recompile(invalid));
                } catch (const std::runtime_error& error) {
                    refused = std::string(error.what()).find("dynamic instance input") != std::string::npos;
                }
                Require(refused, "an unsupported dynamic instance input request was accepted");
            };
            for (const auto invalidSgpr : {-2, 7, 12, 128}) {
                auto invalid = request;
                invalid.graphics->instanceInputSgpr = invalidSgpr;
                refuse(invalid);
            }
            auto capabilities = std::vector<std::uint32_t>(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end());
            std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityDrawParameters));
            auto unsupported = request;
            unsupported.target.supportedCapabilities = capabilities;
            refuse(unsupported);
        }
    }
    const ShaderRecompiler::SrtRuntime runtime{words, 0u, ReadTable};
    const auto capture = ShaderRecompiler::CaptureResources(request, runtime);
    const auto walked = capture->userDataUse.asBase | capture->userDataUse.asValue;
    Require((walked & 3u) == 3u && (walked & 4u) == 0u,
        "the resource walk did not distinguish the table pointer from the numeric instance word: asBase=" + std::to_string(capture->userDataUse.asBase) + " asValue=" + std::to_string(capture->userDataUse.asValue));
    if (!folded && !rebased && !dynamic) {
        auto pointerCode = code;
        pointerCode[2] = 0xf4100205u;
        const std::array<std::uint32_t, 4> pointerWords{0u, 0u, words[0], words[1]};
        auto pointerRequest = request;
        pointerRequest.shader.code = pointerCode;
        pointerRequest.context.userData = pointerWords;
        const ShaderRecompiler::SrtRuntime pointerRuntime{pointerWords, 0u, ReadTable};
        const auto pointerCapture = ShaderRecompiler::CaptureResources(pointerRequest, pointerRuntime);
        const auto pointerWalked = pointerCapture->userDataUse.asBase | pointerCapture->userDataUse.asValue;
        Require((pointerWalked & 12u) == 12u, "the positive control did not mark s10:s11 consumed as a resource pointer");
    }
    const auto vertex = *ShaderRecompiler::Recompile(request, *capture);
    Require(vertex.instanceOffsetSgpr == -1 && vertex.vertexAttributes.empty(), "the probe unexpectedly used fixed-function instance fetching");
    const auto pushBytes = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
    pixelInfo.wave32 = wave == 32u;
    pixelInfo.targetOutputMode[0] = 9u;
    pixelInfo.targetExportMapping.fill(0xe4u);
    const auto pixel = ShaderRecompiler::Recompile({
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0u, {}},
        {wave, 0u, {}, std::nullopt, pixelInfo, std::nullopt, {}},
        device.Target(), PixelPushLayout(pushBytes, device.Target()),
    });
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(pushBytes, device.Target())},
    }};
    AgcDriver::Pm4::DrawParameters draw{0u, Vertices, 0u, instances, 0u, false};
    draw.firstInstance = folded || rebased || dynamic ? base : 0u;
    Output.fill(Sentinel);
    device.Draw(State(wave), draw, shaders);
    device.WaitIdle();
    return Output;
}

void CheckRefusals(const std::shared_ptr<AgcDriver::VulkanDevice>& device) {
    AgcDriver::DriverDetail::DrawProgram program{};
    program.firstUserSgpr = 8u;
    program.userData.resize(4u);
    auto state = State(64u);
    AgcDriver::Pm4::DrawParameters draw{};
    draw.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{};
    draw.indirect->count = 1u;
    draw.indirect->startInstanceSgpr = 10;
    ShaderRecompiler::RecompileResult compiled{};
    const auto refused = [&](const AgcDriver::Graphics::State& graphics, const ShaderRecompiler::RecompileResult& result, std::int32_t sgpr) {
        auto candidate = draw;
        Require(AgcDriver::DriverDetail::Driver::ClassifyIndirectDraw(result, graphics, program, device, candidate, false, sgpr).has_value(), "an incompatible dynamic instance path was accepted");
    };
    refused(state, compiled, 11);
    auto instanced = compiled;
    instanced.vertexAttributes.emplace_back().fetchIndex = 1u;
    refused(state, instanced, 10);
    auto folded = compiled;
    folded.instanceOffsetSgpr = 10;
    refused(state, folded, 10);
    for (const auto path : {AgcDriver::Graphics::ShaderPath::Geometry, AgcDriver::Graphics::ShaderPath::Tessellation}) {
        auto graphics = state;
        graphics.stages.path = path;
        refused(graphics, compiled, 10);
    }
    auto rectangle = state;
    rectangle.rectList = true;
    refused(rectangle, compiled, 10);
    draw.indirect->baseVertexSgpr = 10;
    refused(state, compiled, 10);
}

void Run(AgcDriver::VulkanDevice& device) {
    CheckEncoding();
    constexpr std::array<std::uint32_t, 10> bases{
        0u, 1u, 65534u, 65535u, 65536u, 65537u, 917966u, 0x80000000u, 0xfffffff0u, 0xffffffffu,
    };
    std::uint32_t controls = 0u;
    std::uint32_t counterexamples = 0u;
    for (const auto wave : {32u, 64u}) {
        for (const auto base : bases) {
            const auto instances = base == 0xffffffffu ? 1u : 3u;
            std::array<std::uint32_t, Output.size()> expected{};
            expected.fill(Sentinel);
            for (std::uint32_t instance = 0u; instance < instances; ++instance) {
                const auto index = instance + (base & 0xffffu);
                for (std::uint32_t vertex = 0u; vertex < Vertices; ++vertex) {
                    const auto offset = ((index & (Rows - 1u)) * Vertices + vertex) * Components;
                    expected[offset] = index;
                    expected[offset + 1u] = vertex;
                    expected[offset + 2u] = Marker;
                    expected[offset + 3u] = 0x12345678u;
                }
            }
            const auto reference = Draw(device, wave, base, instances, false);
            for (std::size_t word = 0; word < expected.size(); ++word) {
                Require(reference[word] == expected[word], "wave" + std::to_string(wave) + " base " + std::to_string(base) +
                    " reference word " + std::to_string(word) + " is " + std::to_string(reference[word]) + ", expected " + std::to_string(expected[word]));
            }
            const auto rebased = Draw(device, wave, base, instances, false, true);
            Require(rebased == reference, "subtracting the supplied base did not preserve the masked arithmetic and SGPR overwrite");
            const auto dynamic = Draw(device, wave, base, instances, false, false, true);
            Require(dynamic == reference, "the BaseInstance input did not preserve masked arithmetic and the subsequent SGPR overwrite");
            const auto folded = Draw(device, wave, base, instances, true);
            const bool same = folded == reference;
            Require(same == (base < 65536u), "the whole-word folding counterexample did not distinguish the low-16-bit boundary");
            if (same) ++controls;
            else ++counterexamples;
            std::printf("wave%u base=%u instances=%u reference=exact whole-word-fold=%s supplied-base-rebase=exact BaseInstance-input=exact\n", wave, base, instances, same ? "equal" : "WRONG");
        }
    }
    std::printf("masked instance probe: %u below-boundary controls, %u whole-word-fold counterexamples\n", controls, counterexamples);
}

}

int main() {
    try {
        const std::shared_ptr<AgcDriver::VulkanDevice> device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckRefusals(device);
        Run(*device);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
