#include "SpirvBackend/SpirvFlowEmitter.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <spirv/unified1/spirv.hpp>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ShaderRecompiler {
namespace {

void EmitKillIfBoolFalse(SpirvEmitterState& state, std::uint32_t active) {
    const auto killLabel = state.module.AllocateId();
    const auto mergeLabel = state.module.AllocateId();
    const auto inactive = state.module.AllocateId();
    state.module.AddFunction(spv::OpLogicalNot, TypeBool(state), inactive, active);
    state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, inactive, killLabel, mergeLabel);
    EmitLabel(state, killLabel);
    state.module.AddFunction(spv::OpKill);
    EmitLabel(state, mergeLabel);
}

void EmitKillIfPixelValidMaskInactive(SpirvEmitterState& state) {
    if (state.pixelValidMaskVariable == 0) {
        return;
    }
    const auto maskValue = state.module.AllocateId();
    const auto active = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), maskValue, state.pixelValidMaskVariable);
    state.module.AddFunction(spv::OpINotEqual, TypeBool(state), active, maskValue, ConstantU32(state, 0u));
    EmitKillIfBoolFalse(state, active);
}

// Block metadata is paired with BlockOrder by position (as IrProgram validation does); terminator
// targets name control-flow ids, which are not IR block ids.
const IrBlock* TargetBlock(const IrProgram& program, std::uint32_t id) {
    const auto& blocks = program.BlockOrder();
    const auto& blockInfo = program.Metadata().blockInfo;
    if (blocks.size() != blockInfo.size()) {
        throw std::runtime_error("SPIR-V control flow block metadata is inconsistent");
    }
    for (std::size_t index = 0; index < blockInfo.size(); index++) {
        if (blockInfo[index].id == id) {
            if (blocks[index] == nullptr) {
                throw std::runtime_error("SPIR-V control flow target block is null");
            }
            return blocks[index];
        }
    }
    throw std::runtime_error("SPIR-V control flow target block is missing");
}

const BlockInfo* BlockInfoFor(const IrProgram& program, const IrBlock* block) {
    const auto& blocks = program.BlockOrder();
    const auto& blockInfo = program.Metadata().blockInfo;
    if (blocks.size() != blockInfo.size()) {
        throw std::runtime_error("SPIR-V control flow block metadata is inconsistent");
    }
    if (block == nullptr) {
        throw std::runtime_error("SPIR-V control flow block is null");
    }
    for (std::size_t index = 0; index < blocks.size(); index++) {
        if (blocks[index] == block) {
            return &blockInfo[index];
        }
    }
    throw std::runtime_error("SPIR-V control flow block has no metadata");
}

void EmitReturnTerminator(SpirvValueEmitContext& ctx) {
    auto& state = ctx.state;
    if (state.loopGuardLimit != 0 && state.faultBufferVariable != 0) {
        const auto pc = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), pc, state.loopGuardPc);
        EmitIfCondition(state, Binary(state, spv::OpINotEqual, TypeBool(state), pc, ConstantU32(state, 0u)), [&] {
            RecordBdaFault(state, BdaConstant(state, 0u), ConstantU32(state, state.loopGuardLimit), EmitBinaryU32(state, spv::OpISub, pc, ConstantU32(state, 1u)), BdaAbi::FaultReason::LoopLimit);
        });
    }
    EmitKillIfPixelValidMaskInactive(state);
    state.module.AddFunction(spv::OpReturn);
}

// The loop guard (see SpirvEmitterState::loopGuardLimit) at a branch that may leave a loop: counts the
// evaluation and, past the limit, takes the exit and remembers the block's PC (plus one, 0 = none).
std::uint32_t GuardLoopExit(SpirvEmitterState& state, const BlockInfo& info, std::uint32_t condition, bool exitWhenTrue) {
    const auto visits = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), visits, state.loopGuardVisits);
    const auto next = EmitBinaryU32(state, spv::OpIAdd, visits, ConstantU32(state, 1u));
    state.module.AddFunction(spv::OpStore, state.loopGuardVisits, next);
    const auto tripped = Binary(state, spv::OpUGreaterThan, TypeBool(state), next, ConstantU32(state, state.loopGuardLimit));
    const auto previous = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), previous, state.loopGuardPc);
    const auto first = Binary(state, spv::OpLogicalAnd, TypeBool(state), tripped, Binary(state, spv::OpIEqual, TypeBool(state), previous, ConstantU32(state, 0u)));
    const auto pc = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, TypeU32(state), pc, first, ConstantU32(state, info.startPc + 1u), previous);
    state.module.AddFunction(spv::OpStore, state.loopGuardPc, pc);
    if (exitWhenTrue) return Binary(state, spv::OpLogicalOr, TypeBool(state), condition, tripped);
    const auto stay = state.module.AllocateId();
    state.module.AddFunction(spv::OpLogicalNot, TypeBool(state), stay, tripped);
    return Binary(state, spv::OpLogicalAnd, TypeBool(state), condition, stay);
}

bool IsLoopMerge(const IrProgram& program, std::uint32_t block) {
    for (const auto& info : program.Metadata().blockInfo) {
        if (info.terminator.loopHeader && info.terminator.mergeBlock == block) return true;
    }
    return false;
}

std::uint32_t EmitBranchCondition(SpirvValueEmitContext& ctx, const BlockInfo& info) {
    if (ctx.otherHalf == nullptr || info.terminator.condition == BranchCondition::ScalarInstruction || info.terminator.condition == BranchCondition::GotoVariable) {
        return ctx.Def(info.condition);
    }
    auto& state = ctx.state;
    const auto ballot = ctx.Ballot(info.condition);
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1u);
    const auto kind = info.terminator.condition;
    const bool zero = kind == BranchCondition::ExecZero || kind == BranchCondition::VccZero || kind == BranchCondition::SccZero;
    const auto combined = EmitBinaryU32(state, zero ? spv::OpBitwiseAnd : spv::OpBitwiseOr, low, high);
    state.module.AddFunction(zero ? spv::OpIEqual : spv::OpINotEqual, TypeBool(state), result, combined, ConstantU32(state, zero ? ~0u : 0u));
    return result;
}

