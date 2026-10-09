#include "Translation/FloatInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace ShaderRecompiler {

void TranslateFloatInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateFloatInstruction not implemented");
}

bool TranslationContext::packedFloat16(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool quietSnan) {
    if (floatMode.has_value() && (floatMode->floatMode & 0xccu) != 0xc0u) {
        throw std::runtime_error("packed f16 at pc " + std::to_string(inst.programCounter) + " in FLOAT_MODE " + std::to_string(floatMode->floatMode) + " (f16 denormals not kept or rounding not to nearest even) is not implemented");
    }
    const auto translateLane = [&](bool high) -> IrF32 {
        const IrF32 lhs = readF16LaneAsF32(sourceAt(inst, 0u), high, true);
        const IrF32 rhs = readF16LaneAsF32(sourceAt(inst, 1u), high, true);
        if (accumulator) {
            const IrF32 acc = readF16LaneAsF32(accumulatorOperand(inst), high, true);
            return applyF32ResultModifiers(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value(), &acc.Value()})));
        }
        if (inst.sourceCount == 3u) {
            const IrF32 third = readF16LaneAsF32(sourceAt(inst, 2u), high, true);
            return applyF32ResultModifiers(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value(), &third.Value()})));
        }
        return applyF32ResultModifiers(inst.destination, IrF32(ir.Emit(opcode, IrType::F32, {&lhs.Value(), &rhs.Value()})));
    };
    IrU32 result = packHalf2x16(translateLane(false), translateLane(true));
    if (!inst.destination.clamp || !dx10Clamp()) {
        const auto laneBits = [&](const RdnaOperand& operand, bool high) {
            const IrU32 source = readF16SourceBits(operand);
            const bool selectHigh = high ? operand.opSelHi : operand.opSel;
            IrU32 bits(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&source.Value(), &ir.Constant(selectHigh ? 16u : 0u), &ir.Constant(16u)}));
            if (operand.absolute) bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)));
            if (high ? operand.negateHi : operand.negate) bits = IrU32(ir.BitwiseXor(bits.Value(), ir.Constant(0x8000u)));
            return bits;
        };
        const auto isNan = [&](const IrU32& bits) {
            return IrU1(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), ir.Constant(0x7c00u)));
        };
        const auto isSnan = [&](const IrU32& bits) {
            return IrU1(ir.LogicalAnd(isNan(bits).Value(), ir.IEqual(ir.BitwiseAnd(bits.Value(), ir.Constant(0x0200u)), ir.Constant(0u))));
        };
        const auto quiet = [&](const IrU32& bits) { return ieeeMode ? IrU32(ir.BitwiseOr(bits.Value(), ir.Constant(0x0200u))) : bits; };
        const auto magnitude = [&](const IrU32& bits) { return IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu))); };
        const auto invalidProduct = [&](const IrU32& lhs, const IrU32& rhs) {
            const auto infZero = [&](const IrU32& inf, const IrU32& zero) {
                return IrU1(ir.LogicalAnd(ir.IEqual(magnitude(inf).Value(), ir.Constant(0x7c00u)), ir.IEqual(magnitude(zero).Value(), ir.Constant(0u))));
            };
            return IrU1(ir.LogicalOr(infZero(lhs, rhs).Value(), infZero(rhs, lhs).Value()));
        };
        const auto nanLane = [&](bool high) {
            const std::uint32_t count = accumulator ? 3u : inst.sourceCount;
            std::array<IrU32, 3> bits{};
            for (std::uint32_t index = 0u; index < count; ++index) {
                bits[index] = laneBits(accumulator && index == 2u ? accumulatorOperand(inst) : sourceAt(inst, index), high);
            }
            IrU32 lane(high ? ir.ShiftRightLogical(result.Value(), ir.Constant(16u)) : ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu)));
            if (quietSnan) {
                lane = IrU32(ir.Select(isNan(bits[0]).Value(), bits[1].Value(), lane.Value()));
                lane = IrU32(ir.Select(isNan(bits[1]).Value(), bits[0].Value(), lane.Value()));
                if (!ieeeMode) return lane;
                for (std::uint32_t index = count; index-- > 0u;) {
                    lane = IrU32(ir.Select(isSnan(bits[index]).Value(), quiet(bits[index]).Value(), lane.Value()));
                }
                return lane;
            }
            lane = IrU32(ir.Select(isNan(lane).Value(), ir.Constant(0xfe00u), lane.Value()));
            for (std::uint32_t index = count; index-- > 0u;) {
                IrU1 selected = isNan(bits[index]);
                if (index == 2u) selected = IrU1(ir.LogicalAnd(selected.Value(), ir.LogicalNot(invalidProduct(bits[0], bits[1]).Value())));
                lane = IrU32(ir.Select(selected.Value(), quiet(bits[index]).Value(), lane.Value()));
            }
            return lane;
        };
        const auto clampedLane = [&](bool high) { return inst.destination.clamp ? clampF16Bits(inst.destination, nanLane(high)) : nanLane(high); };
        result = packU16Lanes(clampedLane(false), clampedLane(true));
    }
    writeRawU32(inst.destination, result);
    return true;
}

