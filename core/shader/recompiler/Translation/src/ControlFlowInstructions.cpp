#include "Translation/ControlFlowInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace ShaderRecompiler {

void TranslateControlFlowInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const ControlFlowGraph& cfg) {
    throw std::runtime_error("TranslateControlFlowInstruction not implemented");
}

void TranslationContext::sSubvectorLoop(const RdnaInstruction& inst, bool begin) {
    throw std::runtime_error("GNM subvector loop instructions are not supported by this translator");
}

// s_*_saveexec reads its source before it writes the old exec into the destination, so
// `s_and_saveexec_b64 vcc, vcc` (the Bink 2 decoder kernels gate every sparse coefficient load with
// it) masks exec by the old vcc. Reading the source after the destination write turned that form into
// exec = exec & exec. APS5_SAVEEXEC_WRITE_FIRST=1 restores the old order.
bool SaveexecWritesDestinationFirst() {
    static const bool writeFirst = std::getenv("APS5_SAVEEXEC_WRITE_FIRST") != nullptr;
    return writeFirst;
}

void TranslationContext::sSaveexec(const RdnaInstruction& inst, IrOpcode operation, bool negateExec, bool negateSource, bool write64) {
    // Callers name the lane-wise operation; the exec mask words themselves combine bitwise.
    if (operation == IrOpcode::LogicalAnd) operation = IrOpcode::BitwiseAnd32;
    else if (operation == IrOpcode::LogicalOr) operation = IrOpcode::BitwiseOr32;
    const bool writeFirst = SaveexecWritesDestinationFirst();
    if (write64) {
        const std::array<IrU32, 2> oldExec{IrU32(ir.GetExecLo()), IrU32(ir.GetExecHi())};
        if (writeFirst) writeU32Pair(inst.destination, oldExec);
        const std::array<IrU32, 2> source = readU32Pair(sourceAt(inst, 0u));
        if (!writeFirst) writeU32Pair(inst.destination, oldExec);
        IrValue& lowExecOperand = negateExec ? ir.BitwiseNot(oldExec[0].Value()) : oldExec[0].Value();
        IrValue& lowSourceOperand = negateSource ? ir.BitwiseNot(source[0].Value()) : source[0].Value();
        IrValue& highExecOperand = negateExec ? ir.BitwiseNot(oldExec[1].Value()) : oldExec[1].Value();
        IrValue& highSourceOperand = negateSource ? ir.BitwiseNot(source[1].Value()) : source[1].Value();
        const IrU32 newExecLo(ir.Emit(operation, IrType::U32, {&lowExecOperand, &lowSourceOperand}));
        const IrU32 newExecHi(ir.Emit(operation, IrType::U32, {&highExecOperand, &highSourceOperand}));
        ir.SetExecLo(newExecLo.Value());
        ir.SetExecHi(newExecHi.Value());
        const IrU1 nonZero(ir.LogicalOr(ir.INotEqual(newExecLo.Value(), ir.Constant(0u)), ir.INotEqual(newExecHi.Value(), ir.Constant(0u))));
        ir.SetScc(nonZero.Value());
        ir.SetExec(threadBit({newExecLo, newExecHi}).Value());
        return;
    }
    const IrU32 oldExec(ir.GetExecLo());
    if (writeFirst) writeRawU32(inst.destination, oldExec);
    const IrU32 source = readU32(sourceAt(inst, 0u));
    if (!writeFirst) writeRawU32(inst.destination, oldExec);
    IrValue& execOperand = negateExec ? ir.BitwiseNot(oldExec.Value()) : oldExec.Value();
    IrValue& sourceOperand = negateSource ? ir.BitwiseNot(source.Value()) : source.Value();
    const IrU32 newExec(ir.Emit(operation, IrType::U32, {&execOperand, &sourceOperand}));
    ir.SetExecLo(newExec.Value());
    const IrU1 nonZero(ir.INotEqual(newExec.Value(), ir.Constant(0u)));
    ir.SetScc(nonZero.Value());
    ir.SetExec(threadBit({newExec, IrU32(ir.GetExecHi())}).Value());
}

