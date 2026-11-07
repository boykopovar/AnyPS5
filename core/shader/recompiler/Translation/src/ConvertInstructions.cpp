#include "Translation/ConvertInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <cstdint>
#include <stdexcept>

namespace ShaderRecompiler {

void TranslateConvertInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateConvertInstruction not implemented");
}

IrF32 TranslationContext::selectF32(IrU1 condition, IrF32 trueValue, IrF32 falseValue) {
    return IrF32(ir.Select(condition.Value(), trueValue.Value(), falseValue.Value()));
}

IrU32 TranslationContext::convertF32ToU32Saturated(IrF32 value, float upperBound, float safeUpper, std::uint32_t highResult) {
    const IrF32 zero(ir.ConstantF32(0.0f));
    const IrU1 nan(ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {&value.Value()}));
    const IrU1 low(ir.Emit(IrOpcode::FPOrdLessThanEqual32, IrType::U1, {&value.Value(), &zero.Value()}));
    const IrU1 high(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&value.Value(), &ir.ConstantF32(upperBound)}));
    const IrF32 safeLow = selectF32(IrU1(ir.LogicalOr(nan.Value(), low.Value())), zero, value);
    const IrF32 safe = selectF32(high, IrF32(ir.ConstantF32(safeUpper)), safeLow);
    const IrU32 converted(ir.Emit(IrOpcode::ConvertU32F32, IrType::U32, {&safe.Value()}));
    if (static_cast<std::uint32_t>(safeUpper) == highResult) return converted;
    return IrU32(ir.Select(high.Value(), ir.Constant(highResult), converted.Value()));
}

IrU32 TranslationContext::convertF32ToI32Saturated(IrF32 value, float lowerBound, float upperBound, float safeUpper, std::uint32_t lowerResult, std::uint32_t upperResult) {
    const IrU1 nan(ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {&value.Value()}));
    const IrU1 low(ir.Emit(IrOpcode::FPOrdLessThanEqual32, IrType::U1, {&value.Value(), &ir.ConstantF32(lowerBound)}));
    const IrU1 high(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&value.Value(), &ir.ConstantF32(upperBound)}));
    const IrF32 safeLow = selectF32(low, IrF32(ir.ConstantF32(lowerBound)), value);
    const IrF32 safeHigh = selectF32(high, IrF32(ir.ConstantF32(safeUpper)), safeLow);
    const IrF32 safe = selectF32(nan, IrF32(ir.ConstantF32(0.0f)), safeHigh);
    IrU32 converted(ir.Emit(IrOpcode::ConvertS32F32, IrType::U32, {&safe.Value()}));
    if (static_cast<std::uint32_t>(static_cast<std::int32_t>(lowerBound)) != lowerResult) converted = IrU32(ir.Select(low.Value(), ir.Constant(lowerResult), converted.Value()));
    if (static_cast<std::uint32_t>(static_cast<std::int32_t>(safeUpper)) != upperResult) converted = IrU32(ir.Select(high.Value(), ir.Constant(upperResult), converted.Value()));
    return converted;
}

IrU32 TranslationContext::packU16Lanes(IrU32 low, IrU32 high) {
    const IrU32 maskedLow(ir.BitwiseAnd(low.Value(), ir.Constant(0xffffu)));
    const IrU32 maskedHigh(ir.BitwiseAnd(high.Value(), ir.Constant(0xffffu)));
    return IrU32(ir.BitwiseOr(maskedLow.Value(), ir.ShiftLeftLogical(maskedHigh.Value(), ir.Constant(16u))));
}

void TranslationContext::vCvtF32Ubyte(const RdnaInstruction& inst, std::uint32_t byteIndex) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 shifted(ir.ShiftRightLogical(source.Value(), ir.Constant(byteIndex * 8u)));
    const IrU32 byteValue(ir.BitwiseAnd(shifted.Value(), ir.Constant(0xffu)));
    const IrF32 result(ir.Emit(IrOpcode::ConvertF32U32, IrType::F32, {&byteValue.Value()}));
    writeOperand(inst.destination, &result.Value());
}

bool TranslationContext::directedF32Rounding() const {
    return floatMode.has_value() && (floatMode->floatMode & 3u) != 0u;
}

