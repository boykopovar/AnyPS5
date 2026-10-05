#include "Translation/IntegerInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

void TranslateIntegerInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateIntegerInstruction not implemented");
}

bool TranslationContext::integer16Shift(const RdnaInstruction& inst, IrOpcode opcode, bool arithmetic) {
    const IrU32 value = readU16AsU32(sourceAt(inst, 1u), arithmetic);
    const IrU32 count(ir.BitwiseAnd(readU16AsU32(sourceAt(inst, 0u), false).Value(), ir.Constant(15u)));
    const IrU32 result(ir.Emit(opcode, IrType::U32, {&value.Value(), &count.Value()}));
    write16Bits(inst.destination, IrU32(ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu))));
    return true;
}

bool TranslationContext::integer16Binary(const RdnaInstruction& inst, IrOpcode opcode, bool sign) {
    const IrU32 lhs = readU16AsU32(sourceAt(inst, 0u), sign);
    const IrU32 rhs = readU16AsU32(sourceAt(inst, 1u), sign);
    const IrU32 result(ir.Emit(opcode, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    write16Bits(inst.destination, IrU32(ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu))));
    return true;
}

IrU32 TranslationContext::saturateU16Result(const RdnaInstruction& inst, const IrU32& value, bool sign) {
    if (!inst.destination.clamp) {
        return value;
    }
    if (!sign) {
        return IrU32(ir.Emit(IrOpcode::UMin32, IrType::U32, {&value.Value(), &ir.Constant(0xffffu)}));
    }
    const IrU32 upper(ir.Emit(IrOpcode::SMin32, IrType::U32, {&value.Value(), &ir.Constant(0x7fffu)}));
    return IrU32(ir.Emit(IrOpcode::SMax32, IrType::U32, {&upper.Value(), &ir.Constant(0xffff8000u)}));
}

IrU32 TranslationContext::wideAdd(const RdnaInstruction& inst, const IrU32& low, const IrU32& high, const IrU32& addend, bool sign) {
    const IrU32 sum(ir.IAdd(low.Value(), addend.Value()));
    if (!inst.destination.clamp) {
        return sum;
    }
    const IrU32 carry(ir.Select(ir.ULessThan(sum.Value(), low.Value()), ir.Constant(1u), ir.Constant(0u)));
    IrU32 top(ir.IAdd(high.Value(), carry.Value()));
    if (!sign) {
        return IrU32(ir.Select(ir.INotEqual(top.Value(), ir.Constant(0u)), ir.Constant(0xffffffffu), sum.Value()));
    }
    top = IrU32(ir.IAdd(top.Value(), ir.ShiftRightArithmetic(addend.Value(), ir.Constant(31u))));
    const IrU1 overflow(ir.INotEqual(top.Value(), ir.ShiftRightArithmetic(sum.Value(), ir.Constant(31u))));
    IrValue& saturated = ir.BitwiseXor(ir.ShiftRightArithmetic(top.Value(), ir.Constant(31u)), ir.Constant(0x7fffffffu));
    return IrU32(ir.Select(overflow.Value(), saturated, sum.Value()));
}

bool TranslationContext::vMed3I16(const RdnaInstruction& inst) {
    const IrU32 first = readU16AsU32(sourceAt(inst, 0u), true);
    const IrU32 second = readU16AsU32(sourceAt(inst, 1u), true);
    const IrU32 third = readU16AsU32(sourceAt(inst, 2u), true);
    const IrU32 result(ir.Emit(IrOpcode::SMedTri32, IrType::U32, {&first.Value(), &second.Value(), &third.Value()}));
    write16Bits(inst.destination, IrU32(ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu))));
    return true;
}

bool TranslationContext::integer16Ternary(const RdnaInstruction& inst, IrOpcode opcode, bool sign) {
    const IrU32 first = readU16AsU32(sourceAt(inst, 0u), sign);
    const IrU32 second = readU16AsU32(sourceAt(inst, 1u), sign);
    const IrU32 third = readU16AsU32(sourceAt(inst, 2u), sign);
    const IrU32 result(ir.Emit(opcode, IrType::U32, {&first.Value(), &second.Value(), &third.Value()}));
    write16Bits(inst.destination, IrU32(ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu))));
    return true;
}

bool TranslationContext::integer16Mad(const RdnaInstruction& inst, bool sign, bool wide) {
    const IrU32 lhs = readU16AsU32(sourceAt(inst, 0u), sign);
    const IrU32 rhs = readU16AsU32(sourceAt(inst, 1u), sign);
    const IrU32 product(ir.IMul(lhs.Value(), rhs.Value()));
    if (wide) {
        const IrU32 high(sign ? ir.ShiftRightArithmetic(product.Value(), ir.Constant(31u)) : ir.Constant(0u));
        const IrU32 result = wideAdd(inst, product, high, readU32(sourceAt(inst, 2u)), sign);
        writeOperand(inst.destination, &result.Value());
    } else {
        const IrU32 addend = readU16AsU32(sourceAt(inst, 2u), sign);
        const IrU32 result = saturateU16Result(inst, IrU32(ir.IAdd(product.Value(), addend.Value())), sign);
        write16Bits(inst.destination, IrU32(ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu))));
    }
    return true;
}

bool TranslationContext::vAddSubNcI32(const RdnaInstruction& inst, bool subtract) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    IrU32 result(subtract ? ir.ISub(lhs.Value(), rhs.Value()) : ir.IAdd(lhs.Value(), rhs.Value()));
    if (inst.destination.clamp) {
        IrValue& sameSign = subtract ? ir.BitwiseXor(lhs.Value(), rhs.Value()) : ir.BitwiseXor(rhs.Value(), result.Value());
        IrValue& flipped = ir.BitwiseAnd(ir.BitwiseXor(lhs.Value(), result.Value()), sameSign);
        const IrU1 overflow(ir.INotEqual(ir.ShiftRightLogical(flipped, ir.Constant(31u)), ir.Constant(0u)));
        IrValue& saturated = ir.BitwiseXor(ir.ShiftRightArithmetic(lhs.Value(), ir.Constant(31u)), ir.Constant(0x7fffffffu));
        result = IrU32(ir.Select(overflow.Value(), saturated, result.Value()));
    }
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::packedInteger16Shift(const RdnaInstruction& inst, IrOpcode opcode, bool arithmetic) {
    const auto translateLane = [&](bool highLane) {
        const IrU32 count(ir.BitwiseAnd(readU16LaneAsU32(sourceAt(inst, 0u), highLane, false).Value(), ir.Constant(15u)));
        const IrU32 value = readU16LaneAsU32(sourceAt(inst, 1u), highLane, arithmetic);
        return IrU32(ir.Emit(opcode, IrType::U32, {&value.Value(), &count.Value()}));
    };
    const IrU32 low = translateLane(false);
    const IrU32 high = translateLane(true);
    const IrU32 result = packU16Lanes(low, high);
    writeOperand(inst.destination, &result.Value());
    return true;
}

