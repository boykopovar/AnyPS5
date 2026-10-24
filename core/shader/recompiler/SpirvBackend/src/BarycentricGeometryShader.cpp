#include "Recompiler.hpp"
#include "SpirvBackend/SpirvClipDistances.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <set>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t ParameterLocations = 32u;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Barycentric geometry SPIR-V: " + message);
}

using PerVertexParameter = EmulatedPerVertexParameter;

EmulatedInterface CollectEmulatedInterface(const RecompileResult& vertex, const RecompileResult& fragment) {
    require(fragment.barycentricEmulation.active, "the fragment shader does not emulate barycentrics");
    for (const auto& parameter : fragment.fragmentParameters) {
        require(std::find(vertex.parameterExports.begin(), vertex.parameterExports.end(), parameter.sourceLocation) != vertex.parameterExports.end(), "fragment parameter " + std::to_string(parameter.location) + " is not exported by the vertex shader");
    }
    return DescribeEmulatedInterface(fragment.fragmentParameters, fragment.barycentricEmulation);
}

class BarycentricGeometryEmitter {
public:
    explicit BarycentricGeometryEmitter(std::uint32_t version) : builder(version) {
        builder.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
        voidType = builder.Type(spv::OpTypeVoid);
        uintType = builder.Type(spv::OpTypeInt, 32u, 0u);
        intType = builder.Type(spv::OpTypeInt, 32u, 1u);
        floatType = builder.Type(spv::OpTypeFloat, 32u);
        vec3FloatType = builder.Type(spv::OpTypeVector, floatType, 3u);
        vec4FloatType = builder.Type(spv::OpTypeVector, floatType, 4u);
        vertexArrayType = builder.Type(spv::OpTypeArray, vec4FloatType, constant(uintType, 3u));
        functionType = builder.Type(spv::OpTypeFunction, voidType);
        perVertexType = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}}, {spv::OpDecorate, {spv::DecorationBlock}}}, vec4FloatType);
    }

    std::vector<std::uint32_t> Emit(const std::vector<std::uint32_t>& passed, const std::vector<PerVertexParameter>& perVertex, const BarycentricEmulationLayout& layout) {
        builder.EmitCapability(spv::CapabilityShader);
        builder.EmitCapability(spv::CapabilityGeometry);
        main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        builder.AddExecutionMode(main, spv::ExecutionModeTriangles);
        builder.AddExecutionMode(main, spv::ExecutionModeInvocations, 1u);
        builder.AddExecutionMode(main, spv::ExecutionModeOutputTriangleStrip);
        builder.AddExecutionMode(main, spv::ExecutionModeOutputVertices, 3u);

        const auto glIn = addInterface(spv::StorageClassInput, builder.Type(spv::OpTypeArray, perVertexType, constant(uintType, 3u)));
        std::set<std::uint32_t> sources(passed.begin(), passed.end());
        for (const auto& parameter : perVertex) sources.insert(parameter.sourceLocation);
        std::array<std::uint32_t, ParameterLocations> inputs {};
        for (const auto location : sources) {
            inputs[location] = addInterface(spv::StorageClassInput, vertexArrayType);
            decorate(inputs[location], spv::DecorationLocation, location);
        }
        const auto position = addInterface(spv::StorageClassOutput, vec4FloatType);
        decorate(position, spv::DecorationBuiltIn, spv::BuiltInPosition);
        std::vector<std::uint32_t> passedOutputs;
        for (const auto location : passed) {
            passedOutputs.push_back(addInterface(spv::StorageClassOutput, vec4FloatType));
            decorate(passedOutputs.back(), spv::DecorationLocation, location);
        }
        const auto barycentricOutput = [&](std::uint32_t location) {
            if (location == BarycentricEmulationLayout::NoLocation) return 0u;
            const auto variable = addInterface(spv::StorageClassOutput, vec3FloatType);
            decorate(variable, spv::DecorationLocation, location);
            return variable;
        };
        const auto smooth = barycentricOutput(layout.smoothLocation);
        const auto linear = barycentricOutput(layout.linearLocation);
        const auto reciprocalW = barycentricOutput(layout.reciprocalWLocation);
        if (reciprocalW != 0u) builder.AddAnnotation(spv::OpDecorate, reciprocalW, spv::DecorationNoPerspective);
        std::vector<std::uint32_t> perVertexOutputs;
        for (const auto& parameter : perVertex) {
            perVertexOutputs.push_back(addInterface(spv::StorageClassOutput, vertexArrayType));
            decorate(perVertexOutputs.back(), spv::DecorationLocation, parameter.location);
        }
        builder.EmitEntryPoint(spv::ExecutionModelGeometry, main, "main", interfaces);
        builder.AddFunction(spv::OpLabel, builder.AllocateId());

        const auto ptrInputVec4 = builder.Type(spv::OpTypePointer, spv::StorageClassInput, vec4FloatType);
        const auto ptrOutputVec4 = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, vec4FloatType);
        const auto zero = constant(floatType, 0u);
        const auto one = constant(floatType, std::bit_cast<std::uint32_t>(1.0f));
        std::array<std::array<std::uint32_t, 3>, ParameterLocations> values {};
        for (const auto location : sources) {
            for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) values[location][vertex] = load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, inputs[location], constant(intType, vertex)));
        }
        for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) {
            const auto clipPosition = load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, glIn, constant(intType, vertex), constant(intType, 0u)));
            store(position, clipPosition);
            for (std::size_t i = 0; i < passed.size(); ++i) store(passedOutputs[i], values[passed[i]][vertex]);
            const auto weights = result(spv::OpCompositeConstruct, vec3FloatType, vertex == 0u ? one : zero, vertex == 1u ? one : zero, vertex == 2u ? one : zero);
            if (smooth != 0u) store(smooth, weights);
            if (linear != 0u) store(linear, weights);
            if (reciprocalW != 0u) {
                const auto clipW = result(spv::OpCompositeExtract, floatType, clipPosition, 3u);
                const auto reciprocal = result(spv::OpFDiv, floatType, one, clipW);
                const auto reciprocalWeights = result(spv::OpCompositeConstruct, vec3FloatType, vertex == 0u ? reciprocal : zero, vertex == 1u ? reciprocal : zero, vertex == 2u ? reciprocal : zero);
                store(reciprocalW, reciprocalWeights);
            }
            for (std::size_t i = 0; i < perVertex.size(); ++i) {
                for (std::uint32_t source = 0; source < 3u; ++source) store(result(spv::OpAccessChain, ptrOutputVec4, perVertexOutputs[i], constant(intType, source)), values[perVertex[i].sourceLocation][source]);
            }
            builder.AddFunction(spv::OpEmitVertex);
        }
        builder.AddFunction(spv::OpEndPrimitive);
        builder.AddFunction(spv::OpReturn);
        builder.AddFunction(spv::OpFunctionEnd);
        return builder.Finalize();
    }