bool TranslationContext::float16Unary(const RdnaInstruction& inst, IrOpcode opcode) {
    const RdnaOperand& operand = sourceAt(inst, 0u);
    const IrF32 argument = readF16AsF32(operand);
    const auto [result, invalid] = unaryFloatSpecials(opcode, argument, IrF32(ir.Emit(opcode, IrType::F32, {&argument.Value()})));
    IrU32 bits = packHalf2x16(applyF16ResultModifiers(inst.destination, result), IrF32(ir.ConstantF32(0.0f)));
    if (opcode == IrOpcode::FPFract32) {
        bits = IrU32(ir.Select(ir.IEqual(bits.Value(), ir.Constant(0x3c00u)), ir.Constant(0x3bffu), bits.Value()));
    }
    const IrU32 half = readF16Bits(operand);
    const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(half.Value(), ir.Constant(0x7fffu)), ir.Constant(0x7c00u)));
    const IrU32 special(ir.Select(nan.Value(), quietNan16(half).Value(), ir.Constant(0xfe00u)));
    const IrU32 value(ir.Select(ir.LogicalOr(nan.Value(), invalid.Value()), inst.destination.clamp && dx10Clamp() ? ir.Constant(0u) : special.Value(), bits.Value()));
    write16Bits(inst.destination, value);
    return true;
}

std::pair<IrF32, IrU1> TranslationContext::unaryFloatSpecials(IrOpcode opcode, IrF32 argument, IrF32 result) {
    const IrU32 bits(ir.BitCastU32(argument.Value()));
    const IrU32 magnitude(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)));
    const IrU32 sign(ir.BitwiseAnd(bits.Value(), ir.Constant(0x80000000u)));
    const bool flushes = opcode == IrOpcode::FPRecip32 || opcode == IrOpcode::FPRecipIFlag32 || opcode == IrOpcode::FPRecipSqrt32 || opcode == IrOpcode::FPSqrt || opcode == IrOpcode::FPLog2 || opcode == IrOpcode::FPExp2;
    const IrU1 zero(flushes ? ir.ULessThan(magnitude.Value(), ir.Constant(0x00800000u)) : ir.IEqual(magnitude.Value(), ir.Constant(0u)));
    const IrU1 infinite(ir.IEqual(magnitude.Value(), ir.Constant(0x7f800000u)));
    const IrU1 negative(ir.LogicalAnd(ir.INotEqual(sign.Value(), ir.Constant(0u)), ir.LogicalNot(zero.Value())));
    const IrU32 signedInfinity(ir.BitwiseOr(sign.Value(), ir.Constant(0x7f800000u)));
    const auto pick = [&](IrU1 condition, IrValue& value, IrU32 current) { return IrU32(ir.Select(condition.Value(), value, current.Value())); };
    IrU32 value(ir.BitCastU32(result.Value()));
    IrU1 invalid(ir.ConstantBool(false));
    switch (opcode) {
    case IrOpcode::FPRecip32:
    case IrOpcode::FPRecipIFlag32:
        value = pick(infinite, sign.Value(), value);
        value = pick(zero, signedInfinity.Value(), value);
        break;
    case IrOpcode::FPRecipSqrt32:
        value = pick(infinite, ir.Constant(0u), value);
        value = pick(zero, signedInfinity.Value(), value);
        invalid = negative;
        break;
    case IrOpcode::FPSqrt:
        value = pick(infinite, bits.Value(), value);
        value = pick(zero, sign.Value(), value);
        invalid = negative;
        break;
    case IrOpcode::FPLog2:
        value = pick(infinite, bits.Value(), value);
        value = pick(IrU1(ir.IEqual(bits.Value(), ir.Constant(0x3f800000u))), ir.Constant(0u), value);
        value = pick(zero, ir.Constant(0xff800000u), value);
        invalid = negative;
        break;
    case IrOpcode::FPExp2: {
        const IrU1 overflow(ir.LogicalAnd(ir.IEqual(sign.Value(), ir.Constant(0u)), ir.UGreaterThan(magnitude.Value(), ir.Constant(0x42ffffffu))));
        const IrU1 underflow(ir.LogicalAnd(negative.Value(), ir.UGreaterThan(magnitude.Value(), ir.Constant(0x42fc0000u))));
        value = pick(overflow, ir.Constant(0x7f800000u), value);
        value = pick(underflow, ir.Constant(0u), value);
        value = pick(zero, ir.Constant(0x3f800000u), value);
        break;
    }
    case IrOpcode::FPFract32: {
        const IrU32 unsignedValue(ir.BitwiseAnd(value.Value(), ir.Constant(0x7fffffffu)));
        value = pick(IrU1(ir.UGreaterThan(unsignedValue.Value(), ir.Constant(0x3f7fffffu))), ir.Constant(0x3f7fffffu), unsignedValue);
        invalid = infinite;
        break;
    }
    case IrOpcode::FPSin:
    case IrOpcode::FPCos: {
        const IrF32 whole(ir.Emit(IrOpcode::FPTrunc32, IrType::F32, {&argument.Value()}));
        const IrF32 difference(ir.Emit(IrOpcode::FPSub32, IrType::F32, {&argument.Value(), &whole.Value()}));
        const IrF32 fraction(ir.Emit(IrOpcode::FPAbs32, IrType::F32, {&difference.Value()}));
        const auto fractionIs = [&](float cycle) { return IrU1(ir.Emit(IrOpcode::FPOrdEqual32, IrType::U1, {&fraction.Value(), &ir.ConstantF32(cycle)})); };
        if (opcode == IrOpcode::FPSin) {
            const IrU1 cardinal(ir.LogicalOr(fractionIs(0.0f).Value(), fractionIs(0.5f).Value()));
            value = pick(cardinal, ir.Constant(0u), value);
            value = pick(zero, bits.Value(), value);
        } else {
            value = pick(IrU1(ir.LogicalOr(fractionIs(0.25f).Value(), fractionIs(0.75f).Value())), ir.Constant(0u), value);
        }
        invalid = infinite;
        break;
    }
    default:
        break;
    }
    return {IrF32(ir.BitCastF32(value.Value())), invalid};
}