IrU32 TranslationContext::saturateInteger16(const RdnaOperand& destination, IrU32 value, bool sign) {
    if (!destination.clamp) {
        return value;
    }
    IrValue& lower = ir.Emit(IrOpcode::SMax32, IrType::U32, {&value.Value(), &ir.Constant(sign ? 0xffff8000u : 0u)});
    return IrU32(ir.Emit(IrOpcode::SMin32, IrType::U32, {&lower, &ir.Constant(sign ? 0x7fffu : 0xffffu)}));
}

bool TranslationContext::packedInteger16Binary(const RdnaInstruction& inst, IrOpcode opcode, bool sign) {
    const auto translateLane = [&](bool highLane) {
        const IrU32 lhs = readU16LaneAsU32(sourceAt(inst, 0u), highLane, sign);
        const IrU32 rhs = readU16LaneAsU32(sourceAt(inst, 1u), highLane, sign);
        return saturateInteger16(inst.destination, IrU32(ir.Emit(opcode, IrType::U32, {&lhs.Value(), &rhs.Value()})), sign);
    };
    const IrU32 low = translateLane(false);
    const IrU32 high = translateLane(true);
    const IrU32 result = packU16Lanes(low, high);
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::packedInteger16Mad(const RdnaInstruction& inst, bool sign) {
    const auto translateLane = [&](bool highLane) {
        const IrU32 lhs = readU16LaneAsU32(sourceAt(inst, 0u), highLane, sign);
        const IrU32 rhs = readU16LaneAsU32(sourceAt(inst, 1u), highLane, sign);
        IrU32 product(ir.IMul(lhs.Value(), rhs.Value()));
        if (inst.destination.clamp && !sign) {
            product = IrU32(ir.Emit(IrOpcode::UMin32, IrType::U32, {&product.Value(), &ir.Constant(0xffffu)}));
        }
        const IrU32 addend = readU16LaneAsU32(sourceAt(inst, 2u), highLane, sign);
        return saturateInteger16(inst.destination, IrU32(ir.IAdd(product.Value(), addend.Value())), sign);
    };
    const IrU32 low = translateLane(false);
    const IrU32 high = translateLane(true);
    const IrU32 result = packU16Lanes(low, high);
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::packedInteger16MinMax(const RdnaInstruction& inst, IrOpcode opcode, bool sign) {
    const auto translateLane = [&](bool highLane) {
        const IrU32 lhs = readU16LaneAsU32(sourceAt(inst, 0u), highLane, sign);
        const IrU32 rhs = readU16LaneAsU32(sourceAt(inst, 1u), highLane, sign);
        return IrU32(ir.Emit(opcode, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    };
    const IrU32 low = translateLane(false);
    const IrU32 high = translateLane(true);
    const IrU32 result = packU16Lanes(low, high);
    writeOperand(inst.destination, &result.Value());
    return true;
}

IrU1 TranslationContext::u64MaskBinary(const RdnaInstruction& inst, IrOpcode opcode, bool negateRhs, bool negateResult) {
    const IrU1 lhs = readMask(sourceAt(inst, 0u));
    const IrU1 rhsRaw = readMask(sourceAt(inst, 1u));
    IrValue& rhs = negateRhs ? ir.LogicalNot(rhsRaw.Value()) : rhsRaw.Value();
    const IrU1 result(ir.Emit(opcode, IrType::U1, {&lhs.Value(), &rhs}));
    return negateResult ? IrU1(ir.LogicalNot(result.Value())) : result;
}

bool TranslationContext::sU64Mask(const RdnaInstruction& inst, IrOpcode logicalOpcode, IrOpcode bitOpcode, bool negateRhs, bool negateResult, bool unary) {
    const IrU1 invocationResult = unary ? IrU1(ir.LogicalNot(readMask(sourceAt(inst, 0u)).Value())) : u64MaskBinary(inst, logicalOpcode, negateRhs, negateResult);
    const auto isExecOrVcc = [](const RdnaOperand& operand) {
        switch (operand.kind) {
        case RdnaOperandKind::ExecLo:
        case RdnaOperandKind::ExecHi:
        case RdnaOperandKind::VccLo:
        case RdnaOperandKind::VccHi:
            return true;
        default:
            return false;
        }
    };
    if (isExecOrVcc(inst.destination) || isExecOrVcc(sourceAt(inst, 0u)) || (inst.sourceCount > 1u && isExecOrVcc(sourceAt(inst, 1u)))) {
        const std::array<IrU32, 2> mask = writeMask(inst.destination, invocationResult, true);
        ir.SetScc(ir.INotEqual(ir.BitwiseOr(mask[0].Value(), mask[1].Value()), ir.Constant(0u)));
        return true;
    }
    const std::array<IrU32, 2> lhs = readU32Pair(sourceAt(inst, 0u));
    IrU1 maskValid = readMaskValid(sourceAt(inst, 0u));
    if (inst.sourceCount > 1u) {
        maskValid = IrU1(ir.LogicalAnd(maskValid.Value(), readMaskValid(sourceAt(inst, 1u)).Value()));
    }
    std::array<IrU32, 2> result{};
    if (unary) {
        result = {IrU32(ir.BitwiseNot(lhs[0].Value())), IrU32(ir.BitwiseNot(lhs[1].Value()))};
    } else {
        const std::array<IrU32, 2> rhs = readU32Pair(sourceAt(inst, 1u));
        for (std::uint32_t component = 0u; component < 2u; ++component) {
            IrValue& rhsOperand = negateRhs ? ir.BitwiseNot(rhs[component].Value()) : rhs[component].Value();
            const IrU32 value(ir.Emit(bitOpcode, IrType::U32, {&lhs[component].Value(), &rhsOperand}));
            result[component] = negateResult ? IrU32(ir.BitwiseNot(value.Value())) : value;
        }
    }
    writeU32Pair(inst.destination, result);
    if (inst.destination.kind == RdnaOperandKind::ScalarRegister) {
        const auto dst = static_cast<ScalarReg>(inst.destination.reg);
        ir.SetThreadBitScalarReg(dst, invocationResult.Value());
        ir.SetScalarMaskTag(dst, maskValid.Value());
    }
    ir.SetScc(ir.INotEqual(ir.BitwiseOr(result[0].Value(), result[1].Value()), ir.Constant(0u)));
    return true;
}

bool TranslationContext::simpleInteger(const RdnaInstruction& inst, IrOpcode opcode, IrType type, bool reverse, bool maskShiftCount, bool updateScc) {
    std::array<IrValue*, 3> args{};
    for (std::uint32_t index = 0u; index < inst.sourceCount; ++index) {
        const IrType argType = IrOpcodeArgumentType(opcode, index);
        const RdnaOperand& operand = sourceAt(inst, reverse && index < 2u ? 1u - index : index);
        args[index] = readOperand(operand, argType == IrType::Void ? type : argType);
        if (maskShiftCount && index == 1u) {
            args[index] = &ir.BitwiseAnd(*args[index], ir.Constant(31u));
        }
    }
    const IrType resultType = IrOpcodeType(opcode);
    IrValue* result = nullptr;
    switch (inst.sourceCount) {
    case 1u:
        result = &ir.Emit(opcode, resultType, {args[0]});
        break;
    case 2u:
        result = &ir.Emit(opcode, resultType, {args[0], args[1]});
        break;
    case 3u:
        result = &ir.Emit(opcode, resultType, {args[0], args[1], args[2]});
        break;
    default:
        throw std::runtime_error("invalid simple integer source count");
    }
    writeOperand(inst.destination, result);
    if (updateScc) {
        if (resultType == IrType::U64) {
            ir.SetScc(ir.Emit(IrOpcode::INotEqual64, IrType::U1, {result, &ir.ConstantU64(0)}));
        } else {
            ir.SetScc(ir.INotEqual(*result, ir.Constant(0u)));
        }
    }
    return true;
}

bool TranslationContext::composedIntegerBinary(const RdnaInstruction& inst, IrOpcode opcode, bool negateRhs, bool negateResult, bool updateScc) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhsRaw = readU32(sourceAt(inst, 1u));
    const IrU32 rhs = negateRhs ? IrU32(ir.BitwiseNot(rhsRaw.Value())) : rhsRaw;
    IrU32 result(ir.Emit(opcode, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    if (negateResult) {
        result = IrU32(ir.BitwiseNot(result.Value()));
    }
    writeOperand(inst.destination, &result.Value());
    if (updateScc) {
        ir.SetScc(ir.INotEqual(result.Value(), ir.Constant(0u)));
    }
    return true;
}

bool TranslationContext::vAndOrB32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.BitwiseOr(ir.BitwiseAnd(lhs.Value(), rhs.Value()), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vOr3B32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.BitwiseOr(ir.BitwiseOr(lhs.Value(), rhs.Value()), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vXor3B32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.BitwiseXor(ir.BitwiseXor(lhs.Value(), rhs.Value()), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::sFfI32B64(const RdnaInstruction& inst, bool zero) {
    std::array<IrU32, 2> source = extractU64(readU64(sourceAt(inst, 0u)));
    if (zero) {
        source = {IrU32(ir.BitwiseNot(source[0].Value())), IrU32(ir.BitwiseNot(source[1].Value()))};
    }
    const IrU32 lowLsb(ir.Emit(IrOpcode::FindILsb32, IrType::U32, {&source[0].Value()}));
    const IrU32 highLsb(ir.Emit(IrOpcode::FindILsb32, IrType::U32, {&source[1].Value()}));
    const IrU32 highPosition(ir.IAdd(highLsb.Value(), ir.Constant(32u)));
    const IrU1 lowNonZero(ir.INotEqual(source[0].Value(), ir.Constant(0u)));
    const IrU1 highNonZero(ir.INotEqual(source[1].Value(), ir.Constant(0u)));
    const IrU32 highOrDefault(ir.Select(highNonZero.Value(), highPosition.Value(), ir.Constant(0xffffffffu)));
    const IrU32 result(ir.Select(lowNonZero.Value(), lowLsb.Value(), highOrDefault.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vFfbh32(const RdnaInstruction& inst, bool sign) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    IrValue& value = sign ? ir.BitwiseXor(source.Value(), ir.ShiftRightArithmetic(source.Value(), ir.Constant(31u))) : source.Value();
    const IrU32 msb(ir.Emit(IrOpcode::FindUMsb32, IrType::U32, {&value}));
    const IrU32 position(ir.ISub(ir.Constant(31u), msb.Value()));
    const IrU1 nonZero(ir.INotEqual(value, ir.Constant(0u)));
    const IrU32 result(ir.Select(nonZero.Value(), position.Value(), ir.Constant(0xffffffffu)));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::sFlbitI32B64(const RdnaInstruction& inst, bool sign) {
    IrU64 source = readU64(sourceAt(inst, 0u));
    if (sign) {
        const std::array<IrU32, 2> halves = extractU64(source);
        IrValue& mask = ir.ShiftRightArithmetic(halves[1].Value(), ir.Constant(31u));
        source = IrU64(ir.ConstructU64(ir.BitwiseXor(halves[0].Value(), mask), ir.BitwiseXor(halves[1].Value(), mask)));
    }
    const IrU32 msb(ir.Emit(IrOpcode::FindUMsb64, IrType::U32, {&source.Value()}));
    const IrU32 position(ir.ISub(ir.Constant(63u), msb.Value()));
    const IrU1 nonZero(ir.Emit(IrOpcode::INotEqual64, IrType::U1, {&source.Value(), &ir.ConstantU64(0)}));
    const IrU32 result(ir.Select(nonZero.Value(), position.Value(), ir.Constant(0xffffffffu)));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::integer24(const RdnaInstruction& inst, bool sign, bool addend, bool high) {
    const IrOpcode extractOpcode = sign ? IrOpcode::BitFieldSExtract : IrOpcode::BitFieldUExtract;
    const IrU32 lhsSource = readU32(sourceAt(inst, 0u));
    const IrU32 rhsSource = readU32(sourceAt(inst, 1u));
    const IrU32 lhs(ir.Emit(extractOpcode, IrType::U32, {&lhsSource.Value(), &ir.Constant(0u), &ir.Constant(24u)}));
    const IrU32 rhs(ir.Emit(extractOpcode, IrType::U32, {&rhsSource.Value(), &ir.Constant(0u), &ir.Constant(24u)}));
    const IrOpcode multiplyOpcode = high ? (sign ? IrOpcode::SMulHi : IrOpcode::UMulHi) : IrOpcode::IMul32;
    IrU32 result(ir.Emit(multiplyOpcode, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    if (inst.destination.clamp && !high && !addend) {
        const IrU32 top(ir.Emit(sign ? IrOpcode::SMulHi : IrOpcode::UMulHi, IrType::U32, {&lhs.Value(), &rhs.Value()}));
        IrValue& expected = sign ? ir.ShiftRightArithmetic(result.Value(), ir.Constant(31u)) : ir.Constant(0u);
        IrValue& saturated = sign ? ir.BitwiseXor(ir.ShiftRightArithmetic(top.Value(), ir.Constant(31u)), ir.Constant(0x7fffffffu)) : ir.Constant(0xffffffffu);
        result = IrU32(ir.Select(ir.INotEqual(top.Value(), expected), saturated, result.Value()));
    }
    if (addend) {
        const IrU32 productHigh(inst.destination.clamp ? ir.Emit(sign ? IrOpcode::SMulHi : IrOpcode::UMulHi, IrType::U32, {&lhs.Value(), &rhs.Value()}) : ir.Constant(0u));
        result = wideAdd(inst, result, productHigh, readU32(sourceAt(inst, 2u)), sign);
    }
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vMad64x32(const RdnaInstruction& inst, bool sign) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const std::array<IrU32, 2> add = extractU64(readU64(sourceAt(inst, 2u)));
    const IrU32 mulLow(ir.IMul(lhs.Value(), rhs.Value()));
    const IrU32 mulHigh(ir.Emit(sign ? IrOpcode::SMulHi : IrOpcode::UMulHi, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    const IrU32 low(ir.IAdd(mulLow.Value(), add[0].Value()));
    const IrU1 carryLow(ir.ULessThan(low.Value(), mulLow.Value()));
    const IrU32 high0(ir.IAdd(mulHigh.Value(), add[1].Value()));
    const IrU1 carry0(ir.ULessThan(high0.Value(), mulHigh.Value()));
    const IrU32 carryLowU32(ir.Select(carryLow.Value(), ir.Constant(1u), ir.Constant(0u)));
    const IrU32 high(ir.IAdd(high0.Value(), carryLowU32.Value()));
    const IrU1 carry1(ir.ULessThan(high.Value(), high0.Value()));
    const IrU1 carry(ir.LogicalOr(carry0.Value(), carry1.Value()));
    IrU32 resultLow = low;
    IrU32 resultHigh = high;
    if (inst.destination.clamp) {
        if (sign) {
            IrValue& flipped = ir.BitwiseAnd(ir.BitwiseXor(mulHigh.Value(), high.Value()), ir.BitwiseXor(add[1].Value(), high.Value()));
            const IrU1 overflow(ir.INotEqual(ir.ShiftRightLogical(flipped, ir.Constant(31u)), ir.Constant(0u)));
            IrValue& addSign = ir.ShiftRightArithmetic(add[1].Value(), ir.Constant(31u));
            resultLow = IrU32(ir.Select(overflow.Value(), ir.BitwiseXor(addSign, ir.Constant(0xffffffffu)), low.Value()));
            resultHigh = IrU32(ir.Select(overflow.Value(), ir.BitwiseXor(addSign, ir.Constant(0x7fffffffu)), high.Value()));
        } else {
            resultLow = IrU32(ir.Select(carry.Value(), ir.Constant(0xffffffffu), low.Value()));
            resultHigh = IrU32(ir.Select(carry.Value(), ir.Constant(0xffffffffu), high.Value()));
        }
    }
    const IrU64 result(ir.ConstructU64(resultLow.Value(), resultHigh.Value()));
    writeOperand(inst.destination, &result.Value());
    if (inst.destination2.kind != RdnaOperandKind::Null && inst.destination2.kind != RdnaOperandKind::Unknown) {
        if (!sign) {
            writeMask(inst.destination2, carry);
            return true;
        }
        const IrU32 signsDiffer(ir.ShiftRightLogical(ir.BitwiseXor(mulHigh.Value(), add[1].Value()), ir.Constant(31u)));
        const IrU32 carryU32(ir.Select(carry.Value(), ir.Constant(1u), ir.Constant(0u)));
        writeMask(inst.destination2, IrU1(ir.INotEqual(signsDiffer.Value(), carryU32.Value())));
    }
    return true;
}

bool TranslationContext::vSadU32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 lo(ir.Emit(IrOpcode::UMin32, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    const IrU32 hi(ir.Emit(IrOpcode::UMax32, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result = wideAdd(inst, IrU32(ir.ISub(hi.Value(), lo.Value())), IrU32(ir.Constant(0u)), addend, false);
    writeOperand(inst.destination, &result.Value());
    return true;
}

IrU32 TranslationContext::byteSad(const IrU32& lhs, const IrU32& rhs, std::uint32_t fieldBits, bool masked) {
    IrU32 sum(ir.Constant(0u));
    for (std::uint32_t offset = 0u; offset < 32u; offset += fieldBits) {
        const IrU32 lhsField(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&lhs.Value(), &ir.Constant(offset), &ir.Constant(fieldBits)}));
        const IrU32 rhsField(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&rhs.Value(), &ir.Constant(offset), &ir.Constant(fieldBits)}));
        const IrU32 lo(ir.Emit(IrOpcode::UMin32, IrType::U32, {&lhsField.Value(), &rhsField.Value()}));
        const IrU32 hi(ir.Emit(IrOpcode::UMax32, IrType::U32, {&lhsField.Value(), &rhsField.Value()}));
        IrU32 difference(ir.ISub(hi.Value(), lo.Value()));
        if (masked) {
            const IrU1 reference(ir.INotEqual(rhsField.Value(), ir.Constant(0u)));
            difference = IrU32(ir.Select(reference.Value(), difference.Value(), ir.Constant(0u)));
        }
        sum = IrU32(ir.IAdd(sum.Value(), difference.Value()));
    }
    return sum;
}

bool TranslationContext::subwordSad(const RdnaInstruction& inst, std::uint32_t fieldBits, std::uint32_t sumShift, bool masked) {
    IrU32 sum = byteSad(readU32(sourceAt(inst, 0u)), readU32(sourceAt(inst, 1u)), fieldBits, masked);
    if (sumShift != 0u) {
        sum = IrU32(ir.ShiftLeftLogical(sum.Value(), ir.Constant(sumShift)));
    }
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result = wideAdd(inst, sum, IrU32(ir.Constant(0u)), addend, false);
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vQsadU8(const RdnaInstruction& inst, bool masked, bool wide) {
    const std::array<IrU32, 2> source = readU32Pair(sourceAt(inst, 0u));
    const IrU32 reference = readU32(sourceAt(inst, 1u));
    const auto sad = [&](std::uint32_t shift) {
        if (shift == 0u) {
            return byteSad(source[0], reference, 8u, masked);
        }
        const IrU32 window(ir.BitwiseOr(ir.ShiftRightLogical(source[0].Value(), ir.Constant(shift)), ir.ShiftLeftLogical(source[1].Value(), ir.Constant(32u - shift))));
        return byteSad(window, reference, 8u, masked);
    };
    const std::array<IrU32, 2> accumulator = readU32Pair(sourceAt(inst, 2u));
    if (wide) {
        const std::array<IrU32, 2> accumulatorHigh = readU32Pair(offsetOperand(sourceAt(inst, 2u), 2u));
        const IrU32 zero(ir.Constant(0u));
        writeU32Pair(inst.destination, {wideAdd(inst, sad(0u), zero, accumulator[0], false), wideAdd(inst, sad(8u), zero, accumulator[1], false)});
        writeU32Pair(offsetOperand(inst.destination, 2u),
            {wideAdd(inst, sad(16u), zero, accumulatorHigh[0], false), wideAdd(inst, sad(24u), zero, accumulatorHigh[1], false)});
        return true;
    }
    const auto packed = [&](const IrU32& addend, std::uint32_t shift) {
        const IrU32 low = saturateU16Result(inst, IrU32(ir.IAdd(ir.BitwiseAnd(addend.Value(), ir.Constant(0xffffu)), sad(shift).Value())), false);
        const IrU32 high = saturateU16Result(inst, IrU32(ir.IAdd(ir.ShiftRightLogical(addend.Value(), ir.Constant(16u)), sad(shift + 8u).Value())), false);
        return IrU32(ir.BitwiseOr(ir.BitwiseAnd(low.Value(), ir.Constant(0xffffu)), ir.ShiftLeftLogical(high.Value(), ir.Constant(16u))));
    };
    writeU32Pair(inst.destination, {packed(accumulator[0], 0u), packed(accumulator[1], 16u)});
    return true;
}

bool TranslationContext::vAdd3U32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.IAdd(ir.IAdd(lhs.Value(), rhs.Value()), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::sBitsetB32(const RdnaInstruction& inst, bool set) {
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 0u)).Value(), ir.Constant(31u)));
    const IrU32 bit(ir.ShiftLeftLogical(ir.Constant(1u), offset.Value()));
    const IrU32 old = readU32(inst.destination);
    const IrU32 result(set ? ir.BitwiseOr(old.Value(), bit.Value()) : ir.BitwiseAnd(old.Value(), ir.BitwiseNot(bit.Value())));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::sBitsetB64(const RdnaInstruction& inst, bool set) {
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 0u)).Value(), ir.Constant(63u)));
    const IrU32 wordBit(ir.BitwiseAnd(offset.Value(), ir.Constant(31u)));
    const IrU32 bit(ir.ShiftLeftLogical(ir.Constant(1u), wordBit.Value()));
    const std::array<IrU32, 2> old = readU32Pair(inst.destination);
    const IrU32 lowValue(set ? ir.BitwiseOr(old[0].Value(), bit.Value()) : ir.BitwiseAnd(old[0].Value(), ir.BitwiseNot(bit.Value())));
    const IrU32 highValue(set ? ir.BitwiseOr(old[1].Value(), bit.Value()) : ir.BitwiseAnd(old[1].Value(), ir.BitwiseNot(bit.Value())));
    const IrU1 high(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&offset.Value(), &ir.Constant(32u)}));
    const std::array<IrU32, 2> result{IrU32(ir.Select(high.Value(), old[0].Value(), lowValue.Value())), IrU32(ir.Select(high.Value(), highValue.Value(), old[1].Value()))};
    writeU32Pair(inst.destination, result);
    return true;
}

bool TranslationContext::vBcntU32B32(const RdnaInstruction& inst) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 count(ir.Emit(IrOpcode::BitCount32, IrType::U32, {&source.Value()}));
    const IrU32 addend = readU32(sourceAt(inst, 1u));
    const IrU32 result(ir.IAdd(count.Value(), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vMbcntU32B32(const RdnaInstruction& inst, bool low) {
    const IrU32 lane(ir.Emit(IrOpcode::LaneId, IrType::U32, {}));
    const IrU32 local(ir.BitwiseAnd(lane.Value(), ir.Constant(31u)));
    const IrU32 shifted(ir.ShiftLeftLogical(ir.Constant(1u), local.Value()));
    const IrU32 below(ir.ISub(shifted.Value(), ir.Constant(1u)));
    const IrU1 highLane(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&lane.Value(), &ir.Constant(32u)}));
    const IrU32 threadMask(low ? ir.Select(highLane.Value(), ir.Constant(0xffffffffu), below.Value()) : ir.Select(highLane.Value(), below.Value(), ir.Constant(0u)));
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 active(ir.BitwiseAnd(source.Value(), threadMask.Value()));
    const IrU32 count(ir.Emit(IrOpcode::BitCount32, IrType::U32, {&active.Value()}));
    const IrU32 addend = readU32(sourceAt(inst, 1u));
    const IrU32 result(ir.IAdd(count.Value(), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::sBitreplicateB64B32(const RdnaInstruction& inst) {
    const auto replicate = [&](IrU32 value) {
        IrU32 bits(ir.BitwiseOr(value.Value(), ir.ShiftLeftLogical(value.Value(), ir.Constant(8u))));
        bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x00ff00ffu)));
        bits = IrU32(ir.BitwiseOr(bits.Value(), ir.ShiftLeftLogical(bits.Value(), ir.Constant(4u))));
        bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x0f0f0f0fu)));
        bits = IrU32(ir.BitwiseOr(bits.Value(), ir.ShiftLeftLogical(bits.Value(), ir.Constant(2u))));
        bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x33333333u)));
        bits = IrU32(ir.BitwiseOr(bits.Value(), ir.ShiftLeftLogical(bits.Value(), ir.Constant(1u))));
        bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x55555555u)));
        return IrU32(ir.BitwiseOr(bits.Value(), ir.ShiftLeftLogical(bits.Value(), ir.Constant(1u))));
    };
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 low(ir.BitwiseAnd(source.Value(), ir.Constant(0xffffu)));
    const IrU32 high(ir.ShiftRightLogical(source.Value(), ir.Constant(16u)));
    const IrU32 lowReplicated = replicate(low);
    const IrU32 highReplicated = replicate(high);
    const IrU64 result(ir.ConstructU64(lowReplicated.Value(), highReplicated.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

IrU32 TranslationContext::readRelativeScalar(std::uint32_t base, IrValue& offset) {
    IrU32 result(ir.GetScalarReg(static_cast<ScalarReg>(base)));
    for (std::uint32_t reg = base + 1u; reg < NumScalarRegs; ++reg) {
        IrValue& hit = ir.IEqual(offset, ir.Constant(reg - base));
        result = IrU32(ir.Select(hit, ir.GetScalarReg(static_cast<ScalarReg>(reg)), result.Value()));
    }
    return result;
}

void TranslationContext::writeRelativeScalar(std::uint32_t base, IrValue& offset, IrU32 value) {
    for (std::uint32_t reg = base; reg < NumScalarRegs; ++reg) {
        RdnaOperand target{};
        target.kind = RdnaOperandKind::ScalarRegister;
        target.reg = reg;
        IrValue& hit = ir.IEqual(offset, ir.Constant(reg - base));
        writeRawU32(target, IrU32(ir.Select(hit, value.Value(), ir.GetScalarReg(static_cast<ScalarReg>(reg)))));
    }
}

bool TranslationContext::sMovrel(const RdnaInstruction& inst) {
    const bool wide = inst.op == RdnaOpcode::SMovrelsB64 || inst.op == RdnaOpcode::SMovreldB64;
    const bool relativeSource = inst.op != RdnaOpcode::SMovreldB32 && inst.op != RdnaOpcode::SMovreldB64;
    const bool relativeDestination = inst.op != RdnaOpcode::SMovrelsB32 && inst.op != RdnaOpcode::SMovrelsB64;
    const RdnaOperand& source = sourceAt(inst, 0u);
    if ((relativeSource && source.kind != RdnaOperandKind::ScalarRegister) || (relativeDestination && inst.destination.kind != RdnaOperandKind::ScalarRegister)) {
        throw std::runtime_error("s_movrel operand is not a scalar register");
    }
    IrValue& m0 = ir.GetM0();
    const bool split = inst.op == RdnaOpcode::SMovrelsd2B32;
    IrValue& sourceOffset = split ? ir.BitwiseAnd(m0, ir.Constant(0x3ffu)) : wide ? ir.BitwiseAnd(m0, ir.Constant(~1u)) : m0;
    IrValue& destinationOffset = split ? ir.BitwiseAnd(ir.ShiftRightLogical(m0, ir.Constant(16u)), ir.Constant(0x3ffu)) : sourceOffset;
    const std::uint32_t count = wide ? 2u : 1u;
    std::array<IrU32, 2> values{IrU32(ir.Constant(0u)), IrU32(ir.Constant(0u))};
    if (relativeSource) {
        for (std::uint32_t index = 0u; index < count; ++index) values[index] = readRelativeScalar(source.reg + index, sourceOffset);
    } else if (wide) {
        values = readU32Pair(source);
    } else {
        values[0] = readU32(source);
    }
    if (!relativeDestination) {
        if (wide) writeU32Pair(inst.destination, values);
        else writeRawU32(inst.destination, values[0]);
        return true;
    }
    for (std::uint32_t index = 0u; index < count; ++index) writeRelativeScalar(inst.destination.reg + index, destinationOffset, values[index]);
    return true;
}

bool TranslationContext::sQuadmask(const RdnaInstruction& inst, bool wide) {
    const auto compact = [&](IrU32 value) {
        IrU32 bits(ir.BitwiseOr(value.Value(), ir.ShiftRightLogical(value.Value(), ir.Constant(1u))));
        bits = IrU32(ir.BitwiseOr(bits.Value(), ir.ShiftRightLogical(bits.Value(), ir.Constant(2u))));
        bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x11111111u)));
        bits = IrU32(ir.BitwiseAnd(ir.BitwiseOr(bits.Value(), ir.ShiftRightLogical(bits.Value(), ir.Constant(3u))), ir.Constant(0x03030303u)));
        bits = IrU32(ir.BitwiseAnd(ir.BitwiseOr(bits.Value(), ir.ShiftRightLogical(bits.Value(), ir.Constant(6u))), ir.Constant(0x000f000fu)));
        return IrU32(ir.BitwiseAnd(ir.BitwiseOr(bits.Value(), ir.ShiftRightLogical(bits.Value(), ir.Constant(12u))), ir.Constant(0xffu)));
    };
    if (!wide) {
        const IrU32 quads = compact(readU32(sourceAt(inst, 0u)));
        writeOperand(inst.destination, &quads.Value());
        ir.SetScc(ir.INotEqual(quads.Value(), ir.Constant(0u)));
        return true;
    }
    const std::array<IrU32, 2> source = readU32Pair(sourceAt(inst, 0u));
    const IrU32 lowCompact = compact(source[0]);
    const IrU32 highCompact = compact(source[1]);
    const IrU32 quads(ir.BitwiseOr(lowCompact.Value(), ir.ShiftLeftLogical(highCompact.Value(), ir.Constant(8u))));
    const IrU64 result(ir.ConstructU64(quads.Value(), ir.Constant(0u)));
    writeOperand(inst.destination, &result.Value());
    ir.SetScc(ir.Emit(IrOpcode::INotEqual64, IrType::U1, {&result.Value(), &ir.ConstantU64(0)}));
    return true;
}

