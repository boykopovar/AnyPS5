#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

std::array<IrU32, 2> TranslationContext::ballotMask(IrU1 value) {
    IrValue& mask = ir.Emit(IrOpcode::Ballot, IrType::U32x4, {&value.Value()});
    return {IrU32(ir.CompositeExtract(mask, 0u)), program.WaveSize() == 64u ? IrU32(ir.CompositeExtract(mask, 1u)) : IrU32(ir.Constant(0u))};
}

IrU32 TranslationContext::hostExecWord(std::uint32_t half) {
    const IrU32 word(half == 0u ? ir.GetExecLo() : ir.GetExecHi());
    if (half != 0u && program.WaveSize() != 64u) {
        return word;
    }
    return IrU32(ir.BitwiseAnd(word.Value(), ballotMask(IrU1(ir.ConstantBool(true)))[half].Value()));
}

IrU1 TranslationContext::threadBit(const std::array<IrU32, 2>& mask) {
    const bool wide = program.WaveSize() == 64u;
    if (mask[0].Value().HasImmediate() && (!wide || mask[1].Value().HasImmediate())) {
        const std::uint32_t low = mask[0].Value().ImmediateU32();
        const std::uint32_t high = wide ? mask[1].Value().ImmediateU32() : low;
        if (low == 0u && high == 0u) return IrU1(ir.ConstantBool(false));
        if (low == 0xffffffffu && high == 0xffffffffu) return IrU1(ir.ConstantBool(true));
    }
    const IrU32 lane(ir.Emit(IrOpcode::LaneId, IrType::U32, {}));
    const IrU32 word = program.WaveSize() == 64u ? IrU32(ir.Select(ir.ULessThan(lane.Value(), ir.Constant(32u)), mask[0].Value(), mask[1].Value())) : mask[0];
    const IrU32 bit(ir.BitwiseAnd(lane.Value(), ir.Constant(31u)));
    return IrU1(ir.INotEqual(ir.BitwiseAnd(ir.ShiftRightLogical(word.Value(), bit.Value()), ir.Constant(1u)), ir.Constant(0u)));
}

IrU32 TranslationContext::conditionBit(const RdnaOperand& operand) {
    return IrU32(ir.Select(readMask(operand).Value(), ir.Constant(1u), ir.Constant(0u)));
}

IrU1 TranslationContext::readMask(const RdnaOperand& operand) {
    if (operand.kind == RdnaOperandKind::LiteralConstant || operand.kind == RdnaOperandKind::IntegerInlineConstant || operand.kind == RdnaOperandKind::FloatInlineConstant) {
        return threadBit(readU32Pair(operand));
    }
    switch (operand.kind) {
        case RdnaOperandKind::ScalarRegister: {
            const ScalarReg reg = static_cast<ScalarReg>(operand.reg);
            const std::array<IrU32, 2> mask = program.WaveSize() == 64u ? readU32Pair(operand) : std::array<IrU32, 2>{readRawU32(operand), IrU32(ir.Constant(0u))};
            return IrU1(ir.Emit(IrOpcode::SelectU1, IrType::U1, {&ir.GetScalarMaskTag(reg), &ir.GetThreadBitScalarReg(reg), &threadBit(mask).Value()}));
        }
        case RdnaOperandKind::ExecLo:
        case RdnaOperandKind::ExecHi: return IrU1(ir.GetExec());
        case RdnaOperandKind::VccLo:
        case RdnaOperandKind::VccHi: return program.WaveSize() == 32u ? threadBit({readRawU32(operand), IrU32(ir.Constant(0u))}) : IrU1(ir.GetVcc());
        case RdnaOperandKind::Scc: return IrU1(ir.GetScc());
        case RdnaOperandKind::VccZ: return IrU1(ir.LogicalNot(ir.GetVcc()));
        case RdnaOperandKind::ExecZ: return IrU1(ir.LogicalNot(ir.GetExec()));
        default: return IrU1(ir.INotEqual(readRawU32(operand).Value(), ir.Constant(0u)));
    }
}

IrU1 TranslationContext::readMaskValid(const RdnaOperand& operand) {
    if (operand.kind == RdnaOperandKind::LiteralConstant || operand.kind == RdnaOperandKind::IntegerInlineConstant || operand.kind == RdnaOperandKind::FloatInlineConstant) {
        const bool zero = operand.value == 0u;
        const bool ones = operand.value == 0xffffffffu && operand.kind == RdnaOperandKind::IntegerInlineConstant && operand.signedVal < 0;
        return IrU1(ir.ConstantBool(zero || ones));
    }
    switch (operand.kind) {
        case RdnaOperandKind::Null:
        case RdnaOperandKind::PopsExitingWaveId: return IrU1(ir.ConstantBool(true));
        case RdnaOperandKind::ScalarRegister: return IrU1(ir.GetScalarMaskTag(static_cast<ScalarReg>(operand.reg)));
        case RdnaOperandKind::ExecLo:
        case RdnaOperandKind::ExecHi:
        case RdnaOperandKind::VccLo:
        case RdnaOperandKind::VccHi:
        case RdnaOperandKind::VccZ:
        case RdnaOperandKind::ExecZ:
        case RdnaOperandKind::Scc: return IrU1(ir.ConstantBool(true));
        default: return IrU1(ir.ConstantBool(false));
    }
}

