#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"

namespace ShaderRecompiler {

std::uint32_t BdaConstant(SpirvEmitterState& state, std::uint64_t value) {
    return state.module.Constant(spv::OpConstant, TypeScalarU64(state), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u));
}

std::uint32_t BdaWord(SpirvEmitterState& state, std::uint32_t variable, std::uint32_t index) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, variable, ConstantU32(state, 0u), index);
    return pointer;
}

std::uint32_t BdaLoadWord(SpirvEmitterState& state, std::uint32_t index) {
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, BdaWord(state, state.bdaPagetableVariable, index));
    return value;
}

std::uint32_t BdaLoadAddress(SpirvEmitterState& state, std::uint32_t index) {
    const auto low = Unary(state, spv::OpUConvert, TypeScalarU64(state), BdaLoadWord(state, index));
    const auto high = Unary(state, spv::OpUConvert, TypeScalarU64(state), BdaLoadWord(state, Binary(state, spv::OpIAdd, TypeU32(state), index, ConstantU32(state, 1u))));
    return Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), low, Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), high, BdaConstant(state, 32u)));
}

void RecordBdaFault(SpirvEmitterState& state, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction, BdaAbi::FaultReason reason) {
    const auto pointer = BdaWord(state, state.faultBufferVariable, ConstantU32(state, 0u));
    const auto previous = state.module.AllocateId();
    const auto scope = ConstantU32(state, spv::ScopeDevice);
    const auto relaxed = ConstantU32(state, spv::MemorySemanticsMaskNone);
    state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), previous, pointer, scope, relaxed, relaxed, ConstantU32(state, static_cast<std::uint32_t>(BdaAbi::FaultState::Writing)), ConstantU32(state, 0u));
    const auto won = Binary(state, spv::OpIEqual, TypeBool(state), previous, ConstantU32(state, 0u));
    EmitIfCondition(state, won, [&] {
        const auto store = [&](std::uint32_t index, std::uint32_t value) {
            state.module.AddFunction(spv::OpStore, BdaWord(state, state.faultBufferVariable, ConstantU32(state, index)), value);
        };
        store(1, ConstantU32(state, static_cast<std::uint32_t>(reason)));
        store(2, Unary(state, spv::OpUConvert, TypeU32(state), address));
        store(3, Unary(state, spv::OpUConvert, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeScalarU64(state), address, BdaConstant(state, 32u))));
        store(4, bytes);
        store(5, ConstantU32(state, static_cast<std::uint32_t>(state.program.Resources().stage)));
        store(6, instruction);
        store(7, ConstantU32(state, 0u));
        const auto release = ConstantU32(state, spv::MemorySemanticsReleaseMask | spv::MemorySemanticsUniformMemoryMask);
        state.module.AddFunction(spv::OpAtomicStore, pointer, scope, release, ConstantU32(state, static_cast<std::uint32_t>(BdaAbi::FaultState::Ready)));
    });
}

void ReturnBdaFailureIf(SpirvEmitterState& state, std::uint32_t condition, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction, BdaAbi::FaultReason reason) {
    const auto failed = state.module.AllocateId();
    const auto next = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, next, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, condition, failed, next);
    EmitLabel(state, failed);
    RecordBdaFault(state, address, bytes, instruction, reason);
    state.module.AddFunction(spv::OpReturnValue, BdaConstant(state, 0u));
    EmitLabel(state, next);
}

void StopBdaInvocationIf(SpirvEmitterState& state, std::uint32_t condition) {
    if (!state.bdaStopsInvocations) return;
    const auto failed = state.module.AllocateId();
    const auto next = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, next, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, condition, failed, next);
    EmitLabel(state, failed);
    state.module.AddFunction(state.program.Resources().stage == IrShaderStage::Pixel ? spv::OpKill : spv::OpReturn);
    EmitLabel(state, next);
}

}