private:
    SpirvModule builder;
    std::vector<std::uint32_t> interfaces;
    std::uint32_t main = 0;
    std::uint32_t voidType = 0;
    std::uint32_t uintType = 0;
    std::uint32_t intType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec3FloatType = 0;
    std::uint32_t vec4FloatType = 0;
    std::uint32_t vertexArrayType = 0;
    std::uint32_t functionType = 0;
    std::uint32_t perVertexType = 0;

    std::uint32_t constant(std::uint32_t type, std::uint32_t value) { return builder.Constant(spv::OpConstant, type, value); }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t type, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, type, id, operands...);
        return id;
    }

    std::uint32_t load(std::uint32_t type, std::uint32_t pointer) { return result(spv::OpLoad, type, pointer); }

    void store(std::uint32_t pointer, std::uint32_t value) { builder.AddFunction(spv::OpStore, pointer, value); }

    void decorate(std::uint32_t target, spv::Decoration decoration, std::uint32_t value) { builder.AddAnnotation(spv::OpDecorate, target, decoration, value); }

    std::uint32_t addInterface(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = builder.DefineGlobalVariable(builder.Type(spv::OpTypePointer, storage, type), storage);
        interfaces.push_back(variable);
        return variable;
    }
};

class BarycentricTessellationEmitter {
public:
    BarycentricTessellationEmitter(std::uint32_t version, std::uint32_t clipDistances) : builder(version), clipDistances(clipDistances) {
        builder.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
        voidType = builder.Type(spv::OpTypeVoid);
        boolType = builder.Type(spv::OpTypeBool);
        uintType = builder.Type(spv::OpTypeInt, 32u, 0u);
        intType = builder.Type(spv::OpTypeInt, 32u, 1u);
        floatType = builder.Type(spv::OpTypeFloat, 32u);
        vec3FloatType = builder.Type(spv::OpTypeVector, floatType, 3u);
        vec4FloatType = builder.Type(spv::OpTypeVector, floatType, 4u);
        vertexArrayType = builder.Type(spv::OpTypeArray, vec4FloatType, constant(uintType, 3u));
        functionType = builder.Type(spv::OpTypeFunction, voidType);
        if (clipDistances != 0u) {
            clipArrayType = builder.Type(spv::OpTypeArray, floatType, constant(uintType, clipDistances));
            controlInputClipType = builder.Type(spv::OpTypeArray, clipArrayType, constant(uintType, 3u));
            ptrControlInputClip = builder.Type(spv::OpTypePointer, spv::StorageClassInput, clipArrayType);
            ptrControlOutputClip = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, clipArrayType);
            ptrEvaluationInputClip = builder.Type(spv::OpTypePointer, spv::StorageClassInput, clipArrayType);
            ptrEvaluationOutputClip = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, clipArrayType);
        }
        perVertexType = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}}, {spv::OpDecorate, {spv::DecorationBlock}}}, vec4FloatType);
        perVertexArrayType = builder.Type(spv::OpTypeArray, perVertexType, constant(uintType, 3u));
        stageVertexType = perVertexType;
        if (clipDistances != 0u) stageVertexType = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}}, {spv::OpMemberDecorate, {1u, spv::DecorationBuiltIn, spv::BuiltInClipDistance}}, {spv::OpDecorate, {spv::DecorationBlock}}}, vec4FloatType, clipArrayType);
        stageVertexArrayType = builder.Type(spv::OpTypeArray, stageVertexType, constant(uintType, 3u));
        ptrInputVec4 = builder.Type(spv::OpTypePointer, spv::StorageClassInput, vec4FloatType);
        ptrOutputVec4 = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, vec4FloatType);
    }

    std::vector<std::uint32_t> EmitControl(const std::vector<std::uint32_t>& sources) {
        begin();
        builder.AddExecutionMode(main, spv::ExecutionModeOutputVertices, 3u);
        addDomain();
        const auto invocationId = addInterface(spv::StorageClassInput, intType);
        decorate(invocationId, spv::DecorationBuiltIn, spv::BuiltInInvocationId);
        const auto glIn = addInterface(spv::StorageClassInput, perVertexArrayType);
        const auto glOut = addInterface(spv::StorageClassOutput, stageVertexArrayType);
        std::uint32_t clipIn = 0u;
        if (clipDistances != 0u) {
            clipIn = addInterface(spv::StorageClassInput, controlInputClipType);
            decorate(clipIn, spv::DecorationBuiltIn, spv::BuiltInClipDistance);
        }
        const auto floatArray = [&](std::uint32_t size) { return builder.Type(spv::OpTypeArray, floatType, constant(uintType, size)); };
        const auto outer = addInterface(spv::StorageClassOutput, floatArray(4u));
        decorate(outer, spv::DecorationBuiltIn, spv::BuiltInTessLevelOuter);
        builder.AddAnnotation(spv::OpDecorate, outer, spv::DecorationPatch);
        const auto inner = addInterface(spv::StorageClassOutput, floatArray(2u));
        decorate(inner, spv::DecorationBuiltIn, spv::BuiltInTessLevelInner);
        builder.AddAnnotation(spv::OpDecorate, inner, spv::DecorationPatch);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> parameters;
        for (const auto location : sources) {
            const auto input = addInterface(spv::StorageClassInput, vertexArrayType);
            decorate(input, spv::DecorationLocation, location);
            const auto output = addInterface(spv::StorageClassOutput, vertexArrayType);
            decorate(output, spv::DecorationLocation, location);
            parameters.emplace_back(input, output);
        }
        builder.EmitEntryPoint(spv::ExecutionModelTessellationControl, main, "main", interfaces);
        builder.AddFunction(spv::OpLabel, builder.AllocateId());

        const auto invocation = load(intType, invocationId);
        const auto first = result(spv::OpIEqual, boolType, invocation, constant(intType, 0u));
        const auto writeLevels = builder.AllocateId();
        const auto afterLevels = builder.AllocateId();
        builder.AddFunction(spv::OpSelectionMerge, afterLevels, spv::SelectionControlMaskNone);
        builder.AddFunction(spv::OpBranchConditional, first, writeLevels, afterLevels);
        builder.AddFunction(spv::OpLabel, writeLevels);
        const auto ptrOutputFloat = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, floatType);
        const auto one = constant(floatType, std::bit_cast<std::uint32_t>(1.0f));
        for (std::uint32_t i = 0; i < 4u; ++i) store(result(spv::OpAccessChain, ptrOutputFloat, outer, constant(intType, i)), one);
        for (std::uint32_t i = 0; i < 2u; ++i) store(result(spv::OpAccessChain, ptrOutputFloat, inner, constant(intType, i)), one);
        builder.AddFunction(spv::OpBranch, afterLevels);
        builder.AddFunction(spv::OpLabel, afterLevels);
        store(result(spv::OpAccessChain, ptrOutputVec4, glOut, invocation, constant(intType, 0u)), load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, glIn, invocation, constant(intType, 0u))));
        if (clipDistances != 0u) store(result(spv::OpAccessChain, ptrControlOutputClip, glOut, invocation, constant(intType, 1u)), load(clipArrayType, result(spv::OpAccessChain, ptrControlInputClip, clipIn, invocation)));
        for (const auto& [input, output] : parameters) store(result(spv::OpAccessChain, ptrOutputVec4, output, invocation), load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, input, invocation)));
        return finish();
    }

    std::vector<std::uint32_t> EmitEvaluation(const std::vector<std::uint32_t>& sources, const std::vector<std::uint32_t>& passed, const std::vector<bool>& passedFlat, const std::vector<PerVertexParameter>& perVertex, const BarycentricEmulationLayout& layout, std::uint32_t provokingVertex) {
        begin();
        addDomain();
        const auto tessCoord = addInterface(spv::StorageClassInput, vec3FloatType);
        decorate(tessCoord, spv::DecorationBuiltIn, spv::BuiltInTessCoord);
        const auto glIn = addInterface(spv::StorageClassInput, stageVertexArrayType);
        std::array<std::uint32_t, ParameterLocations> inputs {};
        for (const auto location : sources) {
            inputs[location] = addInterface(spv::StorageClassInput, vertexArrayType);
            decorate(inputs[location], spv::DecorationLocation, location);
        }
        const auto glOut = addInterface(spv::StorageClassOutput, stageVertexType);
        std::vector<std::uint32_t> passedOutputs;
        for (const auto location : passed) {
            passedOutputs.push_back(addInterface(spv::StorageClassOutput, vec4FloatType));
            decorate(passedOutputs.back(), spv::DecorationLocation, location);
        }
        const auto barycentricOutput = [&](std::uint32_t location) {
            if (location == BarycentricEmulationLayout::NoLocation) return 0u;
            const auto variable = addInterface(spv::StorageClassOutput, vec3FloatType);
            decorate(variable, spv::DecorationLocation, location);
            return variable;
        };
        const auto smooth = barycentricOutput(layout.smoothLocation);
        const auto linear = barycentricOutput(layout.linearLocation);
        const auto reciprocalW = barycentricOutput(layout.reciprocalWLocation);
        if (reciprocalW != 0u) builder.AddAnnotation(spv::OpDecorate, reciprocalW, spv::DecorationNoPerspective);
        std::vector<std::uint32_t> perVertexOutputs;
        for (const auto& parameter : perVertex) {
            perVertexOutputs.push_back(addInterface(spv::StorageClassOutput, vertexArrayType));
            decorate(perVertexOutputs.back(), spv::DecorationLocation, parameter.location);
        }
        builder.EmitEntryPoint(spv::ExecutionModelTessellationEvaluation, main, "main", interfaces);
        builder.AddFunction(spv::OpLabel, builder.AllocateId());

        const auto ptrInputFloat = builder.Type(spv::OpTypePointer, spv::StorageClassInput, floatType);
        const auto half = constant(floatType, std::bit_cast<std::uint32_t>(0.5f));
        const auto zero = constant(floatType, 0u);
        const auto one = constant(floatType, std::bit_cast<std::uint32_t>(1.0f));
        std::array<std::uint32_t, 3> corner {};
        for (std::uint32_t i = 0; i < 3u; ++i) corner[i] = result(spv::OpFOrdGreaterThan, boolType, load(floatType, result(spv::OpAccessChain, ptrInputFloat, tessCoord, constant(intType, i))), half);
        auto index = result(spv::OpSelect, intType, corner[2], constant(intType, 2u), constant(intType, 0u));
        index = result(spv::OpSelect, intType, corner[1], constant(intType, 1u), index);
        const auto weight = [&](std::uint32_t vertex) { return result(spv::OpSelect, floatType, result(spv::OpIEqual, boolType, index, constant(intType, vertex)), one, zero); };
        const auto weights = result(spv::OpCompositeConstruct, vec3FloatType, weight(0u), weight(1u), weight(2u));

        store(result(spv::OpAccessChain, ptrOutputVec4, glOut, constant(intType, 0u)), load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, glIn, index, constant(intType, 0u))));
        if (clipDistances != 0u) store(result(spv::OpAccessChain, ptrEvaluationOutputClip, glOut, constant(intType, 1u)), load(clipArrayType, result(spv::OpAccessChain, ptrEvaluationInputClip, glIn, index, constant(intType, 1u))));
        for (std::size_t i = 0; i < passed.size(); ++i) store(passedOutputs[i], load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, inputs[passed[i]], passedFlat[i] ? constant(intType, provokingVertex) : index)));
        if (smooth != 0u) store(smooth, weights);
        if (linear != 0u) store(linear, weights);
        if (reciprocalW != 0u) {
            std::array<std::uint32_t, 3> reciprocalWeights{};
            for (std::uint32_t source = 0; source < 3u; ++source) {
                const auto clipPosition = load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, glIn, constant(intType, source), constant(intType, 0u)));
                const auto clipW = result(spv::OpCompositeExtract, floatType, clipPosition, 3u);
                const auto inverseW = result(spv::OpFDiv, floatType, one, clipW);
                reciprocalWeights[source] = result(spv::OpFMul, floatType, weight(source), inverseW);
            }
            store(reciprocalW, result(spv::OpCompositeConstruct, vec3FloatType, reciprocalWeights[0], reciprocalWeights[1], reciprocalWeights[2]));
        }
        for (std::size_t i = 0; i < perVertex.size(); ++i) {
            for (std::uint32_t source = 0; source < 3u; ++source) store(result(spv::OpAccessChain, ptrOutputVec4, perVertexOutputs[i], constant(intType, source)), load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, inputs[perVertex[i].sourceLocation], constant(intType, source))));
        }
        return finish();
    }

