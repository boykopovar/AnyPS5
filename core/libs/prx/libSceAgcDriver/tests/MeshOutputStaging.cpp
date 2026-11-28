#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::uint32_t checkStagingInitialized(const ShaderRecompiler::RecompileResult& result, const std::string& what) {
    const auto words = result.spirv.Words();
    std::map<std::uint32_t, std::span<const std::uint32_t>> variables;
    std::map<std::uint32_t, std::uint32_t> pointerBases;
    std::map<std::uint32_t, std::uint32_t> loadedPointers;
    std::map<std::uint32_t, spv::Op> constants;
    std::uint32_t outputStores = 0;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        const auto instruction = std::span<const std::uint32_t>(words).subspan(at, words[at] >> 16u);
        if (op == spv::OpVariable) variables[instruction[2]] = instruction;
        if (op == spv::OpConstantNull || op == spv::OpConstant || op == spv::OpConstantComposite) constants[instruction[2]] = op;
        if (op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain) {
            const auto base = pointerBases.find(instruction[3]);
            pointerBases[instruction[2]] = base != pointerBases.end() ? base->second : instruction[3];
        }
        if (op == spv::OpLoad) loadedPointers[instruction[2]] = instruction[3];
        if (op != spv::OpStore) continue;
        const auto base = pointerBases.find(instruction[1]);
        const auto target = variables.find(base != pointerBases.end() ? base->second : instruction[1]);
        if (target == variables.end() || target->second[3] != spv::StorageClassOutput) continue;
        const auto loaded = loadedPointers.find(instruction[2]);
        if (loaded == loadedPointers.end()) continue;
        const auto sourceBase = pointerBases.find(loaded->second);
        const auto source = variables.find(sourceBase != pointerBases.end() ? sourceBase->second : loaded->second);
        if (source == variables.end() || source->second[3] != spv::StorageClassPrivate) continue;
        if (source->second.size() < 5) throw std::runtime_error(what + ": a mesh output is copied from a Private variable that has no initializer");
        const auto initializer = constants.find(source->second[4]);
        if (initializer == constants.end() || initializer->second != spv::OpConstantNull) throw std::runtime_error(what + ": a mesh output is copied from a Private variable whose initializer is not OpConstantNull");
        ++outputStores;
    }
    if (outputStores == 0) throw std::runtime_error(what + ": no mesh output is copied from a Private variable");
    return outputStores;
}

constexpr std::array<std::uint32_t, 5> PrimitiveAndPositionExports{0xf8000141u, 0u, 0xf80008cfu, 0u, 0xbf810000u};

ShaderRecompiler::RecompileRequest meshRequest(std::span<const std::uint32_t> code, std::uint32_t subgroupSize) {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 1> capabilities{spv::CapabilityMeshShadingEXT};
    static constexpr std::array<std::string_view, 1> extensions{"SPV_EXT_mesh_shader"};
    static constexpr std::array<std::uint32_t, 8> hiddenUserWords{};
    RecompileRequest request{};
    request.shader = {ShaderStage::Mesh, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.vertex = ShaderVertexStageInfo{};
    request.context.userData = hiddenUserWords;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010400u;
    request.target.subgroupSize = subgroupSize;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024u;
    request.target.maxWorkgroupSharedMemoryBytes = 32768u;
    request.target.mesh = MeshTargetLimits{{128u, 1u, 1u}, 128u, 32768u, 256u, 256u, 128u, 32768u, 1u, 1u};
    request.layout.pushConstantSizeBytes = 128;
    request.graphics = GraphicsCompileContext{0u, {}, MeshConfiguration{4u, 1u, 3u, 3u, 1u, 64u, 128u, 0u, 4u}, std::nullopt, {}};
    request.useCache = false;
    return request;
}

} // namespace

int main() {
    try {
        for (const std::uint32_t subgroupSize : {64u, 32u}) {
            const auto what = "mesh shader on a " + std::to_string(subgroupSize) + "-lane subgroup";
            const auto stores = checkStagingInitialized(ShaderRecompiler::Recompile(meshRequest(PrimitiveAndPositionExports, subgroupSize)), what);
            std::cout << what << ": " << stores << " mesh output copies read null-initialized Private staging\n";
        }
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