IrF32 TranslationContext::convertMagnitudeToF32Directed(IrU32 magnitude, IrU32 signBits, std::uint32_t rounding, IrF32 native) {
    const IrU32 msb(ir.Emit(IrOpcode::FindUMsb32, IrType::U32, {&magnitude.Value()}));
    const IrU1 inexact(ir.UGreaterThan(msb.Value(), ir.Constant(23u)));
    const IrU32 shift(ir.ISub(msb.Value(), ir.Constant(23u)));
    const IrU32 top24(ir.ShiftRightLogical(magnitude.Value(), shift.Value()));
    const IrU32 discarded(ir.BitwiseAnd(magnitude.Value(), ir.ISub(ir.ShiftLeftLogical(ir.Constant(1u), shift.Value()), ir.Constant(1u))));
    const IrU1 positive(ir.IEqual(signBits.Value(), ir.Constant(0u)));
    const IrU1 away(rounding == 1u ? positive : rounding == 2u ? IrU1(ir.LogicalNot(positive.Value())) : IrU1(ir.ConstantBool(false)));
    const IrU1 increment(ir.LogicalAnd(away.Value(), ir.INotEqual(discarded.Value(), ir.Constant(0u))));
    const IrU32 rounded(ir.IAdd(top24.Value(), ir.Select(increment.Value(), ir.Constant(1u), ir.Constant(0u))));
    const IrU1 overflow(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&rounded.Value(), &ir.Constant(0x1000000u)}));
    const IrU32 finalMsb(ir.Select(overflow.Value(), ir.IAdd(msb.Value(), ir.Constant(1u)), msb.Value()));
    const IrU32 finalTop(ir.Select(overflow.Value(), ir.Constant(0x800000u), rounded.Value()));
    const IrU32 bits(ir.BitwiseOr(signBits.Value(), ir.BitwiseOr(ir.ShiftLeftLogical(ir.IAdd(finalMsb.Value(), ir.Constant(127u)), ir.Constant(23u)), ir.BitwiseAnd(finalTop.Value(), ir.Constant(0x7fffffu)))));
    const IrF32 directed(ir.BitCastF32(bits.Value()));
    const IrU1 useNative(ir.LogicalOr(ir.IEqual(magnitude.Value(), ir.Constant(0u)), ir.LogicalNot(inexact.Value())));
    return selectF32(useNative, native, directed);
}

IrF32 TranslationContext::convertU32ToF32(IrU32 source) {
    const IrF32 native(ir.Emit(IrOpcode::ConvertF32U32, IrType::F32, {&source.Value()}));
    if (!directedF32Rounding()) {
        return native;
    }
    return convertMagnitudeToF32Directed(source, IrU32(ir.Constant(0u)), floatMode->floatMode & 3u, native);
}

IrF32 TranslationContext::convertS32ToF32(IrU32 source) {
    const IrF32 native(ir.Emit(IrOpcode::ConvertF32S32, IrType::F32, {&source.Value()}));
    if (!directedF32Rounding()) {
        return native;
    }
    const IrU32 signBits(ir.BitwiseAnd(source.Value(), ir.Constant(0x80000000u)));
    const IrU32 magnitude(ir.Emit(IrOpcode::IAbs32, IrType::U32, {&source.Value()}));
    return convertMagnitudeToF32Directed(magnitude, signBits, floatMode->floatMode & 3u, native);
}

