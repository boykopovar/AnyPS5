#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTERHELPERS_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTERHELPERS_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include "Optimization/BindingAllocator.hpp"

namespace ShaderRecompiler {

const RdnaImageDimensionInfo& RdnaImageDimensionInfoFor(RdnaImageDimension dimension);
std::uint32_t TypeVoid(SpirvEmitterState& state);
std::uint32_t TypeBool(SpirvEmitterState& state);
std::uint32_t TypeBoolVector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeU32(SpirvEmitterState& state);
std::uint32_t TypeU64(SpirvEmitterState& state);
std::uint32_t TypeScalarU64(SpirvEmitterState& state);
std::uint32_t TypeU32Pair(SpirvEmitterState& state);
std::uint32_t TypeI32(SpirvEmitterState& state);
std::uint32_t TypeI32Pair(SpirvEmitterState& state);
std::uint32_t TypeF32(SpirvEmitterState& state);
std::uint32_t TypeU32Vector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeU32Composite(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeI32Vector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeF32Vector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypePointer(SpirvEmitterState& state, std::uint32_t storageClass, std::uint32_t pointee);
std::uint32_t TypeFunction(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferPointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferElementPointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU64Pointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU64ElementPointer(SpirvEmitterState& state);
std::uint32_t TypePhysicalU32Pointer(SpirvEmitterState& state);
std::uint32_t TypePushConstantElementPointer(SpirvEmitterState& state);
std::uint32_t TypeU32ArrayPointer(SpirvEmitterState& state, std::uint32_t storageClass, std::uint32_t dwords);
std::uint32_t TypeU32ElementPointer(SpirvEmitterState& state, std::uint32_t storageClass);
std::uint32_t TypeId(SpirvEmitterState& state, IrType type);
std::uint32_t GlslStd450(SpirvEmitterState& state);
std::uint32_t PixelParameterLocation(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsFlat(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsCustom(const SpirvEmitterState& state, std::uint32_t attr);
VertexInputScalarKind VertexParameterScalarKind(const SpirvEmitterState& state, std::uint32_t location);
std::uint32_t VertexParameterComponentCount(const SpirvInputBinding& input);
std::uint32_t VertexParameterScalarType(SpirvEmitterState& state, VertexInputScalarKind kind);
std::uint32_t OutputVariableForExport(const SpirvEmitterState& state, const ExportInfo& exp);
std::uint32_t ConstantU32(SpirvEmitterState& state, std::uint32_t value);
std::uint32_t EmitSubgroupLocalInvocationId(SpirvEmitterState& state);
[[noreturn]] void ExitDescriptorBindingFailure(const SpirvEmitterState& state, DescriptorBindingKind kind, std::uint32_t resource, const char* reason);
std::uint32_t ResourceForDescriptor(const SpirvEmitterState& state, DescriptorBindingKind kind, std::uint32_t resource);
std::uint32_t DescriptorElementPointer(SpirvEmitterState& state, std::uint32_t resultPtrType, std::uint32_t variableId, std::uint32_t arrayIndex, DescriptorBindingKind kind, std::uint32_t resource, const char* variableName);
std::uint32_t ImageScalarType(SpirvEmitterState& state, IrTextureNumericClass numericClass);
std::uint32_t ImageVectorType(SpirvEmitterState& state, IrTextureNumericClass numericClass, std::uint32_t components);
std::uint32_t ImageType(SpirvEmitterState& state, const ImageResource& image);
std::uint32_t ImageViewSizeType(SpirvEmitterState& state, RdnaImageDimension dimension);
std::uint32_t LoadSampledImageDescriptor(SpirvEmitterState& state, std::uint32_t resource);
std::uint32_t LoadSamplerDescriptor(SpirvEmitterState& state, std::uint32_t sampler);
std::uint32_t MakeSampledImage(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t sampler);
std::uint32_t StorageImageDescriptorPointer(SpirvEmitterState& state, std::uint32_t resource);
void EmitStorageImageWrite(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t mipLod, std::uint32_t coord, std::uint32_t texel);
std::uint32_t ExecutionModelForStage(IrShaderStage stage);
std::uint32_t ConstantI32(SpirvEmitterState& state, std::int32_t value);
std::uint32_t ConstantF32(SpirvEmitterState& state, std::uint32_t bits);
std::uint32_t FloatBits(float value);
std::uint32_t ConstantF32Value(SpirvEmitterState& state, float value);
std::uint32_t ConstantBool(SpirvEmitterState& state, bool value);
std::uint32_t ConstantU64(SpirvEmitterState& state, std::uint64_t value);
std::uint32_t ConstantU32CompositeZero(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t DefineInterfaceVariable(SpirvEmitterState& state, std::uint32_t type, std::uint32_t storage, const char* name);
void CheckBindings(const IrProgram& program, const BindingAllocationResult& bindings);
void EmitBaseHeader(SpirvModule& module, const IrProgram& program);
void DefineInputs(SpirvEmitterState& state);
void DefineOutputs(SpirvEmitterState& state);
void DefineDescriptors(SpirvEmitterState& state);
void DefineModule(SpirvEmitterState& state);
void DefineTessellationInterfaces(SpirvEmitterState& state);
void DefineTessellationExecutionModes(SpirvEmitterState& state);
void DefineMeshOutputs(SpirvEmitterState& state);
void EmitMeshEntryPoint(SpirvEmitterState& state);
void EmitMeshAllocate(SpirvValueEmitContext& ctx, const IrValue& inst);
std::uint32_t MeshOutputPointer(SpirvEmitterState& state, StageOutputKind kind, std::uint32_t index = 0);
std::uint32_t MeshPrimitivePointer(SpirvEmitterState& state);
DppTargetLane EmitDppQuadPermTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t control);
DppTargetLane EmitDppRowShiftTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t amount, bool left);
DppTargetLane EmitDppRowRotateRightTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t amount);
DppTargetLane EmitDppMirrorTargetLane(SpirvEmitterState& state, std::uint32_t subid, bool halfRow);
DppTargetLane EmitDppTargetLane(SpirvEmitterState& state, std::uint32_t control);
std::uint32_t InputVariableForKind(const SpirvEmitterState& state, StageInputKind kind);
const SpirvInputBinding* SpirvInputBindingForParameter(const SpirvEmitterState& state, std::uint32_t location);
std::uint32_t EmitVertexParameterComponentU32(SpirvEmitterState& state, const SpirvInputBinding& input, std::uint32_t component);
std::uint32_t EmitInputComponentU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component);
std::uint32_t EmitLocalInvocationIndex(SpirvEmitterState& state);
std::uint32_t EmitBallotLaneActiveBool(SpirvEmitterState& state, std::uint32_t ballot, std::uint32_t lane);
std::uint32_t EmitSubgroupLaneActiveBool(SpirvEmitterState& state, std::uint32_t lane);
std::uint32_t EmitBinaryU32(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t lhs, std::uint32_t rhs);
std::uint32_t EmitShaderDataDwordLoad(SpirvEmitterState& state, std::uint32_t dwordIndex);
std::uint32_t StorageBufferPackedStride(const SpirvEmitterState& state, const MemoryInfo& mem);
IrBufferFormat StorageBufferFormat(const SpirvEmitterState& state, const MemoryInfo& mem);
void EmitMemoryOffsets(SpirvEmitterState& state);
std::uint32_t LdsDwordCount(const SpirvEmitterState& state);
MemoryResourceAccess PrepareMemoryResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem);
MemoryResourceAccess PrepareStorageBufferResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem, std::uint32_t variable, std::uint32_t pointerType);
std::uint32_t EmitMemoryElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t rawIndex);
std::uint32_t EmitMemoryElementInBounds(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index);
std::uint32_t EmitMemoryElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index);
std::uint32_t EmitStorageBufferElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index, std::uint32_t pointerType);
std::uint32_t EmitTBufferBitcastU32ToI32(SpirvEmitterState& state, std::uint32_t value);
bool IsSignedFormatComponent(SpirvFormatComponentType type);
std::uint32_t EmitUFloatToF32Bits(SpirvEmitterState& state, std::uint32_t raw, std::uint32_t bits);
std::uint32_t NormalizeFormatComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t raw);
void EmitDeviceAtomicMemoryBarrier(SpirvEmitterState& state);
std::uint32_t EmitFloatAtomicReplacement(SpirvEmitterState& state, std::uint32_t old, std::uint32_t source, bool maxValue);
std::uint32_t EmitDsSwizzleTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t control);
std::uint32_t EmitAndConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t mask);
std::uint32_t EmitShiftRightConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift);
std::uint32_t EmitCompareU32Constant(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t value, std::uint32_t constant);
std::uint32_t EmitSubConstantMinusU32(SpirvEmitterState& state, std::uint32_t constant, std::uint32_t value);
std::uint32_t EmitF32ToF16RtzBits(SpirvEmitterState& state, std::uint32_t f32);
std::uint32_t EmitMinMaxU32Value(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue);
std::uint32_t EmitMinMaxI32Value(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue);
F32Class EmitClassifyF32Bits(SpirvEmitterState& state, std::uint32_t bits);
F32Class EmitClassifyF32(SpirvEmitterState& state, std::uint32_t value);
std::uint32_t EmitClassMaskBitMatch(SpirvEmitterState& state, std::uint32_t mask, std::uint32_t bit, std::uint32_t classMatch);
std::uint32_t EmitClassMaskF32(SpirvEmitterState& state, std::uint32_t value, std::uint32_t mask);
std::uint32_t EmitMinMaxF32Value(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue);
std::uint32_t EmitFlushF32DenormToSignedZero(SpirvEmitterState& state, std::uint32_t value);
std::uint32_t EmitTrigCycleF32(SpirvEmitterState& state, std::uint32_t src, bool preserveSignedZero);
std::uint32_t EmitF16BitsToF32(SpirvEmitterState& state, std::uint32_t bits);
void EmitProgram(SpirvEmitterState& state);
void DefineGetBdaPointer(SpirvEmitterState& state);
void EmitLabel(SpirvEmitterState& state, std::uint32_t label);
std::uint32_t Unary(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t type, std::uint32_t value);
std::uint32_t Binary(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t type, std::uint32_t lhs, std::uint32_t rhs);
std::uint32_t Select(SpirvEmitterState& state, std::uint32_t type, std::uint32_t condition, std::uint32_t trueValue, std::uint32_t falseValue);

