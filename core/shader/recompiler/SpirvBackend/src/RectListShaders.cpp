#include "Recompiler.hpp"
#include "BdaAbi.hpp"
#include "RuntimeAbi.hpp"
#include "SpirvBackend/SpirvClipDistances.hpp"
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

struct PerVertexSource {
    std::uint32_t inputLocation;
    std::uint32_t outputLocation;
};

struct Emulation {
    std::vector<std::uint32_t> barycentrics;
    std::vector<PerVertexSource> perVertex;
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Rect-list SPIR-V: ") + message);
}

class RectListEmitter {
public:
    RectListEmitter(const std::vector<Parameter>& parameters, const Emulation& emulation, spv::ExecutionModel model, std::uint32_t version, std::uint32_t faultBinding, std::uint32_t clipDistances) : builder(version), faultBinding(faultBinding), version(version), clipDistances(clipDistances), parameters(parameters), emulation(emulation) {
        builder.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

        voidType = type(spv::OpTypeVoid);
        uintType = type(spv::OpTypeInt, 32u, 0u);
        intType = type(spv::OpTypeInt, 32u, 1u);
        floatType = type(spv::OpTypeFloat, 32u);
        vec3FloatType = type(spv::OpTypeVector, floatType, 3u);
        vec4FloatType = type(spv::OpTypeVector, floatType, 4u);
        vertexArrayType = array(vec4FloatType, 3u);
        functionType = type(spv::OpTypeFunction, voidType);

            perVertexType = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}}, {spv::OpMemberDecorate, {0u, spv::DecorationInvariant}}, {spv::OpDecorate, {spv::DecorationBlock}}}, vec4FloatType);
        if (clipDistances != 0u) {
            clipArrayType = array(floatType, clipDistances);
            ptrInputClip = pointer(spv::StorageClassInput, clipArrayType);
            ptrOutputClip = pointer(spv::StorageClassOutput, clipArrayType);
        }

        ptrInputVec4Float = pointer(spv::StorageClassInput, vec4FloatType);
        ptrOutputVec4Float = pointer(spv::StorageClassOutput, vec4FloatType);
        ptrInputVec3Float = pointer(spv::StorageClassInput, vec3FloatType);
        ptrOutputVec3Float = pointer(spv::StorageClassOutput, vec3FloatType);
        if (model == spv::ExecutionModelTessellationControl) {
            boolType = type(spv::OpTypeBool);
            vec2BoolType = type(spv::OpTypeVector, boolType, 2u);
            vec2FloatType = type(spv::OpTypeVector, floatType, 2u);
            ptrOutputFloat = pointer(spv::StorageClassOutput, floatType);
        } else {
            ptrInputFloat = pointer(spv::StorageClassInput, floatType);
        }
    }

    std::vector<std::uint32_t> EmitControl(std::uint32_t flatVertex) {
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
        if (clipDistances != 0u) {
            std::array<std::uint32_t, 3> clips {};
            for (std::uint32_t i = 0; i < clips.size(); i++) clips[i] = load(clipArrayType, access(ptrInputClip, clipIn, intConstant(i)));
            const auto own = load(clipArrayType, access(ptrInputClip, clipIn, index));
            std::vector<std::uint32_t> values;
            for (std::uint32_t k = 0; k < clipDistances; k++) {
                auto sum = constant(floatType, 0);
                for (std::uint32_t i = 0; i < clips.size(); i++) sum = result(spv::OpFAdd, floatType, sum, result(spv::OpFMul, floatType, result(spv::OpCompositeExtract, floatType, clips[i], k), barycentric[i]));
                values.push_back(result(spv::OpSelect, floatType, fourth, sum, result(spv::OpCompositeExtract, floatType, own, k)));
            }
            const auto clip = builder.AllocateId();
            builder.AddFunction(spv::OpCompositeConstruct, clipArrayType, clip, std::span<const std::uint32_t>(values));
            store(access(ptrOutputClip, clipOut, invocation), clip);
        }

        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            if (parameters[i].missing) {
                const auto zero = constant(floatType, 0);
                store(access(ptrOutputVec4Float, outputs[i], invocation), result(spv::OpCompositeConstruct, vec4FloatType, zero, zero, zero, zero));
                continue;
            }
            if (parameters[i].flat) {
                store(access(ptrOutputVec4Float, outputs[i], invocation), load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(flatVertex))));
                continue;
            }
            const auto input0 = load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(0)));
            const auto input1 = load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(1)));
            const auto input2 = load(vec4FloatType, access(ptrInputVec4Float, inputs[i], intConstant(2)));
            const auto input3 = interpolate(input0, input1, input2, barycentric);
            const auto value = result(spv::OpSelect, vec4FloatType, isFourth, input3, load(vec4FloatType, access(ptrInputVec4Float, inputs[i], index)));
            store(access(ptrOutputVec4Float, outputs[i], invocation), value);
        }

        if (!emulation.barycentrics.empty()) {
            const auto zero = constant(floatType, 0);
            std::array<std::uint32_t, 3> weight {};
            for (std::uint32_t i = 0; i < weight.size(); i++) {
                const auto own = result(spv::OpSelect, floatType, result(spv::OpIEqual, boolType, index, intConstant(i)), floatOne, zero);
                weight[i] = result(spv::OpSelect, floatType, fourth, barycentric[i], own);
            }
            const auto weights = result(spv::OpCompositeConstruct, vec3FloatType, weight[0], weight[1], weight[2]);
            for (const auto output : barycentricOutputs) store(access(ptrOutputVec3Float, output, invocation), weights);
        }
        for (std::size_t i = 0; i < emulation.perVertex.size(); i++) {
            for (std::uint32_t source = 0; source < 3u; source++) {
                store(access(ptrOutputVec4Float, perVertexOutputs[i], invocation, intConstant(source)), load(vec4FloatType, access(ptrInputVec4Float, perVertexInputs[i], intConstant(source))));
            }
        }

        emit(spv::OpReturn);
        emit(spv::OpFunctionEnd);
        return builder.Finalize();
    }

    std::vector<std::uint32_t> EmitEvaluation() {
        defineEntry(spv::ExecutionModelTessellationEvaluation);

        const auto x = load(floatType, access(ptrInputFloat, tessCoord, intConstant(0)));
        const auto y = load(floatType, access(ptrInputFloat, tessCoord, intConstant(1)));
        const auto half = constant(floatType, std::bit_cast<std::uint32_t>(0.5f));
        const auto xHalf = result(spv::OpFAdd, floatType, x, half);
        const auto yHalf = result(spv::OpFAdd, floatType, y, half);
        const auto xInt = result(spv::OpConvertFToS, intType, xHalf);
        const auto yInt = result(spv::OpConvertFToS, intType, yHalf);
        const auto index = result(spv::OpIAdd, intType, result(spv::OpIMul, intType, yInt, intConstant(2)), xInt);

        const auto position = load(vec4FloatType, access(ptrInputVec4Float, glIn, index, intConstant(0)));
        store(access(ptrOutputVec4Float, glOut, intConstant(0)), position);
        if (clipDistances != 0u) store(clipOut, load(clipArrayType, access(ptrInputClip, clipIn, index)));
        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            store(outputs[i], load(vec4FloatType, access(ptrInputVec4Float, inputs[i], index)));
        }
        for (std::size_t i = 0; i < barycentricOutputs.size(); i++) store(barycentricOutputs[i], load(vec3FloatType, access(ptrInputVec3Float, barycentricInputs[i], index)));
        for (std::size_t i = 0; i < emulation.perVertex.size(); i++) store(perVertexOutputs[i], load(vertexArrayType, access(pointer(spv::StorageClassInput, vertexArrayType), perVertexInputs[i], index)));

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

    std::uint32_t clipVariable(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = addInterface(storage, type);
        decorate(variable, spv::DecorationBuiltIn, spv::BuiltInClipDistance);
        return variable;
    }

    void decorate(std::uint32_t target, spv::Decoration decoration, std::uint32_t value) {
        builder.AddAnnotation(spv::OpDecorate, target, decoration, value);
    }

    void defineEntry(spv::ExecutionModel model) {
        builder.EmitCapability(spv::CapabilityShader);
        builder.EmitCapability(spv::CapabilityTessellation);
        if (clipDistances != 0u) builder.EmitCapability(spv::CapabilityClipDistance);
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
        if (clipDistances != 0u) clipIn = clipVariable(spv::StorageClassInput, array(clipArrayType, tessControl ? 3u : 4u));

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
        for (const auto location : emulation.barycentrics) {
            if (tessControl) continue;
            barycentricInputs.push_back(addInterface(spv::StorageClassInput, array(vec3FloatType, 4u)));
            decorate(barycentricInputs.back(), spv::DecorationLocation, location);
        }
        for (const auto& source : emulation.perVertex) {
            if (!tessControl) {
                perVertexInputs.push_back(addInterface(spv::StorageClassInput, array(vertexArrayType, 4u)));
                decorate(perVertexInputs.back(), spv::DecorationLocation, source.outputLocation);
                continue;
            }
            if (locations[source.inputLocation] == 0) {
                locations[source.inputLocation] = addInterface(spv::StorageClassInput, vertexArrayType);
                decorate(locations[source.inputLocation], spv::DecorationLocation, source.inputLocation);
            }
            perVertexInputs.push_back(locations[source.inputLocation]);
        }
    }

    void defineOutputs(spv::ExecutionModel model) {
        const auto tessControl = model == spv::ExecutionModelTessellationControl;
        if (tessControl) {
            glOut = addInterface(spv::StorageClassOutput, array(perVertexType, 4u));
            if (clipDistances != 0u) clipOut = clipVariable(spv::StorageClassOutput, array(clipArrayType, 4u));
            tessInner = addInterface(spv::StorageClassOutput, array(floatType, 2u));
            decorate(tessInner, spv::DecorationBuiltIn, spv::BuiltInTessLevelInner);
            builder.AddAnnotation(spv::OpDecorate, tessInner, spv::DecorationPatch);
            tessOuter = addInterface(spv::StorageClassOutput, array(floatType, 4u));
            decorate(tessOuter, spv::DecorationBuiltIn, spv::BuiltInTessLevelOuter);
            builder.AddAnnotation(spv::OpDecorate, tessOuter, spv::DecorationPatch);
        } else {
            glOut = addInterface(spv::StorageClassOutput, perVertexType);
            if (clipDistances != 0u) clipOut = clipVariable(spv::StorageClassOutput, clipArrayType);
        }

        outputs.resize(parameters.size());
        for (std::uint32_t i = 0; i < parameters.size(); i++) {
            outputs[i] = addInterface(spv::StorageClassOutput, tessControl ? array(vec4FloatType, 4u) : vec4FloatType);
            decorate(outputs[i], spv::DecorationLocation, parameters[i].outputLocation);
        }
        for (const auto location : emulation.barycentrics) {
            barycentricOutputs.push_back(addInterface(spv::StorageClassOutput, tessControl ? array(vec3FloatType, 4u) : vec3FloatType));
            decorate(barycentricOutputs.back(), spv::DecorationLocation, location);
        }
        for (const auto& source : emulation.perVertex) {
            perVertexOutputs.push_back(addInterface(spv::StorageClassOutput, tessControl ? array(vertexArrayType, 4u) : vertexArrayType));
            decorate(perVertexOutputs.back(), spv::DecorationLocation, source.outputLocation);
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
    std::uint32_t clipDistances;
    std::uint32_t fault = 0;
    const std::vector<Parameter>& parameters;
    const Emulation& emulation;
    std::vector<std::uint32_t> interfaces;
    std::vector<std::uint32_t> inputs;
    std::vector<std::uint32_t> outputs;
    std::vector<std::uint32_t> barycentricInputs;
    std::vector<std::uint32_t> barycentricOutputs;
    std::vector<std::uint32_t> perVertexInputs;
    std::vector<std::uint32_t> perVertexOutputs;
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
    std::uint32_t vertexArrayType = 0;
    std::uint32_t functionType = 0;
    std::uint32_t perVertexType = 0;
    std::uint32_t clipArrayType = 0;
    std::uint32_t ptrInputClip = 0;
    std::uint32_t ptrOutputClip = 0;
    std::uint32_t ptrInputFloat = 0;
    std::uint32_t ptrInputVec4Float = 0;
    std::uint32_t ptrOutputFloat = 0;
    std::uint32_t ptrOutputVec4Float = 0;
    std::uint32_t ptrInputVec3Float = 0;
    std::uint32_t ptrOutputVec3Float = 0;
    std::uint32_t glIn = 0;
    std::uint32_t glOut = 0;
    std::uint32_t clipIn = 0;
    std::uint32_t clipOut = 0;
    std::uint32_t tessInner = 0;
    std::uint32_t tessOuter = 0;
    std::uint32_t tessCoord = 0;
    std::uint32_t invocationId = 0;
};


}