bool TranslationContext::vDivFixupF16(const RdnaInstruction& inst) {
    std::array<IrU32, 3> bits{};
    std::array<IrU32, 3> magnitude{};
    for (std::uint32_t index = 0u; index < bits.size(); ++index) {
        const RdnaOperand& operand = sourceAt(inst, index);
        const IrU32 source = readF16SourceBits(operand);
        bits[index] = IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&source.Value(), &ir.Constant(operand.opSel ? 16u : 0u), &ir.Constant(16u)}));
        if (operand.absolute) {
            bits[index] = IrU32(ir.BitwiseAnd(bits[index].Value(), ir.Constant(0x7fffu)));
        }
        if (operand.negate) {
            bits[index] = IrU32(ir.BitwiseXor(bits[index].Value(), ir.Constant(0x8000u)));
        }
        magnitude[index] = IrU32(ir.BitwiseAnd(bits[index].Value(), ir.Constant(0x7fffu)));
    }
    const auto isNan = [&](std::uint32_t index) { return IrU1(ir.UGreaterThan(magnitude[index].Value(), ir.Constant(0x7c00u))); };
    const auto isInf = [&](std::uint32_t index) { return IrU1(ir.IEqual(magnitude[index].Value(), ir.Constant(0x7c00u))); };
    const auto isZero = [&](std::uint32_t index) { return IrU1(ir.IEqual(magnitude[index].Value(), ir.Constant(0u))); };
    const IrU32 sign(ir.BitwiseAnd(ir.BitwiseXor(bits[1].Value(), bits[2].Value()), ir.Constant(0x8000u)));
    const IrU32 infinity(ir.BitwiseOr(sign.Value(), ir.Constant(0x7c00u)));
    IrU32 result(ir.Select(isNan(0u).Value(), infinity.Value(), ir.BitwiseOr(sign.Value(), magnitude[0].Value())));
    result = IrU32(ir.Select(ir.LogicalOr(isInf(1u).Value(), isZero(2u).Value()), sign.Value(), result.Value()));
    result = IrU32(ir.Select(ir.LogicalOr(isZero(1u).Value(), isInf(2u).Value()), infinity.Value(), result.Value()));
    const IrU1 bothZero(ir.LogicalAnd(isZero(1u).Value(), isZero(2u).Value()));
    const IrU1 bothInf(ir.LogicalAnd(isInf(1u).Value(), isInf(2u).Value()));
    result = IrU32(ir.Select(ir.LogicalOr(bothZero.Value(), bothInf.Value()), ir.Constant(0xfe00u), result.Value()));
    result = IrU32(ir.Select(isNan(1u).Value(), quietNan16(bits[1]).Value(), result.Value()));
    result = IrU32(ir.Select(isNan(2u).Value(), quietNan16(bits[2]).Value(), result.Value()));
    write16Bits(inst.destination, clampF16Bits(inst.destination, result));
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