bool TranslationContext::bfmB32(const RdnaInstruction& inst) {
    const IrU32 count(ir.BitwiseAnd(readU32(sourceAt(inst, 0u)).Value(), ir.Constant(31u)));
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(31u)));
    const IrU32 result(ir.Emit(IrOpcode::BitFieldInsert, IrType::U32, {&ir.Constant(0u), &ir.Constant(0xffffffffu), &offset.Value(), &count.Value()}));
    writeOperand(inst.destination, &result.Value());
    return true;
}

IrU32 TranslationContext::rightMask32(IrU32 count) {
    return IrU32(ir.Emit(IrOpcode::BitFieldInsert, IrType::U32, {&ir.Constant(0u), &ir.Constant(0xffffffffu), &ir.Constant(0u), &count.Value()}));
}

IrU64 TranslationContext::rightMask64(IrU32 count) {
    const IrU1 below32(ir.ULessThan(count.Value(), ir.Constant(32u)));
    const IrU1 above32(ir.UGreaterThan(count.Value(), ir.Constant(32u)));
    const IrU32 lowCount(ir.Select(below32.Value(), count.Value(), ir.Constant(32u)));
    const IrU32 highCount(ir.Select(above32.Value(), ir.ISub(count.Value(), ir.Constant(32u)), ir.Constant(0u)));
    const IrU32 low = rightMask32(lowCount);
    const IrU32 high = rightMask32(highCount);
    return IrU64(ir.ConstructU64(low.Value(), high.Value()));
}