private:
    SpirvModule builder;
    std::uint32_t clipDistances;
    std::vector<std::uint32_t> interfaces;
    std::uint32_t main = 0;
    std::uint32_t voidType = 0;
    std::uint32_t boolType = 0;
    std::uint32_t uintType = 0;
    std::uint32_t intType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec3FloatType = 0;
    std::uint32_t vec4FloatType = 0;
    std::uint32_t vertexArrayType = 0;
    std::uint32_t functionType = 0;
    std::uint32_t perVertexType = 0;
    std::uint32_t perVertexArrayType = 0;
    std::uint32_t clipArrayType = 0;
    std::uint32_t controlInputClipType = 0;
    std::uint32_t ptrInputVec4 = 0;
    std::uint32_t ptrOutputVec4 = 0;
    std::uint32_t ptrControlInputClip = 0;
    std::uint32_t ptrControlOutputClip = 0;
    std::uint32_t ptrEvaluationInputClip = 0;
    std::uint32_t ptrEvaluationOutputClip = 0;
    std::uint32_t stageVertexType = 0;
    std::uint32_t stageVertexArrayType = 0;

    void begin() {
        builder.EmitCapability(spv::CapabilityShader);
        builder.EmitCapability(spv::CapabilityTessellation);
        if (clipDistances != 0u) builder.EmitCapability(spv::CapabilityClipDistance);
        main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
    }

    void addDomain() {
        builder.AddExecutionMode(main, spv::ExecutionModeTriangles);
        builder.AddExecutionMode(main, spv::ExecutionModeSpacingEqual);
        builder.AddExecutionMode(main, spv::ExecutionModeVertexOrderCw);
    }

    std::vector<std::uint32_t> finish() {
        builder.AddFunction(spv::OpReturn);
        builder.AddFunction(spv::OpFunctionEnd);
        return builder.Finalize();
    }

    std::uint32_t constant(std::uint32_t type, std::uint32_t value) { return builder.Constant(spv::OpConstant, type, value); }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t type, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, type, id, operands...);
        return id;
    }

    std::uint32_t load(std::uint32_t type, std::uint32_t pointer) { return result(spv::OpLoad, type, pointer); }

    void store(std::uint32_t pointer, std::uint32_t value) { builder.AddFunction(spv::OpStore, pointer, value); }

    void decorate(std::uint32_t target, spv::Decoration decoration, std::uint32_t value) { builder.AddAnnotation(spv::OpDecorate, target, decoration, value); }

    std::uint32_t addInterface(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = builder.DefineGlobalVariable(builder.Type(spv::OpTypePointer, storage, type), storage);
        interfaces.push_back(variable);
        return variable;
    }
};

}

