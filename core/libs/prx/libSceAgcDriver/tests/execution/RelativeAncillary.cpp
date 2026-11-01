#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
constexpr std::uint32_t Extent = 64u;
alignas(256) std::array<std::byte, Extent * Extent * 4u> pixels{};
alignas(256) constexpr std::array<std::array<float, 4>, 6> positions{{
    {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};
constexpr std::array<std::uint32_t, 6> vertexCode{
    0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> fragmentCode, std::uint32_t offset, std::uint32_t waveSize, std::byte expected, bool reject = false, bool implicitInputs = false) {
    pixels.fill(std::byte{0x40});
    const auto target = device.Target();
    const auto userData = BufferDescriptor(positions.data(), 16u, positions.size());
    const auto code = std::span<const std::uint32_t>(vertexCode);
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32u, 0u, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, {}},
        target, {0u, 0u, 0u, 64u}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto pushBytes = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
    pixel.ancillary = true;
    if (implicitInputs) {
        pixel.inputAddr |= ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionZ) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionW);
        pixel.posX = pixel.posY = pixel.posZ = pixel.posW = true;
    }
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    const std::array<std::uint32_t, 1> pixelData{offset};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(fragmentCode.data()), fragmentCode, 0, {}},
        {waveSize, 0u, pixelData, std::nullopt, pixel, std::nullopt, {}}, target, PixelPushLayout(pushBytes, target)
    };
    fragment.useCache = false;
    const auto pixelResult = [&] {
        try {
            auto result = ShaderRecompiler::Recompile(fragment);
            Require(!reject, "an unsupported ancillary word was accepted");
            return result;
        } catch (const std::exception& error) {
            if (!reject || std::string(error.what()).find("packed pixel ancillary input has an unsupported live use") == std::string::npos) throw;
            return ShaderRecompiler::RecompileResult{};
        }
    }();
    if (reject) return;
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
    draw.firstVertex = 3u;
    draw.firstInstance = 2u;
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    for (const auto value : pixels) Require(value == expected, "relative ancillary draw produced an incorrect pixel: " + std::to_string(std::to_integer<unsigned>(value)));
}

} // namespace

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto waveSize : {32u, 64u}) {
            const std::array<std::uint32_t, 8> read{
                0xbefc0300u, 0x7e028700u, 0xd5480001u, 0x02111101u,
                0x7e020d01u, 0x060202f2u, 0xf800180fu, 0x01010101u
            };
            auto code = std::vector<std::uint32_t>(read.begin(), read.end());
            code.push_back(0xbf810000u);
            std::cerr << "wave " << waveSize << " packed read\n";
            Draw(*device, code, 0u, waveSize, std::byte{255});
            Draw(*device, code, 4u, waveSize, std::byte{255}, false, true);
            const std::array<std::uint32_t, 8> prefix{
                0xd5480000u, 0x02111100u, 0x7e000d00u, 0x7e020280u,
                0x7e0402f2u, 0xbefc0300u, 0x7e008502u, 0x7e008702u
            };
            code.assign(prefix.begin(), prefix.end());
            code.insert(code.end(), {0xf800180fu, 0x01010101u, 0xbf810000u});
            std::cerr << "in-range\n";
            Draw(*device, code, 1u, waveSize, std::byte{255});
            std::cerr << "out-of-range\n";
            Draw(*device, code, 8u, waveSize, std::byte{0});
            code[6] = 0x7e02d100u;
            code.erase(code.begin() + 7);
            std::cerr << "in-range\n";
            Draw(*device, code, 0x00010001u, waveSize, std::byte{255});
            std::cerr << "out-of-range\n";
            Draw(*device, code, 0x00010008u, waveSize, std::byte{0});
            code.assign(prefix.begin(), prefix.begin() + 6);
            code.insert(code.end(), {0xbefe0480u, 0x7e008502u, 0xbefe04c1u, 0xf800180fu, 0x01010101u, 0xbf810000u});
            std::cerr << "inactive write\n";
            Draw(*device, code, 1u, waveSize, std::byte{0});
            for (const auto modifier : {0x0000e402u, 0xff00e402u}) {
                code.assign(prefix.begin(), prefix.begin() + 6);
                code.insert(code.end(), {0x7e0084fau, modifier, 0xf800180fu, 0x01010101u, 0xbf810000u});
                std::cerr << "DPP write " << modifier << '\n';
                Draw(*device, code, 1u, waveSize, modifier == 0xff00e402u ? std::byte{255} : std::byte{0});
                Draw(*device, code, 8u, waveSize, std::byte{0});
            }
            const std::array<std::uint32_t, 6> whole{0xbefc0300u, 0x7e028700u, 0x7e020d01u, 0xf800180fu, 0x01010101u, 0xbf810000u};
            std::cerr << "whole word\n";
            Draw(*device, whole, 0u, waveSize, std::byte{255});
            Draw(*device, whole, 4u, waveSize, std::byte{255}, false, true);
        }
        std::cout << "relative ancillary GPU readback tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