void TranslationContext::addU32(const RdnaInstruction& inst, bool vector, bool useCarryIn) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    IrValue& firstAdd = ir.Emit(IrOpcode::IAddCarry32, IrType::U32x2, {&lhs.Value(), &rhs.Value()});
    const IrU32 sum(ir.Emit(IrOpcode::CompositeExtractU32x2, IrType::U32, {&firstAdd, &ir.Constant(0u)}));
    const IrU32 firstCarry(ir.Emit(IrOpcode::CompositeExtractU32x2, IrType::U32, {&firstAdd, &ir.Constant(1u)}));
    if (!useCarryIn) {
        writeRawU32(inst.destination, sum);
        const IrU1 carryOut(ir.INotEqual(firstCarry.Value(), ir.Constant(0u)));
        if (vector) {
            writeMask(inst.destination2, carryOut);
            return;
        }
        ir.SetScc(carryOut.Value());
        return;
    }
    const IrU1 carryIn = vector ? IrU1(ir.GetVcc()) : IrU1(ir.GetScc());
    const IrU32 carryInU32(ir.Select(carryIn.Value(), ir.Constant(1u), ir.Constant(0u)));
    IrValue& secondAdd = ir.Emit(IrOpcode::IAddCarry32, IrType::U32x2, {&sum.Value(), &carryInU32.Value()});
    const IrU32 result(ir.Emit(IrOpcode::CompositeExtractU32x2, IrType::U32, {&secondAdd, &ir.Constant(0u)}));
    const IrU32 secondCarry(ir.Emit(IrOpcode::CompositeExtractU32x2, IrType::U32, {&secondAdd, &ir.Constant(1u)}));
    const IrU1 carryOut(ir.LogicalOr(ir.INotEqual(firstCarry.Value(), ir.Constant(0u)), ir.INotEqual(secondCarry.Value(), ir.Constant(0u))));
    writeRawU32(inst.destination, result);
    if (vector) {
        writeMask(inst.destination2, carryOut);
        return;
    }
    ir.SetScc(carryOut.Value());
}

void TranslationContext::subU32(const RdnaInstruction& inst, bool vector, bool reverse) {
    const IrU32 first = readU32(sourceAt(inst, 0u));
    const IrU32 second = readU32(sourceAt(inst, 1u));
    const IrU32& lhs = reverse ? second : first;
    const IrU32& rhs = reverse ? first : second;
    const IrU32 result(ir.ISub(lhs.Value(), rhs.Value()));
    const IrU1 borrow(ir.ULessThan(lhs.Value(), rhs.Value()));
    writeRawU32(inst.destination, result);
    if (vector) {
        writeMask(inst.destination2, borrow);
        return;
    }
    ir.SetScc(borrow.Value());
}

void TranslationContext::subbU32(const RdnaInstruction& inst, bool vector, bool reverse) {
    const IrU32 first = readU32(sourceAt(inst, 0u));
    const IrU32 second = readU32(sourceAt(inst, 1u));
    const IrU32& lhs = reverse ? second : first;
    const IrU32& rhs = reverse ? first : second;
    const IrU1 borrowIn = vector ? IrU1(ir.GetVcc()) : IrU1(ir.GetScc());
    const IrU32 borrowInU32(ir.Select(borrowIn.Value(), ir.Constant(1u), ir.Constant(0u)));
    const IrU32 partial(ir.ISub(lhs.Value(), rhs.Value()));
    const IrU1 firstBorrow(ir.ULessThan(lhs.Value(), rhs.Value()));
    const IrU32 result(ir.ISub(partial.Value(), borrowInU32.Value()));
    const IrU1 secondBorrow(ir.ULessThan(partial.Value(), borrowInU32.Value()));
    const IrU1 borrowOut(ir.LogicalOr(firstBorrow.Value(), secondBorrow.Value()));
    writeRawU32(inst.destination, result);
    if (vector) {
        writeMask(inst.destination2, borrowOut);
        return;
    }
    ir.SetScc(borrowOut.Value());
}

void TranslationContext::sAbsdiffI32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 difference(ir.ISub(lhs.Value(), rhs.Value()));
    const IrU32 result(ir.Emit(IrOpcode::IAbs32, IrType::U32, {&difference.Value()}));
    ir.SetScc(ir.INotEqual(result.Value(), ir.Constant(0u)));
    writeRawU32(inst.destination, result);
}