BarycentricEmulationLayout LayoutBarycentricEmulation(std::span<const FragmentParameter> parameters, const BarycentricEmulation& emulation, bool meshPositions) {
    BarycentricEmulationLayout layout;
    if (!emulation.active) return layout;
    std::array<bool, ParameterLocations> used {};
    for (const auto& parameter : parameters) {
        require(parameter.location < ParameterLocations, "fragment parameter location " + std::to_string(parameter.location) + " is out of range");
        if (!parameter.perVertex) used[parameter.location] = true;
    }
    const auto take = [&](std::uint32_t count) {
        for (std::uint32_t first = 0; first + count <= ParameterLocations; ++first) {
            if (std::any_of(used.begin() + first, used.begin() + first + count, [](bool taken) { return taken; })) continue;
            std::fill(used.begin() + first, used.begin() + first + count, true);
            return first;
        }
        throw std::runtime_error("Barycentric geometry SPIR-V: the emulated barycentric inputs exceed " + std::to_string(ParameterLocations) + " locations");
    };
    if (emulation.smooth) layout.smoothLocation = take(1u);
    if (emulation.linear) layout.linearLocation = take(1u);
    if (meshPositions) layout.positionsLocation = take(3u);
    if (emulation.smoothOffset) layout.reciprocalWLocation = take(1u);
    for (const auto& parameter : parameters) {
        if (parameter.perVertex) layout.perVertexLocations.emplace_back(parameter.location, take(3u));
    }
    return layout;
}