void TranslationContext::vCvtF32U32(const RdnaInstruction& inst) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrF32 result = convertU32ToF32(source);
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtF32I32(const RdnaInstruction& inst) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrF32 result = convertS32ToF32(source);
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtU32F32(const RdnaInstruction& inst) {
    const IrF32 value(ir.BitCastF32(*readOperand(sourceAt(inst, 0u), IrType::U32)));
    const IrU32 result = convertF32ToU32Saturated(value, 4294967296.0f, 4294967040.0f, 0xffffffffu);
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtI32F32(const RdnaInstruction& inst) {
    const IrF32 value(ir.BitCastF32(*readOperand(sourceAt(inst, 0u), IrType::U32)));
    const IrU32 result = convertF32ToI32Saturated(value, -2147483648.0f, 2147483648.0f, 2147483520.0f, 0x80000000u, 0x7fffffffu);
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtF16F32(const RdnaInstruction& inst) {
    const IrF32 value(*readOperand(sourceAt(inst, 0u), IrType::F32));
    writeF16Conversion(inst.destination, value, {&value.Value()});
}

void TranslationContext::vCvtF32F16(const RdnaInstruction& inst) {
    const IrF32 value = flushF16InputValue(readF16AsF32(sourceAt(inst, 0u)));
    writeOperand(inst.destination, &value.Value());
}

void TranslationContext::vCvtF1616(const RdnaInstruction& inst, bool signedValue) {
    const IrU32 source = readU16AsU32(sourceAt(inst, 0u), signedValue);
    const IrOpcode opcode = signedValue ? IrOpcode::ConvertF32S32 : IrOpcode::ConvertF32U32;
    const IrF32 value(ir.Emit(opcode, IrType::F32, {&source.Value()}));
    writeF16Conversion(inst.destination, value, {});
}

void TranslationContext::vCvt16F16(const RdnaInstruction& inst, bool signedValue) {
    const IrF32 value = readF16AsF32(sourceAt(inst, 0u));
    if (signedValue) {
        const IrU32 converted = convertF32ToI32Saturated(value, -32768.0f, 32768.0f, 32767.0f, 0xffff8000u, 0x7fffu);
        write16Bits(inst.destination, IrU32(ir.BitwiseAnd(converted.Value(), ir.Constant(0xffffu))));
        return;
    }
    write16Bits(inst.destination, convertF32ToU32Saturated(value, 65536.0f, 65535.0f, 0xffffu));
}

IrU32 TranslationContext::convertFlooredF32ToI32(IrF32 source, IrF32 floored) {
    const IrU32 converted = convertF32ToI32Saturated(floored, -2147483648.0f, 2147483648.0f, 2147483520.0f, 0x80000000u, 0x7fffffffu);
    const IrU1 nan(ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {&source.Value()}));
    const IrU32 sign(ir.ShiftRightLogical(ir.BitCastU32(source.Value()), ir.Constant(31u)));
    const IrU32 nanResult(ir.IAdd(ir.Constant(0x7fffffffu), sign.Value()));
    return IrU32(ir.Select(nan.Value(), nanResult.Value(), converted.Value()));
}

void TranslationContext::vCvtRpiI32F32(const RdnaInstruction& inst) {
    const IrF32 source(*readOperand(sourceAt(inst, 0u), IrType::F32));
    const IrF32 floored(ir.Emit(IrOpcode::FPFloor32, IrType::F32, {&source.Value()}));
    const IrF32 fraction(ir.Emit(IrOpcode::FPSub32, IrType::F32, {&source.Value(), &floored.Value()}));
    const IrU1 roundUp(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&fraction.Value(), &ir.ConstantF32(0.5f)}));
    const IrF32 increment = selectF32(roundUp, IrF32(ir.ConstantF32(1.0f)), IrF32(ir.ConstantF32(0.0f)));
    const IrF32 rounded(ir.Emit(IrOpcode::FPAdd32, IrType::F32, {&floored.Value(), &increment.Value()}));
    const IrU32 result = convertFlooredF32ToI32(source, rounded);
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtFlrI32F32(const RdnaInstruction& inst) {
    const IrF32 source(*readOperand(sourceAt(inst, 0u), IrType::F32));
    const IrF32 rounded(ir.Emit(IrOpcode::FPFloor32, IrType::F32, {&source.Value()}));
    const IrU32 result = convertFlooredF32ToI32(source, rounded);
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vFrexpExpI32F32(const RdnaInstruction& inst) {
    const IrF32 source(*readOperand(sourceAt(inst, 0u), IrType::F32));
    const IrU32 bits(ir.BitCastU32(source.Value()));
    const IrU32 exponent(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&bits.Value(), &ir.Constant(23u), &ir.Constant(8u)}));
    const IrU32 mantissa(ir.BitwiseAnd(bits.Value(), ir.Constant(0x007fffffu)));
    const IrU32 normal(ir.ISub(exponent.Value(), ir.Constant(126u)));
    const IrU32 msb(ir.Emit(IrOpcode::FindUMsb32, IrType::U32, {&mantissa.Value()}));
    const IrU32 subnormal(ir.ISub(msb.Value(), ir.Constant(148u)));
    const IrU1 mantissaNonZero(ir.INotEqual(mantissa.Value(), ir.Constant(0u)));
    const IrU32 denormal(ir.Select(mantissaNonZero.Value(), subnormal.Value(), ir.Constant(0u)));
    const IrU1 exponentNotAllOnes(ir.INotEqual(exponent.Value(), ir.Constant(0xffu)));
    const IrU1 exponentNonZero(ir.INotEqual(exponent.Value(), ir.Constant(0u)));
    const IrU32 normalOrDenormal(ir.Select(exponentNonZero.Value(), normal.Value(), denormal.Value()));
    const IrU32 finite(ir.Select(exponentNotAllOnes.Value(), normalOrDenormal.Value(), ir.Constant(0u)));
    writeOperand(inst.destination, &finite.Value());
}

