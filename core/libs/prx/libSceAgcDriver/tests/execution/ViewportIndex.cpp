#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Strip = Width / AgcDriver::Graphics::MaxViewports;
constexpr std::uint8_t Kept = 0x40;
constexpr std::uint8_t Drawn = 255;
constexpr std::uint32_t ViewportExport = (1u << 19u) | (1u << 21u);
constexpr std::array<std::uint32_t, 5> DrawnViewports{1u, 0u, 2u, 7u, 15u};
constexpr std::uint32_t MultiViewportCapability = 57;
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;

alignas(256) constexpr std::array<std::uint32_t, 106> GeometryCode{
    0x8f6a9003, 0x94fe6ac1, 0xbf88000b, 0xd7650006, 0x000100c1, 0xd7660006, 0x00020cc1, 0x93ebff03,
    0x00040018, 0xd7460006, 0x04190c6b, 0x340c0c82, 0xd8340000, 0x00000506, 0xbf8cc07f, 0xbefe04c1,
    0xbf8a0000, 0x938dff02, 0x00090016, 0x9382ff03, 0x00040018, 0x938cff03, 0x00080008, 0xd7650009,
    0x000100c1, 0xd7660009, 0x000212c1, 0xd746000a, 0x04250c02, 0x7da8120c, 0xbf88002e, 0x361600ff,
    0x0000ffff, 0x2c180090, 0x361a02ff, 0x0000ffff, 0xd8d80000, 0x0b00000b, 0xd8d80000, 0x0c00000c,
    0xd8d80000, 0x0d00000d, 0xbf8cc07f, 0xe0382000, 0x8002100b, 0xe0382010, 0x8002140b, 0xe0382000,
    0x8002180c, 0xe0382010, 0x80021c0c, 0xe0382000, 0x8002200d, 0xe0382010, 0x8002240d, 0x161c1483,
    0x161e14ff, 0x00000060, 0xbf8c3f70, 0xdb7c0400, 0x0000100f, 0xdb7c0410, 0x0000140f, 0xdb7c0420,
    0x0000180f, 0xdb7c0430, 0x00001c0f, 0xdb7c0440, 0x0000200f, 0xdb7c0450, 0x0000240f, 0x4a501c81,
    0x4a521c82, 0x3450508a, 0x34525294, 0xd772002a, 0x04a6510e, 0xbf8cc07f, 0xbefe04c1, 0xbf8a0000,
    0x930e830d, 0xbf078002, 0xbf850003, 0x8f0f8c0d, 0x887c0f0e, 0xbf900009, 0x7da8140d, 0xbf880002,
    0xf8000941, 0x0000002a, 0xbefe04c1, 0x7da8140e, 0xbf88000c, 0x34561485, 0xdbfc0400, 0x2c00002b,
    0xdbfc0410, 0x3000002b, 0xbf8cc07f, 0xf80000cf, 0x2f2e2d2c, 0xf80008d4, 0x00330000, 0xf800020f,
    0x33323130, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80000005, 0xe030201c, 0x80000405, 0xbf8c3f70, 0xf80000cf, 0x03020100, 0xf80008d4, 0x00040000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) std::array<std::byte, 1024> ColorMemory{};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t misc;
};

alignas(256) std::array<Vertex, 6> Vertices{};

template<typename TAction>
void ExpectFailure(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected failure: ") + error.what());
        return;
    }
    throw std::runtime_error("expected a rejection: " + std::string(reason));
}

AgcDriver::QueueState DecodableQueue() {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x1b8, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040}
    };
    const auto address = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    queue.context[0x318] = static_cast<std::uint32_t>(address >> 8u);
    queue.context[0x390] = static_cast<std::uint32_t>(address >> 40u);
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x110] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x111] = std::bit_cast<std::uint32_t>(-2.0f);
    queue.context[0x112] = std::bit_cast<std::uint32_t>(2.0f);
    queue.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x114] = 0;
    queue.context[0xb4] = 0;
    queue.context[0xb5] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x3] = 0;
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    return queue;
}