EmulatedInterface DescribeEmulatedInterface(std::span<const FragmentParameter> parameters, const BarycentricEmulation& emulation, bool meshPositions) {
    EmulatedInterface emulated;
    emulated.layout = LayoutBarycentricEmulation(parameters, emulation, meshPositions);
    for (const auto& parameter : parameters) {
        if (!parameter.perVertex) {
            emulated.passed.push_back(parameter.sourceLocation);
            emulated.passedFlat.push_back(parameter.flat);
            continue;
        }
        const auto relocated = std::find_if(emulated.layout.perVertexLocations.begin(), emulated.layout.perVertexLocations.end(), [&](const auto& entry) { return entry.first == parameter.location; });
        require(relocated != emulated.layout.perVertexLocations.end(), "per-vertex parameter " + std::to_string(parameter.location) + " has no emulated location");
        emulated.perVertex.push_back({parameter.sourceLocation, relocated->second});
    }
    emulated.barycentrics = (emulated.layout.smoothLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u) + (emulated.layout.linearLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u);
    std::set<std::uint32_t> sources(emulated.passed.begin(), emulated.passed.end());
    for (const auto& parameter : emulated.perVertex) sources.insert(parameter.sourceLocation);
    emulated.sources = static_cast<std::uint32_t>(sources.size());
    return emulated;
}

