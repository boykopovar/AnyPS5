#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

IrU32 TranslationContext::readU32(const RdnaOperand& operand) {
    return IrU32(*readOperand(operand, IrType::U32));
}

std::array<IrU32, 2> TranslationContext::readU32Pair(const RdnaOperand& operand) {
    switch (operand.kind) {
        case RdnaOperandKind::LiteralConstant: return {readU32(operand), IrU32(ir.Constant(0u))};
        case RdnaOperandKind::IntegerInlineConstant: return {readU32(operand), IrU32(ir.Constant(operand.signedVal < 0 ? 0xffffffffu : 0u))};
        case RdnaOperandKind::FloatInlineConstant: {
            const auto bits = std::bit_cast<std::uint64_t>(static_cast<double>(std::bit_cast<float>(operand.value)));
            return {IrU32(ir.Constant(static_cast<std::uint32_t>(bits))), IrU32(ir.Constant(static_cast<std::uint32_t>(bits >> 32u)))};
        }
        case RdnaOperandKind::SrcSharedBase:
        case RdnaOperandKind::SrcSharedLimit:
        case RdnaOperandKind::SrcPrivateBase:
        case RdnaOperandKind::SrcPrivateLimit: return {readU32(operand), IrU32(ir.Constant(static_cast<std::uint32_t>(RdnaApertureSourceValue(operand) >> 32u)))};
        default: return {readU32(operand), readU32(offsetOperand(operand, 1u))};
    }
}

IrU64 TranslationContext::readU64(const RdnaOperand& operand) {
    const std::array<IrU32, 2> pair = readU32Pair(operand);
    return IrU64(ir.ConstructU64(pair[0].Value(), pair[1].Value()));
}

std::array<IrU32, 2> TranslationContext::readF64Bits(const RdnaOperand& operand) {
    std::array<IrU32, 2> bits{IrU32(ir.Constant(0u)), IrU32(ir.Constant(operand.value))};
    if (operand.kind == RdnaOperandKind::FloatInlineConstant && operand.value == 0x3e22f983u) {
        bits = {IrU32(ir.Constant(0x6dc9c882u)), IrU32(ir.Constant(0x3fc45f30u))};
    } else if (operand.kind != RdnaOperandKind::LiteralConstant) {
        RdnaOperand source = operand;
        source.absolute = false;
        source.negate = false;
        bits = readU32Pair(source);
    }
    if (operand.absolute) {
        bits[1] = IrU32(ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x7fffffffu)));
    }
    if (operand.negate) {
        bits[1] = IrU32(ir.BitwiseXor(bits[1].Value(), ir.Constant(0x80000000u)));
    }
    return bits;
}

std::array<IrU32, 2> TranslationContext::flushF64Bits(const std::array<IrU32, 2>& bits, std::uint32_t keepBit) {
    if (!floatMode.has_value() || (floatMode->floatMode & keepBit) != 0u) return bits;
    const IrU1 tiny(ir.IEqual(ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x7ff00000u)), ir.Constant(0u)));
    return {IrU32(ir.Select(tiny.Value(), ir.Constant(0u), bits[0].Value())), IrU32(ir.Select(tiny.Value(), ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x80000000u)), bits[1].Value()))};
}

std::array<IrU32, 2> TranslationContext::extractU64(IrU64 value) {
    return {IrU32(ir.CompositeExtract(value.Value(), 0u)), IrU32(ir.CompositeExtract(value.Value(), 1u))};
}

void TranslationContext::writeU32Pair(const RdnaOperand& operand, const std::array<IrU32, 2>& value) {
    writeRawU32(operand, value[0]);
    writeRawU32(offsetOperand(operand, 1u), value[1]);
}

IrF32 TranslationContext::readF16LaneAsF32(const RdnaOperand& operand, bool highLane, bool packed) {
    const bool selectHigh = packed ? (highLane ? operand.opSelHi : operand.opSel) : highLane;
    const IrU32 source = readF16SourceBits(operand);
    const IrU32 raw(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&source.Value(), &ir.Constant(selectHigh ? 16u : 0u), &ir.Constant(16u)}));
    const IrU16 bits(ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&raw.Value()}));
    const IrF16 half(ir.Emit(IrOpcode::BitCastF16U16, IrType::F16, {&bits.Value()}));
    IrF32 value(ir.Emit(IrOpcode::ConvertF32F16, IrType::F32, {&half.Value()}));
    if (operand.absolute) {
        value = IrF32(ir.Emit(IrOpcode::FPAbs32, IrType::F32, {&value.Value()}));
    }
    const bool negate = packed && highLane ? operand.negateHi : operand.negate;
    if (negate) {
        value = IrF32(ir.Emit(IrOpcode::FPNeg32, IrType::F32, {&value.Value()}));
    }
    return value;
}

