#include "Recompiler.hpp"
#include "PipelineSpecialization.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct ModuleFacts {
    std::map<std::uint32_t, std::uint32_t> specializationDefaults;
    std::uint32_t clipDistanceLength = 0;
    bool clipDistanceBuiltIn = false;
    std::size_t multiplies = 0;
    bool pushConstantVariable = false;
    std::set<std::uint32_t> pushDwordLoads;
};

ModuleFacts inspect(std::span<const std::uint32_t> words) {
    ModuleFacts facts;
    std::map<std::uint32_t, std::uint32_t> specIds;
    std::map<std::uint32_t, std::uint32_t> specValues;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::map<std::uint32_t, std::uint32_t> arrayLengths;
    std::map<std::uint32_t, std::uint32_t> pointerTargets;
    std::map<std::uint32_t, std::uint32_t> variableTypes;
    std::set<std::uint32_t> clipVariables;
    std::set<std::uint32_t> pushVariables;
    std::vector<std::uint32_t> pushIndexIds;
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
        if (op == spv::OpVariable && count >= 4 && words[at + 3] == spv::StorageClassPushConstant) pushVariables.insert(words[at + 2]);
        if (op == spv::OpAccessChain && count == 6 && pushVariables.contains(words[at + 3])) pushIndexIds.push_back(words[at + 5]);
        if (op == spv::OpFMul) ++facts.multiplies;
    }
    for (const auto& [id, specId] : specIds) {
        if (const auto value = specValues.find(id); value != specValues.end()) facts.specializationDefaults[specId] = value->second;
    }
    facts.pushConstantVariable = !pushVariables.empty();
    for (const auto id : pushIndexIds) {
        if (const auto value = constants.find(id); value != constants.end()) facts.pushDwordLoads.insert(value->second);
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

ModuleFacts inspect(const ShaderRecompiler::RecompileResult& result) {
    return inspect(result.spirv.Words());
}

constexpr std::array<std::uint32_t, 3> PositionExport{0xf80008cfu, 0u, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> DomainPositionExport{0xf8000941u, 0u, 0xf80008cfu, 0x06050605u, 0xbf810000u};

ShaderRecompiler::RecompileRequest baseRequest(bool remap, ShaderRecompiler::ShaderStage stage = ShaderRecompiler::ShaderStage::Vertex) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    const auto code = stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(DomainPositionExport) : std::span<const std::uint32_t>(PositionExport);
    request.shader = {stage, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    ShaderVertexStageInfo vertex{};
    vertex.depthRemapMode = 2u;
    request.context.vertex = vertex;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    static constexpr std::array<std::uint32_t, 3> capabilities{spv::CapabilityShader, spv::CapabilityClipDistance, spv::CapabilityTessellation};
    request.target.supportedCapabilities = capabilities;
    request.target.depthRangeRemap = remap;
    if (stage == ShaderStage::TessellationEvaluation) {
        request.target.tessellation = TessellationTargetLimits{32u, 128u, 128u, 128u, 4096u, 128u, 128u};
        request.graphics = GraphicsCompileContext{0u, {}, std::nullopt, TessellationConfiguration{3u, 3u, 1u, 2u, 2u}, {0u, 3u, 0u, 1u}};
    }
    request.layout.pushConstantSizeBytes = ShaderRecompiler::DepthRemapPushOffsetBytes;
    request.useCache = false;
    return request;
}

void require(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}

void preparedTransformTests(ShaderRecompiler::ShaderStage stage) {
    using namespace ShaderRecompiler;
    auto request = baseRequest(true, stage);
    request.useCache = true;
    const auto handle = PrepareShader(request);
    const auto materialize = [&](std::uint32_t mode) {
        auto moved = request;
        moved.context.vertex->depthRemapMode = mode;
        const auto invocation = PreparedShaderInvocation::TryCreate(moved, handle);
        require(invocation.has_value(), "the prepared vertex shader did not match a request of its own mode");
        return invocation->Materialize(*invocation->Capture(SrtRuntime{}));
    };
    const auto prepared = inspect(GetPreparedArtifact(*handle).spirv.Words());
    require(prepared.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase) && !prepared.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase + 1u) && !prepared.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase + 2u), "only the depth remap mode may be a specialization constant of the prepared module");
    const auto first = materialize(2u);
    require(materialize(2u)->PipelineVariantId() == first->PipelineVariantId(), "the same depth remap mode did not reuse its materialized vertex shader");
    auto other = request;
    other.context.vertex->depthRemapMode = 1u;
    require(!PreparedShaderInvocation::TryCreate(other, handle).has_value(), "a prepared vertex shader matched a request of another depth remap mode");
}