RecompileResult BuildBarycentricGeometryShader(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target, const std::optional<GeometryStageLimits>& limits) {
    require(fragment.barycentricEmulation.active, "the fragment shader does not emulate barycentrics");
    require(limits.has_value(), "geometry shaders are unavailable");
    require(target.spirvVersion >= 0x00010300u && target.spirvVersion <= 0x00010500u, "unsupported SPIR-V target version");
    require(std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityGeometry) != target.supportedCapabilities.end(), "geometry capability is unavailable");
    const auto emulated = CollectEmulatedInterface(vertex, fragment);
    const auto passed = static_cast<std::uint32_t>(emulated.passed.size());
    const auto perVertex = static_cast<std::uint32_t>(emulated.perVertex.size());
    const auto outputComponents = 4u + 4u * passed + 3u * (emulated.barycentrics + (emulated.layout.reciprocalWLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u)) + 12u * perVertex;
    const auto inputComponents = 4u + 4u * emulated.sources;
    const auto fragmentComponents = 4u * (passed + emulated.barycentrics + 3u * perVertex + (emulated.layout.reciprocalWLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u));
    require(limits->maxGeometryOutputVertices >= 3u && outputComponents <= limits->maxGeometryOutputComponents && 3u * outputComponents <= limits->maxGeometryTotalOutputComponents && inputComponents <= limits->maxGeometryInputComponents && fragmentComponents <= limits->maxFragmentInputComponents, "the emulated barycentric interface exceeds device limits");
    BarycentricGeometryEmitter emitter(target.spirvVersion);
    RecompileResult result;
    result.spirv = emitter.Emit(emulated.passed, emulated.perVertex, emulated.layout);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    return result;
}