RectListShaders BuildRectListShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target) {
    require(target.tessellation.has_value(), "tessellation shaders are unavailable");
    require(target.spirvVersion >= 0x00010300u && target.spirvVersion <= 0x00010500u, "unsupported SPIR-V target version");
    require(std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityTessellation) != target.supportedCapabilities.end(), "tessellation capability is unavailable");
    std::vector<Parameter> parameters;
    Emulation emulation;
    BarycentricEmulationLayout layout;
    if (fragment.barycentricEmulation.active) {
        layout = LayoutBarycentricEmulation(fragment.fragmentParameters, fragment.barycentricEmulation);
        for (const auto location : {layout.smoothLocation, layout.linearLocation}) {
            if (location != BarycentricEmulationLayout::NoLocation) emulation.barycentrics.push_back(location);
        }
    }
    std::set<std::uint32_t> locations;
    for (const auto& input : fragment.fragmentParameters) {
        require(input.location < 32 && input.sourceLocation < 32 && locations.insert(input.location).second, "invalid fragment parameter location");
        require(!input.custom, "custom per-vertex interpolation is unsupported");
        const bool exported = std::find(vertex.parameterExports.begin(), vertex.parameterExports.end(), input.sourceLocation) != vertex.parameterExports.end();
        if (fragment.barycentricEmulation.active && input.perVertex) {
            require(exported, "an emulated per-vertex parameter is not exported by the vertex shader");
            const auto relocated = std::find_if(layout.perVertexLocations.begin(), layout.perVertexLocations.end(), [&](const auto& entry) { return entry.first == input.location; });
            require(relocated != layout.perVertexLocations.end(), "an emulated per-vertex parameter has no location");
            emulation.perVertex.push_back({input.sourceLocation, relocated->second});
            continue;
        }
        parameters.push_back({input.sourceLocation, input.location, input.flat, !exported});
    }
    const auto emulatedSlots = static_cast<std::uint32_t>(emulation.barycentrics.size() + emulation.perVertex.size() * 3u);
    const auto clipDistances = VertexClipDistanceCount(vertex.spirv.Words());
    const auto& limits = *target.tessellation;
    const std::uint32_t perVertexComponents = std::min({limits.maxControlPerVertexInputComponents, limits.maxControlPerVertexOutputComponents, limits.maxEvaluationInputComponents, limits.maxEvaluationOutputComponents});
    const std::uint32_t parameterSlots = perVertexComponents > clipDistances ? (perVertexComponents - clipDistances) / 4u : 0u;
    require(emulatedSlots == 0u || parameterSlots > emulatedSlots, "emulated barycentrics exceed the tessellation interface limits");
    if (const std::size_t capacity = parameterSlots > emulatedSlots ? parameterSlots - 1u - emulatedSlots : 0u; parameters.size() > capacity) {
        parameters.resize(capacity);
    }
    const auto components = static_cast<std::uint32_t>((parameters.size() + emulatedSlots + 1) * 4 + clipDistances);
    require(limits.maxPatchSize >= 4 && components <= limits.maxControlPerVertexInputComponents && components <= limits.maxControlPerVertexOutputComponents && components <= limits.maxEvaluationInputComponents && components <= limits.maxEvaluationOutputComponents && limits.maxControlPerPatchOutputComponents >= 6 && components * 4 + 6 <= limits.maxControlTotalOutputComponents, "tessellation interface exceeds device limits");
    require(clipDistances == 0u || std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityClipDistance) != target.supportedCapabilities.end(), "the vertex clip distances need shaderClipDistance");
    const auto faultBinding = RuntimeAbi::BindingNumber(RuntimeAbi::Stage::TessellationControl, RuntimeAbi::Binding::FaultBuffer);
    RectListEmitter control(parameters, emulation, spv::ExecutionModelTessellationControl, target.spirvVersion, faultBinding, clipDistances);
    RectListEmitter evaluation(parameters, emulation, spv::ExecutionModelTessellationEvaluation, target.spirvVersion, faultBinding, clipDistances);
    RectListShaders shaders;
    shaders.control.spirv = control.EmitControl(0u);
    shaders.control.bindings.push_back({DescriptorKind::StorageBuffer, DescriptorRole::FaultBuffer, 0, faultBinding, 1, {}});
    shaders.control.bdaAbiVersion = BdaAbi::Version;
    shaders.evaluation.spirv = evaluation.EmitEvaluation();
#if ANYPS5_ENABLE_SPIRV_TOOLS
    shaders.control.spirv = ValidateAndOptimizeSpirv(shaders.control.spirv, target.vulkanVersion, target.spirvVersion);
    shaders.evaluation.spirv = ValidateAndOptimizeSpirv(shaders.evaluation.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    shaders.lastVertexControl = shaders.control;
    if (std::any_of(parameters.begin(), parameters.end(), [](const Parameter& parameter) { return parameter.flat && !parameter.missing; })) {
        RectListEmitter lastVertexControl(parameters, emulation, spv::ExecutionModelTessellationControl, target.spirvVersion, faultBinding, clipDistances);
        shaders.lastVertexControl.spirv = lastVertexControl.EmitControl(2u);
#if ANYPS5_ENABLE_SPIRV_TOOLS
        shaders.lastVertexControl.spirv = ValidateAndOptimizeSpirv(shaders.lastVertexControl.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    }
    return shaders;
}

}
