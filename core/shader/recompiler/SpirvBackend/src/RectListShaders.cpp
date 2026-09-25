#include "Recompiler.hpp"
#include "BdaAbi.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {
namespace {

struct Parameter {
    std::uint32_t inputLocation;
    std::uint32_t outputLocation;
    bool flat;
    // The vertex shader never exports this parameter; the fragment shader reads zero, as it would from
    // an unwritten parameter cache entry.
    bool missing = false;
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Rect-list SPIR-V: ") + message);
}

class RectListEmitter {
public:
    RectListEmitter(const std::vector<Parameter>& parameters, spv::ExecutionModel model, std::uint32_t version, std::uint32_t faultBinding) : builder(version), faultBinding(faultBinding), version(version), parameters(parameters) {
        builder.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

        voidType = type(spv::OpTypeVoid);
        uintType = type(spv::OpTypeInt, 32u, 0u);
        intType = type(spv::OpTypeInt, 32u, 1u);
        floatType = type(spv::OpTypeFloat, 32u);
        vec4FloatType = type(spv::OpTypeVector, floatType, 4u);
        functionType = type(spv::OpTypeFunction, voidType);

        perVertexType = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}}, {spv::OpDecorate, {spv::DecorationBlock}}}, vec4FloatType);

        ptrInputVec4Float = pointer(spv::StorageClassInput, vec4FloatType);
        ptrOutputVec4Float = pointer(spv::StorageClassOutput, vec4FloatType);
        if (model == spv::ExecutionModelTessellationControl) {
            boolType = type(spv::OpTypeBool);
            vec2BoolType = type(spv::OpTypeVector, boolType, 2u);
            vec2FloatType = type(spv::OpTypeVector, floatType, 2u);
            ptrOutputFloat = pointer(spv::StorageClassOutput, floatType);
        } else {
            vec3FloatType = type(spv::OpTypeVector, floatType, 3u);
            ptrInputFloat = pointer(spv::StorageClassInput, floatType);
        }
    }

    std::vector<std::uint32_t> EmitControl() {
        defineEntry(spv::ExecutionModelTessellationControl);
        const auto floatOne = constant(floatType, std::bit_cast<std::uint32_t>(1.0f));

        const auto invocation = load(intType, invocationId);
        const auto first = result(spv::OpIEqual, boolType, invocation, intConstant(0));
        const auto writeLevels = builder.AllocateId();
        const auto afterLevels = builder.AllocateId();
        emit(spv::OpSelectionMerge, afterLevels, spv::SelectionControlMaskNone);
        emit(spv::OpBranchConditional, first, writeLevels, afterLevels);
        emit(spv::OpLabel, writeLevels);
        for (std::uint32_t i = 0; i < 4; i++) store(access(ptrOutputFloat, tessOuter, intConstant(i)), floatOne);
        for (std::uint32_t i = 0; i < 2; i++) store(access(ptrOutputFloat, tessInner, intConstant(i)), floatOne);
        emit(spv::OpBranch, afterLevels);
        emit(spv::OpLabel, afterLevels);

        std::array<std::uint32_t, 3> positions {};
        for (std::uint32_t i = 0; i < positions.size(); i++) {
            positions[i] = load(vec4FloatType, access(ptrInputVec4Float, glIn, intConstant(i), intConstant(0)));
        }

        std::array<std::uint32_t, 3> coordinateEqual {};
        for (std::uint32_t i = 0; i < coordinateEqual.size(); i++) {
            const auto left = result(spv::OpVectorShuffle, vec2FloatType, positions[i], positions[i], 0u, 1u);
            const auto right = result(spv::OpVectorShuffle, vec2FloatType, positions[(i + 1u) % 3u], positions[(i + 1u) % 3u], 0u, 1u);
            coordinateEqual[i] = result(spv::OpFOrdEqual, vec2BoolType, left, right);
        }

        std::array<std::uint32_t, 3> barycentric {};
        std::array<std::uint32_t, 3> edgeVertex {};
        const auto floatMinusOne = constant(floatType, std::bit_cast<std::uint32_t>(-1.0f));
        for (std::uint32_t i = 0; i < edgeVertex.size(); i++) {
            const auto previous = (i + 2u) % 3u;
            const auto xy = result(spv::OpLogicalAnd, boolType, result(spv::OpCompositeExtract, boolType, coordinateEqual[i], 0u), result(spv::OpCompositeExtract, boolType, coordinateEqual[previous], 1u));
            const auto yx = result(spv::OpLogicalAnd, boolType, result(spv::OpCompositeExtract, boolType, coordinateEqual[i], 1u), result(spv::OpCompositeExtract, boolType, coordinateEqual[previous], 0u));
            edgeVertex[i] = result(spv::OpLogicalOr, boolType, xy, yx);
            barycentric[i] = result(spv::OpSelect, floatType, edgeVertex[i], floatMinusOne, floatOne);
        }

        auto cornerCount = uintConstant(0);
        for (const auto corner : edgeVertex) cornerCount = result(spv::OpIAdd, uintType, cornerCount, result(spv::OpSelect, uintType, corner, uintConstant(1), uintConstant(0)));
        auto valid = result(spv::OpIEqual, boolType, cornerCount, uintConstant(1));
        for (const auto position : positions) valid = result(spv::OpLogicalAnd, boolType, valid, isFinite(position));
        const auto w = result(spv::OpCompositeExtract, floatType, positions[0], 3u);
        valid = result(spv::OpLogicalAnd, boolType, valid, result(spv::OpFOrdGreaterThan, boolType, w, constant(floatType, 0)));
        for (std::uint32_t i = 1; i < 3; ++i) {
            const auto otherW = result(spv::OpCompositeExtract, floatType, positions[i], 3u);
            valid = result(spv::OpLogicalAnd, boolType, valid, result(spv::OpFOrdEqual, boolType, w, otherW));
        }
        const auto position3 = interpolate(positions[0], positions[1], positions[2], barycentric);
        valid = result(spv::OpLogicalAnd, boolType, valid, isFinite(position3));
        rejectInvalidRectangle(valid);
        auto vertexIndex = result(spv::OpSelect, intType, edgeVertex[2], intConstant(2), intConstant(0));
        vertexIndex = result(spv::OpSelect, intType, edgeVertex[1], intConstant(1), vertexIndex);
        const auto fourth = result(spv::OpIEqual, boolType, invocation, intConstant(3));
        const auto isFourth = result(spv::OpCompositeConstruct, type(spv::OpTypeVector, boolType, 4u), fourth, fourth, fourth, fourth);
        const auto index = result(spv::OpSMod, intType, result(spv::OpIAdd, intType, vertexIndex, invocation), intConstant(3));

        const auto position = result(spv::OpSelect, vec4FloatType, isFourth, position3, load(vec4FloatType, access(ptrInputVec4Float, glIn, index, intConstant(0))));
        store(access(ptrOutputVec4Float, glOut, invocation, intConstant(0)), position);

        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            if (parameters[i].missing) {
                const auto zero = constant(floatType, 0);
                store(access(ptrOutputVec4Float, outputs[i], invocation), result(spv::OpCompositeConstruct, vec4FloatType, zero, zero, zero, zero));
                continue;
            }
            const auto input0 = load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(0)));
            if (parameters[i].flat) {
                store(access(ptrOutputVec4Float, outputs[i], invocation), input0);
                continue;
            }
            const auto input1 = load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(1)));
            const auto input2 = load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(2)));
            const auto input3 = interpolate(input0, input1, input2, barycentric);
            const auto value = result(spv::OpSelect, vec4FloatType, isFourth, input3, load(vec4FloatType, access(ptrInputVec4Float, inputs[i], index)));
            store(access(ptrOutputVec4Float, outputs[i], invocation), value);
        }

        emit(spv::OpReturn);
        emit(spv::OpFunctionEnd);
        return builder.Finalize();
    }

    std::vector<std::uint32_t> EmitEvaluation() {
        defineEntry(spv::ExecutionModelTessellationEvaluation);

        const auto x = load(floatType, access(ptrInputFloat, tessCoord, intConstant(0)));
        const auto y = load(floatType, access(ptrInputFloat, tessCoord, intConstant(1)));
        const auto index = result(spv::OpIAdd, intType, result(spv::OpIMul, intType, result(spv::OpConvertFToS, intType, y), intConstant(2)), result(spv::OpConvertFToS, intType, x));

        const auto position = load(vec4FloatType, access(ptrInputVec4Float, glIn, index, intConstant(0)));
        store(access(ptrOutputVec4Float, glOut, intConstant(0)), position);
        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            store(outputs[i], load(vec4FloatType, access(ptrInputVec4Float, inputs[i], index)));
        }

        emit(spv::OpReturn);
        emit(spv::OpFunctionEnd);
        return builder.Finalize();
    }