IrU32 TranslationContext::readF16SourceBits(const RdnaOperand& operand) {
    if (operand.kind != RdnaOperandKind::FloatInlineConstant) {
        return applyBitSourceModifiers(operand, readRawU32(operand));
    }
    std::uint32_t half = 0u;
    switch (operand.value) {
        case 0x3f000000u: half = 0x3800u; break;
        case 0xbf000000u: half = 0xb800u; break;
        case 0x3f800000u: half = 0x3c00u; break;
        case 0xbf800000u: half = 0xbc00u; break;
        case 0x40000000u: half = 0x4000u; break;
        case 0xc0000000u: half = 0xc000u; break;
        case 0x40800000u: half = 0x4400u; break;
        case 0xc0800000u: half = 0xc400u; break;
        case 0x3e22f983u: half = 0x3118u; break;
        default: throw std::runtime_error("TranslationContext::readF16SourceBits unknown float inline constant bits " + std::to_string(operand.value));
    }
    return applyBitSourceModifiers(operand, IrU32(ir.Constant(half)));
}

IrF32 TranslationContext::readF16AsF32(const RdnaOperand& operand) {
    const IrU32 source = readF16SourceBits(operand);
    const IrU32 raw = operand.opSel ? IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&source.Value(), &ir.Constant(16u), &ir.Constant(16u)})) : source;
    const IrU16 bits(ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&raw.Value()}));
    const IrF16 half(ir.Emit(IrOpcode::BitCastF16U16, IrType::F16, {&bits.Value()}));
    IrF32 value(ir.Emit(IrOpcode::ConvertF32F16, IrType::F32, {&half.Value()}));
    if (operand.absolute) {
        value = IrF32(ir.Emit(IrOpcode::FPAbs32, IrType::F32, {&value.Value()}));
    }
    if (operand.negate) {
        value = IrF32(ir.Emit(IrOpcode::FPNeg32, IrType::F32, {&value.Value()}));
    }
    return value;
}

IrF32 TranslationContext::readMixF32(const RdnaOperand& operand) {
    if (!operand.opSelHi) {
        return IrF32(*readOperand(operand, IrType::F32));
    }
    return flushF16InputValue(readF16LaneAsF32(operand, operand.opSel, false));
}

IrU32 TranslationContext::readF16LaneBits(const RdnaOperand& operand, bool highLane) {
    return readU16LaneRaw(operand, highLane ? operand.opSelHi : operand.opSel);
}

IrU32 TranslationContext::readU16LaneRaw(const RdnaOperand& operand, bool highLane) {
    const IrU32 raw = applyBitSourceModifiers(operand, readRawU32(operand));
    return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&raw.Value(), &ir.Constant(highLane ? 16u : 0u), &ir.Constant(16u)}));
}

IrU32 TranslationContext::readU16LaneAsU32(const RdnaOperand& operand, bool highLane, bool signExtend) {
    IrU32 raw = readU16LaneRaw(operand, highLane ? operand.opSelHi : operand.opSel);
    if (highLane ? operand.negateHi : operand.negate) {
        raw = IrU32(ir.BitwiseXor(raw.Value(), ir.Constant(0x8000u)));
    }
    if (!signExtend) {
        return raw;
    }
    return IrU32(ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&raw.Value(), &ir.Constant(0u), &ir.Constant(16u)}));
}

IrU32 TranslationContext::readU16AsU32(const RdnaOperand& operand, bool signExtend) {
    const IrU32 raw = readU16LaneRaw(operand, operand.opSel);
    if (!signExtend) {
        return raw;
    }
    return IrU32(ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&raw.Value(), &ir.Constant(0u), &ir.Constant(16u)}));
}

