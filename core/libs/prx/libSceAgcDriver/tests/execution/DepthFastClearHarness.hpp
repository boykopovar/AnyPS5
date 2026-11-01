#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_DEPTHFASTCLEARHARNESS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_DEPTHFASTCLEARHARNESS_HPP

#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace DepthFastClearHarness {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

inline constexpr std::uint32_t Width = 64;
inline constexpr std::uint32_t Height = 32;
inline constexpr std::uint32_t Covered = Width * Height;
inline constexpr std::size_t HtileBytes = ((Width + 1023u) / 1024u) * ((Height + 511u) / 512u) * 32768u;

alignas(256) inline std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) inline constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) inline constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0002f2, 0xf800180f, 0x00000000, 0xbf810000,
};

struct Surface {
    std::uint64_t depth;
    std::uint64_t stencil;
    std::uint64_t htile;
    bool htileStencil;
    VkFormat format;
    std::uint32_t slice = 0;
    std::uint8_t clearStencil = 0;
    std::uint32_t swizzle = 0;
};

struct DrawOptions {
    bool depthTest = true;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;
    bool stencilTest = false;
    VkStencilOpState stencil{};
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

inline std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

inline const std::vector<std::array<float, 4>>& Triangles() {
    static const std::vector<std::array<float, 4>> vertices{
        {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f},
    };
    return vertices;
}

struct Shaders {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t vertexPush = 0;
};

inline Shaders Compile(AgcDriver::VulkanDevice& device, const void* vertices = Triangles().data()) {
    const auto target = device.Target();
    Shaders shaders;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertices, 16u, static_cast<std::uint32_t>(Triangles().size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    shaders.vertex = ShaderRecompiler::Recompile(vertex);
    shaders.vertexPush = static_cast<std::uint32_t>(shaders.vertex.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(shaders.vertexPush, target)
    };
    fragment.useCache = false;
    shaders.pixel = ShaderRecompiler::Recompile(fragment);
    return shaders;
}

inline std::uint32_t Draw(AgcDriver::VulkanDevice& device, const Shaders& compiled, const Surface& surface, const DrawOptions& options, VkExtent2D extent = {Width, Height}, std::span<std::byte> pixels = Pixels) {
    std::fill(pixels.begin(), pixels.end(), std::byte{0});
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), extent, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{surface.depth, surface.stencil, extent, surface.format, 1.0f, surface.clearStencil, surface.htile, surface.htileStencil};
    state.depth->firstSlice = surface.slice;
    state.depth->depthSwizzle = surface.swizzle;
    state.depthTest = options.depthTest;
    state.depthWrite = options.depthWrite;
    state.depthCompare = options.depthCompare;
    state.stencilTest = options.stencilTest;
    state.stencilFront = options.stencil;
    state.stencilBack = options.stencil;
    state.renderExtent = extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), options.minDepth, options.maxDepth};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    std::uint32_t covered = 0;
    for (std::size_t pixel = 0; pixel < pixels.size() / 4u; ++pixel) {
        if (std::to_integer<std::uint8_t>(pixels[pixel * 4u]) == 255u) ++covered;
    }
    return covered;
}

}

#endif