IrU32 TranslationContext::normF32(IrU32 bits, bool signedValue) {
    const IrU32 magnitude(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)));
    const IrU1 negative(ir.INotEqual(ir.BitwiseAnd(bits.Value(), ir.Constant(0x80000000u)), ir.Constant(0u)));
    const IrU1 nan(ir.UGreaterThan(magnitude.Value(), ir.Constant(0x7f800000u)));
    const IrU1 saturated(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&magnitude.Value(), &ir.Constant(0x3f800000u)}));
    const std::uint32_t scale = signedValue ? 32767u : 65535u;
    const IrU32 exponent(ir.ShiftRightLogical(magnitude.Value(), ir.Constant(23u)));
    const IrU32 mantissa(ir.BitwiseOr(ir.BitwiseAnd(magnitude.Value(), ir.Constant(0x7fffffu)), ir.Constant(0x800000u)));
    const IrU32 shift(ir.Emit(IrOpcode::UMin32, IrType::U32, {&ir.ISub(ir.Constant(149u), exponent.Value()), &ir.Constant(63u)}));
    const IrU64 product(ir.Emit(IrOpcode::IMul64, IrType::U64, {&ir.ConstructU64(mantissa.Value(), ir.Constant(0u)), &ir.ConstantU64(scale)}));
    const IrU64 halves(ir.Emit(IrOpcode::ShiftRightLogical64, IrType::U64, {&product.Value(), &shift.Value()}));
    const IrU1 sticky(ir.Emit(IrOpcode::INotEqual64, IrType::U1, {&ir.Emit(IrOpcode::ShiftLeftLogical64, IrType::U64, {&halves.Value(), &shift.Value()}), &product.Value()}));
    const IrU32 doubled = extractU64(halves)[0];
    const IrU32 truncated(ir.ShiftRightLogical(doubled.Value(), ir.Constant(1u)));
    const IrU1 roundUp(ir.LogicalAnd(ir.INotEqual(ir.BitwiseAnd(doubled.Value(), ir.Constant(1u)), ir.Constant(0u)),
        ir.LogicalOr(sticky.Value(), ir.INotEqual(ir.BitwiseAnd(truncated.Value(), ir.Constant(1u)), ir.Constant(0u)))));
    IrU32 value(ir.Select(saturated.Value(), ir.Constant(scale), ir.IAdd(truncated.Value(), ir.Select(roundUp.Value(), ir.Constant(1u), ir.Constant(0u)))));
    if (signedValue) {
        value = IrU32(ir.Select(negative.Value(), ir.ISub(ir.Constant(0u), value.Value()), value.Value()));
    } else {
        value = IrU32(ir.Select(negative.Value(), ir.Constant(0u), value.Value()));
    }
    return IrU32(ir.BitwiseAnd(ir.Select(nan.Value(), ir.Constant(0u), value.Value()), ir.Constant(0xffffu)));
}

bool TranslationContext::vLdexpF16(const RdnaInstruction& inst) {
    const IrU32 bits = readF16Bits(sourceAt(inst, 0u));
    const IrF16 half(ir.Emit(IrOpcode::BitCastF16U16, IrType::F16, {&ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&bits.Value()})}));
    const IrF32 value(ir.Emit(IrOpcode::ConvertF32F16, IrType::F32, {&half.Value()}));
    const IrU32 exponent(ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&readU32(sourceAt(inst, 1u)).Value(), &ir.Constant(0u), &ir.Constant(16u)}));
    const IrU32 clamped(ir.Emit(IrOpcode::SMax32, IrType::U32, {&ir.Emit(IrOpcode::SMin32, IrType::U32, {&exponent.Value(), &ir.Constant(64u)}), &ir.Constant(static_cast<std::uint32_t>(-64))}));
    IrValue& power = ir.BitCastF32(ir.ShiftLeftLogical(ir.IAdd(clamped.Value(), ir.Constant(127u)), ir.Constant(23u)));
    const IrF16 scaled(ir.Emit(IrOpcode::ConvertF16F32, IrType::F16, {&ir.Emit(IrOpcode::FPMul32, IrType::F32, {&value.Value(), &power})}));
    const IrU32 result(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {&ir.Emit(IrOpcode::BitCastU16F16, IrType::U16, {&scaled.Value()})}));
    const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), ir.Constant(0x7c00u)));
    write16Bits(inst.destination, clampF16Bits(inst.destination, IrU32(ir.Select(nan.Value(), quietNan16(bits).Value(), result.Value()))));
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
    const IrU32 kept(ir.Select(nan.Value(), quietNan16(bits).Value(), bits.Value()));
    write16Bits(inst.destination, clampF16Bits(inst.destination, IrU32(ir.Select(special.Value(), kept.Value(), converted.Value()))));
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
    IrValue* result = &ir.Emit(IrOpcode::SelectF32, IrType::F32, {&zero.Value(), &ir.ConstantF32(0.0f), &flushTinyProduct(lhs, rhs, &ir.Emit(IrOpcode::FPMul32, IrType::F32, {lhs, rhs})).Value()});
    if (accumulate) {
        IrValue* addend = readOperand(accumulatorOperand(inst), IrType::F32);
        result = &ir.Emit(IrOpcode::FPAdd32, IrType::F32, {result, addend});
    }
    writeOperand(inst.destination, result);
    return true;
}