template<std::uint32_t TOpcode, IrType TValueType, typename... TArguments>
std::uint32_t EmitNative(SpirvEmitterState& state, TArguments... arguments) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(TOpcode, TypeId(state, TValueType), result, arguments...);
    return result;
}

template<std::uint32_t TOpcode, IrType TValueType, typename... TArguments>
std::uint32_t EmitGlsl(SpirvEmitterState& state, TArguments... arguments) {
    return EmitNative<spv::OpExtInst, TValueType>(state, GlslStd450(state), TOpcode, arguments...);
}

template<typename TFunction>
void EmitIfCondition(SpirvEmitterState& state, std::uint32_t condition, TFunction&& function) {
    const auto thenLabel = state.module.AllocateId();
    const auto mergeLabel = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, condition, thenLabel, mergeLabel);
    EmitLabel(state, thenLabel);
    function();
    state.module.AddFunction(spv::OpBranch, mergeLabel);
    EmitLabel(state, mergeLabel);
}

template<typename TFunction>
std::uint32_t EmitValueOrDefaultIfCondition(SpirvEmitterState& state, std::uint32_t condition, std::uint32_t type, std::uint32_t defaultValue, TFunction&& function) {
    const auto thenLabel = state.module.AllocateId();
    const auto thenExit = state.module.AllocateId();
    const auto elseLabel = state.module.AllocateId();
    const auto mergeLabel = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, condition, thenLabel, elseLabel);
    EmitLabel(state, thenLabel);
    const auto thenValue = function();
    state.module.AddFunction(spv::OpBranch, thenExit);
    EmitLabel(state, thenExit);
    state.module.AddFunction(spv::OpBranch, mergeLabel);
    EmitLabel(state, elseLabel);
    state.module.AddFunction(spv::OpBranch, mergeLabel);
    EmitLabel(state, mergeLabel);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, type, value, thenValue, thenExit, defaultValue, elseLabel);
    return value;
}