bool TranslationContext::sBfmB64(const RdnaInstruction& inst) {
    const IrU32 count(ir.BitwiseAnd(readU32(sourceAt(inst, 0u)).Value(), ir.Constant(63u)));
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(63u)));
    const IrU64 mask = rightMask64(count);
    const IrU64 result(ir.Emit(IrOpcode::ShiftLeftLogical64, IrType::U64, {&mask.Value(), &offset.Value()}));
    writeOperand(inst.destination, &result.Value());
    return true;
}

IrU32 TranslationContext::extractBits32(IrU32 source, IrU32 offset, IrU32 rawCount, bool sign) {
    const IrU32 count(ir.Emit(IrOpcode::UMin32, IrType::U32, {&rawCount.Value(), &ir.Constant(32u)}));
    const IrU32 discard(ir.BitwiseAnd(ir.ISub(ir.Constant(32u), count.Value()), ir.Constant(31u)));
    const IrU32 shifted(sign ? ir.ShiftRightArithmetic(source.Value(), offset.Value()) : ir.ShiftRightLogical(source.Value(), offset.Value()));
    const IrU32 aligned(ir.ShiftLeftLogical(shifted.Value(), discard.Value()));
    const IrU32 field(sign ? ir.ShiftRightArithmetic(aligned.Value(), discard.Value()) : ir.ShiftRightLogical(aligned.Value(), discard.Value()));
    return IrU32(ir.Select(ir.INotEqual(count.Value(), ir.Constant(0u)), field.Value(), ir.Constant(0u)));
}