void EmitStructuredTerminator(SpirvValueEmitContext& ctx, const IrProgram& program, const BlockInfo& info) {
    auto& state = ctx.state;
    const Terminator& term = info.terminator;
    const auto emitMerge = [&]() {
        if (term.loopHeader) {
            const IrBlock* merge = TargetBlock(program, term.mergeBlock);
            const IrBlock* cont = TargetBlock(program, term.continueBlock);
            if (merge != nullptr && cont != nullptr) {
                state.module.AddFunction(spv::OpLoopMerge, ctx.Label(merge), ctx.Label(cont), spv::LoopControlMaskNone);
            }
        } else if (term.kind == TerminatorKind::ConditionalBranch && term.mergeBlock != InvalidControlFlowId) {
            if (const IrBlock* merge = TargetBlock(program, term.mergeBlock); merge != nullptr) {
                state.module.AddFunction(spv::OpSelectionMerge, ctx.Label(merge), spv::SelectionControlMaskNone);
            }
        }
    };
    switch (term.kind) {
        case TerminatorKind::Branch: {
            const IrBlock* target = TargetBlock(program, term.trueBlock);
            if (target == nullptr) {
                EmitReturnTerminator(ctx);
                return;
            }
            emitMerge();
            state.module.AddFunction(spv::OpBranch, ctx.Label(target));
            return;
        }
        case TerminatorKind::ConditionalBranch: {
            const IrBlock* trueBlock = TargetBlock(program, term.trueBlock);
            const IrBlock* falseBlock = TargetBlock(program, term.falseBlock);
            if (trueBlock == nullptr || falseBlock == nullptr || info.condition == nullptr) {
                EmitReturnTerminator(ctx);
                return;
            }
            if (const IrValue* resolved = info.condition->Resolve(); !resolved->HasImmediate() && !ctx.definitions.contains(resolved)) {
                const std::string where = resolved->Parent() == nullptr ? std::string("no block") : "block " + std::to_string(resolved->Parent()->Id());
                const std::string message = "branch condition " + std::string(IrOpcodeName(resolved->Opcode())) + " of block " + std::to_string(info.id) + " (defined in " + where + ") was not emitted before the branch";
                ctx.Fail(message.c_str());
            }
            auto condition = EmitBranchCondition(ctx, info);
            if (state.loopGuardLimit != 0 && term.trueBlock != term.falseBlock) {
                if (IsLoopMerge(program, term.trueBlock)) condition = GuardLoopExit(state, info, condition, true);
                else if (IsLoopMerge(program, term.falseBlock)) condition = GuardLoopExit(state, info, condition, false);
            }
            static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
            if (debug) std::fprintf(stderr, "[spirv] block %u branches on %%%u = %s (kind %d)\n", static_cast<unsigned>(info.id), condition, std::string(IrOpcodeName(info.condition->Resolve()->Opcode())).c_str(), static_cast<int>(info.terminator.condition));
            emitMerge();
            state.module.AddFunction(spv::OpBranchConditional, condition, ctx.Label(trueBlock), ctx.Label(falseBlock));
            return;
        }
        default:
            EmitReturnTerminator(ctx);
            return;
    }
}

template<typename TArgument>
decltype(auto) DispatchArgument(SpirvValueEmitContext& ctx, const IrValue& inst, std::size_t index) {
    if constexpr (std::is_same_v<TArgument, const IrValue&>) {
        return inst;
    } else if constexpr (std::is_same_v<TArgument, const IrValue*>) {
        return static_cast<const IrValue*>(inst.Argument(index));
    } else if constexpr (std::is_same_v<TArgument, ScalarReg>) {
        return static_cast<ScalarReg>(inst.Argument(index)->Register().index);
    } else {
        static_assert(std::is_same_v<TArgument, std::uint32_t>);
        return ctx.Def(inst.Argument(index));
    }
}

template<typename TContext, typename TReturn, typename... TArguments>
void Invoke(TReturn (*emit)(TContext&, TArguments...), SpirvValueEmitContext& ctx, const IrValue& inst) {
    static_assert(std::is_same_v<TContext, SpirvValueEmitContext> || std::is_same_v<TContext, SpirvEmitterState>);
    auto& context = [&]() -> TContext& {
        if constexpr (std::is_same_v<TContext, SpirvEmitterState>) {
            return ctx.state;
        } else {
            return ctx;
        }
    }();
    constexpr bool hasInst = (std::is_same_v<TArguments, const IrValue&> || ...);
    [&]<std::size_t... TIndices>(std::index_sequence<TIndices...>) {
        static_assert(((!std::is_same_v<TArguments, const IrValue&> || TIndices == 0) && ...));
        const auto call = [&] {
            return emit(context, DispatchArgument<TArguments>(ctx, inst, TIndices - (hasInst && TIndices != 0 ? 1u : 0u))...);
        };
        if constexpr (std::is_void_v<TReturn>) {
            call();
        } else {
            static_assert(std::is_same_v<TReturn, std::uint32_t>);
            ctx.Define(inst, call());
        }
    }(std::index_sequence_for<TArguments...>{});
}

