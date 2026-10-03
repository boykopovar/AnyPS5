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

IrU32 TranslationContext::readF16Bits(const RdnaOperand& operand) {
    const IrU32 source = readF16SourceBits(operand);
    IrU32 bits(ir.BitwiseAnd((operand.opSel ? ir.ShiftRightLogical(source.Value(), ir.Constant(16u)) : source.Value()), ir.Constant(0xffffu)));
    if (operand.absolute) bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)));
    if (operand.negate) bits = IrU32(ir.BitwiseXor(bits.Value(), ir.Constant(0x8000u)));
    return bits;
}

IrU32 TranslationContext::normF16(IrU32 bits, bool signedValue) {
    const IrU32 magnitude(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)));
    const IrU1 negative(ir.INotEqual(ir.BitwiseAnd(bits.Value(), ir.Constant(0x8000u)), ir.Constant(0u)));
    const IrU1 nan(ir.UGreaterThan(magnitude.Value(), ir.Constant(0x7c00u)));
    const IrU1 saturated(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&magnitude.Value(), &ir.Constant(0x3c00u)}));
    const std::uint32_t scale = signedValue ? 32767u : 65535u;
    const IrU32 exponent(ir.ShiftRightLogical(magnitude.Value(), ir.Constant(10u)));
    const IrU1 normal(ir.INotEqual(exponent.Value(), ir.Constant(0u)));
    const IrU32 mantissa(ir.BitwiseOr(ir.BitwiseAnd(magnitude.Value(), ir.Constant(0x3ffu)), ir.Select(normal.Value(), ir.Constant(0x400u), ir.Constant(0u))));
    const IrU32 shift(ir.ISub(ir.Constant(25u), ir.Select(normal.Value(), exponent.Value(), ir.Constant(1u))));
    const IrU32 product(ir.IMul(mantissa.Value(), ir.Constant(scale)));
    const IrU32 truncated(ir.ShiftRightLogical(product.Value(), shift.Value()));
    const IrU32 remainder(ir.BitwiseAnd(product.Value(), ir.ISub(ir.ShiftLeftLogical(ir.Constant(1u), shift.Value()), ir.Constant(1u))));
    const IrU32 half(ir.ShiftLeftLogical(ir.Constant(1u), ir.ISub(shift.Value(), ir.Constant(1u))));
    const IrU1 roundUp(ir.LogicalOr(ir.UGreaterThan(remainder.Value(), half.Value()),
        ir.LogicalAnd(ir.IEqual(remainder.Value(), half.Value()), ir.INotEqual(ir.BitwiseAnd(truncated.Value(), ir.Constant(1u)), ir.Constant(0u)))));
    IrU32 value(ir.Select(saturated.Value(), ir.Constant(scale), ir.IAdd(truncated.Value(), ir.Select(roundUp.Value(), ir.Constant(1u), ir.Constant(0u)))));
    if (signedValue) {
        value = IrU32(ir.Select(negative.Value(), ir.ISub(ir.Constant(0u), value.Value()), value.Value()));
    } else {
        value = IrU32(ir.Select(negative.Value(), ir.Constant(0u), value.Value()));
    }
    return IrU32(ir.BitwiseAnd(ir.Select(nan.Value(), ir.Constant(0u), value.Value()), ir.Constant(0xffffu)));
}

bool TranslationContext::vLdexpF16(const RdnaInstruction& inst) {
    const IrF32 value = readF16AsF32(sourceAt(inst, 0u));
    const IrU32 exponent(ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&readU32(sourceAt(inst, 1u)).Value(), &ir.Constant(0u), &ir.Constant(16u)}));
    const IrU32 clamped(ir.Emit(IrOpcode::SMax32, IrType::U32, {&ir.Emit(IrOpcode::SMin32, IrType::U32, {&exponent.Value(), &ir.Constant(64u)}), &ir.Constant(static_cast<std::uint32_t>(-64))}));
    IrValue& power = ir.BitCastF32(ir.ShiftLeftLogical(ir.IAdd(clamped.Value(), ir.Constant(127u)), ir.Constant(23u)));
    writeF16(inst.destination, IrF32(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&value.Value(), &power})));
    return true;
}

