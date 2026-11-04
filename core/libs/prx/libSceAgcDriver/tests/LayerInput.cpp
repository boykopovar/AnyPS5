#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

constexpr std::array<std::uint32_t, 9> FragmentCode{
    0xbefc0300u, 0x7e028700u, 0xd5480001u, 0x02111101u,
    0x7e020d01u, 0x060202f2u, 0xf800180fu, 0x01010101u, 0xbf810000u
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> Compile(std::span<const std::uint32_t> capabilities) {
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, FragmentCode, 0, {}};
    request.context.waveSize = 32;
    const std::array<std::uint32_t, 1> data{0u};
    request.context.userData = data;
    ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = PixelInputBit(PixelInput::Ancillary);
    pixel.ancillary = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.supportedCapabilities = capabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = Recompile(request);
    const auto words = result.spirv.Words();
    return {words.begin(), words.end()};
}

bool Declares(std::span<const std::uint32_t> words, spv::Capability capability) {
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if (count == 0 || count > words.size() - cursor) return false;
        if ((words[cursor] & 0xffffu) == spv::OpCapability && words[cursor + 1] == static_cast<std::uint32_t>(capability)) return true;
        cursor += count;
    }
    return false;
}

std::string Extensions(std::span<const std::uint32_t> words) {
    std::string text;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if (count == 0 || count > words.size() - cursor) break;
        if ((words[cursor] & 0xffffu) == spv::OpExtension) {
            text.append(reinterpret_cast<const char*>(&words[cursor + 1]), (count - 1) * 4u);
        }
        cursor += count;
    }
    return text;
}

}

int main() {
    try {
        const std::array<std::uint32_t, 2> withGeometry{spv::CapabilityShader, spv::CapabilityGeometry};
        const std::array<std::uint32_t, 2> withoutGeometry{spv::CapabilityShader, spv::CapabilityShaderViewportIndexLayerEXT};
        const auto geometry = Compile(withGeometry);
        Require(Declares(geometry, spv::CapabilityGeometry), "a Layer input on a device with geometry shaders must declare Geometry");
        Require(!Declares(geometry, spv::CapabilityShaderViewportIndexLayerEXT), "a Layer input on a device with geometry shaders must not declare the viewport index layer capability");
        const auto extension = Compile(withoutGeometry);
        Require(Declares(extension, spv::CapabilityShaderViewportIndexLayerEXT), "a Layer input without geometry shaders must declare the viewport index layer capability");
        Require(!Declares(extension, spv::CapabilityGeometry), "a Layer input without geometry shaders must not declare Geometry");
        Require(Extensions(extension).find("SPV_EXT_shader_viewport_index_layer") != std::string::npos, "a Layer input without geometry shaders must declare SPV_EXT_shader_viewport_index_layer");
        std::printf("layer input capability tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