bool TranslationContext::vMullitF32(const RdnaInstruction& inst) {
    const IrU32 lhs = readU32(sourceAt(inst, 0u));
    const IrU32 rhs = readU32(sourceAt(inst, 1u));
    const IrU32 limit = readU32(sourceAt(inst, 2u));
    const auto magnitude = [&](const IrU32& bits) { return IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu))); };
    const auto isNan = [&](const IrU32& bits) { return IrU1(ir.UGreaterThan(magnitude(bits).Value(), ir.Constant(0x7f800000u))); };
    const auto isZero = [&](const IrU32& bits) { return IrU1(ir.IEqual(magnitude(bits).Value(), ir.Constant(0u))); };
    const IrU1 limitNotPositive(ir.LogicalOr(isZero(limit).Value(), ir.INotEqual(ir.BitwiseAnd(limit.Value(), ir.Constant(0x80000000u)), ir.Constant(0u))));
    const IrU1 rhsNegativeMax(ir.UGreaterThan(rhs.Value(), ir.Constant(0xff7ffffeu)));
    IrU1 lowest(ir.LogicalOr(rhsNegativeMax.Value(), ir.LogicalOr(isNan(limit).Value(), limitNotPositive.Value())));
    lowest = IrU1(ir.LogicalOr(lowest.Value(), isNan(rhs).Value()));
    const IrU32 product(ir.BitCastU32(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&ir.BitCastF32(lhs.Value()), &ir.BitCastF32(rhs.Value())})));
    IrU32 result(ir.Select(isNan(lhs).Value(), quietNan32(lhs).Value(), product.Value()));
    result = IrU32(ir.Select(ir.LogicalOr(isZero(lhs).Value(), isZero(rhs).Value()), ir.Constant(0u), result.Value()));
    result = IrU32(ir.Select(lowest.Value(), ir.Constant(0xff7fffffu), result.Value()));
    writeOperand(inst.destination, &result.Value());
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
    const IrU1 valueNan(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)), ir.Constant(0x7f800000u)));
    const IrU1 scaleUpNumber(ir.LogicalAnd(scaleUp.Value(), ir.LogicalNot(valueNan.Value())));
    const IrU1 scaleDownNumber(ir.LogicalAnd(scaleDown.Value(), ir.LogicalNot(valueNan.Value())));
    const IrU32 scaled(ir.Select(scaleUpNumber.Value(), ir.BitCastU32(up.Value()), ir.Select(scaleDownNumber.Value(), ir.BitCastU32(down.Value()), bits.Value())));
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
    const auto isNan = [&](IrValue& value) { return IrU1(ir.UGreaterThan(ir.BitwiseAnd(ir.BitCastU32(value), ir.Constant(0x7fffffffu)), ir.Constant(0x7f800000u))); };
    const auto quiet = [&](IrValue& value) { return IrU32(ir.BitwiseOr(ir.BitCastU32(value), ir.Constant(0x400000u))); };
    IrValue& computed = ir.Emit(IrOpcode::SelectF32, IrType::F32, {&scale.Value(), &scaledResult, &plain});
    IrU32 result(ir.Select(isNan(computed).Value(), ir.Constant(0xffc00000u), ir.BitCastU32(computed)));
    result = IrU32(ir.Select(isNan(*addend).Value(), quiet(*addend).Value(), result.Value()));
    result = IrU32(ir.Select(isNan(*rhs).Value(), quiet(*rhs).Value(), result.Value()));
    result = IrU32(ir.Select(isNan(*lhs).Value(), quiet(*lhs).Value(), result.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
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
    result = IrU32(ir.Select(isNan(denominator).Value(), quietNan32(denominator).Value(), result.Value()));
    result = IrU32(ir.Select(isNan(numerator).Value(), quietNan32(numerator).Value(), result.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
    return true;
}

bool TranslationContext::floatUnary(const RdnaInstruction& inst, IrOpcode opcode) {
    const IrU32 bits = flushF32Denormal(readU32(sourceAt(inst, 0u)));
    const IrF32 argument(ir.BitCastF32(bits.Value()));
    const auto [result, invalid] = unaryFloatSpecials(opcode, argument, IrF32(ir.Emit(opcode, IrType::F32, {&argument.Value()})));
    const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)), ir.Constant(0x7f800000u)));
    const IrU32 special(ir.Select(nan.Value(), quietNan32(bits).Value(), ir.Constant(0xffc00000u)));
    const IrF32 value(ir.Select(ir.LogicalOr(nan.Value(), invalid.Value()), ir.BitCastF32(special.Value()), result.Value()));
    writeOperand(inst.destination, &value.Value());
    return true;
}