bool TranslationContext::vFrexpF16(const RdnaInstruction& inst, bool exponent) {
    const IrU32 bits = readF16Bits(sourceAt(inst, 0u));
    const IrF32 value = readF16AsF32(sourceAt(inst, 0u));
    const IrU32 word(ir.BitCastU32(value.Value()));
    const IrU1 special(ir.LogicalOr(ir.IEqual(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), ir.Constant(0u)),
        ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), &ir.Constant(0x7c00u)})));
    if (exponent) {
        const IrU32 unbiased(ir.ISub(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&word.Value(), &ir.Constant(23u), &ir.Constant(8u)}), ir.Constant(126u)));
        write16Bits(inst.destination, IrU32(ir.BitwiseAnd(ir.Select(special.Value(), ir.Constant(0u), unbiased.Value()), ir.Constant(0xffffu))));
        return true;
    }
    const IrU32 mantissa(ir.BitwiseOr(ir.BitwiseAnd(word.Value(), ir.Constant(0x807fffffu)), ir.Constant(126u << 23u)));
    const IrF16 half(ir.Emit(IrOpcode::ConvertF16F32, IrType::F16, {&ir.BitCastF32(mantissa.Value())}));
    const IrU32 converted(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {&ir.Emit(IrOpcode::BitCastU16F16, IrType::U16, {&half.Value()})}));
    const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), ir.Constant(0x7c00u)));
    const IrU32 kept(ir.Select(nan.Value(), ir.BitwiseOr(bits.Value(), ir.Constant(0x200u)), bits.Value()));
    write16Bits(inst.destination, IrU32(ir.Select(special.Value(), kept.Value(), converted.Value())));
    return true;
}

bool TranslationContext::vCvtNormF16(const RdnaInstruction& inst, bool signedValue) {
    write16Bits(inst.destination, normF16(readF16Bits(sourceAt(inst, 0u)), signedValue));
    return true;
}

bool TranslationContext::vCvtPknormF16(const RdnaInstruction& inst, bool signedValue) {
    const IrU32 low = normF16(readF16Bits(sourceAt(inst, 0u)), signedValue);
    const IrU32 high = normF16(readF16Bits(sourceAt(inst, 1u)), signedValue);
    const IrU32 result(ir.BitwiseOr(low.Value(), ir.ShiftLeftLogical(high.Value(), ir.Constant(16u))));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vSatPkU8I16(const RdnaInstruction& inst) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const auto saturate = [&](std::uint32_t offset) {
        IrValue& value = ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&source.Value(), &ir.Constant(offset), &ir.Constant(16u)});
        return IrU32(ir.Emit(IrOpcode::SMin32, IrType::U32, {&ir.Emit(IrOpcode::SMax32, IrType::U32, {&value, &ir.Constant(0u)}), &ir.Constant(255u)}));
    };
    const IrU32 result(ir.BitwiseOr(saturate(0u).Value(), ir.ShiftLeftLogical(saturate(16u).Value(), ir.Constant(8u))));
    writeOperand(inst.destination, &result.Value());
    return true;
}

bool TranslationContext::vMulLegacyF32(const RdnaInstruction& inst, bool accumulate) {
    IrValue* lhs = readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* rhs = readOperand(sourceAt(inst, 1u), IrType::F32);
    const auto isZero = [&](IrValue* value) { return IrU1(ir.IEqual(ir.BitwiseAnd(ir.BitCastU32(*value), ir.Constant(0x7fffffffu)), ir.Constant(0u))); };
    const IrU1 zero(ir.LogicalOr(isZero(lhs).Value(), isZero(rhs).Value()));
    IrValue* result = &ir.Emit(IrOpcode::SelectF32, IrType::F32, {&zero.Value(), &ir.ConstantF32(0.0f), &ir.Emit(IrOpcode::FPMul32, IrType::F32, {lhs, rhs})});
    if (accumulate) {
        IrValue* addend = readOperand(accumulatorOperand(inst), IrType::F32);
        result = &ir.Emit(IrOpcode::FPAdd32, IrType::F32, {result, addend});
    }
    writeOperand(inst.destination, result);
    return true;
}