IrU32 TranslationContext::packHalf2x16(IrF32 low, IrF32 high) {
    IrValue& pair = ir.Emit(IrOpcode::CompositeConstructF32x2, IrType::F32x2, {&low.Value(), &high.Value()});
    return IrU32(ir.Emit(IrOpcode::PackHalf2x16, IrType::U32, {&pair}));
}

void TranslationContext::write16Bits(const RdnaOperand& operand, IrU32 value) {
    if (operand.sdwaSel != 6u || operand.explicitSdwaDst) {
        writeRawU32(operand, value);
        return;
    }
    const IrU32 masked(ir.BitwiseAnd(value.Value(), ir.Constant(0xffffu)));
    const IrU32 old = readRawU32(plainOperand(operand));
    const IrU32 cleared(ir.BitwiseAnd(old.Value(), ir.Constant(0xffff0000u)));
    writeRawU32(operand, IrU32(ir.BitwiseOr(cleared.Value(), masked.Value())));
}

bool TranslationContext::directedF16Rounding() const {
    return floatMode.has_value() && ((floatMode->floatMode >> 2u) & 3u) != 0u;
}

IrU32 TranslationContext::flushF16Bits(IrU32 bits, std::uint32_t keepBit) {
    if (!floatMode.has_value() || (floatMode->floatMode & keepBit) != 0u) return bits;
    const IrU1 tiny(ir.ULessThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), ir.Constant(0x0400u)));
    return IrU32(ir.Select(tiny.Value(), ir.BitwiseAnd(bits.Value(), ir.Constant(0x8000u)), bits.Value()));
}

IrU32 TranslationContext::flushF16Input(IrU32 bits) {
    return flushF16Bits(bits, 0x40u);
}

IrU32 TranslationContext::flushF16Result(IrU32 bits) {
    return flushF16Bits(bits, 0x80u);
}

IrF32 TranslationContext::flushF16InputValue(IrF32 value) {
    if (!floatMode.has_value() || (floatMode->floatMode & 0x40u) != 0u) return value;
    return flushTinyF16Value(value, 0x38800000u);
}

IrF32 TranslationContext::flushTinyF16Value(IrF32 value, std::uint32_t threshold) {
    const IrU32 bits(ir.BitCastU32(value.Value()));
    const IrU1 tiny(ir.ULessThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)), ir.Constant(threshold)));
    return IrF32(ir.BitCastF32(ir.Select(tiny.Value(), ir.BitwiseAnd(bits.Value(), ir.Constant(0x80000000u)), bits.Value())));
}

IrU32 TranslationContext::convertF32ToF16Bits(IrF32 value, std::initializer_list<IrValue*> sources) {
    const IrF16 half(ir.Emit(IrOpcode::ConvertF16F32, IrType::F16, {&clampF16Overflow(value, sources).Value()}));
    const IrU32 native(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {&ir.Emit(IrOpcode::BitCastU16F16, IrType::U16, {&half.Value()})}));
    if (!directedF16Rounding()) {
        return native;
    }
    const std::uint32_t rounding = (floatMode->floatMode >> 2u) & 3u;
    const IrU32 bits(ir.BitCastU32(value.Value()));
    const IrU32 sign(ir.BitwiseAnd(ir.ShiftRightLogical(bits.Value(), ir.Constant(16u)), ir.Constant(0x8000u)));
    const IrU32 magnitude(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)));
    const IrU32 exponent(ir.ShiftRightLogical(magnitude.Value(), ir.Constant(23u)));
    const IrU1 normal(ir.UGreaterThan(exponent.Value(), ir.Constant(112u)));
    const IrU32 significand(ir.BitwiseOr(ir.BitwiseAnd(magnitude.Value(), ir.Constant(0x7fffffu)), ir.Constant(0x800000u)));
    const IrU32 shift(ir.Emit(IrOpcode::UMin32, IrType::U32, {&ir.ISub(ir.Constant(126u), exponent.Value()), &ir.Constant(24u)}));
    const IrU32 subnormal(ir.ShiftRightLogical(significand.Value(), shift.Value()));
    const IrU32 truncated(ir.Select(normal.Value(), ir.ISub(ir.ShiftRightLogical(magnitude.Value(), ir.Constant(13u)), ir.Constant(0x1c000u)), subnormal.Value()));
    const IrU1 tiny(ir.ULessThan(exponent.Value(), ir.Constant(103u)));
    const IrU32 discarded(ir.Select(normal.Value(), ir.BitwiseAnd(magnitude.Value(), ir.Constant(0x1fffu)),
        ir.Select(tiny.Value(), magnitude.Value(), ir.ISub(significand.Value(), ir.ShiftLeftLogical(subnormal.Value(), shift.Value())))));
    const IrU1 away(rounding == 1u ? ir.IEqual(sign.Value(), ir.Constant(0u)) : rounding == 2u ? ir.INotEqual(sign.Value(), ir.Constant(0u)) : ir.ConstantBool(false));
    const IrU1 increment(ir.LogicalAnd(away.Value(), ir.INotEqual(discarded.Value(), ir.Constant(0u))));
    const IrU32 rounded(ir.IAdd(truncated.Value(), ir.Select(increment.Value(), ir.Constant(1u), ir.Constant(0u))));
    const IrU32 overflow(fp16Overflow() ? ir.Constant(0x7bffu) : ir.Select(away.Value(), ir.Constant(0x7c00u), ir.Constant(0x7bffu)));
    IrU32 result(ir.Select(ir.UGreaterThan(exponent.Value(), ir.Constant(142u)), overflow.Value(), rounded.Value()));
    if (fp16Overflow()) {
        result = IrU32(ir.Select(ir.IEqual(result.Value(), ir.Constant(0x7c00u)), ir.Constant(0x7bffu), result.Value()));
    }
    result = IrU32(ir.BitwiseOr(sign.Value(), result.Value()));
    return IrU32(ir.Select(ir.ULessThan(magnitude.Value(), ir.Constant(0x7f800000u)), result.Value(), native.Value()));
}