void SetViewport(AgcDriver::QueueState& queue, std::uint32_t index, float xScale, float xOffset, float yScale, float yOffset, float zScale, float zOffset) {
    const auto base = 0x10fu + 6u * index;
    const std::array<float, 6> values{xScale, xOffset, yScale, yOffset, zScale, zOffset};
    for (std::uint32_t i = 0; i < values.size(); ++i) queue.context[base + i] = std::bit_cast<std::uint32_t>(values[i]);
}

bool Same(const VkViewport& a, const VkViewport& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height && a.minDepth == b.minDepth && a.maxDepth == b.maxDepth;
}

bool Same(const VkRect2D& a, const VkRect2D& b) {
    return a.offset.x == b.offset.x && a.offset.y == b.offset.y && a.extent.width == b.extent.width && a.extent.height == b.extent.height;
}

void DecodeTests() {
    auto single = DecodableQueue();
    const auto baseline = AgcDriver::Graphics::DecodeState(single);
    Require(baseline.viewportExports == 0u && baseline.extraViewports.empty() && baseline.viewportScissors.empty(), "a draw without a viewport index export decoded extra viewports");

    auto queue = DecodableQueue();
    queue.context[0x207] = ViewportExport;
    SetViewport(queue, 1, 8.0f, 48.0f, 1.0f, 2.0f, 0.5f, 0.5f);
    queue.context[0x96] = (1u << 16u) | 40u;
    queue.context[0x97] = (3u << 16u) | 60u;
    SetViewport(queue, 5, 4.0f, 10.0f, 2.0f, 6.0f, 0.25f, 0.5f);
    queue.context[0x9e] = (1u << 16u) | 8u;
    queue.context[0x9f] = (3u << 16u) | 12u;
    SetViewport(queue, 15, 2.0f, 30.0f, 3.0f, 20.0f, 0.5f, 0.25f);
    queue.context[0xb2] = 26u;
    queue.context[0xb3] = (3u << 16u) | 33u;
    const auto state = AgcDriver::Graphics::DecodeState(queue);
    Require(state.viewportExports == (1u << 19u), "the viewport index export bit was not kept");
    Require(state.extraViewports.size() == AgcDriver::Graphics::MaxViewports - 1u && state.viewportScissors.size() == AgcDriver::Graphics::MaxViewports, "a viewport index export did not decode all sixteen viewports");
    Require(AgcDriver::Graphics::ViewportCount(state) == AgcDriver::Graphics::MaxViewports, "the viewport count is not sixteen");
    Require(Same(state.viewport, baseline.viewport) && Same(state.viewportScissors[0], baseline.scissor), "viewport 0 changed with a viewport index export");
    Require(Same(state.extraViewports[0], VkViewport{40.0f, 1.0f, 16.0f, 2.0f, 0.5f, 1.0f}), "viewport 1 does not follow its PA_CL_VPORT registers");
    Require(Same(state.viewportScissors[1], VkRect2D{{40, 1}, {20u, 2u}}), "viewport 1 does not use its own PA_SC_VPORT_SCISSOR");
    Require(Same(state.extraViewports[4], VkViewport{6.0f, 4.0f, 8.0f, 4.0f, 0.5f, 0.75f}), "viewport 5 does not follow its PA_CL_VPORT registers");
    Require(Same(state.viewportScissors[5], VkRect2D{{8, 1}, {4u, 2u}}), "viewport 5 does not use its own PA_SC_VPORT_SCISSOR");
    Require(Same(state.extraViewports[14], VkViewport{28.0f, 17.0f, 4.0f, 6.0f, 0.25f, 0.75f}), "viewport 15 does not follow its PA_CL_VPORT registers");
    Require(Same(state.viewportScissors[15], VkRect2D{{26, 0}, {7u, 3u}}), "viewport 15 does not use its own PA_SC_VPORT_SCISSOR");
    for (std::uint32_t index = 2; index < AgcDriver::Graphics::MaxViewports; ++index) {
        if (index == 5u || index == 15u) continue;
        Require(state.viewportScissors[index].extent.width == 0u || state.viewportScissors[index].extent.height == 0u, "viewport " + std::to_string(index) + " with zero scale is not empty");
    }
    std::int64_t left = baseline.scissor.offset.x;
    std::int64_t top = baseline.scissor.offset.y;
    std::int64_t right = left + baseline.scissor.extent.width;
    std::int64_t bottom = top + baseline.scissor.extent.height;
    for (const VkRect2D& rect : {VkRect2D{{40, 1}, {20u, 2u}}, VkRect2D{{8, 1}, {4u, 2u}}, VkRect2D{{26, 0}, {7u, 3u}}}) {
        left = std::min<std::int64_t>(left, rect.offset.x);
        top = std::min<std::int64_t>(top, rect.offset.y);
        right = std::max<std::int64_t>(right, rect.offset.x + rect.extent.width);
        bottom = std::max<std::int64_t>(bottom, rect.offset.y + rect.extent.height);
    }
    const VkRect2D bounds{{static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)}, {static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top)}};
    Require(Same(state.scissor, bounds), "the draw's scissor is not the union of its viewports' scissors");

    AgcDriver::Graphics::GraphicsPipelineRecipe recipe;
    recipe.state = AgcDriver::Graphics::CanonicalPipelineState(state);
    Require(AgcDriver::Graphics::ViewportCount(recipe.state) == AgcDriver::Graphics::MaxViewports, "the canonical pipeline state dropped the viewport count");
    Require(AgcDriver::Graphics::ViewportCount(AgcDriver::Graphics::CanonicalPipelineState(baseline)) == 1u, "the canonical pipeline state of a single-viewport draw has extra viewports");
    auto bytes = AgcDriver::Graphics::EncodeGraphicsPipelineRecipe(recipe);
    AgcDriver::Graphics::GraphicsPipelineRecipe decoded;
    Require(AgcDriver::Graphics::DecodeGraphicsPipelineRecipe(bytes, decoded) && AgcDriver::Graphics::ViewportCount(decoded.state) == AgcDriver::Graphics::MaxViewports, "the journal recipe dropped the viewport count");
    Require(AgcDriver::Graphics::EncodeGraphicsPipelineRecipe(decoded) == bytes, "the journal recipe with sixteen viewports does not round-trip byte for byte");
    Require(bytes.size() >= 4u, "the journal recipe is too short");
    const auto tooMany = AgcDriver::Graphics::MaxViewports;
    std::memcpy(bytes.data() + bytes.size() - 4u, &tooMany, 4u);
    Require(!AgcDriver::Graphics::DecodeGraphicsPipelineRecipe(bytes, decoded), "a journal recipe with seventeen viewports decoded");

    AgcDriver::Graphics::Context unsupported{};
    auto stripped = state;
    AgcDriver::Graphics::ApplyViewportIndex(unsupported, stripped);
    Require(stripped.viewportExports == 0u && stripped.extraViewports.empty() && stripped.viewportScissors.empty() && Same(stripped.scissor, baseline.scissor), "a device without multiViewport kept the extra viewports");
    AgcDriver::Graphics::Context supported{};
    supported.multiViewport = true;
    auto kept = state;
    AgcDriver::Graphics::ApplyViewportIndex(supported, kept);
    Require(kept.viewportExports == state.viewportExports && kept.extraViewports.size() == state.extraViewports.size() && Same(kept.scissor, state.scissor), "a multiViewport device dropped the extra viewports");
    auto partial = state;
    partial.extraViewports.pop_back();
    partial.viewportScissors.pop_back();
    ExpectFailure([&] { AgcDriver::Graphics::ApplyViewportIndex(supported, partial); }, "all 16 viewports");
    auto tessellated = state;
    tessellated.stages.path = AgcDriver::Graphics::ShaderPath::Tessellation;
    AgcDriver::Graphics::ApplyViewportIndex(supported, tessellated);
    Require(tessellated.extraViewports.empty() && Same(tessellated.scissor, baseline.scissor), "a tessellation draw kept the extra viewports");

    auto reserved = queue;
    reserved.context[0x96] |= 0x8000u;
    const auto reservedState = AgcDriver::Graphics::DecodeState(reserved);
    Require(reservedState.viewportScissors[1].extent.width == 0u && Same(reservedState.extraViewports[0], VkViewport{0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}), "a viewport scissor with reserved bits is not empty");

    auto remapped = queue;
    remapped.context[0x204] = 0;
    remapped.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    remapped.context[0x114] = 0;
    AgcDriver::Graphics::Context remapping{};
    remapping.depthClamp = true;
    auto remappedState = AgcDriver::Graphics::DecodeState(remapped);
    ExpectFailure([&] { AgcDriver::Graphics::ApplyDepthRemap(remapping, remappedState); }, "single viewport");
}