void TranslationContext::emitFloat16ClassCompare(const RdnaInstruction& inst, bool cmpx) {
    const IrU32 bits = readF16Bits(sourceAt(inst, 0u));
    const IrU32 mask = readU32(sourceAt(inst, 1u));
    const IrU32 magnitude(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)));
    const IrU1 negative(ir.INotEqual(ir.BitwiseAnd(bits.Value(), ir.Constant(0x8000u)), ir.Constant(0u)));
    const auto pick = [&](std::uint32_t negativeClass, std::uint32_t positiveClass) {
        return IrU32(ir.Select(negative.Value(), ir.Constant(negativeClass), ir.Constant(positiveClass)));
    };
    IrU32 kind = pick(3u, 8u);
    kind = IrU32(ir.Select(ir.ULessThan(magnitude.Value(), ir.Constant(0x400u)), pick(4u, 7u).Value(), kind.Value()));
    kind = IrU32(ir.Select(ir.IEqual(magnitude.Value(), ir.Constant(0u)), pick(5u, 6u).Value(), kind.Value()));
    kind = IrU32(ir.Select(ir.IEqual(magnitude.Value(), ir.Constant(0x7c00u)), pick(2u, 9u).Value(), kind.Value()));
    const IrU1 nan(ir.UGreaterThan(magnitude.Value(), ir.Constant(0x7c00u)));
    const IrU32 nanKind(ir.Select(ir.INotEqual(ir.BitwiseAnd(magnitude.Value(), ir.Constant(0x200u)), ir.Constant(0u)), ir.Constant(1u), ir.Constant(0u)));
    kind = IrU32(ir.Select(nan.Value(), nanKind.Value(), kind.Value()));
    const IrU1 hit(ir.INotEqual(ir.BitwiseAnd(ir.ShiftRightLogical(mask.Value(), kind.Value()), ir.Constant(1u)), ir.Constant(0u)));
    emitCompareResult(inst, hit, false, cmpx);
}