bool TranslationContext::floatBinary(const RdnaInstruction& inst, IrOpcode opcode, bool reverse) {
    std::array<IrValue*, 2> args{};
    for (std::uint32_t index = 0u; index < args.size(); ++index) {
        const RdnaOperand& operand = sourceAt(inst, reverse ? 1u - index : index);
        args[index] = readOperand(operand, IrOpcodeArgumentType(opcode, index));
    }
    IrValue* result = &ir.Emit(opcode, IrOpcodeType(opcode), {args[0], args[1]});
    if (opcode == IrOpcode::FPMul32) result = &flushTinyProduct(args[0], args[1], result).Value();
    writeOperand(inst.destination, result);
    return true;
}

bool TranslationContext::floatTernary(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool mix) {
    std::array<IrValue*, 3> args{};
    for (std::uint32_t index = 0u; index < args.size(); ++index) {
        const RdnaOperand& operand = accumulator && index == 2u ? accumulatorOperand(inst) : sourceAt(inst, index);
        const IrType type = IrOpcodeArgumentType(opcode, index);
        args[index] = type == IrType::F32 && mix ? &readMixF32(operand).Value() : readOperand(operand, type);
    }
    IrValue* result = &ir.Emit(opcode, IrOpcodeType(opcode), {args[0], args[1], args[2]});
    if (opcode == IrOpcode::FPFma32) result = &flushTinyProduct(args[0], args[1], result, args[2]).Value();
    writeOperand(inst.destination, result);
    return true;
}

bool TranslationContext::ieeeMinMaxF32(const RdnaInstruction& inst, IrOpcode opcode) {
    const bool ternary = opcode == IrOpcode::FPMinTri32 || opcode == IrOpcode::FPMaxTri32 || opcode == IrOpcode::FPMedTri32;
    std::array<IrValue*, 3> args{};
    for (std::uint32_t index = 0u; index < (ternary ? 3u : 2u); ++index) args[index] = readOperand(sourceAt(inst, index), IrType::F32);
    const auto signaling = [&](IrValue* value) {
        IrValue& bits = ir.BitCastU32(*value);
        return &ir.LogicalAnd(ir.IEqual(ir.BitwiseAnd(bits, ir.Constant(0x7fc00000u)), ir.Constant(0x7f800000u)), ir.INotEqual(ir.BitwiseAnd(bits, ir.Constant(0x003fffffu)), ir.Constant(0u)));
    };
    const auto quiet = [&](IrValue* value) { return &ir.BitCastF32(ir.BitwiseOr(ir.BitCastU32(*value), ir.Constant(0x00400000u))); };
    const auto pair = [&](IrOpcode pairOpcode, IrValue* lhs, IrValue* rhs) {
        IrValue& plain = ir.Emit(pairOpcode, IrType::F32, {lhs, rhs});
        IrValue& rhsChecked = ir.Emit(IrOpcode::SelectF32, IrType::F32, {signaling(rhs), quiet(rhs), &plain});
        return &ir.Emit(IrOpcode::SelectF32, IrType::F32, {signaling(lhs), quiet(lhs), &rhsChecked});
    };
    IrValue* result = nullptr;
    if (opcode == IrOpcode::FPMin32 || opcode == IrOpcode::FPMax32) {
        result = pair(opcode, args[0], args[1]);
    } else if (opcode == IrOpcode::FPMinTri32 || opcode == IrOpcode::FPMaxTri32) {
        const auto pairOpcode = opcode == IrOpcode::FPMinTri32 ? IrOpcode::FPMin32 : IrOpcode::FPMax32;
        result = pair(pairOpcode, pair(pairOpcode, args[0], args[1]), args[2]);
    } else {
        const auto isNan = [&](IrValue* value) { return &ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {value}); };
        IrValue& anyNan = ir.LogicalOr(ir.LogicalOr(*isNan(args[0]), *isNan(args[1])), *isNan(args[2]));
        IrValue* minimum = pair(IrOpcode::FPMin32, pair(IrOpcode::FPMin32, args[0], args[1]), args[2]);
        IrValue& median = ir.Emit(IrOpcode::FPMedTri32, IrType::F32, {args[0], args[1], args[2]});
        result = &ir.Emit(IrOpcode::SelectF32, IrType::F32, {&anyNan, minimum, &median});
    }
    writeOperand(inst.destination, result);
    return true;
}