void TranslationContext::sAddSubI32(const RdnaInstruction& inst, bool subtract) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 result(subtract ? ir.ISub(lhs.Value(), rhs.Value()) : ir.IAdd(lhs.Value(), rhs.Value()));
    const IrU32 signCheck(subtract ? ir.BitwiseAnd(ir.BitwiseXor(lhs.Value(), rhs.Value()), ir.BitwiseXor(lhs.Value(), result.Value()))
                                    : ir.BitwiseAnd(ir.BitwiseXor(lhs.Value(), result.Value()), ir.BitwiseXor(rhs.Value(), result.Value())));
    ir.SetScc(ir.INotEqual(ir.BitwiseAnd(signCheck.Value(), ir.Constant(0x80000000u)), ir.Constant(0u)));
    writeRawU32(inst.destination, result);
}

void TranslationContext::sLshlAddU32(const RdnaInstruction& inst, std::uint32_t shiftAmount) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 shifted(ir.ShiftLeftLogical(lhs.Value(), ir.Constant(shiftAmount)));
    const IrU32 result(ir.IAdd(shifted.Value(), rhs.Value()));
    writeRawU32(inst.destination, result);
}

void TranslationContext::scalarMinMax32(const RdnaInstruction& inst, IrOpcode valueOpcode, IrOpcode compareOpcode) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 result(ir.Emit(valueOpcode, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    const IrU1 selected(ir.Emit(compareOpcode, IrType::U1, {&lhs.Value(), &rhs.Value()}));
    ir.SetScc(selected.Value());
    writeRawU32(inst.destination, result);
}

void TranslationContext::emitControlNop() {
    (void)ir.Emit(IrOpcode::ControlNop, IrType::Void, {});
}

void TranslationContext::emitWaitcnt() {
    (void)ir.Emit(IrOpcode::Waitcnt, IrType::Void, {});
}

void TranslationContext::sBarrier() {
    (void)ir.Emit(IrOpcode::Barrier, IrType::Void, {});
}

void TranslationContext::sSendmsg(const RdnaInstruction&) {
    (void)ir.Emit(IrOpcode::Sendmsg, IrType::Void, {});
}

void TranslationContext::sTtracedata() {
    (void)ir.Emit(IrOpcode::TtraceData, IrType::Void, {});
}

void TranslationContext::sInstPrefetch() {
    (void)ir.Emit(IrOpcode::InstPrefetch, IrType::Void, {});
}

void TranslationContext::sGetpcB64(const RdnaInstruction& inst) {
    const IrU64 pc(ir.ConstantU64(static_cast<std::uint64_t>(currentProgramCounter) + 4u));
    writeU32Pair(inst.destination, extractU64(pc));
}

void TranslationContext::sCselectB32(const RdnaInstruction& inst) {
    const IrU32 trueValue = readU32(sourceAt(inst, 0u));
    const IrU32 falseValue = readU32(sourceAt(inst, 1u));
    const IrU32 result(ir.Select(ir.GetScc(), trueValue.Value(), falseValue.Value()));
    writeRawU32(inst.destination, result);
}

void TranslationContext::scalarSelect64(const RdnaInstruction& inst, const RdnaOperand& falseSource) {
    const std::array<IrU32, 2> trueValue = readU32Pair(sourceAt(inst, 0u));
    const std::array<IrU32, 2> falseValue = readU32Pair(falseSource);
    IrValue& condition = ir.GetScc();
    const IrU32 low(ir.Select(condition, trueValue[0].Value(), falseValue[0].Value()));
    const IrU32 high(ir.Select(condition, trueValue[1].Value(), falseValue[1].Value()));
    writeU32Pair(inst.destination, {low, high});
}

void TranslationContext::movB32(const RdnaInstruction& inst, bool applyFloatModifiers) {
    if (applyFloatModifiers) {
        IrF32 value(*readOperand(sourceAt(inst, 0u), IrType::F32));
        value = applyF32ResultModifiers(inst.destination, value);
        writeOperand(inst.destination, &value.Value());
        return;
    }
    IrValue* value = readOperand(sourceAt(inst, 0u), IrType::U32);
    writeOperand(inst.destination, value);
}

void TranslationContext::sMovB64(const RdnaInstruction& inst) {
    const std::array<IrU32, 2> value = readU32Pair(sourceAt(inst, 0u));
    writeU32Pair(inst.destination, value);
}

void TranslationContext::sWqm(const RdnaInstruction& inst, bool wide) {
    if (wide) {
        const std::array<IrU32, 2> source = readU32Pair(sourceAt(inst, 0u));
        const IrU64 wide64(ir.ConstructU64(source[0].Value(), source[1].Value()));
        const IrU64 result(ir.Emit(IrOpcode::WqmU64, IrType::U64, {&wide64.Value()}));
        writeU32Pair(inst.destination, extractU64(result));
        return;
    }
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU64 wide64(ir.ConstructU64(source.Value(), ir.Constant(0u)));
    const IrU64 result(ir.Emit(IrOpcode::WqmU64, IrType::U64, {&wide64.Value()}));
    writeRawU32(inst.destination, extractU64(result)[0]);
}

// M0 holds a uniform register offset. Vector registers are SSA values with fixed indices, so an indexed
// access becomes a select over every register the program references at or past the base.
void TranslationContext::vMovrelsB32(const RdnaInstruction& inst) {
    const RdnaOperand& source = sourceAt(inst, 0u);
    if (source.kind != RdnaOperandKind::VectorRegister) {
        throw std::runtime_error("v_movrels_b32 source is not a vector register");
    }
    IrValue& offset = ir.GetM0();
    IrU32 result(ir.GetVectorReg(static_cast<VectorReg>(source.reg)));
    for (std::uint32_t reg = source.reg + 1u; reg < currentVectorLimit; ++reg) {
        IrValue& hit = ir.IEqual(offset, ir.Constant(reg - source.reg));
        result = IrU32(ir.Select(hit, ir.GetVectorReg(static_cast<VectorReg>(reg)), result.Value()));
    }
    writeRawU32(inst.destination, result);
}

void TranslationContext::vMovreldB32(const RdnaInstruction& inst) {
    const RdnaOperand destination = plainOperand(inst.destination);
    if (destination.kind != RdnaOperandKind::VectorRegister) {
        throw std::runtime_error("v_movreld_b32 destination is not a vector register");
    }
    const IrU32 value = readU32(sourceAt(inst, 0u));
    IrValue& offset = ir.GetM0();
    for (std::uint32_t reg = destination.reg; reg < currentVectorLimit; ++reg) {
        RdnaOperand target = destination;
        target.reg = reg;
        IrValue& hit = ir.IEqual(offset, ir.Constant(reg - destination.reg));
        writeRawU32(target, IrU32(ir.Select(hit, value.Value(), ir.GetVectorReg(static_cast<VectorReg>(reg)))));
    }
}

void TranslationContext::vReadfirstlaneB32(const RdnaInstruction& inst) {
    const IrU32 value = readU32(sourceAt(inst, 0u));
    const IrU32 result(ir.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&value.Value(), &ir.GetExec()}));
    writeRawU32(inst.destination, result);
}

