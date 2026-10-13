#include "Recompiler.hpp"
#include "PipelineSpecialization.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {

struct ModuleFacts {
    std::map<std::uint32_t, std::uint32_t> specializationDefaults;
    std::uint32_t clipDistanceLength = 0;
    bool clipDistanceBuiltIn = false;
    std::size_t multiplies = 0;
};

ModuleFacts inspect(const ShaderRecompiler::RecompileResult& result) {
    ModuleFacts facts;
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> specIds;
    std::map<std::uint32_t, std::uint32_t> specValues;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::map<std::uint32_t, std::uint32_t> arrayLengths;
    std::map<std::uint32_t, std::uint32_t> pointerTargets;
    std::map<std::uint32_t, std::uint32_t> variableTypes;
    std::set<std::uint32_t> clipVariables;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        const auto count = words[at] >> 16u;
        if (op == spv::OpDecorate && count == 4 && words[at + 2] == spv::DecorationSpecId) specIds[words[at + 1]] = words[at + 3];
        if (op == spv::OpDecorate && count == 4 && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == spv::BuiltInClipDistance) clipVariables.insert(words[at + 1]);
        if (op == spv::OpSpecConstant && count == 4) specValues[words[at + 2]] = words[at + 3];
        if (op == spv::OpConstant && count == 4) constants[words[at + 2]] = words[at + 3];
        if (op == spv::OpTypeArray && count == 4) arrayLengths[words[at + 1]] = words[at + 3];
        if (op == spv::OpTypePointer && count == 4) pointerTargets[words[at + 1]] = words[at + 3];
        if (op == spv::OpVariable && count >= 4) variableTypes[words[at + 2]] = words[at + 1];
        if (op == spv::OpFMul) ++facts.multiplies;
    }
    for (const auto& [id, specId] : specIds) {
        if (const auto value = specValues.find(id); value != specValues.end()) facts.specializationDefaults[specId] = value->second;
    }
    for (const auto variable : clipVariables) {
        facts.clipDistanceBuiltIn = true;
        const auto pointer = variableTypes.find(variable);
        if (pointer == variableTypes.end()) continue;
        const auto array = pointerTargets.find(pointer->second);
        if (array == pointerTargets.end()) continue;
        const auto length = arrayLengths.find(array->second);
        if (length == arrayLengths.end()) continue;
        const auto constant = constants.find(length->second);
        if (constant != constants.end()) facts.clipDistanceLength = constant->second;
    }
    return facts;
}

constexpr std::array<std::uint32_t, 3> PositionExport{0xf80008cfu, 0u, 0xbf810000u};

ShaderRecompiler::RecompileRequest baseRequest(bool remap) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, 0x10000u, PositionExport, 0, {}};
    request.context.waveSize = 64;
    ShaderVertexStageInfo vertex{};
    vertex.depthRemapMode = 2u;
    vertex.depthRemapScale = std::bit_cast<std::uint32_t>(1.0f);
    vertex.depthRemapOffset = std::bit_cast<std::uint32_t>(0.0f);
    request.context.vertex = vertex;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    static constexpr std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityClipDistance};
    request.target.supportedCapabilities = capabilities;
    request.target.depthRangeRemap = remap;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

void require(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}

}

int main() {
    using namespace ShaderRecompiler;
    try {
        const auto remapped = inspect(Recompile(baseRequest(true)));
        const auto plain = inspect(Recompile(baseRequest(false)));
        require(remapped.clipDistanceBuiltIn && remapped.clipDistanceLength == plain.clipDistanceLength + 2u, "the remap did not add its two clip distances");
        require(remapped.multiplies >= plain.multiplies + 2u, "the remap does not scale the clip z and offset it by w");
        require(!plain.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase) && !remapped.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase + 3u), "unexpected specialization constants");
        std::cout << "Depth range remap tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Depth range remap test failed: " << error.what() << '\n';
        for (const bool remap : {true, false}) {
            const auto facts = inspect(Recompile(baseRequest(remap)));
            std::cerr << "  remap=" << remap << ": spec ids";
            for (const auto& [id, value] : facts.specializationDefaults) std::cerr << ' ' << id << '=' << value;
            std::cerr << "; clip builtin " << facts.clipDistanceBuiltIn << " length " << facts.clipDistanceLength << "; fmul " << facts.multiplies << '\n';
        }
        return 1;
    }
}
