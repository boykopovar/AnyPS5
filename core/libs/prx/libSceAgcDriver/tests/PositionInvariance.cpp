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

constexpr std::array<std::uint32_t, 1> VertexCapabilities{spv::CapabilityClipDistance};

ShaderRecompiler::RecompileRequest baseRequest(ShaderRecompiler::ShaderStage stage) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {stage, 0x10000u, PositionExport, 0, {}};
    request.context.waveSize = 64;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.supportedCapabilities = VertexCapabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

void verifyVertex() {
    using namespace ShaderRecompiler;
    requireInvariantPositions(Recompile(baseRequest(ShaderStage::Vertex)), "vertex shader");
}

std::uint32_t clipDistanceLength(const ShaderRecompiler::RecompileResult& result) {
    const auto& words = result.spirv.Words();
    std::uint32_t variable = 0;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto count = words[at] >> 16u;
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpDecorate && count == 4 && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == spv::BuiltInClipDistance) variable = words[at + 1];
    }
    if (variable == 0) return 0;
    std::uint32_t pointer = 0;
    std::uint32_t array = 0;
    std::uint32_t length = 0;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpVariable && words[at + 2] == variable) pointer = words[at + 1];
    }
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpTypePointer && words[at + 1] == pointer) array = words[at + 3];
    }
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpTypeArray && words[at + 1] == array) length = words[at + 3];
    }
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpConstant && words[at + 2] == length) return words[at + 3];
    }
    throw std::runtime_error("the clip distance array has no constant length");
}

constexpr std::array<std::uint32_t, 7> DistanceExports{0xf80000dfu, 0u, 0xf80000efu, 0u, 0xf80008cfu, 0u, 0xbf810000u};

void verifyGuestClipDistances() {
    using namespace ShaderRecompiler;
    for (const auto [enabled, expected] : {std::pair{0x007fu, 8u}, std::pair{0x00ffu, 8u}, std::pair{0xf007u, 4u}, std::pair{0xf00fu, 4u}}) {
        auto request = baseRequest(ShaderStage::Vertex);
        request.shader.code = DistanceExports;
        request.context.vertex->paClVsOutCntl = 0x00c00000u | enabled;
        const auto length = clipDistanceLength(Recompile(request));
        if (length != expected) throw std::runtime_error("clip and cull distance enables " + std::to_string(enabled) + " gave a clip distance array of " + std::to_string(length));
    }
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
        verifyGuestClipDistances();
        verifyMesh();
        std::cout << "Vertex and mesh position outputs are Invariant\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