template<typename TFunction>
std::uint32_t EmitValueOrZeroIfCondition(SpirvEmitterState& state, std::uint32_t condition, TFunction&& function) {
    return EmitValueOrDefaultIfCondition(state, condition, TypeU32(state), ConstantU32(state, 0u), std::forward<TFunction>(function));
}

// Same structure with both arms emitting code; each arm gets its own exit block so the phi stays
// valid when an arm opens nested selections.
template<typename TThen, typename TElse>
std::uint32_t EmitValueIfElse(SpirvEmitterState& state, std::uint32_t condition, std::uint32_t type, TThen&& thenFunction, TElse&& elseFunction) {
    const auto thenLabel = state.module.AllocateId();
    const auto thenExit = state.module.AllocateId();
    const auto elseLabel = state.module.AllocateId();
    const auto elseExit = state.module.AllocateId();
    const auto mergeLabel = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, condition, thenLabel, elseLabel);
    EmitLabel(state, thenLabel);
    const auto thenValue = thenFunction();
    state.module.AddFunction(spv::OpBranch, thenExit);
    EmitLabel(state, thenExit);
    state.module.AddFunction(spv::OpBranch, mergeLabel);
    EmitLabel(state, elseLabel);
    const auto elseValue = elseFunction();
    state.module.AddFunction(spv::OpBranch, elseExit);
    EmitLabel(state, elseExit);
    state.module.AddFunction(spv::OpBranch, mergeLabel);
    EmitLabel(state, mergeLabel);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, type, value, thenValue, thenExit, elseValue, elseExit);
    return value;
}