bool TranslationContext::sBfeU32(const RdnaInstruction& inst, bool sign) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 field = readU32(sourceAt(inst, 1u));
    const IrU32 offset(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&field.Value(), &ir.Constant(0u), &ir.Constant(5u)}));
    const IrU32 rawCount(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&field.Value(), &ir.Constant(16u), &ir.Constant(7u)}));
    const IrU32 result = extractBits32(source, offset, rawCount, sign);
    writeOperand(inst.destination, &result.Value());
    ir.SetScc(ir.INotEqual(result.Value(), ir.Constant(0u)));
    return true;
}

bool TranslationContext::sBfeU64(const RdnaInstruction& inst) {
    const IrU64 source = readU64(sourceAt(inst, 0u));
    const IrU32 field = readU32(sourceAt(inst, 1u));
    const IrU32 offset(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&field.Value(), &ir.Constant(0u), &ir.Constant(6u)}));
    const IrU32 rawCount(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&field.Value(), &ir.Constant(16u), &ir.Constant(7u)}));
    const IrU32 available(ir.ISub(ir.Constant(64u), offset.Value()));
    const IrU32 count(ir.Emit(IrOpcode::UMin32, IrType::U32, {&rawCount.Value(), &available.Value()}));
    const IrU64 shifted(ir.Emit(IrOpcode::ShiftRightLogical64, IrType::U64, {&source.Value(), &offset.Value()}));
    const IrU64 mask = rightMask64(count);
    const IrU64 result(ir.Emit(IrOpcode::BitwiseAnd64, IrType::U64, {&shifted.Value(), &mask.Value()}));
    writeOperand(inst.destination, &result.Value());
    ir.SetScc(ir.Emit(IrOpcode::INotEqual64, IrType::U1, {&result.Value(), &ir.ConstantU64(0)}));
    return true;
}