void EmitDirectInstruction(SpirvValueEmitContext& ctx, const IrValue& inst) {
    switch (inst.Opcode()) {
        case IrOpcode::Identity: return Invoke(EmitIdentity, ctx, inst);
        case IrOpcode::Phi: return Invoke(EmitPhi, ctx, inst);
        case IrOpcode::IAdd32: return Invoke(EmitIAdd32, ctx, inst);
        case IrOpcode::ISub32: return Invoke(EmitISub32, ctx, inst);
        case IrOpcode::IMul32: return Invoke(EmitIMul32, ctx, inst);
        case IrOpcode::Unreachable: return Invoke(EmitUnreachable, ctx, inst);
        case IrOpcode::Barrier: return Invoke(EmitBarrier, ctx, inst);
        case IrOpcode::ReadFirstLane: return Invoke(EmitReadFirstLane, ctx, inst);
        case IrOpcode::ReadLane: return Invoke(EmitReadLane, ctx, inst);
        case IrOpcode::Ballot: return Invoke(EmitBallot, ctx, inst);
        case IrOpcode::Void: return Invoke(EmitVoid, ctx, inst);
        case IrOpcode::Reference: return Invoke(EmitReference, ctx, inst);
        case IrOpcode::ReferenceU32: return Invoke(EmitReferenceU32, ctx, inst);
        case IrOpcode::GetUserData: return Invoke(EmitGetUserData, ctx, inst);
        case IrOpcode::GetShaderBase: return Invoke(EmitGetShaderBase, ctx, inst);
        case IrOpcode::MeshDrawParameter: return Invoke(EmitMeshDrawParameter, ctx, inst);
        case IrOpcode::MeshAllocate: return Invoke(EmitMeshAllocate, ctx, inst);
        case IrOpcode::TessellationBase: return Invoke(EmitTessellationBase, ctx, inst);
        case IrOpcode::GetTessellationAttribute: return Invoke(EmitGetTessellationAttribute, ctx, inst);
        case IrOpcode::SetTessellationAttribute: return Invoke(EmitSetTessellationAttribute, ctx, inst);
        case IrOpcode::GetBuiltin: return Invoke(EmitGetBuiltin, ctx, inst);
        case IrOpcode::GetThreadBitScalarRegister: return Invoke(EmitGetThreadBitScalarRegister, ctx, inst);
        case IrOpcode::SetThreadBitScalarRegister: return Invoke(EmitSetThreadBitScalarRegister, ctx, inst);
        case IrOpcode::GetScalarMaskTag: return Invoke(EmitGetScalarMaskTag, ctx, inst);
        case IrOpcode::SetScalarMaskTag: return Invoke(EmitSetScalarMaskTag, ctx, inst);
        case IrOpcode::GetScalarRegister: return Invoke(EmitGetScalarRegister, ctx, inst);
        case IrOpcode::SetScalarRegister: return Invoke(EmitSetScalarRegister, ctx, inst);
        case IrOpcode::GetVectorRegister: return Invoke(EmitGetVectorRegister, ctx, inst);
        case IrOpcode::SetVectorRegister: return Invoke(EmitSetVectorRegister, ctx, inst);
        case IrOpcode::GetGotoVariable: return Invoke(EmitGetGotoVariable, ctx, inst);
        case IrOpcode::SetGotoVariable: return Invoke(EmitSetGotoVariable, ctx, inst);
        case IrOpcode::GetScc: return Invoke(EmitGetScc, ctx, inst);
        case IrOpcode::SetScc: return Invoke(EmitSetScc, ctx, inst);
        case IrOpcode::GetExec: return Invoke(EmitGetExec, ctx, inst);
        case IrOpcode::SetExec: return Invoke(EmitSetExec, ctx, inst);
        case IrOpcode::GetExecLo: return Invoke(EmitGetExecLo, ctx, inst);
        case IrOpcode::SetExecLo: return Invoke(EmitSetExecLo, ctx, inst);
        case IrOpcode::GetExecHi: return Invoke(EmitGetExecHi, ctx, inst);
        case IrOpcode::SetExecHi: return Invoke(EmitSetExecHi, ctx, inst);
        case IrOpcode::GetVcc: return Invoke(EmitGetVcc, ctx, inst);
        case IrOpcode::SetVcc: return Invoke(EmitSetVcc, ctx, inst);
        case IrOpcode::GetVccLo: return Invoke(EmitGetVccLo, ctx, inst);
        case IrOpcode::SetVccLo: return Invoke(EmitSetVccLo, ctx, inst);
        case IrOpcode::GetVccHi: return Invoke(EmitGetVccHi, ctx, inst);
        case IrOpcode::SetVccHi: return Invoke(EmitSetVccHi, ctx, inst);
        case IrOpcode::GetM0: return Invoke(EmitGetM0, ctx, inst);
        case IrOpcode::SetM0: return Invoke(EmitSetM0, ctx, inst);
        case IrOpcode::UndefU1: return Invoke(EmitUndefU1, ctx, inst);
        case IrOpcode::UndefU8: return Invoke(EmitUndefU8, ctx, inst);
        case IrOpcode::UndefU16: return Invoke(EmitUndefU16, ctx, inst);
        case IrOpcode::UndefU32: return Invoke(EmitUndefU32, ctx, inst);
        case IrOpcode::UndefU64: return Invoke(EmitUndefU64, ctx, inst);
        case IrOpcode::BitCastU16F16: return Invoke(EmitBitCastU16F16, ctx, inst);
        case IrOpcode::BitCastF16U16: return Invoke(EmitBitCastF16U16, ctx, inst);
        case IrOpcode::BitCastU32F32: return Invoke(EmitBitCastU32F32, ctx, inst);
        case IrOpcode::BitCastF32U32: return Invoke(EmitBitCastF32U32, ctx, inst);
        case IrOpcode::ConvertU16U32: return Invoke(EmitConvertU16U32, ctx, inst);
        case IrOpcode::ConvertU32U16: return Invoke(EmitConvertU32U16, ctx, inst);
        case IrOpcode::ConvertU8U32: return Invoke(EmitConvertU8U32, ctx, inst);
        case IrOpcode::ConvertU32U8: return Invoke(EmitConvertU32U8, ctx, inst);
        case IrOpcode::ConvertF32F16: return Invoke(EmitConvertF32F16, ctx, inst);
        case IrOpcode::ConvertF16F32: return Invoke(EmitConvertF16F32, ctx, inst);
        case IrOpcode::ConvertS32F32: return Invoke(EmitConvertS32F32, ctx, inst);
        case IrOpcode::ConvertU32F32: return Invoke(EmitConvertU32F32, ctx, inst);
        case IrOpcode::ConvertF32S32: return Invoke(EmitConvertF32S32, ctx, inst);
        case IrOpcode::ConvertF32U32: return Invoke(EmitConvertF32U32, ctx, inst);
        case IrOpcode::CompositeConstructU64: return Invoke(EmitCompositeConstructU64, ctx, inst);
        case IrOpcode::CompositeConstructU32x2: return Invoke(EmitCompositeConstructU32x2, ctx, inst);
        case IrOpcode::CompositeConstructU32x3: return Invoke(EmitCompositeConstructU32x3, ctx, inst);
        case IrOpcode::CompositeConstructF32x2: return Invoke(EmitCompositeConstructF32x2, ctx, inst);
        case IrOpcode::CompositeConstructU32x4: return Invoke(EmitCompositeConstructU32x4, ctx, inst);
        case IrOpcode::CompositeExtractU64: return Invoke(EmitCompositeExtractU64, ctx, inst);
        case IrOpcode::CompositeExtractU32x2: return Invoke(EmitCompositeExtractU32x2, ctx, inst);
        case IrOpcode::CompositeExtractU32x3: return Invoke(EmitCompositeExtractU32x3, ctx, inst);
        case IrOpcode::CompositeExtractU32x4: return Invoke(EmitCompositeExtractU32x4, ctx, inst);
        case IrOpcode::PackHalf2x16: return Invoke(EmitPackHalf2x16, ctx, inst);
        case IrOpcode::PackSnorm2x16: return Invoke(EmitPackSnorm2x16, ctx, inst);
        case IrOpcode::PackUnorm2x16: return Invoke(EmitPackUnorm2x16, ctx, inst);
        case IrOpcode::PackFloat2x16Rtz: return Invoke(EmitPackFloat2x16Rtz, ctx, inst);
        case IrOpcode::FPAbs32: return Invoke(EmitFPAbs32, ctx, inst);
        case IrOpcode::FPNeg32: return Invoke(EmitFPNeg32, ctx, inst);
        case IrOpcode::FPSaturate32: return Invoke(EmitFPSaturate32, ctx, inst);
        case IrOpcode::BitFieldInsert: return Invoke(EmitBitFieldInsert, ctx, inst);
        case IrOpcode::BitFieldUExtract: return Invoke(EmitBitFieldUExtract, ctx, inst);
        case IrOpcode::BitFieldSExtract: return Invoke(EmitBitFieldSExtract, ctx, inst);
        case IrOpcode::DppMoveU32: return Invoke(EmitDppMoveU32, ctx, inst);
        case IrOpcode::DppUpdateU32: return Invoke(EmitDppUpdateU32, ctx, inst);
        case IrOpcode::WqmU64: return Invoke(EmitWqmU64, ctx, inst);
        case IrOpcode::SelectU1: return Invoke(EmitSelectU1, ctx, inst);
        case IrOpcode::SelectF32: return Invoke(EmitSelectF32, ctx, inst);
        case IrOpcode::IAdd64: return Invoke(EmitIAdd64, ctx, inst);
        case IrOpcode::IAddCarry32: return Invoke(EmitIAddCarry32, ctx, inst);
        case IrOpcode::ISub64: return Invoke(EmitISub64, ctx, inst);
        case IrOpcode::IMul64: return Invoke(EmitIMul64, ctx, inst);
        case IrOpcode::UDiv32: return Invoke(EmitUDiv32, ctx, inst);
        case IrOpcode::SMulHi: return Invoke(EmitSMulHi, ctx, inst);
        case IrOpcode::UMulHi: return Invoke(EmitUMulHi, ctx, inst);
        case IrOpcode::IAbs32: return Invoke(EmitIAbs32, ctx, inst);
        case IrOpcode::ShiftLeftLogical32: return Invoke(EmitShiftLeftLogical32, ctx, inst);
        case IrOpcode::ShiftLeftLogical64: return Invoke(EmitShiftLeftLogical64, ctx, inst);
        case IrOpcode::ShiftRightLogical32: return Invoke(EmitShiftRightLogical32, ctx, inst);
        case IrOpcode::ShiftRightLogical64: return Invoke(EmitShiftRightLogical64, ctx, inst);
        case IrOpcode::ShiftRightArithmetic32: return Invoke(EmitShiftRightArithmetic32, ctx, inst);
        case IrOpcode::ShiftRightArithmetic64: return Invoke(EmitShiftRightArithmetic64, ctx, inst);
        case IrOpcode::BitwiseAnd32: return Invoke(EmitBitwiseAnd32, ctx, inst);
        case IrOpcode::BitwiseAnd64: return Invoke(EmitBitwiseAnd64, ctx, inst);
        case IrOpcode::BitwiseOr32: return Invoke(EmitBitwiseOr32, ctx, inst);
        case IrOpcode::BitwiseXor32: return Invoke(EmitBitwiseXor32, ctx, inst);
        case IrOpcode::BitwiseNot32: return Invoke(EmitBitwiseNot32, ctx, inst);
        case IrOpcode::BitReverse32: return Invoke(EmitBitReverse32, ctx, inst);
        case IrOpcode::BitCount32: return Invoke(EmitBitCount32, ctx, inst);
        case IrOpcode::BitCount64: return Invoke(EmitBitCount64, ctx, inst);
        case IrOpcode::FindUMsb32: return Invoke(EmitFindUMsb32, ctx, inst);
        case IrOpcode::FindUMsb64: return Invoke(EmitFindUMsb64, ctx, inst);
        case IrOpcode::FindILsb32: return Invoke(EmitFindILsb32, ctx, inst);
        case IrOpcode::SMin32: return Invoke(EmitSMin32, ctx, inst);
        case IrOpcode::UMin32: return Invoke(EmitUMin32, ctx, inst);
        case IrOpcode::SMax32: return Invoke(EmitSMax32, ctx, inst);
        case IrOpcode::UMax32: return Invoke(EmitUMax32, ctx, inst);
        case IrOpcode::SMinTri32: return Invoke(EmitSMinTri32, ctx, inst);
        case IrOpcode::UMinTri32: return Invoke(EmitUMinTri32, ctx, inst);
        case IrOpcode::SMaxTri32: return Invoke(EmitSMaxTri32, ctx, inst);
        case IrOpcode::UMaxTri32: return Invoke(EmitUMaxTri32, ctx, inst);
        case IrOpcode::SMedTri32: return Invoke(EmitSMedTri32, ctx, inst);
        case IrOpcode::UMedTri32: return Invoke(EmitUMedTri32, ctx, inst);
        case IrOpcode::SLessThan32: return Invoke(EmitSLessThan32, ctx, inst);
        case IrOpcode::SLessThan64: return Invoke(EmitSLessThan64, ctx, inst);
        case IrOpcode::ULessThan32: return Invoke(EmitULessThan32, ctx, inst);
        case IrOpcode::ULessThan64: return Invoke(EmitULessThan64, ctx, inst);
        case IrOpcode::IEqual32: return Invoke(EmitIEqual32, ctx, inst);
        case IrOpcode::IEqual64: return Invoke(EmitIEqual64, ctx, inst);
        case IrOpcode::SLessThanEqual32: return Invoke(EmitSLessThanEqual32, ctx, inst);
        case IrOpcode::ULessThanEqual32: return Invoke(EmitULessThanEqual32, ctx, inst);
        case IrOpcode::SGreaterThan32: return Invoke(EmitSGreaterThan32, ctx, inst);
        case IrOpcode::UGreaterThan32: return Invoke(EmitUGreaterThan32, ctx, inst);
        case IrOpcode::UGreaterThan64: return Invoke(EmitUGreaterThan64, ctx, inst);
        case IrOpcode::INotEqual32: return Invoke(EmitINotEqual32, ctx, inst);
        case IrOpcode::INotEqual64: return Invoke(EmitINotEqual64, ctx, inst);
        case IrOpcode::SGreaterThanEqual32: return Invoke(EmitSGreaterThanEqual32, ctx, inst);
        case IrOpcode::UGreaterThanEqual32: return Invoke(EmitUGreaterThanEqual32, ctx, inst);
        case IrOpcode::LogicalOr: return Invoke(EmitLogicalOr, ctx, inst);
        case IrOpcode::LogicalAnd: return Invoke(EmitLogicalAnd, ctx, inst);
        case IrOpcode::LogicalXor: return Invoke(EmitLogicalXor, ctx, inst);
        case IrOpcode::LogicalNot: return Invoke(EmitLogicalNot, ctx, inst);
        case IrOpcode::FPOrdEqual32: return Invoke(EmitFPOrdEqual32, ctx, inst);
        case IrOpcode::FPUnordEqual32: return Invoke(EmitFPUnordEqual32, ctx, inst);
        case IrOpcode::FPOrdNotEqual32: return Invoke(EmitFPOrdNotEqual32, ctx, inst);
        case IrOpcode::FPUnordNotEqual32: return Invoke(EmitFPUnordNotEqual32, ctx, inst);
        case IrOpcode::FPOrdLessThan32: return Invoke(EmitFPOrdLessThan32, ctx, inst);
        case IrOpcode::FPUnordLessThan32: return Invoke(EmitFPUnordLessThan32, ctx, inst);
        case IrOpcode::FPOrdGreaterThan32: return Invoke(EmitFPOrdGreaterThan32, ctx, inst);
        case IrOpcode::FPUnordGreaterThan32: return Invoke(EmitFPUnordGreaterThan32, ctx, inst);
        case IrOpcode::FPOrdLessThanEqual32: return Invoke(EmitFPOrdLessThanEqual32, ctx, inst);
        case IrOpcode::FPUnordLessThanEqual32: return Invoke(EmitFPUnordLessThanEqual32, ctx, inst);
        case IrOpcode::FPOrdGreaterThanEqual32: return Invoke(EmitFPOrdGreaterThanEqual32, ctx, inst);
        case IrOpcode::FPUnordGreaterThanEqual32: return Invoke(EmitFPUnordGreaterThanEqual32, ctx, inst);
        case IrOpcode::FPIsNan32: return Invoke(EmitFPIsNan32, ctx, inst);
        case IrOpcode::FPCmpClass32: return Invoke(EmitFPCmpClass32, ctx, inst);
        case IrOpcode::FPAdd32: return Invoke(EmitFPAdd32, ctx, inst);
        case IrOpcode::FPSub32: return Invoke(EmitFPSub32, ctx, inst);
        case IrOpcode::FPFma32: return Invoke(EmitFPFma32, ctx, inst);
        case IrOpcode::FPMul32: return Invoke(EmitFPMul32, ctx, inst);
        case IrOpcode::FPMin32: return Invoke(EmitFPMin32, ctx, inst);
        case IrOpcode::FPMax32: return Invoke(EmitFPMax32, ctx, inst);
        case IrOpcode::FPMinTri32: return Invoke(EmitFPMinTri32, ctx, inst);
        case IrOpcode::FPMaxTri32: return Invoke(EmitFPMaxTri32, ctx, inst);
        case IrOpcode::FPMedTri32: return Invoke(EmitFPMedTri32, ctx, inst);
        case IrOpcode::FPRecip32: return Invoke(EmitFPRecip32, ctx, inst);
        case IrOpcode::FPRecipIFlag32: return Invoke(EmitFPRecipIFlag32, ctx, inst);
        case IrOpcode::FPRecipSqrt32: return Invoke(EmitFPRecipSqrt32, ctx, inst);
        case IrOpcode::FPSqrt: return Invoke(EmitFPSqrt, ctx, inst);
        case IrOpcode::FPSin: return Invoke(EmitFPSin, ctx, inst);
        case IrOpcode::FPCos: return Invoke(EmitFPCos, ctx, inst);
        case IrOpcode::FPExp2: return Invoke(EmitFPExp2, ctx, inst);
        case IrOpcode::FPLog2: return Invoke(EmitFPLog2, ctx, inst);
        case IrOpcode::FPLdexp: return Invoke(EmitFPLdexp, ctx, inst);
        case IrOpcode::FPRoundEven32: return Invoke(EmitFPRoundEven32, ctx, inst);
        case IrOpcode::FPFloor32: return Invoke(EmitFPFloor32, ctx, inst);
        case IrOpcode::FPCeil32: return Invoke(EmitFPCeil32, ctx, inst);
        case IrOpcode::FPTrunc32: return Invoke(EmitFPTrunc32, ctx, inst);
        case IrOpcode::FPFract32: return Invoke(EmitFPFract32, ctx, inst);
        case IrOpcode::LaneId: return Invoke(EmitLaneId, ctx, inst);
        case IrOpcode::WriteLane: return Invoke(EmitWriteLane, ctx, inst);
        case IrOpcode::Permlane16U32: return Invoke(EmitPermlane16U32, ctx, inst);
        case IrOpcode::BpermuteU32: return Invoke(EmitBpermuteU32, ctx, inst);
        case IrOpcode::GetSrtResource: return Invoke(EmitGetSrtResource, ctx, inst);
        case IrOpcode::GetBufferResource: return Invoke(EmitGetBufferResource, ctx, inst);
        case IrOpcode::GetAddressResource: return Invoke(EmitGetAddressResource, ctx, inst);
        case IrOpcode::GetScratchResource: return Invoke(EmitGetScratchResource, ctx, inst);
        case IrOpcode::GetImageResource: return Invoke(EmitGetImageResource, ctx, inst);
        case IrOpcode::GetSamplerResource: return Invoke(EmitGetSamplerResource, ctx, inst);
        case IrOpcode::MakeImageAddress: return Invoke(EmitMakeImageAddress, ctx, inst);
        case IrOpcode::ReadConst: return Invoke(EmitReadConst, ctx, inst);
        case IrOpcode::ReadConstBuffer: return Invoke(EmitReadConstBuffer, ctx, inst);
        case IrOpcode::LoadAddressU8: return Invoke(EmitLoadAddressU8, ctx, inst);
        case IrOpcode::LoadAddressU16: return Invoke(EmitLoadAddressU16, ctx, inst);
        case IrOpcode::LoadAddressU32: return Invoke(EmitLoadAddressU32, ctx, inst);
        case IrOpcode::StoreAddressU8: return Invoke(EmitStoreAddressU8, ctx, inst);
        case IrOpcode::StoreAddressU16: return Invoke(EmitStoreAddressU16, ctx, inst);
        case IrOpcode::StoreAddressU32: return Invoke(EmitStoreAddressU32, ctx, inst);
        case IrOpcode::LoadBufferU8: return Invoke(EmitLoadBufferU8, ctx, inst);
        case IrOpcode::LoadBufferU16: return Invoke(EmitLoadBufferU16, ctx, inst);
        case IrOpcode::LoadBufferU32: return Invoke(EmitLoadBufferU32, ctx, inst);
        case IrOpcode::LoadBufferU32x2: return Invoke(EmitLoadBufferU32x2, ctx, inst);
        case IrOpcode::LoadBufferU32x3: return Invoke(EmitLoadBufferU32x3, ctx, inst);
        case IrOpcode::LoadBufferU32x4: return Invoke(EmitLoadBufferU32x4, ctx, inst);
        case IrOpcode::StoreBufferU8: return Invoke(EmitStoreBufferU8, ctx, inst);
        case IrOpcode::StoreBufferU16: return Invoke(EmitStoreBufferU16, ctx, inst);
        case IrOpcode::StoreBufferU32: return Invoke(EmitStoreBufferU32, ctx, inst);
        case IrOpcode::StoreBufferU32x2: return Invoke(EmitStoreBufferU32x2, ctx, inst);
        case IrOpcode::StoreBufferU32x3: return Invoke(EmitStoreBufferU32x3, ctx, inst);
        case IrOpcode::StoreBufferU32x4: return Invoke(EmitStoreBufferU32x4, ctx, inst);
        case IrOpcode::BufferAtomicSwap32: return Invoke(EmitBufferAtomicSwap32, ctx, inst);
        case IrOpcode::BufferAtomicCmpSwap32: return Invoke(EmitBufferAtomicCmpSwap32, ctx, inst);
        case IrOpcode::BufferAtomicSwap64: return Invoke(EmitBufferAtomicSwap64, ctx, inst);
        case IrOpcode::BufferAtomicIAdd32: return Invoke(EmitBufferAtomicIAdd32, ctx, inst);
        case IrOpcode::BufferAtomicISub32: return Invoke(EmitBufferAtomicISub32, ctx, inst);
        case IrOpcode::BufferAtomicSMin32: return Invoke(EmitBufferAtomicSMin32, ctx, inst);
        case IrOpcode::BufferAtomicUMin32: return Invoke(EmitBufferAtomicUMin32, ctx, inst);
        case IrOpcode::BufferAtomicSMax32: return Invoke(EmitBufferAtomicSMax32, ctx, inst);
        case IrOpcode::BufferAtomicUMax32: return Invoke(EmitBufferAtomicUMax32, ctx, inst);
        case IrOpcode::BufferAtomicAnd32: return Invoke(EmitBufferAtomicAnd32, ctx, inst);
        case IrOpcode::BufferAtomicOr32: return Invoke(EmitBufferAtomicOr32, ctx, inst);
        case IrOpcode::BufferAtomicOr64: return Invoke(EmitBufferAtomicOr64, ctx, inst);
        case IrOpcode::BufferAtomicXor32: return Invoke(EmitBufferAtomicXor32, ctx, inst);
        case IrOpcode::BufferAtomicFMin32: return Invoke(EmitBufferAtomicFMin32, ctx, inst);
        case IrOpcode::BufferAtomicFMax32: return Invoke(EmitBufferAtomicFMax32, ctx, inst);
        case IrOpcode::LoadSharedU8: return Invoke(EmitLoadSharedU8, ctx, inst);
        case IrOpcode::LoadSharedU16: return Invoke(EmitLoadSharedU16, ctx, inst);
        case IrOpcode::LoadSharedU32: return Invoke(EmitLoadSharedU32, ctx, inst);
        case IrOpcode::LoadSharedU32x2: return Invoke(EmitLoadSharedU32x2, ctx, inst);
        case IrOpcode::LoadSharedU32x3: return Invoke(EmitLoadSharedU32x3, ctx, inst);
        case IrOpcode::LoadSharedU32x4: return Invoke(EmitLoadSharedU32x4, ctx, inst);
        case IrOpcode::WriteSharedU8: return Invoke(EmitWriteSharedU8, ctx, inst);
        case IrOpcode::WriteSharedU16: return Invoke(EmitWriteSharedU16, ctx, inst);
        case IrOpcode::WriteSharedU32: return Invoke(EmitWriteSharedU32, ctx, inst);
        case IrOpcode::WriteSharedU32x2: return Invoke(EmitWriteSharedU32x2, ctx, inst);
        case IrOpcode::WriteSharedU32x3: return Invoke(EmitWriteSharedU32x3, ctx, inst);
        case IrOpcode::WriteSharedU32x4: return Invoke(EmitWriteSharedU32x4, ctx, inst);
        case IrOpcode::SharedAtomicFMin32: return Invoke(EmitSharedAtomicFMin32, ctx, inst);
        case IrOpcode::SharedAtomicFMax32: return Invoke(EmitSharedAtomicFMax32, ctx, inst);
        case IrOpcode::SharedAtomicSwap32: return Invoke(EmitSharedAtomicSwap32, ctx, inst);
        case IrOpcode::SharedAtomicIAdd32: return Invoke(EmitSharedAtomicIAdd32, ctx, inst);
        case IrOpcode::SharedAtomicISub32: return Invoke(EmitSharedAtomicISub32, ctx, inst);
        case IrOpcode::SharedAtomicInc32: return Invoke(EmitSharedAtomicInc32, ctx, inst);
        case IrOpcode::SharedAtomicDec32: return Invoke(EmitSharedAtomicDec32, ctx, inst);
        case IrOpcode::SharedAtomicSMin32: return Invoke(EmitSharedAtomicSMin32, ctx, inst);
        case IrOpcode::SharedAtomicUMin32: return Invoke(EmitSharedAtomicUMin32, ctx, inst);
        case IrOpcode::SharedAtomicSMax32: return Invoke(EmitSharedAtomicSMax32, ctx, inst);
        case IrOpcode::SharedAtomicUMax32: return Invoke(EmitSharedAtomicUMax32, ctx, inst);
        case IrOpcode::SharedAtomicAnd32: return Invoke(EmitSharedAtomicAnd32, ctx, inst);
        case IrOpcode::SharedAtomicOr32: return Invoke(EmitSharedAtomicOr32, ctx, inst);
        case IrOpcode::SharedAtomicXor32: return Invoke(EmitSharedAtomicXor32, ctx, inst);
        case IrOpcode::DataAppend: return Invoke(EmitDataAppend, ctx, inst);
        case IrOpcode::DataConsume: return Invoke(EmitDataConsume, ctx, inst);
        case IrOpcode::SwizzleU32: return Invoke(EmitSwizzleU32, ctx, inst);
        case IrOpcode::ImageQueryDimensions: return Invoke(EmitImageQueryDimensions, ctx, inst);
        case IrOpcode::ImageQueryLod: return Invoke(EmitImageQueryLod, ctx, inst);
        case IrOpcode::ImageRead: return Invoke(EmitImageRead, ctx, inst);
        case IrOpcode::ImageWrite: return Invoke(EmitImageWrite, ctx, inst);
        case IrOpcode::ImageSampleRaw: return Invoke(EmitImageSampleRaw, ctx, inst);
        case IrOpcode::ImageGatherRaw: return Invoke(EmitImageGatherRaw, ctx, inst);
        case IrOpcode::ImageAtomicSwap32: return Invoke(EmitImageAtomicSwap32, ctx, inst);
        case IrOpcode::ImageAtomicIAdd32: return Invoke(EmitImageAtomicIAdd32, ctx, inst);
        case IrOpcode::ImageAtomicUMin32: return Invoke(EmitImageAtomicUMin32, ctx, inst);
        case IrOpcode::ImageAtomicUMax32: return Invoke(EmitImageAtomicUMax32, ctx, inst);
        case IrOpcode::ImageAtomicAnd32: return Invoke(EmitImageAtomicAnd32, ctx, inst);
        case IrOpcode::ImageAtomicOr32: return Invoke(EmitImageAtomicOr32, ctx, inst);
        case IrOpcode::ImageAtomicXor32: return Invoke(EmitImageAtomicXor32, ctx, inst);
        case IrOpcode::GetAttribute: return Invoke(EmitGetAttribute, ctx, inst);
        case IrOpcode::GetInterpolationParameter: return Invoke(EmitGetInterpolationParameter, ctx, inst);
        case IrOpcode::SetAttribute: return Invoke(EmitSetAttribute, ctx, inst);
        case IrOpcode::ControlNop: return Invoke(EmitControlNop, ctx, inst);
        case IrOpcode::Waitcnt: return Invoke(EmitWaitcnt, ctx, inst);
        case IrOpcode::Sendmsg: return Invoke(EmitSendmsg, ctx, inst);
        case IrOpcode::TtraceData: return Invoke(EmitTtraceData, ctx, inst);
        case IrOpcode::InstPrefetch: return Invoke(EmitInstPrefetch, ctx, inst);
        case IrOpcode::SelectU32: return Invoke(EmitSelectU32, ctx, inst);
        default: ctx.Fail(inst, "has no direct SPIR-V emitter");
    }
}

