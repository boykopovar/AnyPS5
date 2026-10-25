#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

PFN_vkCmdBindPipeline NativeBind = nullptr;
std::atomic<std::uint64_t> GraphicsBinds{0u};

VKAPI_ATTR void VKAPI_CALL CountBind(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipeline pipeline) {
    if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) ++GraphicsBinds;
    NativeBind(commands, point, pipeline);
}

struct BindTrace {
    AgcDriver::Graphics::DeviceFunctions* functions;
    explicit BindTrace(const AgcDriver::VulkanDevice& device) {
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
        functions = const_cast<AgcDriver::Graphics::DeviceFunctions*>(context.functions);
        Require(functions != nullptr && functions->cmdBindPipeline != nullptr, "native pipeline bind table is unavailable");
        NativeBind = functions->cmdBindPipeline;
        functions->cmdBindPipeline = CountBind;
    }
    ~BindTrace() { functions->cmdBindPipeline = NativeBind; }
};

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::byte, 128 * 128 * 4> DepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> StencilMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PlainPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> ColumnPixelCode{
    0x7e040f00, 0x7e0e02f2, 0xf8000082, 0x00000200, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> RowPixelCode{
    0x7e040f01, 0x7e0e02f2, 0xf8000082, 0x00000200, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 6> Vertices{{
    {-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f},
    {-1.0f, -1.0f, 0.75f, 1.0f}, {3.0f, -1.0f, 0.75f, 1.0f}, {-1.0f, 3.0f, 0.75f, 1.0f},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo(std::uint32_t shaderControl) {
    const AgcDriver::Registers context{
        {0x1b3, 0x700u}, {0x1b4, 0x700u}, {0x1b6, 0u}, {0x1c5, 9u}, {0x203, shaderControl},
    };
    return AgcDriver::Graphics::DecodePixelStageInfo(context, {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u});
}

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t shaderControl, const VkStencilOpState& stencil, std::uint32_t vertices = 3u, std::uint32_t instances = 1u, bool depthFail = false, const std::function<void(AgcDriver::Graphics::State&)>& alter = {}, const std::function<void(AgcDriver::Pm4::DrawParameters&)>& alterDraw = {}, std::span<const std::array<float, 4>> vertexData = Vertices, std::span<const std::uint32_t> vertexCode = VertexCode, std::span<const std::uint32_t> pixelData = {}, std::span<const ShaderRecompiler::MemoryRegion> pixelSrt = {}, const std::function<void(ShaderRecompiler::RecompileResult&)>& alterVertex = {}) {
    Pixels.fill(Kept);
    auto target = device.Target();
    std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
    std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT));
    target.supportedCapabilities = capabilities;

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertexData.data(), 16u, static_cast<std::uint32_t>(vertexData.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(vertexCode)}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    auto vertexResult = ShaderRecompiler::Recompile(vertex);
    if (alterVertex) alterVertex(vertexResult);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    auto pixelUserData = pixelData.empty() ? std::vector<std::uint32_t>(8, 0u) : std::vector<std::uint32_t>(pixelData.begin(), pixelData.end());
    std::vector<ShaderRecompiler::MemoryRegion> pixelMemory{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}};
    pixelMemory.insert(pixelMemory.end(), pixelSrt.begin(), pixelSrt.end());
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, PixelInfo(shaderControl), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    if (!pixelResult.stencilReferences.empty()) {
        Require(pixelResult.stencilReferenceDword * sizeof(std::uint32_t) + sizeof(std::uint32_t) == pixelResult.pushConstants.size(), "dynamic reference is not reserved after the stage payload");
        for (std::size_t cursor = 5; cursor < pixelResult.spirv.size();) {
            const auto count = pixelResult.spirv[cursor] >> 16u;
            Require(count != 0u && count <= pixelResult.spirv.size() - cursor, "invalid materialized replay module");
            Require((pixelResult.spirv[cursor] & 0xffffu) != spv::OpSpecConstant, "expected reference remains a pipeline specialization");
            cursor += count;
        }
    }
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), reinterpret_cast<std::uintptr_t>(StencilMemory.data()), {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
    state.depthTest = depthFail;
    state.depthCompare = depthFail ? VK_COMPARE_OP_NEVER : VK_COMPARE_OP_ALWAYS;
    state.stencilTest = true;
    state.stencilFront = stencil;
    state.stencilBack = stencil;
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
    if (alter) alter(state);
    AgcDriver::Pm4::DrawParameters draw{0, vertices, 0, instances, 0, false};
    if (alterDraw) alterDraw(draw);
    const auto bindsBefore = GraphicsBinds.load();
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    if (!pixelResult.stencilReferences.empty()) Require(GraphicsBinds.load() - bindsBefore == 1u, "stencil replay rebound its shader pipeline within an original draw");
}

