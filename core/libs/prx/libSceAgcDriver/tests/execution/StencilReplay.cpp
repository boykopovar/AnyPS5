#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "StencilReplay.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "../WatchedBlock.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

PFN_vkCmdDraw NativeDraw = nullptr;
PFN_vkCmdDrawIndexed NativeDrawIndexed = nullptr;
std::atomic<std::uint64_t> GraphicsDraws{0u};

VKAPI_ATTR void VKAPI_CALL CountBind(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipeline pipeline) {
    if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) ++GraphicsBinds;
    NativeBind(commands, point, pipeline);
}

VKAPI_ATTR void VKAPI_CALL CountDraw(VkCommandBuffer commands, std::uint32_t vertices, std::uint32_t instances, std::uint32_t firstVertex, std::uint32_t firstInstance) {
    ++GraphicsDraws;
    NativeDraw(commands, vertices, instances, firstVertex, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL CountDrawIndexed(VkCommandBuffer commands, std::uint32_t indices, std::uint32_t instances, std::uint32_t firstIndex, std::int32_t vertexOffset, std::uint32_t firstInstance) {
    ++GraphicsDraws;
    NativeDrawIndexed(commands, indices, instances, firstIndex, vertexOffset, firstInstance);
}

struct BindTrace {
    AgcDriver::Graphics::DeviceFunctions* functions;
    explicit BindTrace(const AgcDriver::VulkanDevice& device) {
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
        functions = const_cast<AgcDriver::Graphics::DeviceFunctions*>(context.functions);
        Require(functions != nullptr, "native pipeline bind table is unavailable");
        NativeBind = reinterpret_cast<PFN_vkCmdBindPipeline>(context.deviceProc(context.device, "vkCmdBindPipeline"));
        NativeDraw = reinterpret_cast<PFN_vkCmdDraw>(context.deviceProc(context.device, "vkCmdDraw"));
        NativeDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(context.deviceProc(context.device, "vkCmdDrawIndexed"));
        functions->cmdBindPipeline = CountBind;
        functions->cmdDraw = CountDraw;
        functions->cmdDrawIndexed = CountDrawIndexed;
    }
    ~BindTrace() {
        functions->cmdBindPipeline = NativeBind;
        functions->cmdDraw = NativeDraw;
        functions->cmdDrawIndexed = NativeDrawIndexed;
    }
};

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::byte, 128 * 128 * 4> DepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> StencilMemory{};
alignas(256) std::array<std::byte, 128 * 128 * 4> MultisampleDepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> MultisampleStencilMemory{};
alignas(256) std::array<std::byte, 128 * 128 * 4> MultisampleColorMemory{};
alignas(256) std::array<std::byte, 128 * 128 * 4> SampledDepthMemory{};

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
    AgcDriver::Registers context{
        {0x003, 0u}, {0x0b4, 0u}, {0x0b5, 0x3f800000u}, {0x1b3, 0x700u}, {0x1b4, 0x700u}, {0x1b6, 0u}, {0x1c5, 9u}, {0x203, shaderControl},
    };
    if ((shaderControl & 6u) == 4u) {
        context.insert_or_assign(0x011u, 1u);
        context.insert_or_assign(0x10bu, 0x40u);
        context.insert_or_assign(0x10cu, 0xff0000u);
        context.insert_or_assign(0x200u, 1u);
    }
    return AgcDriver::Graphics::DecodePixelStageInfo(context, {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u});
}

bool LastReplayStored = false;
bool CachedShaders = false;
std::uint32_t ExpectedBinds = 1u;
std::uint64_t LastDraws = 0;

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t shaderControl, const VkStencilOpState& stencil, std::uint32_t vertices = 3u, std::uint32_t instances = 1u, bool depthFail = false, const std::function<void(AgcDriver::Graphics::State&)>& alter = {}, const std::function<void(AgcDriver::Pm4::DrawParameters&)>& alterDraw = {}, std::span<const std::array<float, 4>> vertexData = Vertices, std::span<const std::uint32_t> vertexCode = VertexCode, std::span<const std::uint32_t> pixelData = {}, std::span<const ShaderRecompiler::MemoryRegion> pixelSrt = {}, const std::function<void(ShaderRecompiler::RecompileResult&)>& alterVertex = {}, std::uint32_t pixelPushBytes = 0u) {
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
    vertex.useCache = CachedShaders;
    auto vertexResult = ShaderRecompiler::Recompile(vertex);
    if (alterVertex) alterVertex(vertexResult);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    auto pixelUserData = pixelData.empty() ? std::vector<std::uint32_t>(8, 0u) : std::vector<std::uint32_t>(pixelData.begin(), pixelData.end());
    std::vector<ShaderRecompiler::MemoryRegion> pixelMemory{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}};
    pixelMemory.insert(pixelMemory.end(), pixelSrt.begin(), pixelSrt.end());
    auto pixelLayout = PixelPushLayout(vertexPush, target);
    if (pixelPushBytes != 0u) pixelLayout.pushConstantSizeBytes = pixelPushBytes;
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, PixelInfo(shaderControl), std::nullopt, pixelMemory},
        target,
        pixelLayout
    };
    fragment.useCache = CachedShaders;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    if (!pixelResult.stencilReferences.empty()) {
        Require(pixelResult.stencilReferenceDword * sizeof(std::uint32_t) + sizeof(std::uint32_t) == pixelResult.pushConstants.size(), "dynamic reference is not reserved after the stage payload");
        LastReplayStored = false;
        for (std::size_t cursor = 5; cursor < pixelResult.spirv.size();) {
            const auto count = pixelResult.spirv[cursor] >> 16u;
            LastReplayStored = LastReplayStored || (pixelResult.spirv[cursor] & 0xffffu) == spv::OpConvertUToPtr;
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
    const auto drawsBefore = GraphicsDraws.load();
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    LastDraws = GraphicsDraws.load() - drawsBefore;
    if (!pixelResult.stencilReferences.empty()) Require(GraphicsBinds.load() - bindsBefore == ExpectedBinds, "stencil replay rebound its shader pipeline within an original draw");
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
        const auto storeDevice = ShaderRecompiler::StencilReplayLanesFixed(device->Target().supportedCapabilities);
        Require(!LastReplayStored, "a shader that needs no lane guarantee kept the replay store");
        constexpr std::array<std::uint32_t, 21> waveBranch{
            0x7e040f00u, 0xbe84047eu, 0x36060481u, 0x7daa0680u, 0xbf880007u, 0xbefe0404u, 0x7e0802ffu,
            0x000000ffu, 0x7d8a0680u, 0xd5010005u, 0x01a90104u, 0xbf820003u, 0xbefe0404u, 0x7e0a02ffu,
            0x000000feu, 0x7e0e02f2u, 0xf8000082u, 0x00000500u, 0xf800180fu, 0x07070707u, 0xbf810000u
        };
        Draw(*device, alternating, exported, replace());
        Draw(*device, waveBranch, exported, equal(91u));
        Check(all, "a wave branch on a lane predicate sees every lane of the original draw");
        Require(LastReplayStored == storeDevice, "a shader that needs the lane guarantee did not get the replay store");
        alignas(256) constexpr std::array<std::array<float, 4>, 4> stripVertices{{
            {-1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f},
        }};
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, alternating, exported, replace(), 4u, 1u, false, [](AgcDriver::Graphics::State& state) { state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; }, {}, stripVertices);
        Draw(*device, PlainPixelCode, plain, equal(255u));
        Check(even, "a replayed triangle strip draws both of its triangles");
        std::array<bool, 4> culled{};
        const bool dynamicFace = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).graphicsPipelineLibrary;
        for (std::uint32_t variant = 0; variant < (dynamicFace ? 4u : 0u); ++variant) {
            Draw(*device, PlainPixelCode, plain, replace(0u));
            Draw(*device, alternating, exported, replace(), 4u, 1u, false, [variant](AgcDriver::Graphics::State& state) {
                state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                state.cullMode = (variant & 1u) != 0u ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
                state.frontFace = (variant & 2u) != 0u ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
            }, {}, stripVertices);
            Draw(*device, PlainPixelCode, plain, equal(255u));
            std::uint32_t count = 0;
            for (std::uint32_t y = 0; y < Height; ++y) for (std::uint32_t x = 0; x < Width; ++x) if (Pixels[(y * Width + x) * 4u] == std::byte{255}) ++count;
            culled[variant] = count == 0u;
            if (!culled[variant]) Check(even, "a replayed culled triangle strip draws both of its triangles or none");
        }
        Require(!dynamicFace || (culled[0] != culled[1] && culled[0] != culled[2] && culled[1] != culled[3]), "culling a replayed triangle strip does not follow the face state");
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
        for (const int unsafe : {0, 1, 2}) {
            if (unsafe == 1 && dynamicFace) continue;
            bool refused = false;
            try {
                Draw(*device, alternating, exported, replace(), 3u, 1u, false, [&](AgcDriver::Graphics::State& state) {
                    if (unsafe == 0) state.primitiveRestart = true;
                    else state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                    if (unsafe == 1) state.cullMode = VK_CULL_MODE_BACK_BIT;
                    if (unsafe == 2) state.stencilBack.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
                });
            } catch (const std::exception& error) {
                refused = std::string(error.what()).find("supports triangle lists, and triangle strips") != std::string::npos;
            }
            Require(refused, "unsafe primitive routing was not refused");
        }
        Draw(*device, ColumnPixelCode, exported, replace());
        Check(all, "unbounded stencil export");
        Draw(*device, PlainPixelCode, plain, equal(5u));
        Check([](std::uint32_t x, std::uint32_t) { return x == 5u; }, "unbounded stencil export replaces each column's own value");
        const auto replayContext = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const bool bitPasses = replayContext.graphicsPipelineLibrary && replayContext.dynamicColorWriteMask;
        Draw(*device, PlainPixelCode, plain, replace(0u));
        const auto plainDraws = LastDraws;
        Draw(*device, ColumnPixelCode, exported, replace());
        Require(LastDraws == plainDraws + (bitPasses ? 9u : 255u), "an unbounded replace export recorded " + std::to_string(LastDraws + 1u - plainDraws) + " passes");
        Draw(*device, ColumnPixelCode, exported, replace(), 3u, 1u, false, [](AgcDriver::Graphics::State& state) {
            state.blend.colorWriteMask = 0u;
            state.blends = {state.blend};
        });
        Require(LastDraws == plainDraws + (replayContext.graphicsPipelineLibrary ? 9u : 255u), "an unbounded replace export that writes no color recorded " + std::to_string(LastDraws + 1u - plainDraws) + " passes");
        Check([](std::uint32_t, std::uint32_t) { return false; }, "an unbounded replace export with no color write mask writes no color");
        Draw(*device, PlainPixelCode, plain, equal(5u));
        Check([](std::uint32_t x, std::uint32_t) { return x == 5u; }, "an unbounded replace export that writes no color replaces each column's own value");
        const auto columnsHold = [&](const std::function<std::uint32_t(std::uint32_t)>& value, std::string_view what) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                Draw(*device, PlainPixelCode, plain, equal(value(x)));
                Check([&](std::uint32_t column, std::uint32_t) { return value(column) == value(x); }, what);
            }
        };
        Draw(*device, PlainPixelCode, plain, replace(0xffu));
        Draw(*device, ColumnPixelCode, exported, replace());
        columnsHold([](std::uint32_t x) { return x; }, "an unbounded replace export clears the bits its value does not set");
        auto highColumns = std::vector<std::uint32_t>(ColumnPixelCode.begin(), ColumnPixelCode.end());
        highColumns.insert(highColumns.begin() + 1, {(0x1cu << 25u) | (2u << 17u) | (2u << 9u) | 0xffu, 0xc0u});
        Draw(*device, PlainPixelCode, plain, replace(0x3fu));
        Draw(*device, highColumns, exported, replace());
        columnsHold([](std::uint32_t x) { return x | 0xc0u; }, "an unbounded replace export sets the high bits of its value");
        Draw(*device, PlainPixelCode, plain, replace(0xa5u));
        Draw(*device, ColumnPixelCode, exported, replace(0u, 0x0fu));
        columnsHold([](std::uint32_t x) { return 0xa0u | (x & 0x0fu); }, "an unbounded replace export keeps the bits outside the write mask");
        const auto additive = [](AgcDriver::Graphics::State& state) {
            state.blend.blendEnable = VK_TRUE;
            state.blend.srcColorBlendFactor = state.blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.blend.srcAlphaBlendFactor = state.blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.blend.colorBlendOp = state.blend.alphaBlendOp = VK_BLEND_OP_ADD;
            state.blends = {state.blend};
        };
        alignas(256) constexpr std::array<std::uint32_t, 5> faintPixelCode{0x7e0e02ffu, 0x3d800000u, 0xf800180fu, 0x07070707u, 0xbf810000u};
        Draw(*device, faintPixelCode, plain, replace(0u), 3u, 1u, false, additive);
        const auto blendedOnce = Pixels[0];
        Require(blendedOnce != Kept && blendedOnce != std::byte{255}, "the additive blend reference did not blend");
        auto faintColumns = std::vector<std::uint32_t>(ColumnPixelCode.begin(), ColumnPixelCode.end());
        const auto white = std::find(faintColumns.begin(), faintColumns.end(), 0x7e0e02f2u);
        *white = 0x7e0e02ffu;
        faintColumns.insert(white + 1, 0x3d800000u);
        Draw(*device, faintColumns, exported, replace(), 3u, 1u, false, additive);
        for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) Require(Pixels[pixel * 4u] == blendedOnce, "an unbounded replace export blends its color once: pixel " + std::to_string(pixel) + " is " + std::to_string(static_cast<unsigned>(Pixels[pixel * 4u])) + ", expected " + std::to_string(static_cast<unsigned>(blendedOnce)));
        auto depthColumns = std::vector<std::uint32_t>(ColumnPixelCode.begin(), ColumnPixelCode.end());
        const auto columnZ = std::find(depthColumns.begin(), depthColumns.end(), 0xf8000082u);
        const auto columnZOffset = static_cast<std::size_t>(columnZ - depthColumns.begin());
        depthColumns.insert(columnZ, {0x7e0c02ffu, 0x3f400000u});
        depthColumns[columnZOffset + 2u] = 0xf8000083u;
        depthColumns[columnZOffset + 3u] = (2u << 8u) | 6u;
        auto farColumns = Vertices;
        for (auto& vertex : farColumns) vertex[2] = 1.0f;
        const auto depthLess = [](AgcDriver::Graphics::State& state) {
            state.depthTest = true;
            state.depthWrite = true;
            state.depthCompare = VK_COMPARE_OP_LESS;
        };
        Draw(*device, PlainPixelCode, plain, replace(0u), 3u, 1u, false, [](AgcDriver::Graphics::State& state) {
            state.depthTest = true;
            state.depthWrite = true;
            state.depthCompare = VK_COMPARE_OP_ALWAYS;
        }, {}, farColumns);
        Draw(*device, depthColumns, 0x53u, replace(), 3u, 1u, false, depthLess);
        Check(all, "an unbounded replace export with depth draws where its depth passes");
        columnsHold([](std::uint32_t x) { return x; }, "an unbounded replace export tests each bit against the depth from before the draw");
        Draw(*device, PlainPixelCode, plain, replace(0x77u));
        Draw(*device, depthColumns, 0x53u, replace(), 3u, 1u, false, depthLess);
        Check([](std::uint32_t, std::uint32_t) { return false; }, "an unbounded replace export behind the depth draws no color");
        Draw(*device, PlainPixelCode, plain, equal(0x77u));
        Check(all, "an unbounded replace export behind the depth keeps the stencil");
        bool earlyRefused = false;
        try { Draw(*device, alternating, 0x1012u, replace()); } catch (const std::exception& error) { earlyRefused = std::string(error.what()).find("late tests") != std::string::npos; }
        Require(earlyRefused, "early stencil testing was not refused");
        bool orderedRefused = false;
        try { Draw(*device, alternating, 0x10052u, replace()); } catch (const std::exception& error) { orderedRefused = std::string(error.what()).find("late tests") != std::string::npos; }
        Require(orderedRefused, "ordered fragment execution was not refused");
        using IndirectDraw = AgcDriver::Pm4::DrawParameters::IndirectDraw;
        const auto indirectDraw = [](const void* records, std::uint32_t opcode, std::uint32_t recordBytes, std::uint32_t count, const std::uint32_t* countWord = nullptr) {
            IndirectDraw indirect{reinterpret_cast<std::uintptr_t>(records), opcode, recordBytes, recordBytes, count, countWord != nullptr, reinterpret_cast<std::uintptr_t>(countWord), 0x280u, 0x280u, 0x280u, false, 0u};
            indirect.vertexRule = IndirectDraw::Rule::InPlace;
            indirect.instanceRule = IndirectDraw::Rule::InPlace;
            return indirect;
        };
        for (const bool nearLast : {true, false}) {
            alignas(256) const std::array<std::uint32_t, 8> overlapping = nearLast ? std::array<std::uint32_t, 8>{3u, 1u, 3u, 0u, 3u, 1u, 0u, 0u} : std::array<std::uint32_t, 8>{3u, 1u, 0u, 0u, 3u, 1u, 3u, 0u};
            Draw(*device, PlainPixelCode, plain, replace(0x33u));
            Draw(*device, ordered, exported, replace(), 0u, 0u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(overlapping.data(), 0x2cu, 16u, 2u); });
            Draw(*device, PlainPixelCode, plain, equal(nearLast ? 255u : 0u));
            Check(all, "the last of two overlapping indirect records sets the stencil");
        }
        {
            alignas(256) const std::array<std::uint32_t, 5> record{6u, 2u, 0u, 99u, 7u};
            Draw(*device, PlainPixelCode, plain, replace(0u));
            Draw(*device, ordered, exported, instanceOrder, 6u, 0u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) {
                draw.indexed = true;
                draw.indexAddress = reinterpret_cast<std::uintptr_t>(indices.data());
                draw.indexSize = 4u;
                draw.indirect = indirectDraw(record.data(), 0x25u, 20u, 1u);
                draw.indirect->vertexRule = IndirectDraw::Rule::Constant;
                draw.indirect->vertexConstant = 1u;
            }, indexedVertices);
            Draw(*device, PlainPixelCode, plain, equal(0u));
            Check(all, "an indexed indirect record replays with the constant base vertex and its own instances");
        }
        {
            constexpr std::array<std::uint32_t, 8> constant{0x7e0a02ffu, 0x5au, 0x7e0e02f2u, 0xf8000082u, 5u << 8u, 0xf800180fu, 0x07070707u, 0xbf810000u};
            alignas(256) const std::array<std::uint32_t, 10> records{3u, 1u, 6u, 0u, 0u, 6u, 1u, 3u, 0u, 0u};
            Draw(*device, PlainPixelCode, plain, replace(0u));
            Draw(*device, constant, exported, replace(), 6u, 0u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) {
                draw.indexed = true;
                draw.indexAddress = reinterpret_cast<std::uintptr_t>(indices.data());
                draw.indexSize = 4u;
                draw.indirect = indirectDraw(records.data(), 0x38u, 20u, 2u);
                draw.indirect->vertexRule = IndirectDraw::Rule::Constant;
                draw.indirect->vertexConstant = 1u;
            }, indexedVertices);
            Check(all, "a single-reference indexed indirect draw");
            Draw(*device, PlainPixelCode, plain, equal(0x5au));
            Check(all, "indexed records past the bound index count are skipped and longer ones clamped");
        }
        {
            alignas(256) const std::array<std::uint32_t, 8> records{3u, 1u, 0u, 0u, 3u, 1u, 3u, 0u};
            alignas(256) const std::uint32_t drawn = 1u;
            Draw(*device, PlainPixelCode, plain, replace(0x33u));
            Draw(*device, ordered, exported, replace(), 0u, 0u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(records.data(), 0x2cu, 16u, 2u, &drawn); });
            Draw(*device, PlainPixelCode, plain, equal(255u));
            Check(all, "a record count read from memory below the packet's maximum");
        }
        {
            alignas(256) const std::array<std::uint32_t, 8> records{3u, 1u, 0u, 0u, 3u, 2u, 0u, 0xffffffffu};
            bool overflowRefused = false;
            try { Draw(*device, ordered, exported, replace(), 0u, 0u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(records.data(), 0x2cu, 16u, 2u); }); } catch (const std::exception& error) { overflowRefused = std::string(error.what()).find("draw indices overflow") != std::string::npos; }
            Require(overflowRefused, "an indirect record whose instances overflow was not refused");
        }
        {
            alignas(256) const std::array<std::uint32_t, 8> records{0u, 1u, 0u, 0u, 3u, 0u, 0u, 0u};
            Draw(*device, PlainPixelCode, plain, replace(0x33u));
            ExpectedBinds = 0u;
            Draw(*device, ordered, exported, replace(), 0u, 0u, false, {}, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(records.data(), 0x2cu, 16u, 2u); });
            ExpectedBinds = 1u;
            Check([](std::uint32_t, std::uint32_t) { return false; }, "indirect records without vertices or instances draw nothing");
            Draw(*device, PlainPixelCode, plain, equal(0x33u));
            Check(all, "indirect records without vertices or instances keep the stencil");
        }
        {
            constexpr std::array<std::uint32_t, 12> fullscreen{
                0x7e040f00u, 0x36060481u,
                0x7c000000u | (0xd2u << 17u) | (3u << 9u) | 0x80u,
                0x34100488u, 0x7e0e02f2u, 0x7e0c02ffu, 0x3f400000u,
                0xf8000083u, (8u << 8u) | 6u,
                0xf800180fu, 0x07070707u, 0xbf810000u
            };
            alignas(256) const std::array<std::uint32_t, 4> record{3u, 1u, 0u, 0u};
            Draw(*device, PlainPixelCode, plain, replace(0xeeu));
            const auto singleDraws = LastDraws;
            Draw(*device, fullscreen, 0x45u, replace(), 0u, 0u, false, [](AgcDriver::Graphics::State& state) {
                state.depthTest = true;
                state.depthWrite = true;
                state.depthCompare = VK_COMPARE_OP_ALWAYS;
            }, [&](AgcDriver::Pm4::DrawParameters& draw) {
                draw.indirect = indirectDraw(record.data(), 0x24u, 16u, 1u);
                draw.indirect->vertexRule = IndirectDraw::Rule::Constant;
                draw.indirect->instanceRule = IndirectDraw::Rule::Constant;
            });
            Check(even, "an indirect fullscreen triangle with depth, kill and an unbounded operation-value export");
            Require(LastDraws == singleDraws + (bitPasses ? 9u : 255u), "an indirect unbounded replace export recorded " + std::to_string(LastDraws + 1u - singleDraws) + " passes");
            Draw(*device, PlainPixelCode, plain, equal(4u));
            Check([](std::uint32_t x, std::uint32_t) { return x == 4u; }, "each kept column replaces its own operation value");
            Draw(*device, PlainPixelCode, plain, equal(0xeeu));
            Check(odd, "killed columns keep the stencil");
        }
        {
            alignas(256) constexpr std::array<std::array<float, 4>, 9> stripRecordVertices{{
                {-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f}, {0.0f, 0.0f, 0.75f, 1.0f},
                {0.0f, -1.0f, 0.75f, 1.0f}, {1.0f, -1.0f, 0.75f, 1.0f}, {0.0f, 1.0f, 0.75f, 1.0f}, {1.0f, 1.0f, 0.75f, 1.0f}, {0.0f, 0.0f, 0.75f, 1.0f},
            }};
            alignas(256) constexpr std::array<std::array<float, 4>, 10> stripIndexedVertices{{
                {0.0f, 0.0f, 0.75f, 1.0f}, {-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f}, {0.0f, 0.0f, 0.75f, 1.0f},
                {0.0f, -1.0f, 0.75f, 1.0f}, {1.0f, -1.0f, 0.75f, 1.0f}, {0.0f, 1.0f, 0.75f, 1.0f}, {1.0f, 1.0f, 0.75f, 1.0f}, {0.0f, 0.0f, 0.75f, 1.0f},
            }};
            alignas(256) constexpr std::array<std::uint32_t, 9> stripIndices{7u, 7u, 0u, 1u, 2u, 3u, 4u, 5u, 6u};
            alignas(256) const std::array<std::uint32_t, 8> stripRecords{3u, 1u, 0u, 0u, 4u, 1u, 4u, 0u};
            alignas(256) const std::array<std::uint32_t, 10> stripIndexedRecords{3u, 1u, 2u, 1u, 0u, 4u, 1u, 5u, 2u, 0u};
            const auto left = [](std::uint32_t x, std::uint32_t) { return x < Width / 2u; };
            const auto right = [](std::uint32_t x, std::uint32_t) { return x >= Width / 2u; };
            for (const bool indexed : {false, true}) {
                for (const bool culledStrip : {false, true}) {
                    if (culledStrip && !dynamicFace) continue;
                    const std::string what = std::string(indexed ? "an indexed" : "a non-indexed") + (culledStrip ? " culled" : "") + " indirect triangle strip";
                    Draw(*device, PlainPixelCode, plain, replace(0x33u));
                    Draw(*device, ordered, exported, replace(), indexed ? static_cast<std::uint32_t>(stripIndices.size()) : 0u, 0u, false, [&](AgcDriver::Graphics::State& state) {
                        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                        if (culledStrip) {
                            state.cullMode = VK_CULL_MODE_BACK_BIT;
                            state.frontFace = facing;
                        }
                    }, [&](AgcDriver::Pm4::DrawParameters& draw) {
                        if (indexed) {
                            draw.indexed = true;
                            draw.indexAddress = reinterpret_cast<std::uintptr_t>(stripIndices.data());
                            draw.indexSize = 4u;
                            draw.indirect = indirectDraw(stripIndexedRecords.data(), 0x25u, 20u, 2u);
                        } else {
                            draw.indirect = indirectDraw(stripRecords.data(), 0x2cu, 16u, 2u);
                        }
                    }, indexed ? std::span<const std::array<float, 4>>(stripIndexedVertices) : std::span<const std::array<float, 4>>(stripRecordVertices));
                    Draw(*device, PlainPixelCode, plain, equal(255u));
                    Check(left, what + " keeps the first record's export where the second record does not draw");
                    Draw(*device, PlainPixelCode, plain, equal(0u));
                    Check(right, what + " restarts its triangle parity and first vertex at the second record");
                }
            }
        }
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
        Draw(*device, userColor, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, colorData, {}, {}, sizeof(std::uint32_t));
        Check(all, "dynamic reference spills an otherwise full shader-data push payload");
        auto bufferColor = std::vector<std::uint32_t>(alternating.begin(), alternating.end());
        *std::find(bufferColor.begin(), bufferColor.end(), 0x7e0e02f2u) = 0x7e0e0208u;
        bufferColor.insert(bufferColor.begin(), {0xf4200200u, 0xfa000000u, 0xbf8cc07fu});
        alignas(256) const std::array<std::uint32_t, 4> colorBuffer{0x3f800000u, 0u, 0u, 0u};
        const auto colorDescriptor = BufferDescriptor(colorBuffer.data(), 4u, static_cast<std::uint32_t>(colorBuffer.size()));
        const std::array<ShaderRecompiler::MemoryRegion, 1> colorMemory{{{reinterpret_cast<std::uintptr_t>(colorBuffer.data()), std::as_bytes(std::span(colorBuffer))}}};
        Draw(*device, bufferColor, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, colorDescriptor, colorMemory, {}, sizeof(std::uint32_t));
        Check(all, "dynamic reference keeps guest buffer offsets in shader data outside the push constants");
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
        alignas(256) std::array<std::uint32_t, Width + 1u> columnValues{};
        alignas(256) std::array<float, Width> columnTexels{};
        columnValues[0] = 0x2fu;
        for (std::uint32_t x = 0; x < Width; ++x) {
            columnValues[x + 1u] = 0x30u + x;
            columnTexels[x] = 1.0f;
        }
        constexpr std::array<std::uint32_t, 15> bufferImageCode{
            0x7e060f00u, 0x7e080280u, 0xf0001108u, 0x00010503u, 0xe0302000u, 0x80000603u, 0xe0302000u, 0x80030803u,
            0xbf8c3f70u, 0x7e0e02f2u, 0xf8000082u, 0x00000600u, 0xf800180fu, 0x07050805u, 0xbf810000u,
        };
        const auto columnBuffer = BufferDescriptor(columnValues.data() + 1, 4u, Width);
        const auto alignedBuffer = BufferDescriptor(columnValues.data(), 4u, Width + 1u);
        const auto texelAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(columnTexels.data()));
        const std::array<std::uint32_t, 8> columnTexture{static_cast<std::uint32_t>(texelAddress >> 8u), static_cast<std::uint32_t>((texelAddress >> 40u) & 0xffu) | (22u << 20u) | (((Width - 1u) & 3u) << 30u), (Width - 1u) >> 2u, 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
        std::vector<std::uint32_t> bufferImageData(columnBuffer.begin(), columnBuffer.end());
        bufferImageData.insert(bufferImageData.end(), columnTexture.begin(), columnTexture.end());
        bufferImageData.insert(bufferImageData.end(), alignedBuffer.begin(), alignedBuffer.end());
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, bufferImageCode, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, bufferImageData);
        Check(all, "a replayed export beside an image draws the image's texels");
        Draw(*device, PlainPixelCode, plain, equal(0x35u));
        Check([](std::uint32_t x, std::uint32_t) { return x == 5u; }, "a replayed export beside an image reads its guest buffer at a 4-byte offset");
        const auto at = [](const auto& memory) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(memory.data())); };
        const auto depthTexture = [](std::uint64_t address, std::uint32_t height) {
            return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (22u << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
        };
        const auto sampling = [&](std::uint64_t address, std::uint32_t height) {
            std::vector<std::uint32_t> data(columnBuffer.begin(), columnBuffer.end());
            const auto texture = depthTexture(address, height);
            data.insert(data.end(), texture.begin(), texture.end());
            data.insert(data.end(), alignedBuffer.begin(), alignedBuffer.end());
            return data;
        };
        auto farVertices = Vertices;
        for (auto& vertex : farVertices) vertex[2] = 1.0f;
        Draw(*device, PlainPixelCode, plain, replace(0u), 3u, 1u, false, [&](AgcDriver::Graphics::State& state) {
            state.depth = AgcDriver::Graphics::DepthTarget{at(SampledDepthMemory), 0u, {Width, 1u}, VK_FORMAT_D32_SFLOAT, 0.0f, 0};
            state.stencilTest = false;
            state.depthTest = true;
            state.depthWrite = true;
            state.depthCompare = VK_COMPARE_OP_ALWAYS;
            state.renderExtent = {Width, 1u};
            state.viewport = {0, 1.0f, static_cast<float>(Width), -1.0f, 0, 1};
            state.scissor = {{0, 0}, {Width, 1u}};
        }, {}, farVertices);
        Draw(*device, PlainPixelCode, plain, replace(0u));
        Draw(*device, bufferImageCode, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, sampling(at(SampledDepthMemory), 1u));
        Check(all, "a replayed export samples a depth surface it does not render to");
        Draw(*device, PlainPixelCode, plain, equal(0x35u));
        Check([](std::uint32_t x, std::uint32_t) { return x == 5u; }, "a replayed export beside a sampled depth surface replaces each column's own value");
        bool ownDepthRefused = false;
        try { Draw(*device, bufferImageCode, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, sampling(at(DepthMemory), Height)); }
        catch (const std::exception& error) { ownDepthRefused = std::string_view(error.what()).find("feedback") != std::string_view::npos; if (!ownDepthRefused) throw; }
        Require(ownDepthRefused, "a replayed export sampling its own depth target was not refused");
        const auto limits = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).limits;
        std::string multisampleSkip = (limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts & limits.framebufferStencilSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0u ? std::string{} : "the device has no four-sample colour, depth and stencil targets";
        const auto multisampled = [&](bool colorSamples) {
            return [&, colorSamples](AgcDriver::Graphics::State& state) {
                state.samples = 4u;
                state.depth = AgcDriver::Graphics::DepthTarget{at(MultisampleDepthMemory), at(MultisampleStencilMemory), {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
                state.depth->samples = 4u;
                if (!colorSamples) {
                    state.truncatedCoverage = true;
                    return;
                }
                state.color = {at(MultisampleColorMemory), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, MultisampleColorMemory.size(), 0xe4u};
                state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
                state.color.samples = 4u;
                state.colors = {state.color};
            };
        };
        if (multisampleSkip.empty()) {
            try { Draw(*device, PlainPixelCode, plain, replace(0u), 3u, 1u, false, multisampled(false)); }
            catch (const std::exception& error) {
                if (std::string_view(error.what()).find("truncated coverage reduction") == std::string_view::npos) throw;
                multisampleSkip = "the device lacks truncated coverage reduction for a single-sample colour target over four-sample depth";
            }
        }
        if (multisampleSkip.empty()) {
            Draw(*device, ColumnPixelCode, exported, replace(), 3u, 1u, false, multisampled(true));
            Draw(*device, PlainPixelCode, plain, equal(5u), 3u, 1u, false, multisampled(false));
            Check([](std::uint32_t x, std::uint32_t) { return x == 5u; }, "a four-sample unbounded stencil export replaces each column's own value");
            Draw(*device, alternating, exported, replace(), 3u, 1u, false, multisampled(true));
            Draw(*device, PlainPixelCode, plain, equal(255u), 3u, 1u, false, multisampled(false));
            Check(even, "a four-sample bounded stencil export replaces even columns");
        } else {
            std::printf("stencil replay four-sample checks skipped: %s\n", multisampleSkip.c_str());
        }
        {
            constexpr std::size_t PlaneBytes = 65536;
            constexpr std::size_t HeapBytes = 3 * PlaneBytes;
            constexpr std::size_t StencilOffset = PlaneBytes;
            constexpr std::size_t ValueOffset = 2 * PlaneBytes;
#ifdef _WIN32
            auto* heap = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, HeapBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
            auto* heap = static_cast<std::uint8_t*>(std::aligned_alloc(PlaneBytes, HeapBytes));
#endif
            Require(heap != nullptr, "cannot allocate the guest heap holding the depth target");
            std::memset(heap, 0, HeapBytes);
            const std::uint32_t value = 0x2000u;
            std::memcpy(heap + ValueOffset, &value, sizeof(value));
            GuestAllocations::Mutation().Add(heap, HeapBytes, true, true, true);
            const auto heapAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(heap));
            const auto heapDepth = [&](AgcDriver::Graphics::State& state) {
                state.depth = AgcDriver::Graphics::DepthTarget{heapAddress, heapAddress + StencilOffset, {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
            };
            const auto pointer = [](std::uint64_t address) { return std::array<std::uint32_t, 2>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)}; };
            constexpr std::array<std::uint32_t, 14> addressed{
                0x7e080280u, 0xdc308000u, 0x05000004u, 0xbf8c3f70u,
                0x7e040f00u, 0x36060481u,
                0x7c000000u | (0xd2u << 17u) | (3u << 9u) | 0x80u,
                0x34100488u, 0x4a101105u, 0x7e0c02ffu, 0x3f400000u,
                0xf8001883u, (8u << 8u) | 6u,
                0xbf810000u
            };
            alignas(256) const std::array<std::uint32_t, 4> record{3u, 1u, 0u, 0u};
            const auto valuePointer = pointer(heapAddress + ValueOffset);
            Draw(*device, PlainPixelCode, plain, replace(0xeeu), 3u, 1u, false, [&](AgcDriver::Graphics::State& state) {
                heapDepth(state);
                state.depthTest = true;
                state.depthWrite = true;
                state.depthCompare = VK_COMPARE_OP_ALWAYS;
            });
            Draw(*device, addressed, 0x45u, replace(), 0u, 0u, false, [&](AgcDriver::Graphics::State& state) {
                heapDepth(state);
                state.colors.clear();
                state.blends.clear();
                state.hasColorTarget = false;
                state.depthTest = true;
                state.depthWrite = true;
                state.depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL;
            }, [&](AgcDriver::Pm4::DrawParameters& draw) {
                draw.indirect = indirectDraw(record.data(), 0x24u, 16u, 1u);
                draw.indirect->vertexRule = IndirectDraw::Rule::Constant;
                draw.indirect->instanceRule = IndirectDraw::Rule::Constant;
            }, Vertices, VertexCode, valuePointer);
            Draw(*device, PlainPixelCode, plain, equal(0x24u), 3u, 1u, false, heapDepth);
            Check([](std::uint32_t x, std::uint32_t) { return x == 4u; }, "an address-reading fullscreen export over a depth target in a registered heap replaces each kept column's value");
            Draw(*device, PlainPixelCode, plain, equal(0xeeu), 3u, 1u, false, heapDepth);
            Check(odd, "an address-reading fullscreen export keeps the stencil of killed columns");
            auto exportedDepth = Vertices;
            for (auto& vertex : exportedDepth) vertex[2] = 0.75f;
            Draw(*device, PlainPixelCode, plain, replace(0u), 3u, 1u, false, [&](AgcDriver::Graphics::State& state) {
                heapDepth(state);
                state.stencilTest = false;
                state.depthTest = true;
                state.depthCompare = VK_COMPARE_OP_EQUAL;
            }, {}, exportedDepth);
            Check(even, "an address-reading fullscreen export writes its exported depth on kept columns");
            constexpr std::array<std::uint32_t, 10> declared{
                0x7e060f00u, 0xe0302000u, 0x80000603u, 0xbf8c3f70u,
                0x7e0e02f2u, 0xf8000082u, 6u << 8u,
                0xf800180fu, 0x07070707u, 0xbf810000u
            };
            constexpr std::array<std::uint32_t, 14> declaredAndAddressed{
                0x7e080280u, 0xdc308000u, 0x05040004u,
                0x7e060f00u, 0xe0302000u, 0x80000603u, 0xbf8c3f70u,
                0x4a0c0d05u,
                0x7e0e02f2u, 0xf8000082u, 6u << 8u,
                0xf800180fu, 0x07070707u, 0xbf810000u
            };
            auto* const heapVertices = reinterpret_cast<std::array<float, 4>*>(heap + ValueOffset + 0x1000u);
            std::memcpy(heapVertices, Vertices.data(), sizeof(Vertices));
            const std::span<const std::array<float, 4>> vertices(heapVertices, Vertices.size());
            const auto feedbackRefused = [&](const void* memory, bool addressReads, const std::function<void(AgcDriver::Graphics::State&)>& alter) {
                const auto buffer = BufferDescriptor(memory, 4u, Width);
                std::vector<std::uint32_t> data(buffer.begin(), buffer.end());
                if (addressReads) data.insert(data.end(), valuePointer.begin(), valuePointer.end());
                try {
                    if (addressReads) Draw(*device, declaredAndAddressed, exported, replace(), 3u, 1u, false, alter, {}, vertices, VertexCode, data);
                    else Draw(*device, declared, exported, replace(), 3u, 1u, false, alter, {}, vertices, VertexCode, data);
                } catch (const std::exception& error) {
                    if (std::string_view(error.what()).find("depth or stencil feedback reads") == std::string_view::npos) throw;
                    return true;
                }
                return false;
            };
            for (const std::size_t plane : {std::size_t{0}, StencilOffset}) {
                for (const std::size_t offset : {std::size_t{0}, std::size_t{0x2000}}) {
                    for (const bool addressReads : {false, true}) {
                        Require(!feedbackRefused(heap + ValueOffset + 0x100u, addressReads, heapDepth), addressReads ? "an address-reading export with a buffer outside its depth and stencil planes was refused" : "an export with a buffer outside its depth and stencil planes was refused");
                        const auto what = std::string(addressReads ? "an address-reading export" : "an export") + " with a buffer " + (offset == 0u ? "at the base of" : "past the first texels of") + " its own " + (plane == 0u ? "depth" : "stencil") + " plane was not refused";
                        Require(feedbackRefused(heap + plane + offset, addressReads, heapDepth), what);
                    }
                }
            }
            const auto recordedHeapDepth = [&](AgcDriver::Graphics::State& state) {
                heapDepth(state);
                state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
                state.colors = {state.color};
            };
            CachedShaders = true;
            for (const std::size_t plane : {std::size_t{0}, StencilOffset}) {
                Require(!feedbackRefused(heap + ValueOffset + 0x100u, false, recordedHeapDepth), "an export with a cached build over a buffer outside its depth and stencil planes was refused");
                Require(feedbackRefused(heap + plane + 0x2000u, false, recordedHeapDepth), plane == 0u ? "an export whose cached build moved its read-only buffer onto its own depth plane was not refused" : "an export whose cached build moved its read-only buffer onto its own stencil plane was not refused");
            }
            CachedShaders = false;
            if ((limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts & limits.framebufferStencilSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0u) {
                std::puts("stencil replay four-sample feedback checks skipped: the device has no four-sample colour, depth and stencil targets");
            } else {
                Require(!feedbackRefused(heap + ValueOffset + 0x100u, false, multisampled(true)), "a four-sample export with a buffer outside its depth and stencil planes was refused");
                Require(feedbackRefused(MultisampleDepthMemory.data() + 0x8000u, false, multisampled(true)), "a four-sample export with a buffer past the first texels of its own depth plane was not refused");
                Require(feedbackRefused(MultisampleStencilMemory.data() + 0x8000u, false, multisampled(true)), "a four-sample export with a buffer past the first texels of its own stencil plane was not refused");
            }
        }
        if ((limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts & limits.framebufferStencilSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0u) {
            std::puts("stencil replay native depth write checks skipped: the device has no four-sample colour, depth and stencil targets");
        } else {
            const VkExtent2D extent{Width, Height};
            const auto depthBytes = static_cast<std::size_t>(AgcDriver::Graphics::MultisampledDepthBytes_nid_no_patch(extent, 4u, 4u));
            const auto stencilBytes = static_cast<std::size_t>(AgcDriver::Graphics::MultisampledDepthBytes_nid_no_patch(extent, 1u, 4u));
            auto* const nativeDepth = AllocateWatched(depthBytes, 65536u);
            auto* const nativeStencil = AllocateWatched(stencilBytes, 65536u);
            if (nativeDepth == nullptr || nativeStencil == nullptr) {
                std::puts("stencil replay native depth write checks skipped: guest memory is not write-watched");
            } else {
                const auto depthAddress = reinterpret_cast<std::uint64_t>(nativeDepth);
                const auto native = [&](AgcDriver::Graphics::State& state) {
                    multisampled(true)(state);
                    state.depth = AgcDriver::Graphics::DepthTarget{depthAddress, reinterpret_cast<std::uint64_t>(nativeStencil), extent, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
                    state.depth->samples = 4u;
                    state.depth->depthSwizzle = 24u;
                };
                const auto hostWritten = [&] {
                    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
                    AgcDriver::Graphics::AppendHostWrittenDepthRanges(depthAddress, depthBytes, ranges);
                    return !ranges.empty();
                };
                alignas(256) const std::array<std::uint32_t, 8> overflowing{3u, 1u, 0u, 0u, 3u, 2u, 0u, 0xffffffffu};
                bool overflowRefused = false;
                try { Draw(*device, ordered, exported, replace(), 0u, 0u, false, native, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(overflowing.data(), 0x2cu, 16u, 2u); }); } catch (const std::exception& error) { overflowRefused = std::string(error.what()).find("draw indices overflow") != std::string::npos; if (!overflowRefused) throw; }
                Require(overflowRefused, "an indirect record whose instances overflow was not refused on a native multisample depth target");
                if (!AgcDriver::Graphics::ResidentDepthGuardOverlaps_nid_no_patch(depthAddress, depthBytes)) {
                    std::puts("stencil replay native depth write checks skipped: the device does not sample native multisample depth");
                } else {
                    Require(!hostWritten(), "a refused indirect stencil replay marked its native multisample depth written");
                    alignas(256) const std::array<std::uint32_t, 8> empty{0u, 1u, 0u, 0u, 3u, 0u, 0u, 0u};
                    ExpectedBinds = 0u;
                    Draw(*device, ordered, exported, replace(), 0u, 0u, false, native, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(empty.data(), 0x2cu, 16u, 2u); });
                    ExpectedBinds = 1u;
                    Require(!hostWritten(), "indirect records without vertices or instances marked their native multisample depth written");
                    alignas(256) const std::array<std::uint32_t, 4> drawn{3u, 1u, 0u, 0u};
                    Draw(*device, ordered, exported, replace(), 0u, 0u, false, native, [&](AgcDriver::Pm4::DrawParameters& draw) { draw.indirect = indirectDraw(drawn.data(), 0x2cu, 16u, 1u); });
                    Require(hostWritten(), "an indirect stencil replay that draws left its native multisample depth unwritten");
                    std::puts("stencil replay native depth write checks passed");
                }
            }
        }
        std::puts("stencil replay tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