void TranslationContext::vCvtOffF32I4(const RdnaInstruction& inst) {
    const IrU32 source = readU32(sourceAt(inst, 0u));
    const IrU32 nibble(ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&source.Value(), &ir.Constant(0u), &ir.Constant(4u)}));
    const IrF32 value(ir.Emit(IrOpcode::ConvertF32S32, IrType::F32, {&nibble.Value()}));
    const IrF32 result(ir.Emit(IrOpcode::FPMul32, IrType::F32, {&value.Value(), &ir.ConstantF32(1.0f / 16.0f)}));
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtPkrtzF16F32(const RdnaInstruction& inst) {
    const IrF32 lhs(*readOperand(sourceAt(inst, 0u), IrType::F32));
    const IrF32 rhs(*readOperand(sourceAt(inst, 1u), IrType::F32));
    IrU32 result(ir.Emit(IrOpcode::PackFloat2x16Rtz, IrType::U32, {&lhs.Value(), &rhs.Value()}));
    if (floatMode.has_value() && (floatMode->floatMode & 0x80u) == 0u) {
        const IrU32 low = flushF16Result(IrU32(ir.BitwiseAnd(result.Value(), ir.Constant(0xffffu))));
        const IrU32 high = flushF16Result(IrU32(ir.ShiftRightLogical(result.Value(), ir.Constant(16u))));
        result = packU16Lanes(low, high);
    }
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtPknormF32(const RdnaInstruction& inst, bool signedValue) {
    const IrU32 low = normF32(flushF32Denormal(readU32(sourceAt(inst, 0u))), signedValue);
    const IrU32 high = normF32(flushF32Denormal(readU32(sourceAt(inst, 1u))), signedValue);
    const IrU32 result(ir.BitwiseOr(low.Value(), ir.ShiftLeftLogical(high.Value(), ir.Constant(16u))));
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vCvtPkU8F32(const RdnaInstruction& inst) {
    const IrU32 bits = flushF32Denormal(IrU32(*readOperand(sourceAt(inst, 0u), IrType::U32)));
    const IrF32 source(ir.BitCastF32(bits.Value()));
    const IrF32 zero(ir.ConstantF32(0.0f));
    const IrU1 nan(ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {&source.Value()}));
    const IrU1 low(ir.Emit(IrOpcode::FPOrdLessThanEqual32, IrType::U1, {&source.Value(), &zero.Value()}));
    const IrU1 high(ir.Emit(IrOpcode::FPOrdGreaterThanEqual32, IrType::U1, {&source.Value(), &ir.ConstantF32(255.0f)}));
    const std::uint32_t roundMode = floatMode.has_value() ? floatMode->floatMode & 3u : 0u;
    const IrOpcode rounding = roundMode == 1u ? IrOpcode::FPCeil32 : roundMode == 2u ? IrOpcode::FPFloor32 : roundMode == 3u ? IrOpcode::FPTrunc32 : IrOpcode::FPRoundEven32;
    const IrF32 rounded(ir.Emit(rounding, IrType::F32, {&source.Value()}));
    const IrF32 bounded = selectF32(high, IrF32(ir.ConstantF32(255.0f)), rounded);
    const IrF32 safe = selectF32(IrU1(ir.LogicalOr(nan.Value(), low.Value())), zero, bounded);
    IrU32 byteValue(ir.Emit(IrOpcode::ConvertU32F32, IrType::U32, {&safe.Value()}));
    if (roundMode == 1u && (f32DenormalFlush & 1u) == 0u) {
        const IrU1 positiveDenormal(ir.ULessThan(ir.ISub(bits.Value(), ir.Constant(1u)), ir.Constant(0x007fffffu)));
        byteValue = IrU32(ir.Select(positiveDenormal.Value(), ir.Constant(1u), byteValue.Value()));
    }
    const IrU32 index(ir.BitwiseAnd(readU32(sourceAt(inst, 1u)).Value(), ir.Constant(3u)));
    const IrU32 shift(ir.ShiftLeftLogical(index.Value(), ir.Constant(3u)));
    const IrU32 mask(ir.ShiftLeftLogical(ir.Constant(0xffu), shift.Value()));
    const IrU32 base(ir.BitwiseAnd(readU32(sourceAt(inst, 2u)).Value(), ir.BitwiseNot(mask.Value())));
    const IrU32 result(ir.BitwiseOr(base.Value(), ir.ShiftLeftLogical(byteValue.Value(), shift.Value())));
    writeOperand(inst.destination, &result.Value());
}

void TranslationContext::vPackB32F16(const RdnaInstruction& inst) {
    const auto half = [&](std::uint32_t index) {
        const IrU32 bits = flushF16Input(readF16Bits(sourceAt(inst, index)));
        const IrU1 nan(ir.UGreaterThan(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)), ir.Constant(0x7c00u)));
        return IrU32(ir.Select(nan.Value(), quietNan16(bits).Value(), bits.Value()));
    };
    const IrU32 low = half(0u);
    const IrU32 high(ir.ShiftLeftLogical(half(1u).Value(), ir.Constant(16u)));
    const IrU32 result(ir.BitwiseOr(low.Value(), high.Value()));
    writeOperand(inst.destination, &result.Value());
}

void TranslateConvertInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateConvertInstruction not implemented");
}

}
