#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Instruction {
    spv::Op op;
    std::span<const std::uint32_t> words;
};

struct Block {
    std::uint32_t label = 0;
    std::vector<Instruction> body;
};

std::vector<Block> functionBlocks(std::span<const std::uint32_t> words, std::map<std::uint32_t, Instruction>& definitions, std::uint32_t& cullVariable) {
    std::vector<Block> blocks;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        const auto instruction = words.subspan(at, words[at] >> 16u);
        if (op == spv::OpDecorate && instruction.size() == 4 && instruction[2] == spv::DecorationBuiltIn && instruction[3] == spv::BuiltInCullPrimitiveEXT) cullVariable = instruction[1];
        if (op == spv::OpConstant || op == spv::OpConstantTrue || op == spv::OpConstantFalse || op == spv::OpINotEqual || op == spv::OpLogicalOr || op == spv::OpBitwiseAnd) definitions[instruction[2]] = {op, instruction};
        if (op == spv::OpLabel) {
            blocks.push_back({instruction[1], {}});
            continue;
        }
        if (!blocks.empty()) blocks.back().body.push_back({op, instruction});
    }
    return blocks;
}

bool isConstant(const std::map<std::uint32_t, Instruction>& definitions, std::uint32_t id, std::uint32_t value) {
    const auto it = definitions.find(id);
    return it != definitions.end() && it->second.op == spv::OpConstant && it->second.words.size() == 4 && it->second.words[3] == value;
}

bool isNullPrimitiveBit(const std::map<std::uint32_t, Instruction>& definitions, std::uint32_t id) {
    const auto it = definitions.find(id);
    if (it == definitions.end() || it->second.op != spv::OpINotEqual) return false;
    const auto testsMask = [&](std::uint32_t operand) {
        const auto masked = definitions.find(operand);
        return masked != definitions.end() && masked->second.op == spv::OpBitwiseAnd && (isConstant(definitions, masked->second.words[3], 0x80000000u) || isConstant(definitions, masked->second.words[4], 0x80000000u));
    };
    const auto a = it->second.words[3];
    const auto b = it->second.words[4];
    return (testsMask(a) && isConstant(definitions, b, 0u)) || (testsMask(b) && isConstant(definitions, a, 0u));
}

bool isLayerDecision(const std::map<std::uint32_t, Instruction>& definitions, std::uint32_t id) {
    const auto it = definitions.find(id);
    return it != definitions.end() && it->second.op == spv::OpLogicalOr && (isNullPrimitiveBit(definitions, it->second.words[3]) || isNullPrimitiveBit(definitions, it->second.words[4]));
}

std::uint32_t checkCullDecisions(const ShaderRecompiler::RecompileResult& result, bool layer, const char* what) {
    std::map<std::uint32_t, Instruction> definitions;
    std::uint32_t cullVariable = 0;
    const auto blocks = functionBlocks(result.spirv.Words(), definitions, cullVariable);
    if (cullVariable == 0) throw std::runtime_error(std::string(what) + ": the module declares no CullPrimitiveEXT output");
    std::set<std::uint32_t> cullPointers{cullVariable};
    for (const auto& block : blocks) {
        for (const auto& instruction : block.body) {
            if ((instruction.op == spv::OpAccessChain || instruction.op == spv::OpInBoundsAccessChain) && cullPointers.contains(instruction.words[3])) cullPointers.insert(instruction.words[2]);
        }
    }
    const auto constantValue = [&](std::uint32_t id) -> int {
        const auto it = definitions.find(id);
        if (it == definitions.end()) return -1;
        return it->second.op == spv::OpConstantTrue ? 1 : it->second.op == spv::OpConstantFalse ? 0 : -1;
    };
    std::map<std::uint32_t, const Block*> byLabel;
    for (const auto& block : blocks) byLabel[block.label] = &block;
    std::set<const Instruction*> explained;
    std::uint32_t decisions = 0;
    for (const auto& block : blocks) {
        for (std::size_t i = 0; i < block.body.size(); ++i) {
            const auto& store = block.body[i];
            if (store.op != spv::OpStore || !cullPointers.contains(store.words[1]) || constantValue(store.words[2]) != 0) continue;
            const auto& branch = block.body.back();
            if (branch.op != spv::OpBranchConditional) throw std::runtime_error(std::string(what) + ": the false cull store is not followed by a conditional branch in its block");
            const auto condition = branch.words[1];
            if (!(layer ? isLayerDecision(definitions, condition) : isNullPrimitiveBit(definitions, condition))) throw std::runtime_error(std::string(what) + ": the branch after the false cull store does not test the cull decision");
            const auto taken = byLabel.find(branch.words[2]);
            if (taken == byLabel.end()) throw std::runtime_error(std::string(what) + ": the branch target is missing");
            bool storesTrue = false;
            for (const auto& inner : taken->second->body) {
                if (inner.op == spv::OpStore && inner.words[1] == store.words[1] && constantValue(inner.words[2]) == 1) {
                    storesTrue = true;
                    explained.insert(&inner);
                }
            }
            if (!storesTrue) throw std::runtime_error(std::string(what) + ": the taken branch does not store true to the same cull element");
            explained.insert(&store);
            ++decisions;
        }
    }
    for (const auto& block : blocks) {
        for (const auto& instruction : block.body) {
            if (instruction.op == spv::OpStore && cullPointers.contains(instruction.words[1]) && !explained.contains(&instruction)) throw std::runtime_error(std::string(what) + ": a CullPrimitiveEXT store is not part of a false store followed by a true store under the decision");
        }
    }
    if (decisions == 0) throw std::runtime_error(std::string(what) + ": no cull decision found");
    return decisions;
}

constexpr std::array<std::uint32_t, 5> PrimitiveAndPositionExports{0xf8000141u, 0u, 0xf80008cfu, 0u, 0xbf810000u};
constexpr std::array<std::uint32_t, 7> PrimitiveLayerAndPositionExports{0xf8000141u, 0u, 0xf80000dfu, 0u, 0xf80008cfu, 0u, 0xbf810000u};
constexpr std::uint32_t LayerOutput = (1u << 18u) | (1u << 21u);

ShaderRecompiler::RecompileRequest meshRequest(std::span<const std::uint32_t> code, std::uint32_t paClVsOutCntl) {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 2> capabilities{spv::CapabilityMeshShadingEXT, spv::CapabilityShaderViewportIndexLayerEXT};
    static constexpr std::array<std::string_view, 1> extensions{"SPV_EXT_mesh_shader"};
    static constexpr std::array<std::uint32_t, 8> hiddenUserWords{};
    RecompileRequest request{};
    request.shader = {ShaderStage::Mesh, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.vertex = ShaderVertexStageInfo{};
    request.context.vertex->paClVsOutCntl = paClVsOutCntl;
    request.context.userData = hiddenUserWords;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010400u;
    request.target.subgroupSize = 64;
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
        checkCullDecisions(ShaderRecompiler::Recompile(meshRequest(PrimitiveAndPositionExports, 0u)), false, "mesh shader");
        checkCullDecisions(ShaderRecompiler::Recompile(meshRequest(PrimitiveLayerAndPositionExports, LayerOutput)), true, "mesh shader with a layer output");
        std::cout << "CullPrimitiveEXT stores write false, then true under the cull decision\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
