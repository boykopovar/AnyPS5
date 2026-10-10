#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void requireInvariantPositions(const ShaderRecompiler::RecompileResult& result, const char* what) {
    constexpr std::uint32_t Variable = ~0u;
    const auto& words = result.spirv.Words();
    std::set<std::pair<std::uint32_t, std::uint32_t>> positions;
    std::set<std::pair<std::uint32_t, std::uint32_t>> invariant;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        const auto count = words[at] >> 16u;
        if (op == spv::OpMemberDecorate && count >= 4) {
            if (words[at + 3] == spv::DecorationBuiltIn && count == 5 && words[at + 4] == spv::BuiltInPosition) positions.emplace(words[at + 1], words[at + 2]);
            if (words[at + 3] == spv::DecorationInvariant) invariant.emplace(words[at + 1], words[at + 2]);
        }
        if (op == spv::OpDecorate && count >= 3) {
            if (words[at + 2] == spv::DecorationBuiltIn && count == 4 && words[at + 3] == spv::BuiltInPosition) positions.emplace(words[at + 1], Variable);
            if (words[at + 2] == spv::DecorationInvariant) invariant.emplace(words[at + 1], Variable);
        }
    }
    if (positions.empty()) throw std::runtime_error(std::string(what) + ": the module declares no position output");
    for (const auto& position : positions) {
        if (!invariant.contains(position)) throw std::runtime_error(std::string(what) + ": the position output is not Invariant");
    }
}

constexpr std::array<std::uint32_t, 3> PositionExport{0xf80008cfu, 0u, 0xbf810000u};

ShaderRecompiler::RecompileRequest baseRequest(ShaderRecompiler::ShaderStage stage) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {stage, 0x10000u, PositionExport, 0, {}};
    request.context.waveSize = 64;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

void verifyVertex() {
    using namespace ShaderRecompiler;
    requireInvariantPositions(Recompile(baseRequest(ShaderStage::Vertex)), "vertex shader");
}

void verifyMesh() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 1> capabilities{spv::CapabilityMeshShadingEXT};
    static constexpr std::array<std::string_view, 1> extensions{"SPV_EXT_mesh_shader"};
    static constexpr std::array<std::uint32_t, 8> hiddenUserWords{};
    auto request = baseRequest(ShaderStage::Mesh);
    request.context.userData = hiddenUserWords;
    request.target.spirvVersion = 0x00010400u;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024u;
    request.target.maxWorkgroupSharedMemoryBytes = 32768u;
    request.target.mesh = MeshTargetLimits{{128u, 1u, 1u}, 128u, 32768u, 256u, 256u, 128u, 32768u, 1u, 1u};
    request.graphics = GraphicsCompileContext{0u, {}, MeshConfiguration{4u, 1u, 3u, 3u, 1u, 64u, 128u, 0u, 4u}, std::nullopt, {}};
    requireInvariantPositions(Recompile(request), "mesh shader");
}

} // namespace

int main() {
    try {
        verifyVertex();
        verifyMesh();
        std::cout << "Vertex and mesh position outputs are Invariant\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