bool TranslationContext::vBfeU32(const RdnaInstruction& inst, bool sign) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(31u)));
    const IrU32 rawCount(ir.BitwiseAnd(readU32(sourceAt(inst, 2u)).Value(), ir.Constant(31u)));
    const IrU32 available(ir.ISub(ir.Constant(32u), offset.Value()));
    const IrU32 count(ir.Emit(IrOpcode::UMin32, IrType::U32, {&rawCount.Value(), &available.Value()}));
    const IrOpcode opcode = sign ? IrOpcode::BitFieldSExtract : IrOpcode::BitFieldUExtract;
    const IrU32 result(ir.Emit(opcode, IrType::U32, {&source.Value(), &offset.Value(), &count.Value()}));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vBfiB32(const RdnaInstruction& inst) {
    const IrU32 bits = readU32(sourceAt(inst, 0u));
    const IrU32 insert = readU32(sourceAt(inst, 1u));
    const IrU32 base = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.BitwiseOr(ir.BitwiseAnd(bits.Value(), insert.Value()), ir.BitwiseAnd(ir.BitwiseNot(bits.Value()), base.Value())));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::sBitcmpB32(const RdnaInstruction& inst, bool expected) {
    const IrU32 value = readU32(sourceAt(inst, 0u));
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(31u)));
    const IrU32 bit(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&value.Value(), &offset.Value(), &ir.Constant(1u)}));
    writeCompareResult(inst.destination, IrU1(ir.IEqual(bit.Value(), ir.Constant(expected ? 1u : 0u))));
    return true;
}