std::array<IrU32, 2> TranslationContext::writeMask(const RdnaOperand& operand, IrU1 value, bool write64) {
    const std::array<IrU32, 2> mask = ballotMask(value);
    switch (operand.kind) {
        case RdnaOperandKind::ScalarRegister: {
            const ScalarReg reg = static_cast<ScalarReg>(operand.reg);
            ir.SetThreadBitScalarReg(reg, value.Value());
            ir.SetScalarMaskTag(reg, ir.ConstantBool(true));
            if (RegIndex(reg) > 0u) {
                ir.SetScalarMaskTag(static_cast<ScalarReg>(RegIndex(reg) - 1u), ir.ConstantBool(false));
            }
            ir.SetScalarReg(reg, mask[0].Value());
            if ((write64 || program.WaveSize() == 64u) && RegIndex(reg) + 1u < NumScalarRegs) {
                const ScalarReg high = static_cast<ScalarReg>(RegIndex(reg) + 1u);
                ir.SetScalarReg(high, mask[1].Value());
                ir.SetThreadBitScalarReg(high, ir.ConstantBool(false));
                ir.SetScalarMaskTag(high, ir.ConstantBool(false));
            }
            return mask;
        }
        case RdnaOperandKind::ExecLo:
        case RdnaOperandKind::ExecHi:
            ir.SetExec(value.Value());
            ir.SetExecLo(mask[0].Value());
            ir.SetExecHi(mask[1].Value());
            return mask;
        case RdnaOperandKind::VccLo:
        case RdnaOperandKind::VccHi:
            if (!write64 && program.WaveSize() == 32u) {
                writeRawU32(operand, mask[0]);
                return mask;
            }
            ir.SetVcc(value.Value());
            ir.SetVccLo(mask[0].Value());
            ir.SetVccHi(mask[1].Value());
            return mask;
        case RdnaOperandKind::Scc:
            ir.SetScc(value.Value());
            return mask;
        default:
            writeRawU32(operand, mask[0]);
            writeRawU32(offsetOperand(operand, 1u), mask[1]);
            return mask;
    }
}

void TranslationContext::writeCompareResult(const RdnaOperand& operand, IrU1 value) {
    if (operand.kind == RdnaOperandKind::Scc) {
        writeOperand(operand, &value.Value());
        return;
    }
    writeMask(operand, IrU1(ir.LogicalAnd(ir.GetExec(), value.Value())));
}

void TranslationContext::AddBranchCondition(const BasicBlock& source, BlockInfo& info) {
    if (source.terminator.gotoValue >= 0) {
        if (source.terminator.gotoVariable == InvalidControlFlowId) {
            throw std::runtime_error("TranslationContext::AddBranchCondition block " + std::to_string(source.id) + " sets an invalid goto variable");
        }
        ir.SetGotoVariable(source.terminator.gotoVariable, ir.ConstantBool(source.terminator.gotoValue != 0));
    }
    if (source.terminator.kind == TerminatorKind::IndirectBranch) {
        if (source.terminator.indirectSelectorCode != InvalidControlFlowId) {
            info.indirectTarget = &readScalarCode(source.terminator.indirectSelectorCode).Value();
        } else if (source.terminator.indirectPcSgpr != InvalidControlFlowId) {
            info.indirectTarget = &ir.GetScalarReg(static_cast<ScalarReg>(source.terminator.indirectPcSgpr));
        } else {
            throw std::runtime_error("TranslationContext::AddBranchCondition block " + std::to_string(source.id) + " has no indirect branch selector");
        }
        (void)ir.Emit(IrOpcode::ReferenceU32, IrType::Void, {info.indirectTarget});
        return;
    }
    if (source.terminator.kind != TerminatorKind::ConditionalBranch) {
        return;
    }
    IrU1 condition;
    switch (source.terminator.condition) {
        case BranchCondition::Always: condition = IrU1(ir.ConstantBool(true)); break;
        case BranchCondition::SccZero: condition = IrU1(ir.LogicalNot(ir.GetScc())); break;
        case BranchCondition::SccNonZero: condition = IrU1(ir.GetScc()); break;
        case BranchCondition::VccZero: condition = IrU1(ir.LogicalNot(ir.GetVcc())); break;
        case BranchCondition::VccNonZero: condition = IrU1(ir.GetVcc()); break;
        case BranchCondition::ExecZero: condition = IrU1(ir.LogicalNot(ir.GetExec())); break;
        case BranchCondition::ExecNonZero: condition = IrU1(ir.GetExec()); break;
        case BranchCondition::IndirectTarget: {
            if (source.terminator.indirectTargetProgramCounters.size() != 1u || source.terminator.indirectPcSgpr == InvalidControlFlowId) {
                throw std::runtime_error("invalid lowered indirect branch");
            }
            auto& base = ir.Emit(IrOpcode::GetShaderBase, IrType::U64, {});
            const IrU64 target(ir.Emit(IrOpcode::IAdd64, IrType::U64, {&base, &ir.ConstantU64(source.terminator.indirectTargetProgramCounters.front())}));
            condition = IrU1(ir.IEqual(ir.GetScalarReg(static_cast<ScalarReg>(source.terminator.indirectPcSgpr)), extractU64(target)[0].Value()));
            break;
        }
        case BranchCondition::ScalarInstruction:
            if (instructionBranchCondition.Value().IsEmpty()) {
                throw std::runtime_error("TranslationContext::AddBranchCondition block " + std::to_string(source.id) + " has no scalar branch condition");
            }
            condition = instructionBranchCondition;
            break;
        case BranchCondition::GotoVariable:
            if (source.terminator.gotoVariable == InvalidControlFlowId) {
                throw std::runtime_error("TranslationContext::AddBranchCondition block " + std::to_string(source.id) + " reads an invalid goto variable");
            }
            condition = IrU1(ir.GetGotoVariable(source.terminator.gotoVariable));
            break;
        case BranchCondition::Unknown:
            throw std::runtime_error("TranslationContext::AddBranchCondition block " + std::to_string(source.id) + " has an unknown branch condition");
    }
    info.condition = &condition.Value();
    (void)ir.Emit(IrOpcode::Reference, IrType::Void, {info.condition});
}

}