void EmitStructuredInstruction(SpirvValueEmitContext& ctx, StructuredFunctionState& functionState, const IrValue& inst) {
    if (inst.Opcode() == IrOpcode::Phi) {
        const auto type = TypeId(ctx.state, inst.Type());
        if (type == 0 || inst.ArgumentCount() == 0) {
            ctx.Fail(inst, "has no native SPIR-V representation");
        }
        for (std::size_t index = 0; index < inst.ArgumentCount(); index++) {
            const IrBlock* predecessor = inst.PhiBlock(index);
            if (predecessor == nullptr || !ctx.state.labels.contains(predecessor)) {
                ctx.Fail(inst, "has a predecessor outside the structured function");
            }
        }
        functionState.deferredPhis.push_back({ctx.state.module.AddDeferredPhi(type, ctx.Result(inst), inst.ArgumentCount()), &inst, ctx.half});
        return;
    }
    EmitDirectInstruction(ctx, inst);
}

void EmitStructuredBlock(SpirvValueEmitContext& ctx, StructuredFunctionState& functionState, const IrBlock* block) {
    auto& state = ctx.state;
    state.currentBlock = block;
    EmitLabel(state, ctx.Label(block));
    bool emittedNonPhi = false;
    // Wave LDS ordering (see WaveLdsScope): a barrier separates an LDS write from the next LDS access
    // and a read from the next write. The block may be entered right after a write.
    bool ldsWritten = true;
    bool ldsRead = false;
    for (const IrValue* inst : block->Instructions()) {
        if (inst->Opcode() == IrOpcode::Phi) {
            if (emittedNonPhi) {
                ctx.Fail(*inst, "appears after a non-Phi instruction");
            }
        } else {
            emittedNonPhi = true;
        }
        if (state.waveLdsScope != 0) {
            const auto access = SharedAccessOf(inst->Opcode());
            if (inst->Opcode() == IrOpcode::Barrier) {
                ldsWritten = false;
                ldsRead = false;
            } else if (access != SharedAccess::None) {
                const bool writes = access != SharedAccess::Read;
                if (ldsWritten || (writes && ldsRead)) {
                    state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, state.waveLdsScope), ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask));
                    ldsWritten = false;
                    ldsRead = false;
                }
                ldsWritten |= writes;
                ldsRead |= access != SharedAccess::Write;
            }
        }
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            if (half != 0 && ctx.otherHalf == nullptr) {
                ctx.Fail(*inst, "requires a second lane context");
            }
            SpirvValueEmitContext& lane = half == 0 ? ctx : *ctx.otherHalf;
            state.laneHalf = half;
            if (half == 0 || (inst->Opcode() != IrOpcode::Barrier && inst->Opcode() != IrOpcode::MeshAllocate)) {
                EmitStructuredInstruction(lane, functionState, *inst);
            }
        }
        state.laneHalf = 0;
    }
}