void BuildQuad(std::uint32_t viewport) {
    const std::array<std::array<float, 2>, 6> corners{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
    for (std::uint32_t k = 0; k < corners.size(); ++k) Vertices[k] = {{corners[k][0], corners[k][1], 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f}, viewport << 16u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

AgcDriver::Graphics::ColorTarget Target(std::byte* memory) {
    AgcDriver::Graphics::ColorTarget color{reinterpret_cast<std::uintptr_t>(memory), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, 0, 0xe4u};
    color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    color.bytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::SurfaceForTarget(color)).guestBytes;
    return color;
}

class GuestSurface {
public:
    explicit GuestSurface(std::size_t bytes) {
#ifdef _WIN32
        block = static_cast<std::byte*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::byte*>(std::aligned_alloc(65536, bytes));
#endif
        Require(block != nullptr, "cannot allocate the color target");
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
        std::memset(block, Kept, bytes);
    }

    ~GuestSurface() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;
    std::byte* Data() const { return block; }

private:
    std::byte* block = nullptr;
};

VkRect2D StripScissor(std::uint32_t viewport) {
    return {{static_cast<std::int32_t>(viewport * Strip), 0}, {Strip, viewport + 1u}};
}

void SetStrips(AgcDriver::Graphics::State& state) {
    state.viewportExports = 1u << 19u;
    std::array<VkViewport, AgcDriver::Graphics::MaxViewports> viewports{};
    state.viewportScissors.clear();
    for (std::uint32_t index = 0; index < AgcDriver::Graphics::MaxViewports; ++index) {
        viewports[index] = {static_cast<float>(index * Strip), static_cast<float>(Height), static_cast<float>(Strip), -static_cast<float>(Height), 0, 1};
        state.viewportScissors.push_back(StripScissor(index));
    }
    state.viewport = viewports[0];
    state.extraViewports.assign(viewports.begin() + 1, viewports.end());
    state.scissor = {{0, 0}, {Width, Height}};
}

void CommonState(AgcDriver::Graphics::State& state, const AgcDriver::Graphics::ColorTarget& color) {
    state.color = color;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.negativeOneToOne = false;
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    SetStrips(state);
}

ShaderRecompiler::RecompileResult Fragment(const ShaderRecompiler::SpirvTarget& target, std::uint32_t mainPush) {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(mainPush, target)
    };
    fragment.useCache = false;
    return ShaderRecompiler::Recompile(fragment);
}

void DrawVertexPath(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color) {
    const auto target = device.Target();
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(Vertices.size()));
    std::vector<std::uint32_t> userData(4, 0u);
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), userData.begin());
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = ViewportExport;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const auto pixelResult = Fragment(target, vertexPush);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64, 64, std::nullopt, std::nullopt};
    CommonState(state, color);
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushAllPending("viewport index vertex test");
    device.WaitIdle();
}