private:
    std::uint32_t isFinite(std::uint32_t vector) {
        std::uint32_t finite = 0;
        for (std::uint32_t component = 0; component < 4; ++component) {
            const auto value = result(spv::OpCompositeExtract, floatType, vector, component);
            const auto invalid = result(spv::OpLogicalOr, boolType, result(spv::OpIsNan, boolType, value), result(spv::OpIsInf, boolType, value));
            const auto valid = result(spv::OpLogicalNot, boolType, invalid);
            finite = component == 0 ? valid : result(spv::OpLogicalAnd, boolType, finite, valid);
        }
        return finite;
    }

    void defineFault() {
        const auto words = type(spv::OpTypeRuntimeArray, uintType);
        decorate(words, spv::DecorationArrayStride, 4);
        const auto block = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpDecorate, {spv::DecorationBlock}}, {spv::OpMemberDecorate, {0u, spv::DecorationOffset, 0u}}}, words);
        fault = builder.DefineGlobalVariable(pointer(spv::StorageClassStorageBuffer, block), spv::StorageClassStorageBuffer);
        decorate(fault, spv::DecorationDescriptorSet, 0);
        decorate(fault, spv::DecorationBinding, faultBinding);
        if (version >= 0x00010400u) interfaces.push_back(fault);
    }

    std::uint32_t faultWord(std::uint32_t index) {
        return access(pointer(spv::StorageClassStorageBuffer, uintType), fault, uintConstant(0), uintConstant(index));
    }

    void rejectInvalidRectangle(std::uint32_t valid) {
        const auto failed = builder.AllocateId();
        const auto next = builder.AllocateId();
        emit(spv::OpSelectionMerge, next, spv::SelectionControlMaskNone);
        emit(spv::OpBranchConditional, valid, next, failed);
        emit(spv::OpLabel, failed);
        const auto scope = uintConstant(spv::ScopeDevice);
        const auto relaxed = uintConstant(spv::MemorySemanticsMaskNone);
        const auto state = faultWord(0);
        const auto previous = result(spv::OpAtomicCompareExchange, uintType, state, scope, relaxed, relaxed, uintConstant(static_cast<std::uint32_t>(BdaAbi::FaultState::Writing)), uintConstant(0));
        const auto won = result(spv::OpIEqual, boolType, previous, uintConstant(0));
        const auto write = builder.AllocateId();
        const auto done = builder.AllocateId();
        emit(spv::OpSelectionMerge, done, spv::SelectionControlMaskNone);
        emit(spv::OpBranchConditional, won, write, done);
        emit(spv::OpLabel, write);
        store(faultWord(1), uintConstant(static_cast<std::uint32_t>(BdaAbi::FaultReason::InvalidRectangle)));
        for (std::uint32_t i = 2; i < 8; ++i) store(faultWord(i), uintConstant(0));
        emit(spv::OpAtomicStore, state, scope, uintConstant(spv::MemorySemanticsReleaseMask | spv::MemorySemanticsUniformMemoryMask), uintConstant(static_cast<std::uint32_t>(BdaAbi::FaultState::Ready)));
        emit(spv::OpBranch, done);
        emit(spv::OpLabel, done);
        emit(spv::OpReturn);
        emit(spv::OpLabel, next);
    }

    template <typename... TArgs>
    std::uint32_t type(spv::Op opcode, TArgs... operands) {
        return builder.Type(opcode, operands...);
    }

    std::uint32_t constant(std::uint32_t type, std::uint32_t value) {
        return builder.Constant(spv::OpConstant, type, value);
    }

    std::uint32_t pointer(spv::StorageClass storage, std::uint32_t valueType) {
        return type(spv::OpTypePointer, storage, valueType);
    }

    std::uint32_t array(std::uint32_t valueType, std::uint32_t size) {
        return type(spv::OpTypeArray, valueType, uintConstant(size));
    }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t type, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, type, id, operands...);
        return id;
    }

    template <typename... TArgs>
    std::uint32_t resultWithoutType(spv::Op opcode, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, id, operands...);
        return id;
    }

    template <typename... TArgs>
    void emit(spv::Op opcode, TArgs... operands) {
        builder.AddFunction(opcode, operands...);
    }

    template <typename... TArgs>
    std::uint32_t access(std::uint32_t pointerType, std::uint32_t base, TArgs... indices) {
        return result(spv::OpAccessChain, pointerType, base, indices...);
    }

    std::uint32_t load(std::uint32_t type, std::uint32_t pointer) { return result(spv::OpLoad, type, pointer); }

    void store(std::uint32_t pointer, std::uint32_t value) { emit(spv::OpStore, pointer, value); }

    std::uint32_t intConstant(std::uint32_t value) { return constant(intType, value); }

    std::uint32_t uintConstant(std::uint32_t value) { return constant(uintType, value); }

    std::uint32_t addInterface(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = builder.DefineGlobalVariable(pointer(storage, type), storage);
        interfaces.push_back(variable);
        return variable;
    }

    void decorate(std::uint32_t target, spv::Decoration decoration, std::uint32_t value) {
        builder.AddAnnotation(spv::OpDecorate, target, decoration, value);
    }

    void defineEntry(spv::ExecutionModel model) {
        builder.EmitCapability(spv::CapabilityShader);
        builder.EmitCapability(spv::CapabilityTessellation);
        main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        if (model == spv::ExecutionModelTessellationControl) {
            builder.AddExecutionMode(main, spv::ExecutionModeOutputVertices, 4u);
        } else {
            builder.AddExecutionMode(main, spv::ExecutionModeQuads);
            builder.AddExecutionMode(main, spv::ExecutionModeSpacingEqual);
            builder.AddExecutionMode(main, spv::ExecutionModeVertexOrderCw);
        }
        defineInputs(model);
        defineOutputs(model);
        if (model == spv::ExecutionModelTessellationControl) defineFault();
        builder.EmitEntryPoint(model, main, "main", interfaces);
        resultWithoutType(spv::OpLabel);
    }

    void defineInputs(spv::ExecutionModel model) {
        const auto tessControl = model == spv::ExecutionModelTessellationControl;
        if (tessControl) {
            invocationId = addInterface(spv::StorageClassInput, intType);
            decorate(invocationId, spv::DecorationBuiltIn, spv::BuiltInInvocationId);
        } else {
            tessCoord = addInterface(spv::StorageClassInput, vec3FloatType);
            decorate(tessCoord, spv::DecorationBuiltIn, spv::BuiltInTessCoord);
        }
        glIn = addInterface(spv::StorageClassInput, array(perVertexType, tessControl ? 3u : 4u));

        inputs.resize(parameters.size());
        std::array<std::uint32_t, 32> locations {};
        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            if (tessControl && parameters[i].missing) continue;
            const auto location = tessControl ? parameters[i].inputLocation : parameters[i].outputLocation;
            if (tessControl && locations[location] != 0) {
                inputs[i] = locations[location];
                continue;
            }
            inputs[i] = addInterface(spv::StorageClassInput, array(vec4FloatType, tessControl ? 3u : 4u));
            decorate(inputs[i], spv::DecorationLocation, location);
            locations[location] = inputs[i];
        }
    }

    void defineOutputs(spv::ExecutionModel model) {
        const auto tessControl = model == spv::ExecutionModelTessellationControl;
        if (tessControl) {
            glOut = addInterface(spv::StorageClassOutput, array(perVertexType, 4u));
            tessInner = addInterface(spv::StorageClassOutput, array(floatType, 2u));
            decorate(tessInner, spv::DecorationBuiltIn, spv::BuiltInTessLevelInner);
            builder.AddAnnotation(spv::OpDecorate, tessInner, spv::DecorationPatch);
            tessOuter = addInterface(spv::StorageClassOutput, array(floatType, 4u));
            decorate(tessOuter, spv::DecorationBuiltIn, spv::BuiltInTessLevelOuter);
            builder.AddAnnotation(spv::OpDecorate, tessOuter, spv::DecorationPatch);
        } else {
            glOut = addInterface(spv::StorageClassOutput, perVertexType);
        }

        outputs.resize(parameters.size());
        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            outputs[i] = addInterface(spv::StorageClassOutput, tessControl ? array(vec4FloatType, 4u) : vec4FloatType);
            decorate(outputs[i], spv::DecorationLocation, parameters[i].outputLocation);
        }
    }

    std::uint32_t interpolate(std::uint32_t v0, std::uint32_t v1, std::uint32_t v2, const std::array<std::uint32_t, 3>& barycentric) {
        const auto p0 = result(spv::OpVectorTimesScalar, vec4FloatType, v0, barycentric[0]);
        const auto p1 = result(spv::OpVectorTimesScalar, vec4FloatType, v1, barycentric[1]);
        const auto p2 = result(spv::OpVectorTimesScalar, vec4FloatType, v2, barycentric[2]);
        return result(spv::OpFAdd, vec4FloatType, p0, result(spv::OpFAdd, vec4FloatType, p1, p2));
    }

    SpirvModule builder;
    std::uint32_t faultBinding;
    std::uint32_t version;
    std::uint32_t fault = 0;
    const std::vector<Parameter>& parameters;
    std::vector<std::uint32_t> interfaces;
    std::vector<std::uint32_t> inputs;
    std::vector<std::uint32_t> outputs;
    std::uint32_t main = 0;
    std::uint32_t voidType = 0;
    std::uint32_t boolType = 0;
    std::uint32_t uintType = 0;
    std::uint32_t intType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec2BoolType = 0;
    std::uint32_t vec2FloatType = 0;
    std::uint32_t vec3FloatType = 0;
    std::uint32_t vec4FloatType = 0;
    std::uint32_t functionType = 0;
    std::uint32_t perVertexType = 0;
    std::uint32_t ptrInputFloat = 0;
    std::uint32_t ptrInputVec4Float = 0;
    std::uint32_t ptrOutputFloat = 0;
    std::uint32_t ptrOutputVec4Float = 0;
    std::uint32_t glIn = 0;
    std::uint32_t glOut = 0;
    std::uint32_t tessInner = 0;
    std::uint32_t tessOuter = 0;
    std::uint32_t tessCoord = 0;
    std::uint32_t invocationId = 0;
};


}