template<typename TFunction>
std::uint32_t AtomicUpdate(SpirvEmitterState& state, std::uint32_t pointer, ResourceKind kind, TFunction&& function) {
    const auto scope = kind == ResourceKind::Lds ? spv::ScopeWorkgroup : spv::ScopeDevice;
    const auto memory = [&]() {
        switch (kind) {
        case ResourceKind::Lds:
            return spv::MemorySemanticsWorkgroupMemoryMask;
        case ResourceKind::Image:
            return spv::MemorySemanticsImageMemoryMask;
        default:
            return spv::MemorySemanticsUniformMemoryMask;
        }
    }();
    const auto preheader = state.module.AllocateId();
    const auto header = state.module.AllocateId();
    const auto cont = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    const auto initial = state.module.AllocateId();
    const auto observed = state.module.AllocateId();
    const auto exchanged = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, preheader);
    EmitLabel(state, preheader);
    state.module.AddFunction(spv::OpAtomicLoad, TypeU32(state), initial, pointer, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsMaskNone));
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    state.module.AddFunction(spv::OpPhi, TypeU32(state), observed, initial, preheader, exchanged, cont);
    const auto next = function(observed);
    state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), exchanged, pointer, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, spv::MemorySemanticsMaskNone), next, observed);
    const auto success = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), success, exchanged, observed);
    state.module.AddFunction(spv::OpLoopMerge, merge, cont, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, success, merge, cont);
    EmitLabel(state, cont);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    state.module.AddFunction(spv::OpMemoryBarrier, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | memory));
    return observed;
}

}

#endif