void DrawMeshPath(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color) {
    const auto target = device.Target();
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(Vertices.size()));
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = ViewportExport;
    std::vector<std::uint32_t> userData(12u, 0u);
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), userData.begin() + 8);
    const ShaderRecompiler::MeshConfiguration mesh{4u, 4u, 12u, 12u, 4u, 64u, 1024u, 0u, 4u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> geometryMemory{{{reinterpret_cast<std::uintptr_t>(GeometryCode.data()), std::as_bytes(std::span(GeometryCode))}}};
    ShaderRecompiler::RecompileRequest geometry{
        {ShaderStage::Mesh, reinterpret_cast<std::uintptr_t>(GeometryCode.data()), GeometryCode, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, vertexInfo, geometryMemory},
        target,
        {0, 0, 0, ShaderRecompiler::MeshDrawPushOffsetBytes}
    };
    geometry.graphics = ShaderRecompiler::GraphicsCompileContext{0, {}, mesh, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}};
    geometry.useCache = false;
    const auto geometryResult = ShaderRecompiler::Recompile(geometry);
    const auto geometryPush = static_cast<std::uint32_t>(geometryResult.pushConstants.size());
    const auto pixelResult = Fragment(target, geometryPush);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Mesh, &geometryResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(geometryPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Geometry, 0x20u, 64, 64, mesh, std::nullopt};
    state.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
    CommonState(state, color);
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushAllPending("viewport index mesh test");
    device.WaitIdle();
}

