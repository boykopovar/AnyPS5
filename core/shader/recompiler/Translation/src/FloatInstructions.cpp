#include "Translation/FloatInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ShaderRecompiler {

void TranslateFloatInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateFloatInstruction not implemented");
}

bool TranslationContext::packedFloat16(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool quietSnan) {
    const auto translateLane = [&](bool high) -> IrF32 {
        const IrF32 lhs = readF16LaneAsF32(sourceAt(inst, 0u), high, true);
        const IrF32 rhs = readF16LaneAsF32(sourceAt(inst, 1u), high, true);
        if (accumulator) {
            const IrF32 acc = readF16LaneAsF32(inst.destination, high, true);
            return applyF32ResultModifiers(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value(), &acc.Value()})));
        }
        if (inst.sourceCount == 3u) {
            const IrF32 third = readF16LaneAsF32(sourceAt(inst, 2u), high, true);
            return applyF32ResultModifiers(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value(), &third.Value()})));
        }
        return applyF32ResultModifiers(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value()})));
    };
    const RdnaOperand raw = plainOperand(inst.destination);
    IrU32 result = packHalf2x16(translateLane(false), translateLane(true));
    if (quietSnan) {
        const auto quietSnanLane = [&](const RdnaOperand& operand, bool high) {
            const IrU32 bits = readU16LaneAsU32(operand, high, false);
            const IrU32 exponent(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7c00u)));
            const IrU32 payload(ir.BitwiseAnd(bits.Value(), ir.Constant(0x01ffu)));
            const IrU1 snan(ir.LogicalAnd(ir.IEqual(exponent.Value(), ir.Constant(0x7c00u)), ir.INotEqual(payload.Value(), ir.Constant(0u))));
            const IrU32 quiet(ir.BitwiseOr(bits.Value(), ir.Constant(0x0200u)));
            return std::pair<IrU1, IrU32>(snan, quiet);
        };
        const auto overrideLane = [&](bool high) {
            const auto [lhsSnan, lhsQuiet] = quietSnanLane(sourceAt(inst, 0u), high);
            const auto [rhsSnan, rhsQuiet] = quietSnanLane(sourceAt(inst, 1u), high);
            const IrU32 normal(high ? ir.ShiftRightLogical(result.Value(), ir.Constant(16u)) : ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu)));
            return IrU32(ir.Select(lhsSnan.Value(), lhsQuiet.Value(), ir.Select(rhsSnan.Value(), rhsQuiet.Value(), normal.Value())));
        };
        result = packU16Lanes(overrideLane(false), overrideLane(true));
    }
    writeOperand(raw, &result.Value());
    return true;
}

bool TranslationContext::float16Unary(const RdnaInstruction& inst, IrOpcode opcode, bool invalidNegative) {
    const IrF32 argument = readF16AsF32(sourceAt(inst, 0u));
    const IrF32 result(ir.Emit(opcode, IrType::F32, {&argument.Value()}));
    if (!invalidNegative) {
        writeF16(inst.destination, result);
        return true;
    }
    const IrU1 negative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&argument.Value(), &ir.ConstantF32(0.0f)}));
    const IrF32 modified = applyF32ResultModifiers(inst.destination, result);
    const IrU32 bits = packHalf2x16(modified, IrF32(ir.ConstantF32(0.0f)));
    const IrU32 invalid(ir.Constant(inst.destination.clamp ? 0u : 0xfe00u));
    write16Bits(inst.destination, IrU32(ir.Select(negative.Value(), invalid.Value(), bits.Value())));
    return true;
}

bool TranslationContext::float16Trig(const RdnaInstruction& inst, IrOpcode opcode) {
    const IrF32 argument = readF16AsF32(sourceAt(inst, 0u));
    const IrU32 magnitude(ir.BitwiseAnd(ir.BitCastU32(argument.Value()), ir.Constant(0x7fffffffu)));
    const IrF32 raw(ir.Emit(opcode, IrType::F32, {&argument.Value()}));
    const IrF32 fraction(ir.Emit(IrOpcode::FPFract32, IrType::F32, {&argument.Value()}));
    const IrF32 zero(ir.ConstantF32(0.0f));
    IrF32 result = raw;
    if (opcode == IrOpcode::FPSin) {
        const IrU1 whole(ir.Emit(IrOpcode::FPOrdEqual32, IrType::U1, {&fraction.Value(), &zero.Value()}));
        const IrU1 half(ir.Emit(IrOpcode::FPOrdEqual32, IrType::U1, {&fraction.Value(), &ir.ConstantF32(0.5f)}));
        const IrU1 nonzero(ir.INotEqual(magnitude.Value(), ir.Constant(0u)));
        const IrU1 cardinal(ir.LogicalOr(ir.LogicalAnd(whole.Value(), nonzero.Value()), half.Value()));
        result = selectF32(cardinal, zero, raw);
    } else {
        const IrU1 quarter(ir.Emit(IrOpcode::FPOrdEqual32, IrType::U1, {&fraction.Value(), &ir.ConstantF32(0.25f)}));
        const IrU1 threeQuarters(ir.Emit(IrOpcode::FPOrdEqual32, IrType::U1, {&fraction.Value(), &ir.ConstantF32(0.75f)}));
        const IrU1 quarterCycle(ir.LogicalOr(quarter.Value(), threeQuarters.Value()));
        result = selectF32(quarterCycle, zero, raw);
    }
    const IrU1 infinite(ir.IEqual(magnitude.Value(), ir.Constant(0x7f800000u)));
    const IrF32 modified = applyF32ResultModifiers(inst.destination, result);
    const IrU32 bits = packHalf2x16(modified, zero);
    const IrU32 invalid(ir.Constant(inst.destination.clamp ? 0u : 0xfe00u));
    write16Bits(inst.destination, IrU32(ir.Select(infinite.Value(), invalid.Value(), bits.Value())));
    return true;
}