bool TranslationContext::vDivScaleF32(const RdnaInstruction& inst) {
    const IrF32 value(*readOperand(sourceAt(inst, 0u), IrType::F32));
    const IrU32 bits(ir.BitCastU32(value.Value()));
    const IrU32 denominator(ir.BitCastU32(*readOperand(sourceAt(inst, 1u), IrType::F32)));
    const IrU32 numerator(ir.BitCastU32(*readOperand(sourceAt(inst, 2u), IrType::F32)));
    const auto exponent = [&](IrU32 word) {
        return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&word.Value(), &ir.Constant(23u), &ir.Constant(8u)}));
    };
    const IrU32 denominatorExponent = exponent(denominator);
    const IrU32 numeratorExponent = exponent(numerator);
    const IrU32 difference(ir.ISub(numeratorExponent.Value(), denominatorExponent.Value()));
    const IrU1 nearMax(ir.Emit(IrOpcode::SGreaterThanEqual32, IrType::U1, {&difference.Value(), &ir.Constant(96u)}));
    const IrU1 nearMin(ir.Emit(IrOpcode::SLessThanEqual32, IrType::U1, {&difference.Value(), &ir.Constant(static_cast<std::uint32_t>(-96))}));
    const IrU1 denominatorDenormal(ir.IEqual(denominatorExponent.Value(), ir.Constant(0u)));
    const IrU1 denominatorHuge(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&denominatorExponent.Value(), &ir.Constant(253u)}));
    const IrU1 numeratorTiny(ir.Emit(IrOpcode::ULessThanEqual32, IrType::U1, {&numeratorExponent.Value(), &ir.Constant(24u)}));
    const IrU1 isDenominator(ir.IEqual(bits.Value(), denominator.Value()));
    const IrU1 notNearMax(ir.LogicalNot(nearMax.Value()));
    const IrU1 notDenormal(ir.LogicalNot(denominatorDenormal.Value()));
    const IrU1 normalRange(ir.LogicalAnd(notNearMax.Value(), notDenormal.Value()));
    const IrU1 belowHuge(ir.LogicalAnd(normalRange.Value(), ir.LogicalNot(denominatorHuge.Value())));
    const IrU1 scaleUp(ir.LogicalOr(
        ir.LogicalOr(ir.LogicalAnd(nearMax.Value(), isDenominator.Value()), ir.LogicalAnd(notNearMax.Value(), denominatorDenormal.Value())),
        ir.LogicalAnd(belowHuge.Value(), ir.LogicalOr(ir.LogicalAnd(nearMin.Value(), ir.LogicalNot(isDenominator.Value())),
            ir.LogicalAnd(ir.LogicalNot(nearMin.Value()), numeratorTiny.Value())))));
    const IrU1 scaleDown(ir.LogicalAnd(ir.LogicalAnd(normalRange.Value(), denominatorHuge.Value()),
        ir.LogicalOr(ir.LogicalNot(nearMin.Value()), isDenominator.Value())));
    const IrF32 up(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&value.Value(), &ir.ConstantF32(18446744073709551616.0f)}));
    const IrF32 down(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&value.Value(), &ir.ConstantF32(5.42101086242752217e-20f)}));
    const IrU32 scaled(ir.Select(scaleUp.Value(), ir.BitCastU32(up.Value()), ir.Select(scaleDown.Value(), ir.BitCastU32(down.Value()), bits.Value())));
    const auto isZero = [&](IrU32 word) { return IrU1(ir.IEqual(ir.BitwiseAnd(word.Value(), ir.Constant(0x7fffffffu)), ir.Constant(0u))); };
    const IrU1 zero(ir.LogicalOr(isZero(denominator).Value(), isZero(numerator).Value()));
    const IrU32 result(ir.Select(zero.Value(), ir.Constant(0xffc00000u), scaled.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
    writeMask(inst.destination2, IrU1(ir.LogicalOr(nearMax.Value(), ir.LogicalAnd(notDenormal.Value(), nearMin.Value()))));
    return true;
}

bool TranslationContext::vDivFmasF32(const RdnaInstruction& inst) {
    RdnaOperand vcc{};
    vcc.kind = RdnaOperandKind::VccLo;
    const IrU1 scale = readMask(vcc);
    IrValue* lhs = readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* rhs = readOperand(sourceAt(inst, 1u), IrType::F32);
    IrValue* addend = readOperand(sourceAt(inst, 2u), IrType::F32);
    const auto exponent = [&](IrValue& value) {
        return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&ir.BitCastU32(value), &ir.Constant(23u), &ir.Constant(8u)}));
    };
    const IrU1 up(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&exponent(*addend).Value(), &ir.Constant(128u)}));
    IrValue& power = ir.Emit(IrOpcode::SelectF32, IrType::F32, {&up.Value(), &ir.ConstantF32(18446744073709551616.0f), &ir.ConstantF32(5.42101086242752217e-20f)});
    IrValue& plain = ir.Emit(IrOpcode::FPFma32, IrType::F32, {lhs, rhs, addend});
    const IrU32 plainExponent = exponent(plain);
    const IrU1 plainInfinite(ir.IEqual(ir.BitwiseAnd(ir.BitCastU32(plain), ir.Constant(0x7fffffffu)), ir.Constant(0x7f800000u)));
    const IrU1 rescale(ir.LogicalOr(ir.LogicalAnd(up.Value(), ir.IEqual(plainExponent.Value(), ir.Constant(0u))), ir.LogicalAnd(ir.LogicalNot(up.Value()), plainInfinite.Value())));
    IrValue& lhsMagnitude = ir.BitwiseAnd(ir.BitCastU32(*lhs), ir.Constant(0x7fffffffu));
    IrValue& rhsMagnitude = ir.BitwiseAnd(ir.BitCastU32(*rhs), ir.Constant(0x7fffffffu));
    const IrU1 lhsLarger(ir.UGreaterThan(lhsMagnitude, rhsMagnitude));
    const IrU1 scaleLhs(ir.LogicalOr(ir.LogicalAnd(up.Value(), ir.LogicalNot(lhsLarger.Value())), ir.LogicalAnd(ir.LogicalNot(up.Value()), lhsLarger.Value())));
    IrValue& scaled = ir.Emit(IrOpcode::SelectF32, IrType::F32, {&scaleLhs.Value(), lhs, rhs});
    IrValue& other = ir.Emit(IrOpcode::SelectF32, IrType::F32, {&scaleLhs.Value(), rhs, lhs});
    IrValue& rescaled = ir.Emit(IrOpcode::FPFma32, IrType::F32, {&ir.Emit(IrOpcode::FPMul32, IrType::F32, {&scaled, &power}), &other,
        &ir.Emit(IrOpcode::FPMul32, IrType::F32, {addend, &power})});
    IrValue& afterwards = ir.Emit(IrOpcode::FPMul32, IrType::F32, {&plain, &power});
    IrValue& scaledResult = ir.Emit(IrOpcode::SelectF32, IrType::F32, {&rescale.Value(), &rescaled, &afterwards});
    writeOperand(inst.destination, &ir.Emit(IrOpcode::SelectF32, IrType::F32, {&scale.Value(), &scaledResult, &plain}));
    return true;
}