void PatchStructuredPhis(SpirvValueEmitContext& ctx, StructuredFunctionState& functionState) {
    for (const SpirvDeferredPhiPatch& deferred : functionState.deferredPhis) {
        SpirvValueEmitContext& lane = deferred.half == 0 ? ctx : *ctx.otherHalf;
        for (std::size_t index = 0; index < deferred.instruction->ArgumentCount(); index++) {
            const IrBlock* predecessor = deferred.instruction->PhiBlock(index);
            const auto found = functionState.blockExitLabels.find(predecessor);
            if (found == functionState.blockExitLabels.end()) {
                ctx.Fail(*deferred.instruction, "has a predecessor that was not emitted");
            }
            ctx.state.module.PatchDeferredPhi(deferred.phi, index, lane.Def(deferred.instruction->Argument(index)), found->second);
        }
    }
}

}

void EmitControlFlow(SpirvModule& module, const IrProgram& program) {
    throw std::runtime_error("EmitControlFlow not implemented");
}

void EmitControlFlow(SpirvValueEmitContext& context, StructuredFunctionState& functionState, const IrProgram& program) {
    auto& state = context.state;
    const std::vector<IrBlock*>& blocks = program.BlockOrder();
    if (blocks.empty() || blocks.front() == nullptr) {
        context.Fail("structured control flow requires at least one block");
    }
    if (state.loopGuardLimit != 0) {
        const auto pointer = TypePointer(state, spv::StorageClassPrivate, TypeU32(state));
        if (state.loopGuardVisits == 0) state.loopGuardVisits = state.module.DefineGlobalVariable(pointer, spv::StorageClassPrivate);
        if (state.loopGuardPc == 0) state.loopGuardPc = state.module.DefineGlobalVariable(pointer, spv::StorageClassPrivate);
        state.module.AddFunction(spv::OpStore, state.loopGuardVisits, ConstantU32(state, 0u));
        state.module.AddFunction(spv::OpStore, state.loopGuardPc, ConstantU32(state, 0u));
    }
    state.module.AddFunction(spv::OpBranch, context.Label(blocks.front()));
    static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
    if (debug) {
        const auto& infos = program.Metadata().blockInfo;
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            const auto* byId = BlockInfoFor(program, blocks[index]);
            std::fprintf(stderr, "[spirv] order %zu: IR block %u, positional info id %u, id-matched info %s (kind %d true %u false %u)\n", index, blocks[index]->Id(),
                         index < infos.size() ? infos[index].id : 0xffffffffu, byId ? std::to_string(byId->id).c_str() : "none",
                         byId ? static_cast<int>(byId->terminator.kind) : -1, byId ? byId->terminator.trueBlock : 0u, byId ? byId->terminator.falseBlock : 0u);
        }
    }
    for (const IrBlock* block : blocks) {
        if (block == nullptr) {
            context.Fail("structured control flow contains a null block");
        }
        const BlockInfo* info = BlockInfoFor(program, block);
        if (info == nullptr) {
            context.Fail("structured control flow block has no terminator metadata");
        }
        EmitStructuredBlock(context, functionState, block);
        functionState.blockExitLabels.emplace(block, state.currentLabel);
        EmitStructuredTerminator(context, program, *info);
    }
    PatchStructuredPhis(context, functionState);
}

void EmitVoid(SpirvValueEmitContext&) {
}

void EmitBarrier(SpirvEmitterState& state) {
    const bool tessellation = state.program.Resources().stage == IrShaderStage::TessellationControl;
    const auto memoryScope = tessellation ? spv::ScopeInvocation : spv::ScopeWorkgroup;
    const auto semantics = tessellation ? spv::MemorySemanticsMaskNone : spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask;
    state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, memoryScope), ConstantU32(state, semantics));
}

void EmitUnreachable(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Fail(inst, "must be lowered before SPIR-V emission");
}

void EmitReference(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitReferenceU32(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitControlNop(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitWaitcnt(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitSendmsg(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitTtraceData(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitInstPrefetch(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitPhi(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

}
