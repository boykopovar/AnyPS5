#include "Translation/TranslationContext.hpp"
#include <array>

namespace ShaderRecompiler {

IrU32 TranslationContext::readU32(const RdnaOperand& operand) {
    return IrU32(*readOperand(operand, IrType::U32));
}

std::array<IrU32, 2> TranslationContext::readU32Pair(const RdnaOperand& operand) {
    return {readU32(operand), readU32(offsetOperand(operand, 1u))};
}

IrU64 TranslationContext::readU64(const RdnaOperand& operand) {
    const std::array<IrU32, 2> pair = readU32Pair(operand);
    return IrU64(ir.ConstructU64(pair[0].Value(), pair[1].Value()));
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
    const IrU32 raw = readU16LaneRaw(operand, selectHigh);
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

IrF32 TranslationContext::readF16AsF32(const RdnaOperand& operand) {
    const IrU32 raw = applyBitSourceModifiers(operand, readRawU32(operand));
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
    if (!operand.opSel) {
        return IrF32(*readOperand(operand, IrType::F32));
    }
    return readF16LaneAsF32(operand, operand.opSelHi, false);
}

IrU32 TranslationContext::readF16LaneBits(const RdnaOperand& operand, bool highLane) {
    return readU16LaneRaw(operand, highLane ? operand.opSelHi : operand.opSel);
}

IrU32 TranslationContext::readU16LaneRaw(const RdnaOperand& operand, bool highLane) {
    const IrU32 raw = applyBitSourceModifiers(operand, readRawU32(operand));
    return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&raw.Value(), &ir.Constant(highLane ? 16u : 0u), &ir.Constant(16u)}));
}

IrU32 TranslationContext::readU16LaneAsU32(const RdnaOperand& operand, bool highLane, bool signExtend) {
    const IrU32 raw = readU16LaneRaw(operand, highLane ? operand.opSelHi : operand.opSel);
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
    if (operand.sdwaSel != 6u) {
        writeRawU32(operand, value);
        return;
    }
    const IrU32 masked(ir.BitwiseAnd(value.Value(), ir.Constant(0xffffu)));
    const IrU32 old = readRawU32(plainOperand(operand));
    const IrU32 cleared(ir.BitwiseAnd(old.Value(), ir.Constant(0xffff0000u)));
    writeRawU32(operand, IrU32(ir.BitwiseOr(cleared.Value(), masked.Value())));
}

void TranslationContext::writeF16(const RdnaOperand& operand, IrF32 value) {
    const IrF16 half(ir.Emit(IrOpcode::ConvertF16F32, IrType::F16, {&value.Value()}));
    const IrU16 bits(ir.Emit(IrOpcode::BitCastU16F16, IrType::U16, {&half.Value()}));
    write16Bits(operand, IrU32(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {&bits.Value()})));
}

}