void Check(const std::function<bool(std::uint32_t, std::uint32_t)>& drawn, std::string_view what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = drawn(x, y) ? std::byte{255} : Kept;
            const auto actual = Pixels[(y * Width + x) * 4u];
            Require(actual == expected, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + std::to_string(static_cast<unsigned>(actual)) + ", expected " + (drawn(x, y) ? "drawn" : "stencil-rejected"));
        }
    }
}

}


int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const BindTrace binds(*device);
        constexpr std::uint32_t exported = 0x52u;
        constexpr std::uint32_t plain = 0x50u;
        constexpr std::array<std::uint32_t, 12> alternating{
            0x7e040f00u, 0x36060481u,
            0x7c000000u | (0xc2u << 17u) | (3u << 9u) | 0x80u,
            0x7e0802ffu, 0xffu,
            (1u << 25u) | (5u << 17u) | (4u << 9u) | 0x80u,
            0x7e0e02f2u, 0xf8000082u, 5u << 8u,
            0xf800180fu, 0x07070707u, 0xbf810000u
        };
        constexpr std::array<std::uint32_t, 10> ordered{
            0x7c000000u | (4u << 17u) | (2u << 9u) | 0xf0u,
            0x7e0802ffu, 0xffu,
            (1u << 25u) | (5u << 17u) | (4u << 9u) | 0x80u,
            0x7e0e02f2u, 0xf8000082u, 5u << 8u,
            0xf800180fu, 0x07070707u, 0xbf810000u
        };
        const auto replace = [](std::uint32_t reference = 0u, std::uint32_t mask = 0xffu) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, mask, reference}; };
        const auto equal = [](std::uint32_t reference, std::uint32_t mask = 0xffu) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, mask, 0u, reference}; };
        const auto all = [](std::uint32_t, std::uint32_t) { return true; };
        const auto even = [](std::uint32_t x, std::uint32_t) { return x % 2u == 0u; };
        const auto odd = [](std::uint32_t x, std::uint32_t) { return x % 2u != 0u; };
        bool rejectedContract = false;
        try {
            Draw(*device, alternating, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, {}, {}, [&](ShaderRecompiler::RecompileResult& result) { result.hostSubgroupSize = device->Target().subgroupSize == 64u ? 32u : 64u; });
        } catch (const std::exception& error) {
            rejectedContract = std::string_view(error.what()).find("subgroup contract differs") != std::string_view::npos;
            if (!rejectedContract) throw;
        }
        Require(rejectedContract, "a stale vertex subgroup-size contract was accepted");
        rejectedContract = false;
        try {
            Draw(*device, alternating, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, {}, {}, [](ShaderRecompiler::RecompileResult& result) { auto words = result.spirv.Copy(); words[1] = 0x00010600u; result.spirv = std::move(words); });
        } catch (const std::exception& error) {
            rejectedContract = std::string_view(error.what()).find("subgroup contract differs") != std::string_view::npos;
            if (!rejectedContract) throw;
        }
        Require(rejectedContract, "a varying-size module contract was accepted");
        Draw(*device, alternating, exported, replace());
        Check(all, "finite stencil export");
        Draw(*device, PlainPixelCode, plain, equal(255u));
        Check(even, "even columns exported 255");
        Draw(*device, PlainPixelCode, plain, equal(0u));
        Check(odd, "odd columns exported zero");
        Draw(*device, alternating, exported, equal(91u));
        Check(all, "exported references replace compare reference");
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, ordered, exported, replace(), 6u);
        Draw(*device, PlainPixelCode, plain, equal(0u));
        Check(all, "overlapping primitives retain original order");
        Draw(*device, PlainPixelCode, plain, replace(0u));
        const VkStencilOpState instanceOrder{VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_GREATER, 0xffu, 0xffu, 0u};
        Draw(*device, ordered, exported, instanceOrder, 6u, 2u);
        Draw(*device, PlainPixelCode, plain, equal(0u));
        Check(all, "primitive order within every instance");
        alignas(256) constexpr std::array<std::uint32_t, 6> indices{0u, 1u, 2u, 3u, 4u, 5u};
        alignas(256) std::array<std::array<float, 4>, 7> indexedVertices{};
        std::copy(Vertices.begin(), Vertices.end(), indexedVertices.begin() + 1);
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, ordered, exported, instanceOrder, 6u, 2u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) {
            draw.indexed = true;
            draw.indexAddress = reinterpret_cast<std::uintptr_t>(indices.data());
            draw.indexSize = 4u;
            draw.firstVertex = 1u;
            draw.firstInstance = 7u;
        }, indexedVertices);
        Draw(*device, PlainPixelCode, plain, equal(0u));
        Check(all, "indexed primitives preserve base vertex and nonzero first instance");
        alignas(256) std::array<std::array<float, 4>, 72> instanceVertices{};
        std::copy(Vertices.begin(), Vertices.end(), instanceVertices.begin() + 56);
        std::copy(Vertices.begin() + 3, Vertices.end(), instanceVertices.begin() + 64);
        std::copy(Vertices.begin(), Vertices.begin() + 3, instanceVertices.begin() + 67);
        constexpr std::array<std::uint32_t, 8> instanceVertexCode{0x34101083u, 0x4a0a0b08u, 0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, ordered, exported, instanceOrder, 6u, 2u, false, {}, [](AgcDriver::Pm4::DrawParameters& draw) { draw.firstInstance = 7u; }, instanceVertices, instanceVertexCode);
        Check(all, "nonzero instance indices select live vertices");
        Draw(*device, PlainPixelCode, plain, equal(255u));
        Check(all, "each replayed instance preserves its observable instance index");
        Draw(*device, PlainPixelCode, plain, replace(0x55u));
        Draw(*device, alternating, exported, replace(0u, 0xaau));
        Draw(*device, PlainPixelCode, plain, equal(0xffu));
        Check(even, "stencil write mask preserves even columns");
        Draw(*device, PlainPixelCode, plain, equal(0x55u));
        Check(odd, "stencil write mask preserves odd columns");
        auto highBits = alternating;
        highBits[4] = 0x1234ffu;
        Draw(*device, highBits, exported, replace());
        Draw(*device, PlainPixelCode, plain, equal(255u));
        Check(even, "stencil reference truncates to eight attachment bits");
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, PlainPixelCode, plain, replace(0u), 3u, 1u, false, [](AgcDriver::Graphics::State& state) { state.cullMode = VK_CULL_MODE_BACK_BIT; });
        const auto facing = Pixels[0] == std::byte{255} ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        for (const bool back : {false, true}) {
            Draw(*device, PlainPixelCode, plain, replace(0u));
            Draw(*device, alternating, exported, replace(), 3u, 1u, false, [&](AgcDriver::Graphics::State& state) {
                state.frontFace = back ? (facing == VK_FRONT_FACE_COUNTER_CLOCKWISE ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE) : facing;
                state.stencilFront.writeMask = 0xaau;
                state.stencilBack.writeMask = 0x55u;
                state.stencilBack.passOp = VK_STENCIL_OP_INVERT;
            });
            Draw(*device, PlainPixelCode, plain, equal(back ? 0x55u : 0xaau));
            Check(back ? all : even, "distinct front and back stencil operations and write masks");
        }
        Draw(*device, PlainPixelCode, plain, replace(0x0fu));
        Draw(*device, alternating, exported, equal(1u, 0x0fu));
        Check(even, "stencil compare masks exported reference");
        const VkStencilOpState depthFailure{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 0u};
        Draw(*device, alternating, exported, depthFailure, 3u, 1u, true);
        Check([](std::uint32_t, std::uint32_t) { return false; }, "depth failure rejects colors");
        Draw(*device, PlainPixelCode, plain, equal(255u));
        Check(even, "depth-fail replace uses exported reference");
        auto depthStencil = std::vector<std::uint32_t>(alternating.begin(), alternating.end());
        const auto mrtz = std::find(depthStencil.begin(), depthStencil.end(), 0xf8000082u);
        const auto mrtzOffset = static_cast<std::size_t>(mrtz - depthStencil.begin());
        depthStencil.insert(mrtz, {0x7e0c02ffu, 0x3f400000u});
        depthStencil[mrtzOffset + 2u] = 0xf8000083u;
        depthStencil[mrtzOffset + 3u] = (5u << 8u) | 6u;
        Draw(*device, depthStencil, 0x53u, replace(), 3u, 1u, false, [](AgcDriver::Graphics::State& state) {
            state.depthTest = true;
            state.depthWrite = true;
            state.depthCompare = VK_COMPARE_OP_ALWAYS;
        });
        auto midDepth = Vertices;
        for (auto& vertex : midDepth) vertex[2] = 0.5f;
        Draw(*device, PlainPixelCode, plain, replace(), 3u, 1u, false, [](AgcDriver::Graphics::State& state) {
            state.depthTest = true;
            state.depthCompare = VK_COMPARE_OP_LESS;
        }, {}, midDepth);
        Check(all, "simultaneous depth and stencil exports preserve shader depth replacement");
        for (const bool restart : {false, true}) {
            bool refused = false;
            try {
                Draw(*device, alternating, exported, replace(), 3u, 1u, false, [&](AgcDriver::Graphics::State& state) { if (restart) state.primitiveRestart = true; else state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; });
            } catch (const std::exception& error) {
                refused = std::string(error.what()).find("direct triangle lists") != std::string::npos;
            }
            Require(refused, "unsafe primitive routing was not refused");
        }
        Draw(*device, ColumnPixelCode, exported, replace());
        Check(all, "unbounded stencil export");
        Draw(*device, PlainPixelCode, plain, equal(5u));
        Check([](std::uint32_t x, std::uint32_t) { return x == 5u; }, "unbounded stencil export replaces each column's own value");
        bool earlyRefused = false;
        try { Draw(*device, alternating, 0x1012u, replace()); } catch (const std::exception& error) { earlyRefused = std::string(error.what()).find("late tests") != std::string::npos; }
        Require(earlyRefused, "early stencil testing was not refused");
        bool orderedRefused = false;
        try { Draw(*device, alternating, 0x10052u, replace()); } catch (const std::exception& error) { orderedRefused = std::string(error.what()).find("late tests") != std::string::npos; }
        Require(orderedRefused, "ordered fragment execution was not refused");
        bool indirectRefused = false;
        try { Draw(*device, alternating, exported, replace(), 3u, 1u, false, {}, [](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect.emplace(); }); } catch (const std::exception& error) { indirectRefused = std::string(error.what()).find("direct triangle lists") != std::string::npos; }
        Require(indirectRefused, "an indirect replay draw was not refused");
        auto killed = std::vector<std::uint32_t>(alternating.begin(), alternating.end());
        killed.insert(std::find(killed.begin(), killed.end(), 0xf800180fu), 0x7c000000u | (0xd2u << 17u) | (3u << 9u) | 0x80u);
        Draw(*device, killed, exported, replace());
        Check(even, "shader kill remains effective before reference filtering");
        auto skippedExport = std::vector<std::uint32_t>(alternating.begin(), alternating.end());
        skippedExport.insert(skippedExport.begin(), 0xbe94037eu);
        skippedExport.insert(skippedExport.begin() + 7, 0x7c000000u | (0xd2u << 17u) | (3u << 9u) | 0x80u);
        skippedExport.insert(std::find(skippedExport.begin(), skippedExport.end(), 0xf800180fu), 0xbefe0314u);
        bool coverageRefused = false;
        try { Draw(*device, skippedExport, exported, replace()); } catch (const std::exception& error) { coverageRefused = std::string(error.what()).find("every kept path") != std::string::npos; }
        Require(coverageRefused, "a stencil export skipped by kept fragments was not refused");
        auto userColor = std::vector<std::uint32_t>(alternating.begin(), alternating.end());
        *std::find(userColor.begin(), userColor.end(), 0x7e0e02f2u) = 0x7e0e0200u;
        const std::array<std::uint32_t, 1> colorData{0x3f800000u};
        Draw(*device, userColor, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, colorData);
        Check(all, "dynamic reference preserves the adjacent shader-data push payload");
        auto guarded = std::vector<std::uint32_t>(alternating.begin(), alternating.end());
        *std::find(guarded.begin(), guarded.end(), 0x7e0e02f2u) = 0x7e0e020cu;
        guarded.insert(guarded.begin(), {0xf4040280u, 0xfa000018u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu});
        alignas(256) std::array<std::uint32_t, 16> root{};
        const std::uint32_t payload = 0x3f800000u;
        const auto rootAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root.data()));
        const std::array<std::uint32_t, 2> rootData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u)};
        const auto payloadAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&payload));
        root[6] = static_cast<std::uint32_t>(payloadAddress);
        root[7] = static_cast<std::uint32_t>(payloadAddress >> 32u);
        const std::array<ShaderRecompiler::MemoryRegion, 2> guardedMemory{{{rootAddress, std::as_bytes(std::span(root))}, {payloadAddress, std::as_bytes(std::span(&payload, 1))}}};
        Draw(*device, guarded, exported, replace(), 6u, 2u, false, {}, {}, Vertices, VertexCode, rootData, guardedMemory);
        Check(all, "readonly guarded SRT survives repeated references, primitives and instances");
        root[6] = root[7] = 0u;
        bool faultThrown = false;
        try { Draw(*device, guarded, exported, replace(), 6u, 2u, false, {}, {}, Vertices, VertexCode, rootData, std::span(guardedMemory).first(1)); }
        catch (const std::exception& error) { faultThrown = std::string(error.what()).find("BDA") != std::string::npos; }
        Require(faultThrown, "poisoned readonly SRT replay did not throw a host fault");
        Require(std::ranges::all_of(Pixels, [](std::byte value) { return value == Kept; }), "poisoned replay published guest color results before its fault");
        std::puts("stencil replay tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