bool TranslationContext::float16Binary(const RdnaInstruction& inst, IrOpcode opcode, bool reverse) {
    const IrF32 lhs = readF16AsF32(sourceAt(inst, reverse ? 1u : 0u));
    const IrF32 rhs = readF16AsF32(sourceAt(inst, reverse ? 0u : 1u));
    writeF16(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value()})));
    return true;
}

bool TranslationContext::float16Ternary(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool mix) {
    std::array<IrValue*, 3> args{};
    for (std::uint32_t index = 0u; index < args.size(); ++index) {
        const RdnaOperand& operand = accumulator && index == 2u ? accumulatorOperand(inst) : sourceAt(inst, index);
        args[index] = mix ? &readMixF32(operand).Value() : &readF16AsF32(operand).Value();
    }
    writeF16(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {args[0], args[1], args[2]})));
    return true;
}

bool TranslationContext::floatUnary(const RdnaInstruction& inst, IrOpcode opcode) {
    IrValue* argument = readOperand(sourceAt(inst, 0u), IrOpcodeArgumentType(opcode, 0u));
    IrValue& result = ir.Emit(opcode, IrOpcodeType(opcode), {argument});
    writeOperand(inst.destination, &result);
    return true;
}

bool TranslationContext::floatBinary(const RdnaInstruction& inst, IrOpcode opcode, bool reverse) {
    std::array<IrValue*, 2> args{};
    for (std::uint32_t index = 0u; index < args.size(); ++index) {
        const RdnaOperand& operand = sourceAt(inst, reverse ? 1u - index : index);
        args[index] = readOperand(operand, IrOpcodeArgumentType(opcode, index));
    }
    IrValue& result = ir.Emit(opcode, IrOpcodeType(opcode), {args[0], args[1]});
    writeOperand(inst.destination, &result);
    return true;
}

bool TranslationContext::floatTernary(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool mix) {
    std::array<IrValue*, 3> args{};
    for (std::uint32_t index = 0u; index < args.size(); ++index) {
        const RdnaOperand& operand = accumulator && index == 2u ? accumulatorOperand(inst) : sourceAt(inst, index);
        const IrType type = IrOpcodeArgumentType(opcode, index);
        args[index] = type == IrType::F32 && mix ? &readMixF32(operand).Value() : readOperand(operand, type);
    }
    IrValue& result = ir.Emit(opcode, IrOpcodeType(opcode), {args[0], args[1], args[2]});
    writeOperand(inst.destination, &result);
    return true;
}