void TranslationContext::vReadlaneB32(const RdnaInstruction& inst) {
    const IrU32 value = readU32(sourceAt(inst, 0u));
    const IrU32 lane = readU32(sourceAt(inst, 1u));
    const IrU32 result(ir.Emit(IrOpcode::ReadLane, IrType::U32, {&value.Value(), &lane.Value()}));
    writeRawU32(inst.destination, result);
}

void TranslationContext::vWritelaneB32(const RdnaInstruction& inst) {
    const IrU32 value = readU32(sourceAt(inst, 0u));
    const IrU32 lane = readU32(sourceAt(inst, 1u));
    const IrU32 previous = readRawU32(plainOperand(inst.destination));
    const IrU32 result(ir.Emit(IrOpcode::WriteLane, IrType::U32, {&value.Value(), &lane.Value(), &previous.Value()}));
    writeRawU32(inst.destination, result);
}

void TranslationContext::vPermlane16B32(const RdnaInstruction& inst, bool x16) {
    const IrU32 value = readU32(sourceAt(inst, 0u));
    const IrU32 selectLow = readU32(sourceAt(inst, 1u));
    const IrU32 selectHigh = readU32(sourceAt(inst, 2u));
    const PermlaneFlags flags{x16, inst.destination.opSel, inst.destination.opSelHi};
    const IrU32 result(ir.Emit(IrOpcode::Permlane16U32, IrType::U32, {&value.Value(), &selectLow.Value(), &selectHigh.Value(), &ir.GetExec()}, flags));
    writeRawU32(inst.destination, result);
}

void TranslateControlFlowInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateControlFlowInstruction not implemented");
}

}