bool TranslationContext::sBitcmpB64(const RdnaInstruction& inst, bool expected) {
    const IrU64 value = readU64(sourceAt(inst, 0u));
    const IrU32 offset(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(63u)));
    const IrU64 shifted(ir.Emit(IrOpcode::ShiftRightLogical64, IrType::U64, {&value.Value(), &offset.Value()}));
    const IrU64 bit(ir.Emit(IrOpcode::BitwiseAnd64, IrType::U64, {&shifted.Value(), &ir.ConstantU64(1u)}));
    writeCompareResult(inst.destination, IrU1(ir.Emit(IrOpcode::IEqual64, IrType::U1, {&bit.Value(), &ir.ConstantU64(expected ? 1u : 0u)})));
    return true;
}

bool TranslationContext::vAlignbitB32(const RdnaInstruction& inst) {
    const IrU32 hi = readU32(sourceAt(inst, 0u));
    const IrU32 lo = readU32(sourceAt(inst, 1u));
    const IrU32 shift(ir.BitwiseAnd(readU32(sourceAt(inst, 2u)).Value(), ir.Constant(31u)));
    const IrU32 loPart(ir.ShiftRightLogical(lo.Value(), shift.Value()));
    const IrU32 inverse(ir.BitwiseAnd(ir.ISub(ir.Constant(32u), shift.Value()), ir.Constant(31u)));
    const IrU32 hiPartRaw(ir.ShiftLeftLogical(hi.Value(), inverse.Value()));
    const IrU1 shiftNonZero(ir.INotEqual(shift.Value(), ir.Constant(0u)));
    const IrU32 hiPart(ir.Select(shiftNonZero.Value(), hiPartRaw.Value(), ir.Constant(0u)));
    const IrU32 result(ir.BitwiseOr(loPart.Value(), hiPart.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vAlignbyteB32(const RdnaInstruction& inst) {
    const IrU32 hi = readU32(sourceAt(inst, 0u));
    const IrU32 lo = readU32(sourceAt(inst, 1u));
    const IrU32 byteOffset(ir.BitwiseAnd(readU32(sourceAt(inst, 2u)).Value(), ir.Constant(3u)));
    const IrU32 bitOffset(ir.ShiftLeftLogical(byteOffset.Value(), ir.Constant(3u)));
    const IrU64 concatenated(ir.ConstructU64(lo.Value(), hi.Value()));
    const IrU64 shifted(ir.Emit(IrOpcode::ShiftRightLogical64, IrType::U64, {&concatenated.Value(), &bitOffset.Value()}));
    const std::array<IrU32, 2> extracted = extractU64(shifted);
    writeOperand(inst.destination, &extracted[0].Value());
    return true;
}

bool TranslationContext::vLshlAddU32(const RdnaInstruction& inst) {
    const IrU32 shift(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(31u)));
    const IrU32 shifted(ir.ShiftLeftLogical(readU32(sourceAt(inst, 0u)).Value(), shift.Value()));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.IAdd(shifted.Value(), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vAddLshlU32(const RdnaInstruction& inst) {
    const IrU32 shift(ir.BitwiseAnd(readU32(sourceAt(inst, 2u)).Value(), ir.Constant(31u)));
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 sum(ir.IAdd(lhs.Value(), rhs.Value()));
    const IrU32 result(ir.ShiftLeftLogical(sum.Value(), shift.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vPermB32(const RdnaInstruction& inst) {
    const IrU32 high = readU32(sourceAt(inst, 0u));
    const IrU32 low = readU32(sourceAt(inst, 1u));
    const IrU32 selectors = readU32(sourceAt(inst, 2u));
    IrValue* result = &ir.Constant(0u);
    for (std::uint32_t byte = 0u; byte < 4u; ++byte) {
        IrValue& selector = ir.BitwiseAnd(ir.ShiftRightLogical(selectors.Value(), ir.Constant(byte * 8u)), ir.Constant(255u));
        IrValue& signIndex = ir.IAdd(ir.IMul(ir.BitwiseAnd(selector, ir.Constant(3u)), ir.Constant(2u)), ir.Constant(1u));
        IrValue& index = ir.Select(ir.ULessThan(selector, ir.Constant(8u)), selector, signIndex);
        IrValue& word = ir.Select(ir.ULessThan(index, ir.Constant(4u)), low.Value(), high.Value());
        IrValue& shift = ir.IMul(ir.BitwiseAnd(index, ir.Constant(3u)), ir.Constant(8u));
        IrValue& value = ir.BitwiseAnd(ir.ShiftRightLogical(word, shift), ir.Constant(255u));
        IrValue& sign = ir.Select(ir.UGreaterThan(value, ir.Constant(127u)), ir.Constant(255u), ir.Constant(0u));
        IrValue& selected = ir.Select(ir.ULessThan(selector, ir.Constant(8u)), value, sign);
        IrValue& fill = ir.Select(ir.IEqual(selector, ir.Constant(12u)), ir.Constant(0u), ir.Constant(255u));
        IrValue& output = ir.Select(ir.ULessThan(selector, ir.Constant(12u)), selected, fill);
        result = &ir.BitwiseOr(*result, ir.ShiftLeftLogical(output, ir.Constant(byte * 8u)));
    }
    writeOperand(inst.destination, result);
    return true;
}

bool TranslationContext::vLerpU8(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 rounding = readU32(sourceAt(inst, 2u));
    IrU32 result(ir.Constant(0u));
    for (std::uint32_t offset = 0u; offset < 32u; offset += 8u) {
        const IrU32 lhsByte(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&lhs.Value(), &ir.Constant(offset), &ir.Constant(8u)}));
        const IrU32 rhsByte(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&rhs.Value(), &ir.Constant(offset), &ir.Constant(8u)}));
        const IrU32 roundUp(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&rounding.Value(), &ir.Constant(offset), &ir.Constant(1u)}));
        const IrU32 sum(ir.IAdd(ir.IAdd(lhsByte.Value(), rhsByte.Value()), roundUp.Value()));
        const IrU32 average(ir.ShiftRightLogical(sum.Value(), ir.Constant(1u)));
        result = IrU32(ir.BitwiseOr(result.Value(), ir.ShiftLeftLogical(average.Value(), ir.Constant(offset))));
    }
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vXadU32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 xorValue(ir.BitwiseXor(lhs.Value(), rhs.Value()));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.IAdd(xorValue.Value(), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vLshlOrB32(const RdnaInstruction& inst) {
    const IrU32 shift(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(31u)));
    const IrU32 shifted(ir.ShiftLeftLogical(readU32(sourceAt(inst, 0u)).Value(), shift.Value()));
    const IrU32 addend = readU32(sourceAt(inst, 2u));
    const IrU32 result(ir.BitwiseOr(shifted.Value(), addend.Value()));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::integerDot(const RdnaInstruction& inst, std::uint32_t elementBits, bool sign, bool accumulator) {
    IrU32 result = readU32(accumulator ? accumulatorOperand(inst) : sourceAt(inst, 2u));
    if (elementBits == 16u) {
        for (const bool highLane : {false, true}) {
            const IrU32 lhs = readU16LaneAsU32(sourceAt(inst, 0u), highLane, sign);
            const IrU32 rhs = readU16LaneAsU32(sourceAt(inst, 1u), highLane, sign);
            result = IrU32(ir.IAdd(result.Value(), ir.IMul(lhs.Value(), rhs.Value())));
        }
    } else {
        const IrOpcode extractOpcode = sign ? IrOpcode::BitFieldSExtract : IrOpcode::BitFieldUExtract;
        const IrU32 lhsSource = readU32(sourceAt(inst, 0u));
        const IrU32 rhsSource = readU32(sourceAt(inst, 1u));
        for (std::uint32_t offset = 0u; offset < 32u; offset += elementBits) {
            const IrU32 lhs(ir.Emit(extractOpcode, IrType::U32, {&lhsSource.Value(), &ir.Constant(offset), &ir.Constant(elementBits)}));
            const IrU32 rhs(ir.Emit(extractOpcode, IrType::U32, {&rhsSource.Value(), &ir.Constant(offset), &ir.Constant(elementBits)}));
            result = IrU32(ir.IAdd(result.Value(), ir.IMul(lhs.Value(), rhs.Value())));
        }
    }
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vCndmaskB32(const RdnaInstruction& inst) {
    RdnaOperand defaultMask{};
    defaultMask.kind = RdnaOperandKind::VccLo;
    const RdnaOperand& maskOperand = inst.sourceCount >= 3u ? sourceAt(inst, 2u) : defaultMask;
    const IrU1 condition = readMask(maskOperand);
    const RdnaOperand& falseOperand = sourceAt(inst, 0u);
    const RdnaOperand& trueOperand = sourceAt(inst, 1u);
    const IrU32 falseValue = readU32(falseOperand);
    const IrU32 trueValue = readU32(trueOperand);
    writeOperand(inst.destination, &ir.Select(condition.Value(), trueValue.Value(), falseValue.Value()));
    return true;
}

bool TranslationContext::packB16(const RdnaInstruction& inst, bool high0, bool high1) {
    const IrU32 source0 = readU32(sourceAt(inst, 0u));
    const IrU32 source1 = readU32(sourceAt(inst, 1u));
    const IrU32 lo(high0 ? ir.ShiftRightLogical(source0.Value(), ir.Constant(16u)) : source0.Value());
    const IrU32 hi(high1 ? ir.ShiftRightLogical(source1.Value(), ir.Constant(16u)) : source1.Value());
    const IrU32 result = packU16Lanes(lo, hi);
    writeOperand(inst.destination, &result.Value());
    return true;
}

void TranslateIntegerInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateIntegerInstruction not implemented");
}

}
