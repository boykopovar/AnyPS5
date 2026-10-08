#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string_view>
#include <vector>

namespace {

struct Module {
    std::vector<std::uint32_t> capabilities;
    std::vector<std::string_view> extensions;
    bool layerOutput = false;
    std::uint32_t version = 0;
};

Module inspect(const std::vector<std::uint32_t>& words) {
    Module module;
    if (words.size() > 1) module.version = words[1];
    std::vector<std::uint32_t> layerIds;
    for (std::size_t at = 5; at < words.size();) {
        const auto length = words[at] >> 16u;
        const auto opcode = words[at] & 0xffffu;
        if (length == 0 || at + length > words.size()) break;
        if (opcode == 17u && length == 2) module.capabilities.push_back(words[at + 1]);
        if (opcode == 10u && length >= 2) module.extensions.emplace_back(reinterpret_cast<const char*>(&words[at + 1]));
        if (opcode == 71u && length == 4 && words[at + 2] == 11u && words[at + 3] == 9u) module.layerOutput = true;
        at += length;
    }
    return module;
}

bool has(const std::vector<std::uint32_t>& values, std::uint32_t value) {
    for (const auto entry : values) {
        if (entry == value) return true;
    }
    return false;
}

bool named(const std::vector<std::string_view>& values, std::string_view value) {
    for (const auto entry : values) {
        if (entry == value) return true;
    }
    return false;
}

Module recompile(std::uint32_t control, bool layerExtension) {
    using namespace ShaderRecompiler;
    static const std::array<std::uint32_t, 5> code{0xf80000cfu, 0x00000000u, 0xf80008dfu, 0x01010101u, 0xbf810000u};
    static const std::array<std::uint32_t, 2> withExtension{1u, 5254u};
    static const std::array<std::uint32_t, 1> without{1u};
    ShaderVertexStageInfo vertex{};
    vertex.paClVsOutCntl = control;
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.vertex = vertex;
    request.target.vulkanVersion = layerExtension ? 0x00401000u : 0x00402000u;
    request.target.spirvVersion = layerExtension ? 0x00010300u : 0x00010500u;
    request.target.subgroupSize = 64;
    if (layerExtension) request.target.supportedCapabilities = withExtension;
    else request.target.supportedCapabilities = without;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return inspect(Recompile(request).spirv);
}

}

int main() {
    try {
        constexpr std::uint32_t layer = (1u << 18u) | (1u << 21u);
        const auto extension = recompile(layer, true);
        if (!extension.layerOutput || !has(extension.capabilities, 5254u) || has(extension.capabilities, 69u) || !named(extension.extensions, "SPV_EXT_shader_viewport_index_layer") || extension.version != 0x00010300u) {
            std::fprintf(stderr, "a vertex layer export with VK_EXT_shader_viewport_index_layer: layer %d, version 0x%x\n", extension.layerOutput ? 1 : 0, extension.version);
            return 1;
        }
        const auto core = recompile(layer, false);
        if (!core.layerOutput || !has(core.capabilities, 69u) || has(core.capabilities, 5254u) || core.version != 0x00010500u) {
            std::fprintf(stderr, "a vertex layer export without the extension: layer %d, version 0x%x\n", core.layerOutput ? 1 : 0, core.version);
            return 1;
        }
        const auto ignored = recompile(0u, true);
        if (ignored.layerOutput || has(ignored.capabilities, 5254u)) {
            std::fprintf(stderr, "a misc export without USE_VTX_RENDER_TARGET_INDX wrote gl_Layer\n");
            return 1;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "unexpected error: %s\n", error.what());
        return 1;
    }
    return 0;
}