bool TranslationContext::vFmaLegacyF32(const RdnaInstruction& inst) {
    IrValue* lhs = readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* rhs = readOperand(sourceAt(inst, 1u), IrType::F32);
    IrValue* addend = readOperand(sourceAt(inst, 2u), IrType::F32);
    const auto isZero = [&](IrValue* value) { return IrU1(ir.IEqual(ir.BitwiseAnd(ir.BitCastU32(*value), ir.Constant(0x7fffffffu)), ir.Constant(0u))); };
    const IrU1 zero(ir.LogicalOr(isZero(lhs).Value(), isZero(rhs).Value()));
    const auto factor = [&](IrValue* value) { return &ir.Emit(IrOpcode::SelectF32, IrType::F32, {&zero.Value(), &ir.ConstantF32(0.0f), value}); };
    writeOperand(inst.destination, &ir.Emit(IrOpcode::FPFma32, IrType::F32, {factor(lhs), factor(rhs), addend}));
    return true;
}

bool TranslationContext::vFrexpMantF32(const RdnaInstruction& inst) {
    const IrU32 bits = flushF32Denormal(readU32(sourceAt(inst, 0u)));
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
    const IrU32 special(ir.Select(zero.Value(), bits.Value(), quietNan32(bits).Value()));
    const IrU32 result(ir.Select(exponentAllOnes.Value(), special.Value(), finite.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
    return true;
}

bool TranslationContext::vLdexpF32(const RdnaInstruction& inst) {
    const IrU32 bits = readU32(sourceAt(inst, 0u));
    const IrU32 offset = readU32(sourceAt(inst, 1u));
    const IrU32 sign(ir.BitwiseAnd(bits.Value(), ir.Constant(0x80000000u)));
    const IrU32 exponent(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&bits.Value(), &ir.Constant(23u), &ir.Constant(8u)}));
    const IrU32 clamped(ir.Emit(IrOpcode::SMax32, IrType::U32, {&ir.Emit(IrOpcode::SMin32, IrType::U32, {&offset.Value(), &ir.Constant(512u)}), &ir.Constant(static_cast<std::uint32_t>(-512))}));
    const IrU32 scaled(ir.IAdd(exponent.Value(), clamped.Value()));
    const IrU32 normal(ir.BitwiseOr(ir.BitwiseAnd(bits.Value(), ir.Constant(0x807fffffu)), ir.ShiftLeftLogical(scaled.Value(), ir.Constant(23u))));
    const IrU32 overflow(ir.BitwiseOr(sign.Value(), ir.Constant(0x7f800000u)));
    IrU32 result(ir.Select(ir.Emit(IrOpcode::SGreaterThanEqual32, IrType::U1, {&scaled.Value(), &ir.Constant(255u)}), overflow.Value(), normal.Value()));
    result = IrU32(ir.Select(ir.Emit(IrOpcode::SLessThanEqual32, IrType::U1, {&scaled.Value(), &ir.Constant(0u)}), sign.Value(), result.Value()));
    const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)), ir.Constant(0x7f800000u)));
    result = IrU32(ir.Select(ir.IEqual(exponent.Value(), ir.Constant(0xffu)), ir.Select(nan.Value(), quietNan32(bits).Value(), bits.Value()), result.Value()));
    result = IrU32(ir.Select(ir.IEqual(exponent.Value(), ir.Constant(0u)), sign.Value(), result.Value()));
    writeOperand(inst.destination, &ir.BitCastF32(result.Value()));
    return true;
}

bool TranslationContext::vDot2cF32F16(const RdnaInstruction& inst) {
    RdnaOperand lhs = sourceAt(inst, 0u);
    lhs.opSel = false;
    lhs.opSelHi = true;
    RdnaOperand rhs = sourceAt(inst, 1u);
    rhs.opSel = false;
    rhs.opSelHi = true;
    return float16Dot2(inst, lhs, rhs, readU32(accumulatorOperand(inst)));
}