IrU32 TranslationContext::flushTinyF16Conversion(IrF32 value, IrU32 bits) {
    if (!floatMode.has_value() || (floatMode->floatMode & 0x80u) != 0u) return bits;
    const std::uint32_t rounding = (floatMode->floatMode >> 2u) & 3u;
    const IrU32 word(ir.BitCastU32(value.Value()));
    const IrU32 magnitude(ir.BitwiseAnd(word.Value(), ir.Constant(0x7fffffffu)));
    const IrU32 sign(ir.BitwiseAnd(ir.ShiftRightLogical(word.Value(), ir.Constant(16u)), ir.Constant(0x8000u)));
    IrValue* limit = &ir.Constant(rounding == 0u ? 0x387ff000u : 0x38800000u);
    if (rounding == 1u || rounding == 2u) {
        const IrU1 away(ir.IEqual(sign.Value(), ir.Constant(rounding == 1u ? 0u : 0x8000u)));
        limit = &ir.Select(away.Value(), ir.Constant(0x387fe001u), ir.Constant(0x38800000u));
    }
    const IrU1 tiny(ir.ULessThan(magnitude.Value(), *limit));
    return IrU32(ir.Select(tiny.Value(), sign.Value(), bits.Value()));
}

void TranslationContext::writeF16Conversion(const RdnaOperand& operand, IrF32 value, std::initializer_list<IrValue*> sources) {
    if (!directedF16Rounding() && (!floatMode.has_value() || (floatMode->floatMode & 0x80u) != 0u)) {
        writeF16(operand, value, sources);
        return;
    }
    const IrF32 modified = applyF16ResultModifiers(operand, value);
    write16Bits(operand, flushTinyF16Conversion(modified, convertF32ToF16Bits(modified, sources)));
}

void TranslationContext::writeF16(const RdnaOperand& operand, IrF32 value, std::initializer_list<IrValue*> sources, IrValue* invalid) {
    const IrF16 half(ir.Emit(IrOpcode::ConvertF16F32, IrType::F16, {&applyF16ResultModifiers(operand, clampF16Overflow(value, sources)).Value()}));
    const IrU16 bits(ir.Emit(IrOpcode::BitCastU16F16, IrType::U16, {&half.Value()}));
    IrU32 word(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {&bits.Value()}));
    if (invalid != nullptr) {
        const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(word.Value(), ir.Constant(0x7fffu)), ir.Constant(0x7c00u)));
        word = IrU32(ir.Select(ir.LogicalAnd(nan.Value(), *invalid), ir.Constant(0xfe00u), word.Value()));
    }
    write16Bits(operand, word);
}

}
