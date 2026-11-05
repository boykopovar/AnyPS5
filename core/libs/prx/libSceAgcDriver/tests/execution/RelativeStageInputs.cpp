#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Extent = 64u;
alignas(256) std::array<std::byte, Extent * Extent * 4u> pixels{};
alignas(256) std::array<std::uint32_t, 16> output{};
alignas(256) constexpr std::array<std::array<float, 4>, 6> positions{{
    {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<std::uint32_t, 11> relativeVertexCode{
    0xbefc0385u, 0x7e028700u, 0xbefc0388u, 0x7e008700u, 0x4a000300u,
    0xe0382000u, 0x80000000u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u
};
constexpr std::array<std::uint32_t, 6> vertexCode{
    0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u
};
constexpr std::array<std::uint32_t, 5> relativePixelCode{
    0xbefc0383u, 0x7e008700u, 0xf800180fu, 0x00000000u, 0xbf810000u
};
constexpr std::array<std::uint32_t, 4> pixelCode{
    0x7e0002f2u, 0xf800180fu, 0x00000000u, 0xbf810000u
};
constexpr std::array<std::uint32_t, 9> computeCode{
    0xbefc0382u, 0x7e028700u, 0x34020283u, 0x4a000300u, 0x34000082u,
    0x7e028700u, 0xe0701000u, 0x80000100u, 0xbf810000u
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, stride == 0u ? 0x31016facu : 0x01016facu};
}

void Draw(AgcDriver::VulkanDevice& device, bool relativeVertex) {
    pixels.fill(std::byte{0x40});
    const auto target = device.Target();
    const auto userData = BufferDescriptor(positions.data(), 16u, positions.size());
    const auto code = relativeVertex ? std::span<const std::uint32_t>(relativeVertexCode) : std::span<const std::uint32_t>(vertexCode);
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32u, 0u, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, {}},
        target, {0u, 0u, 0u, 64u}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto pushBytes = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    if (!relativeVertex) {
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX)
            | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY)
            | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionZ)
            | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionW);
        pixel.posX = pixel.posY = pixel.posZ = pixel.posW = true;
    }
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    const auto fragmentCode = relativeVertex ? std::span<const std::uint32_t>(pixelCode) : std::span<const std::uint32_t>(relativePixelCode);
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(fragmentCode.data()), fragmentCode, 0, {}},
        {32u, 0u, {}, std::nullopt, pixel, std::nullopt, {}}, target, PixelPushLayout(pushBytes, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0u}, {ShaderStage::Fragment, &pixelResult, PixelPushOffset(pushBytes, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
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
    AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false};
    draw.firstVertex = relativeVertex ? 1u : 3u;
    draw.firstInstance = 2u;
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    for (const auto value : pixels) Require(value == std::byte{255}, "relative stage input draw did not cover every pixel in white");
}

void Dispatch(AgcDriver::VulkanDevice& device) {
    output.fill(0xdeadbeefu);
    const auto userData = BufferDescriptor(output.data(), 0u, sizeof(output));
    const ShaderRecompiler::ShaderComputeStageInfo compute{{8u, 1u, 2u}, 0u, {false, false, false}, false, 3u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(computeCode.data()), computeCode, 0, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, {}}, device.Target(), {0u, 0u, 0u, 128u}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1u, 1u, 1u);
    device.WaitIdle();
    for (std::size_t index = 0; index < output.size(); ++index) {
        Require(output[index] == index / 8u, "relative stage input dispatch did not preserve the implicit Z input at output " + std::to_string(index)
            + ": got " + std::to_string(output[index]) + ", expected " + std::to_string(index / 8u));
    }
}

template<typename TAction>
bool Run(const char* name, TAction action) {
    try {
        action();
        std::cout << "relative stage inputs: " << name << " GPU readback passed\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "relative stage inputs: " << name << ": " << error.what() << '\n';
        return false;
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        bool passed = Run("vertex", [&] { Draw(*device, true); });
        passed &= Run("pixel", [&] { Draw(*device, false); });
        passed &= Run("compute", [&] { Dispatch(*device); });
        if (!passed) return 1;
        std::cout << "relative stage input tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