bool TranslationContext::vDot2F32F16(const RdnaInstruction& inst) {
    return float16Dot2(inst, sourceAt(inst, 0u), sourceAt(inst, 1u), readU32(sourceAt(inst, 2u)));
}

bool TranslationContext::float16Dot2(const RdnaInstruction& inst, const RdnaOperand& lhs, const RdnaOperand& rhs, IrU32 accumulator) {
    const auto packed = [&](const RdnaOperand& operand) {
        const IrU32 source = readF16SourceBits(operand);
        const auto half = [&](bool highLane) {
            const bool selectHigh = highLane ? operand.opSelHi : operand.opSel;
            IrU32 bits(selectHigh ? ir.ShiftRightLogical(source.Value(), ir.Constant(16u)) : ir.BitwiseAnd(source.Value(), ir.Constant(0xffffu)));
            if (highLane ? operand.negateHi : operand.negate) {
                bits = IrU32(ir.BitwiseXor(bits.Value(), ir.Constant(0x8000u)));
            }
            return bits;
        };
        return IrU32(ir.BitwiseOr(half(false).Value(), ir.ShiftLeftLogical(half(true).Value(), ir.Constant(16u))));
    };
    const IrU32 a = packed(lhs);
    const IrU32 b = packed(rhs);
    const IrU32 result(ir.Emit(IrOpcode::FPDot2F32F16, IrType::U32, {&a.Value(), &b.Value(), &accumulator.Value()}));
    RdnaOperand destination = inst.destination;
    destination.clamp = false;
    writeOperand(destination, &ir.BitCastF32(result.Value()));
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
    const IrF32 x(ir.BitCastF32(readU32(sourceAt(inst, 0u)).Value()));
    const IrF32 y(ir.BitCastF32(readU32(sourceAt(inst, 1u)).Value()));
    const IrF32 z(ir.BitCastF32(readU32(sourceAt(inst, 2u)).Value()));
    const IrF32 nx(ir.BitCastF32(ir.BitwiseXor(ir.BitCastU32(x.Value()), ir.Constant(0x80000000u))));
    const IrF32 ny(ir.BitCastF32(ir.BitwiseXor(ir.BitCastU32(y.Value()), ir.Constant(0x80000000u))));
    const IrF32 nz(ir.BitCastF32(ir.BitwiseXor(ir.BitCastU32(z.Value()), ir.Constant(0x80000000u))));
    const auto exponentZero = [&](IrF32 value) {
        return IrU1(ir.IEqual(ir.BitwiseAnd(ir.BitCastU32(value.Value()), ir.Constant(0x7f800000u)), ir.Constant(0u)));
    };
    const auto flushed = [&](IrF32 value) {
        return IrF32(ir.BitCastF32(ir.Select(exponentZero(value).Value(), ir.Constant(0u), ir.BitCastU32(value.Value()))));
    };
    const auto magnitude = [&](IrF32 value) {
        return IrF32(ir.BitCastF32(ir.BitwiseAnd(ir.BitCastU32(value.Value()), ir.Constant(0x7fffffffu))));
    };
    const IrF32 fx = flushed(x);
    const IrF32 fy = flushed(y);
    const IrF32 fz = flushed(z);
    const IrF32 ax = magnitude(fx);
    const IrF32 ay = magnitude(fy);
    const IrF32 az = magnitude(fz);
    const IrU1 zDominatesX(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&az.Value(), &ax.Value()}));
    const IrU1 zDominatesY(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&az.Value(), &ay.Value()}));
    const IrU1 zFace(ir.LogicalAnd(zDominatesX.Value(), zDominatesY.Value()));
    const IrU1 yFace(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&ay.Value(), &ax.Value()}));
    const IrU1 xNegative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&fx.Value(), &ir.ConstantF32(0.0f)}));
    const IrU1 yNegative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&fy.Value(), &ir.ConstantF32(0.0f)}));
    const IrU1 zNegative(ir.Emit(IrOpcode::FPOrdLessThan32, IrType::U1, {&fz.Value(), &ir.ConstantF32(0.0f)}));
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
            const IrF32 major = selectFace(x, y, z);
            const IrF32 twice(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&major.Value(), &ir.ConstantF32(2.0f)}));
            const IrU1 nan(ir.UGreaterThan(ir.BitCastU32(magnitude(major).Value()), ir.Constant(0x7f800000u)));
            result = selectF32(nan, major, selectF32(exponentZero(major), IrF32(ir.ConstantF32(0.0f)), twice));
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
