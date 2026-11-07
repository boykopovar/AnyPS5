#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t PlainControl = 0x50u;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::byte, 128 * 128 * 4> DepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> StencilMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 3> Vertices{{
    {-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f},
}};

struct Shaders {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t pixelPush = 0;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo() {
    const AgcDriver::Registers context{
        {0x1b3, 0x700u}, {0x1b4, 0x700u}, {0x1b6, 0u}, {0x1c5, 9u}, {0x203, PlainControl},
    };
    return AgcDriver::Graphics::DecodePixelStageInfo(context, {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u});
}

Shaders Compile(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), 16u, static_cast<std::uint32_t>(Vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    Shaders shaders{ShaderRecompiler::Recompile(vertex), {}, 0};
    const auto vertexPush = static_cast<std::uint32_t>(shaders.vertex.pushConstants.size());
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, PixelInfo(), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    shaders.pixel = ShaderRecompiler::Recompile(fragment);
    shaders.pixelPush = PixelPushOffset(vertexPush, target);
    return shaders;
}

AgcDriver::QueueState StencilQueue(std::uint32_t ops, std::uint32_t refMask, std::uint32_t depthControl) {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x1b8, 0}, {0x207, 0}, {0x203, 0x800},
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
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x000, 0}, {0x002, 0}, {0x007, (1u << 16u) | 3u}, {0x00a, 0}, {0x00b, 0}, {0x010, 0x22900983u}, {0x011, 0x20000181u},
        {0x012, 0x100}, {0x014, 0x100}, {0x013, 0x200}, {0x015, 0x200}, {0x005, 0x300}, {0x01e, 0x1},
        {0x10b, ops}, {0x10c, refMask}, {0x10d, refMask}, {0x200, depthControl}
    };
    const auto address = reinterpret_cast<std::uintptr_t>(Pixels.data());
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
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    return queue;
}

void Draw(AgcDriver::VulkanDevice& device, const Shaders& shaders, const VkStencilOpState& stencil) {
    Pixels.fill(Kept);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> stages{{
        {ShaderStage::Vertex, &shaders.vertex, 0},
        {ShaderStage::Fragment, &shaders.pixel, shaders.pixelPush}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), reinterpret_cast<std::uintptr_t>(StencilMemory.data()), {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
    state.depthTest = false;
    state.depthCompare = VK_COMPARE_OP_ALWAYS;
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
    device.Draw(state, AgcDriver::Pm4::DrawParameters{0, 3u, 0, 1u, 0, false}, stages);
    device.WaitIdle();
}

std::uint32_t Expected(std::uint32_t op, std::uint32_t stored, std::uint32_t value, std::uint32_t writeMask) {
    const auto result = op == 10u ? stored & value : op == 11u ? stored | value : op == 12u ? stored ^ value : ~(stored ^ value);
    return ((stored & ~writeMask) | (result & writeMask)) & 0xffu;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = Compile(*device);
        const auto replace = [](std::uint32_t value) { return VkStencilOpState{VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, value}; };
        const auto equal = [](std::uint32_t value) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, value}; };
        std::size_t checked = 0;
        for (const auto stored : {0xa5u, 0x3cu}) {
            for (const auto op : {10u, 11u, 12u, 15u}) {
                for (const auto [writeMask, value] : {std::pair{0xffu, 0x3cu}, std::pair{0x0fu, 0x96u}, std::pair{0xf6u, 0xffu}}) {
                    for (const auto [ops, compare] : {std::pair{op << 4u, 7u}, std::pair{op, 0u}}) {
                        const auto refMask = 0xff00u | (writeMask << 16u) | (value << 24u);
                        const auto face = AgcDriver::Graphics::DecodeState(StencilQueue(ops, refMask, 1u | (compare << 8u))).stencilFront;
                        const auto expected = Expected(op, stored, value, writeMask);
                        const auto name = "stencil operation " + std::to_string(op) + " in DB_STENCIL_CONTROL " + std::to_string(ops) + " with DB_STENCILREFMASK " + std::to_string(refMask) + " on stored value " + std::to_string(stored);
                        Draw(*device, shaders, replace(stored));
                        Draw(*device, shaders, face);
                        Draw(*device, shaders, equal(expected));
                        for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
                            Require(Pixels[pixel * 4u] == std::byte{255}, name + ": pixel " + std::to_string(pixel) + " does not hold the expected stencil value " + std::to_string(expected));
                        }
                        Draw(*device, shaders, equal(expected ^ 0x80u));
                        Require(std::ranges::all_of(std::span(Pixels).first(4), [](std::byte value) { return value == Kept; }), name + ": the stencil value also equals " + std::to_string(expected ^ 0x80u));
                        ++checked;
                    }
                }
            }
        }
        std::printf("stencil logic operation tests passed: %zu states\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