BarycentricTessellationShaders BuildBarycentricTessellationShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target, std::uint32_t maxFragmentInputComponents, std::uint32_t provokingVertex) {
    require(fragment.barycentricEmulation.active, "the fragment shader does not emulate barycentrics");
    require(provokingVertex == 0u || provokingVertex == 2u, "the provoking vertex must be the first or the last of the triangle");
    require(target.tessellation.has_value(), "tessellation shaders are unavailable");
    require(target.spirvVersion >= 0x00010300u && target.spirvVersion <= 0x00010500u, "unsupported SPIR-V target version");
    require(std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityTessellation) != target.supportedCapabilities.end(), "tessellation capability is unavailable");
    const auto emulated = CollectEmulatedInterface(vertex, fragment);
    const auto passed = static_cast<std::uint32_t>(emulated.passed.size());
    const auto perVertex = static_cast<std::uint32_t>(emulated.perVertex.size());
    const auto clipDistances = VertexClipDistanceCount(vertex.spirv.Words());
    require(clipDistances == 0u || std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityClipDistance) != target.supportedCapabilities.end(), "the vertex clip distances need shaderClipDistance");
    const auto controlComponents = 4u + clipDistances + 4u * emulated.sources;
    const auto evaluationOutputComponents = 4u + clipDistances + 4u * passed + 3u * (emulated.barycentrics + (emulated.layout.reciprocalWLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u)) + 12u * perVertex;
    const auto fragmentComponents = 4u * (passed + emulated.barycentrics + 3u * perVertex + (emulated.layout.reciprocalWLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u));
    const auto& limits = *target.tessellation;
    require(limits.maxPatchSize >= 3u && controlComponents <= limits.maxControlPerVertexInputComponents && controlComponents <= limits.maxControlPerVertexOutputComponents && 3u * controlComponents + 4u <= limits.maxControlTotalOutputComponents && limits.maxControlPerPatchOutputComponents >= 4u && controlComponents <= limits.maxEvaluationInputComponents && evaluationOutputComponents <= limits.maxEvaluationOutputComponents && fragmentComponents <= maxFragmentInputComponents, "the emulated barycentric interface exceeds device limits");
    std::set<std::uint32_t> sourceSet(emulated.passed.begin(), emulated.passed.end());
    for (const auto& parameter : emulated.perVertex) sourceSet.insert(parameter.sourceLocation);
    const std::vector<std::uint32_t> sources(sourceSet.begin(), sourceSet.end());
    BarycentricTessellationShaders shaders;
    shaders.control.spirv = BarycentricTessellationEmitter(target.spirvVersion, clipDistances).EmitControl(sources);
    shaders.evaluation.spirv = BarycentricTessellationEmitter(target.spirvVersion, clipDistances).EmitEvaluation(sources, emulated.passed, emulated.passedFlat, emulated.perVertex, emulated.layout, provokingVertex);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    shaders.control.spirv = ValidateAndOptimizeSpirv(shaders.control.spirv, target.vulkanVersion, target.spirvVersion);
    shaders.evaluation.spirv = ValidateAndOptimizeSpirv(shaders.evaluation.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    return shaders;
}

}