void pushedTransformTests(ShaderRecompiler::ShaderStage stage) {
    using namespace ShaderRecompiler;
    const auto remapped = Recompile(baseRequest(true, stage));
    const auto plain = Recompile(baseRequest(false, stage));
    const auto facts = inspect(remapped);
    require(!facts.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase + 1u) && !facts.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase + 2u), "the depth remap scale or offset is a specialization constant");
    require(facts.pushConstantVariable && facts.pushDwordLoads.contains(DepthRemapPushOffsetBytes / 4u) && facts.pushDwordLoads.contains(DepthRemapPushOffsetBytes / 4u + 1u), "the remapped vertex shader does not read the scale and offset from the push block");
    require(!inspect(plain).pushConstantVariable, "a vertex shader without a depth remap gained a push constant block");
    auto crowded = baseRequest(true, stage);
    crowded.layout.pushConstantSizeBytes = DepthRemapPushOffsetBytes + 4u;
    bool refused = false;
    try {
        static_cast<void>(Recompile(crowded));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    require(refused, "a remapped vertex shader with push data over the depth remap words was accepted");
}

void clipDistanceCapacityTests(ShaderRecompiler::ShaderStage stage) {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 7> vertex{0xf80008cfu, 0u, 0xf80008dfu, 0u, 0xf80008efu, 0u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 9> domain{0xf8000941u, 0u, 0xf80008cfu, 0x05050505u, 0xf80008dfu, 0x05050505u, 0xf80008efu, 0x05050505u, 0xbf810000u};
    auto request = baseRequest(true, stage);
    request.shader.code = stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(domain) : std::span<const std::uint32_t>(vertex);
    request.context.vertex->paClVsOutCntl = 0xc0003fu;
    require(inspect(Recompile(request)).clipDistanceLength == 8u, "the remap did not preserve six guest clip distances beside its two planes");
    for (const auto control : {0xc0007fu, 0xc0403fu, 0xc000ffu}) {
        request.context.vertex->paClVsOutCntl = control;
        bool refused = false;
        try {
            static_cast<void>(Recompile(request));
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("two free clip-distance components") != std::string::npos;
        }
        require(refused, "a remap that exceeds the combined clip and cull distance capacity was accepted");
    }
}

}

int main() {
    using namespace ShaderRecompiler;
    try {
        for (const auto stage : {ShaderStage::Vertex, ShaderStage::TessellationEvaluation}) {
            const auto remapped = inspect(Recompile(baseRequest(true, stage)));
            const auto plain = inspect(Recompile(baseRequest(false, stage)));
            require(remapped.clipDistanceBuiltIn && remapped.clipDistanceLength == plain.clipDistanceLength + 2u, "the remap did not add its two clip distances");
            require(remapped.multiplies >= plain.multiplies + 2u, "the remap does not scale the clip z and offset it by w");
            require(!plain.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase) && !remapped.specializationDefaults.contains(PipelineSpecialization::DepthRemapBase + 3u), "unexpected specialization constants");
            preparedTransformTests(stage);
            pushedTransformTests(stage);
            clipDistanceCapacityTests(stage);
        }
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