RectListShaders BuildRectListShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target) {
    require(target.tessellation.has_value(), "tessellation shaders are unavailable");
    require(target.spirvVersion >= 0x00010300u && target.spirvVersion <= 0x00010400u, "unsupported SPIR-V target version");
    require(std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityTessellation) != target.supportedCapabilities.end(), "tessellation capability is unavailable");
    std::vector<Parameter> parameters;
    std::set<std::uint32_t> locations;
    for (const auto& input : fragment.fragmentParameters) {
        require(input.location < 32 && input.sourceLocation < 32 && locations.insert(input.location).second, "invalid fragment parameter location");
        require(!input.perVertex, "custom per-vertex interpolation is unsupported");
        const bool exported = std::find(vertex.parameterExports.begin(), vertex.parameterExports.end(), input.sourceLocation) != vertex.parameterExports.end();
        parameters.push_back({input.sourceLocation, input.location, input.flat, !exported});
    }
    const auto components = static_cast<std::uint32_t>((parameters.size() + 1) * 4);
    const auto& limits = *target.tessellation;
    require(limits.maxPatchSize >= 4 && components <= limits.maxControlPerVertexInputComponents && components <= limits.maxControlPerVertexOutputComponents && components <= limits.maxEvaluationInputComponents && components <= limits.maxEvaluationOutputComponents && limits.maxControlPerPatchOutputComponents >= 6 && components * 4 + 6 <= limits.maxControlTotalOutputComponents, "tessellation interface exceeds device limits");
    std::uint32_t faultBinding = 0;
    for (const auto* shader : {&vertex, &fragment}) {
        for (const auto& binding : shader->bindings) {
            require(binding.binding < std::numeric_limits<std::uint32_t>::max(), "descriptor binding overflow");
            faultBinding = std::max(faultBinding, binding.binding + 1);
        }
    }
    RectListEmitter control(parameters, spv::ExecutionModelTessellationControl, target.spirvVersion, faultBinding);
    RectListEmitter evaluation(parameters, spv::ExecutionModelTessellationEvaluation, target.spirvVersion, faultBinding);
    RectListShaders shaders;
    shaders.control.spirv = control.EmitControl();
    shaders.control.bindings.push_back({DescriptorKind::StorageBuffer, DescriptorRole::FaultBuffer, 0, faultBinding, 1, {}});
    shaders.control.bdaAbiVersion = BdaAbi::Version;
    shaders.evaluation.spirv = evaluation.EmitEvaluation();
#if ANYPS5_ENABLE_SPIRV_TOOLS
    shaders.control.spirv = ValidateAndOptimizeSpirv(shaders.control.spirv, target.vulkanVersion, target.spirvVersion);
    shaders.evaluation.spirv = ValidateAndOptimizeSpirv(shaders.evaluation.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    return shaders;
}

}