bool TranslationContext::vDivFixupF32(const RdnaInstruction& inst) {
    const IrU32 quotient(ir.BitCastU32(*readOperand(sourceAt(inst, 0u), IrType::F32)));
    const IrU32 denominator(ir.BitCastU32(*readOperand(sourceAt(inst, 1u), IrType::F32)));
    const IrU32 numerator(ir.BitCastU32(*readOperand(sourceAt(inst, 2u), IrType::F32)));
    const auto magnitude = [&](IrU32 word) { return IrU32(ir.BitwiseAnd(word.Value(), ir.Constant(0x7fffffffu))); };
    const auto isNan = [&](IrU32 word) { return IrU1(ir.UGreaterThan(magnitude(word).Value(), ir.Constant(0x7f800000u))); };
    const auto isInf = [&](IrU32 word) { return IrU1(ir.IEqual(magnitude(word).Value(), ir.Constant(0x7f800000u))); };
    const auto isZero = [&](IrU32 word) { return IrU1(ir.IEqual(magnitude(word).Value(), ir.Constant(0u))); };
    const auto exponent = [&](IrU32 word) {
        return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&word.Value(), &ir.Constant(23u), &ir.Constant(8u)}));
    };
    const IrU32 sign(ir.BitwiseAnd(ir.BitwiseXor(denominator.Value(), numerator.Value()), ir.Constant(0x80000000u)));
    const IrU32 infinity(ir.BitwiseOr(sign.Value(), ir.Constant(0x7f800000u)));
    const IrU32 difference(ir.ISub(exponent(numerator).Value(), exponent(denominator).Value()));
    const IrU1 underflow(ir.Emit(IrOpcode::SLessThan32, IrType::U1, {&difference.Value(), &ir.Constant(static_cast<std::uint32_t>(-150))}));
    IrU32 result(ir.BitwiseOr(sign.Value(), magnitude(quotient).Value()));
    result = IrU32(ir.Select(isNan(quotient).Value(), infinity.Value(), result.Value()));
    result = IrU32(ir.Select(underflow.Value(), sign.Value(), result.Value()));
    result = IrU32(ir.Select(ir.LogicalOr(isInf(denominator).Value(), isZero(numerator).Value()), sign.Value(), result.Value()));
    result = IrU32(ir.Select(ir.LogicalOr(isZero(denominator).Value(), isInf(numerator).Value()), infinity.Value(), result.Value()));
    const IrU1 invalid(ir.LogicalOr(ir.LogicalAnd(isZero(denominator).Value(), isZero(numerator).Value()), ir.LogicalAnd(isInf(denominator).Value(), isInf(numerator).Value())));
    result = IrU32(ir.Select(invalid.Value(), ir.Constant(0xffc00000u), result.Value()));
    result = IrU32(ir.Select(isNan(denominator).Value(), ir.BitwiseOr(denominator.Value(), ir.Constant(0x400000u)), result.Value()));
    result = IrU32(ir.Select(isNan(numerator).Value(), ir.BitwiseOr(numerator.Value(), ir.Constant(0x400000u)), result.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
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