std::uint64_t BlockOffset(const AgcDriver::Graphics::TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < equation.bits.size(); ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12u) & (mask & 0xfff000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

void Check(const std::byte* memory, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t viewport, const std::string& what) {
    using namespace AgcDriver::Graphics;
    const auto geometry = DescribeSurface(SurfaceForTarget(color));
    const auto thin = ThinBlockLayout(ColorTextureTileMode(color.tileMode), 4);
    const auto& mip = geometry.mips.at(0);
    const auto* equation = FindTextureSwizzleEquation(static_cast<std::uint32_t>(color.tileMode), 4);
    Require(equation != nullptr, "missing the target's swizzle equation");
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto* texel = memory + (static_cast<std::uint64_t>(y / thin[2]) * mip.blocksPerRow + x / thin[1]) * thin[0] + BlockOffset(*equation, x, y);
            const auto scissor = StripScissor(viewport);
            const bool inside = x >= static_cast<std::uint32_t>(scissor.offset.x) && x < scissor.offset.x + scissor.extent.width && y < scissor.extent.height;
            const auto expected = inside ? Drawn : Kept;
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const auto actual = std::to_integer<std::uint8_t>(texel[channel]);
                Require(actual == expected, what + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected) + (inside ? " (viewport " + std::to_string(viewport) + ")" : " (outside the selected viewport)"));
            }
        }
    }
}

}

int main() {
    try {
        DecodeTests();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        const auto supported = [&](std::uint32_t capability) { return std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), capability) != target.supportedCapabilities.end(); };
        if (!supported(MultiViewportCapability) || !supported(ShaderViewportIndexLayerCapability)) {
            std::puts("viewport index decode tests passed; draws skipped, the device has no multiViewport or VK_EXT_shader_viewport_index_layer");
            return VulkanTestSkipped;
        }
        const auto bytes = (Target(nullptr).bytes + 65535u) & ~std::size_t{65535u};
        for (const std::uint32_t viewport : DrawnViewports) {
            BuildQuad(viewport);
            const GuestSurface memory(bytes);
            const auto color = Target(memory.Data());
            DrawVertexPath(*device, color);
            Check(memory.Data(), color, viewport, "vertex path, every vertex in viewport " + std::to_string(viewport));
        }
        if (!target.mesh.has_value() || device->DeviceName().starts_with("llvmpipe")) {
            std::puts("viewport index vertex tests passed; mesh draws skipped, the device has no VK_EXT_mesh_shader compiler for these programs");
            return 0;
        }
        for (const std::uint32_t viewport : DrawnViewports) {
            BuildQuad(viewport);
            const GuestSurface memory(bytes);
            const auto color = Target(memory.Data());
            DrawMeshPath(*device, color);
            Check(memory.Data(), color, viewport, "mesh path, every vertex in viewport " + std::to_string(viewport));
        }
        std::puts("viewport index tests passed: the vertex and mesh paths draw into the viewport their vertices select");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