bool TranslationContext::vFrexpMantF32(const RdnaInstruction& inst) {
    const IrU32 bits = readU32(sourceAt(inst, 0u));
    const IrU32 exponent(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&bits.Value(), &ir.Constant(23u), &ir.Constant(8u)}));
    const IrU32 mantissa(ir.BitwiseAnd(bits.Value(), ir.Constant(0x007fffffu)));
    const IrU32 sign(ir.BitwiseAnd(bits.Value(), ir.Constant(0x80000000u)));
    const IrU32 base(ir.BitwiseOr(sign.Value(), ir.Constant(0x3f000000u)));
    const IrU32 normal(ir.BitwiseOr(base.Value(), mantissa.Value()));
    const IrU32 msb(ir.Emit(IrOpcode::FindUMsb32, IrType::U32, {&mantissa.Value()}));
    const IrU32 shift(ir.ISub(ir.Constant(23u), msb.Value()));
    const IrU32 fraction(ir.BitwiseAnd(ir.ShiftLeftLogical(mantissa.Value(), shift.Value()), ir.Constant(0x007fffffu)));
    const IrU32 subnormal(ir.BitwiseOr(base.Value(), fraction.Value()));
    const IrU1 zero(ir.IEqual(mantissa.Value(), ir.Constant(0u)));
    const IrU1 exponentNonZero(ir.INotEqual(exponent.Value(), ir.Constant(0u)));
    const IrU32 zeroOrSubnormal(ir.Select(zero.Value(), bits.Value(), subnormal.Value()));
    const IrU32 finite(ir.Select(exponentNonZero.Value(), normal.Value(), zeroOrSubnormal.Value()));
    const IrU1 exponentAllOnes(ir.IEqual(exponent.Value(), ir.Constant(0xffu)));
    const IrU32 result(ir.Select(exponentAllOnes.Value(), bits.Value(), finite.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
    return true;
}

bool TranslationContext::vDot2cF32F16(const RdnaInstruction& inst) {
    RdnaOperand a = sourceAt(inst, 0u);
    a.opSel = false;
    a.opSelHi = true;
    RdnaOperand b = sourceAt(inst, 1u);
    b.opSel = false;
    b.opSelHi = true;
    const IrF32 aLow = readF16LaneAsF32(a, false);
    const IrF32 aHigh = readF16LaneAsF32(a, true);
    const IrF32 bLow = readF16LaneAsF32(b, false);
    const IrF32 bHigh = readF16LaneAsF32(b, true);
    const IrF32 accumulator = readMixF32(inst.destination);
    const IrF32 low(ir.Emit(IrOpcode::FPFma32, IrType::F32, {&aLow.Value(), &bLow.Value(), &accumulator.Value()}));
    const IrF32 result(ir.Emit(IrOpcode::FPFma32, IrType::F32, {&aHigh.Value(), &bHigh.Value(), &low.Value()}));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vCubeidF32(const RdnaInstruction& inst) {
    return floatCube(inst, 0u);
}

bool TranslationContext::vCubescF32(const RdnaInstruction& inst) {
    return floatCube(inst, 1u);
}

bool TranslationContext::vCubetcF32(const RdnaInstruction& inst) {
    return floatCube(inst, 2u);
}

bool TranslationContext::vCubemaF32(const RdnaInstruction& inst) {
    return floatCube(inst, 3u);
}

bool TranslationContext::floatCube(const RdnaInstruction& inst, std::uint32_t resultKind) {
    const IrF32 x = readMixF32(sourceAt(inst, 0u));
    const IrF32 y = readMixF32(sourceAt(inst, 1u));
    const IrF32 z = readMixF32(sourceAt(inst, 2u));
    const IrF32 nx(ir.Emit(IrOpcode::FPNeg32, IrType::F32, {&x.Value()}));
    const IrF32 ny(ir.Emit(IrOpcode::FPNeg32, IrType::F32, {&y.Value()}));
    const IrF32 nz(ir.Emit(IrOpcode::FPNeg32, IrType::F32, {&z.Value()}));
    const IrF32 ax(ir.Emit(IrOpcode::FPAbs32, IrType::F32, {&x.Value()}));
    const IrF32 ay(ir.Emit(IrOpcode::FPAbs32, IrType::F32, {&y.Value()}));
    const IrF32 az(ir.Emit(IrOpcode::FPAbs32, IrType::F32, {&z.Value()}));
    const IrU1 zDominatesX(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&az.Value(), &ax.Value()}));
    const IrU1 zDominatesY(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&az.Value(), &ay.Value()}));
    const IrU1 zFace(ir.LogicalAnd(zDominatesX.Value(), zDominatesY.Value()));
    const IrU1 yFace(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&ay.Value(), &ax.Value()}));
    const IrU1 xNegative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&x.Value(), &ir.ConstantF32(0.0f)}));
    const IrU1 yNegative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&y.Value(), &ir.ConstantF32(0.0f)}));
    const IrU1 zNegative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&z.Value(), &ir.ConstantF32(0.0f)}));
    const auto selectFace = [&](IrF32 xValue, IrF32 yValue, IrF32 zValue) {
        return selectF32(zFace, zValue, selectF32(yFace, yValue, xValue));
    };
    IrF32 result(ir.ConstantF32(0.0f));
    switch (resultKind) {
        case 0u: {
            const IrF32 xResult = selectF32(xNegative, IrF32(ir.ConstantF32(1.0f)), IrF32(ir.ConstantF32(0.0f)));
            const IrF32 yResult = selectF32(yNegative, IrF32(ir.ConstantF32(3.0f)), IrF32(ir.ConstantF32(2.0f)));
            const IrF32 zResult = selectF32(zNegative, IrF32(ir.ConstantF32(5.0f)), IrF32(ir.ConstantF32(4.0f)));
            result = selectFace(xResult, yResult, zResult);
            break;
        }
        case 1u: {
            const IrF32 xResult = selectF32(xNegative, z, nz);
            const IrF32 zResult = selectF32(zNegative, nx, x);
            result = selectFace(xResult, x, zResult);
            break;
        }
        case 2u:
            result = selectFace(ny, selectF32(yNegative, nz, z), ny);
            break;
        case 3u: {
            const IrF32 two(ir.ConstantF32(2.0f));
            const IrF32 xTwo(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&x.Value(), &two.Value()}));
            const IrF32 yTwo(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&y.Value(), &two.Value()}));
            const IrF32 zTwo(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&z.Value(), &two.Value()}));
            result = selectFace(xTwo, yTwo, zTwo);
            break;
        }
        default:
            throw std::runtime_error("invalid cube result kind");
    }
    writeOperand(inst.destination, &result.Value());
    return true;
}

void TranslateFloatInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateFloatInstruction not implemented");
}

}
