#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>

namespace ShaderRecompiler {

IrU64 TranslationContext::readF64(const RdnaOperand& operand) {
    const std::array<IrU32, 2> bits = readF64Bits(operand);
    return IrU64(ir.ConstructU64(bits[0].Value(), bits[1].Value()));
}

bool TranslationContext::float64Operation(const RdnaInstruction& inst, IrOpcode opcode) {
    const std::size_t count = IrOpcodeOperandCount(opcode);
    std::array<IrValue*, 3> args{};
    for (std::uint32_t index = 0u; index < count; ++index) {
        const RdnaOperand& operand = sourceAt(inst, index);
        switch (IrOpcodeArgumentType(opcode, index)) {
            case IrType::U64: args[index] = &readF64(operand).Value(); break;
            case IrType::F32: args[index] = readOperand(operand, IrType::F32); break;
            default: args[index] = &readU32(operand).Value(); break;
        }
    }
    const IrType type = IrOpcodeType(opcode);
    IrValue* result = count == 1u ? &ir.Emit(opcode, type, {args[0]}) : count == 2u ? &ir.Emit(opcode, type, {args[0], args[1]}) : &ir.Emit(opcode, type, {args[0], args[1], args[2]});
    if (type == IrType::U64) {
        writeF64Result(inst.destination, *result);
        return true;
    }
    RdnaOperand destination = inst.destination;
    destination.omod = 0u;
    writeOperand(destination, result);
    return true;
}

void TranslationContext::writeF64Result(const RdnaOperand& operand, IrValue& value) {
    RdnaOperand destination = operand;
    destination.omod = 0u;
    IrValue* result = &value;
    if (destination.clamp) {
        result = &ir.Emit(IrOpcode::FPSaturate64, IrType::U64, {result});
    }
    writeOperand(destination, result);
}

bool TranslationContext::vDivScaleF64(const RdnaInstruction& inst) {
    const auto value = readF64Bits(sourceAt(inst, 0u));
    const auto denominator = readF64Bits(sourceAt(inst, 1u));
    const auto numerator = readF64Bits(sourceAt(inst, 2u));
    const auto exponent = [&](IrU32 high) {
        return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&high.Value(), &ir.Constant(20u), &ir.Constant(11u)}));
    };
    const IrU32 denominatorExponent = exponent(denominator[1]);
    const IrU32 numeratorExponent = exponent(numerator[1]);
    const IrU32 difference(ir.ISub(numeratorExponent.Value(), denominatorExponent.Value()));
    const IrU1 nearMax(ir.Emit(IrOpcode::SGreaterThanEqual32, IrType::U1, {&difference.Value(), &ir.Constant(768u)}));
    const IrU1 nearMin(ir.Emit(IrOpcode::SLessThanEqual32, IrType::U1, {&difference.Value(), &ir.Constant(static_cast<std::uint32_t>(-768))}));
    const IrU1 denominatorDenormal(ir.IEqual(denominatorExponent.Value(), ir.Constant(0u)));
    const IrU1 denominatorHuge(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&denominatorExponent.Value(), &ir.Constant(2045u)}));
    const IrU1 numeratorTiny(ir.Emit(IrOpcode::ULessThanEqual32, IrType::U1, {&numeratorExponent.Value(), &ir.Constant(53u)}));
    const IrU1 isDenominator(ir.LogicalAnd(ir.IEqual(value[0].Value(), denominator[0].Value()), ir.IEqual(value[1].Value(), denominator[1].Value())));
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
    IrValue& bits = ir.ConstructU64(value[0].Value(), value[1].Value());
    IrValue& power = ir.Select(scaleUp.Value(), ir.Constant(128u), ir.Constant(static_cast<std::uint32_t>(-128)));
    IrValue& scaled = ir.Emit(IrOpcode::FPLdexp64, IrType::U64, {&bits, &power});
    const auto magnitude = [&](IrU32 high) { return IrU32(ir.BitwiseAnd(high.Value(), ir.Constant(0x7fffffffu))); };
    const IrU1 valueNan(ir.LogicalOr(ir.UGreaterThan(magnitude(value[1]).Value(), ir.Constant(0x7ff00000u)),
        ir.LogicalAnd(ir.IEqual(magnitude(value[1]).Value(), ir.Constant(0x7ff00000u)), ir.INotEqual(value[0].Value(), ir.Constant(0u)))));
    const IrU1 rescale(ir.LogicalAnd(ir.LogicalOr(scaleUp.Value(), scaleDown.Value()), ir.LogicalNot(valueNan.Value())));
    const auto isZero = [&](const std::array<IrU32, 2>& word) {
        return IrU1(ir.IEqual(ir.BitwiseOr(magnitude(word[1]).Value(), word[0].Value()), ir.Constant(0u)));
    };
    const IrU1 zero(ir.LogicalOr(isZero(denominator).Value(), isZero(numerator).Value()));
    std::array<IrU32, 2> result{IrU32(ir.CompositeExtract(scaled, 0u)), IrU32(ir.CompositeExtract(scaled, 1u))};
    result[0] = IrU32(ir.Select(rescale.Value(), result[0].Value(), value[0].Value()));
    result[1] = IrU32(ir.Select(rescale.Value(), result[1].Value(), value[1].Value()));
    result[0] = IrU32(ir.Select(zero.Value(), ir.Constant(0u), result[0].Value()));
    result[1] = IrU32(ir.Select(zero.Value(), ir.Constant(0xfff80000u), result[1].Value()));
    writeOperand(inst.destination, &ir.ConstructU64(result[0].Value(), result[1].Value()));
    writeMask(inst.destination2, IrU1(ir.LogicalOr(nearMax.Value(), ir.LogicalAnd(notDenormal.Value(), nearMin.Value()))));
    return true;
}

bool TranslationContext::vDivFmasF64(const RdnaInstruction& inst) {
    RdnaOperand vcc{};
    vcc.kind = RdnaOperandKind::VccLo;
    const IrU1 scale = readMask(vcc);
    const auto addend = readF64Bits(sourceAt(inst, 2u));
    IrValue& addendExponent = ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&addend[1].Value(), &ir.Constant(20u), &ir.Constant(11u)});
    const IrU1 up(ir.Emit(IrOpcode::UGreaterThanEqual32, IrType::U1, {&addendExponent, &ir.Constant(1024u)}));
    IrValue& power = ir.Select(scale.Value(), ir.Select(up.Value(), ir.Constant(128u), ir.Constant(static_cast<std::uint32_t>(-128))), ir.Constant(0u));
    IrValue& result = ir.Emit(IrOpcode::FPFmaScale64, IrType::U64,
        {&readF64(sourceAt(inst, 0u)).Value(), &readF64(sourceAt(inst, 1u)).Value(), &ir.ConstructU64(addend[0].Value(), addend[1].Value()), &power});
    writeF64Result(inst.destination, result);
    return true;
}

bool TranslationContext::vDivFixupF64(const RdnaInstruction& inst) {
    const auto quotient = readF64Bits(sourceAt(inst, 0u));
    const auto denominator = readF64Bits(sourceAt(inst, 1u));
    const auto numerator = readF64Bits(sourceAt(inst, 2u));
    const auto magnitude = [&](IrU32 high) { return IrU32(ir.BitwiseAnd(high.Value(), ir.Constant(0x7fffffffu))); };
    const auto lowNonzero = [&](const std::array<IrU32, 2>& word) { return IrU1(ir.INotEqual(word[0].Value(), ir.Constant(0u))); };
    const auto isNan = [&](const std::array<IrU32, 2>& word) {
        return IrU1(ir.LogicalOr(ir.UGreaterThan(magnitude(word[1]).Value(), ir.Constant(0x7ff00000u)),
            ir.LogicalAnd(ir.IEqual(magnitude(word[1]).Value(), ir.Constant(0x7ff00000u)), lowNonzero(word).Value())));
    };
    const auto isInf = [&](const std::array<IrU32, 2>& word) {
        return IrU1(ir.LogicalAnd(ir.IEqual(magnitude(word[1]).Value(), ir.Constant(0x7ff00000u)), ir.LogicalNot(lowNonzero(word).Value())));
    };
    const auto isZero = [&](const std::array<IrU32, 2>& word) {
        return IrU1(ir.IEqual(ir.BitwiseOr(magnitude(word[1]).Value(), word[0].Value()), ir.Constant(0u)));
    };
    const auto exponent = [&](IrU32 high) {
        return IrU32(ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&high.Value(), &ir.Constant(20u), &ir.Constant(11u)}));
    };
    const IrU32 sign(ir.BitwiseAnd(ir.BitwiseXor(denominator[1].Value(), numerator[1].Value()), ir.Constant(0x80000000u)));
    const std::array<IrU32, 2> zero{IrU32(ir.Constant(0u)), sign};
    const std::array<IrU32, 2> infinity{IrU32(ir.Constant(0u)), IrU32(ir.BitwiseOr(sign.Value(), ir.Constant(0x7ff00000u)))};
    const std::array<IrU32, 2> defaultNan{IrU32(ir.Constant(0u)), IrU32(ir.Constant(0xfff80000u))};
    const auto quiet = [&](const std::array<IrU32, 2>& word) {
        return std::array<IrU32, 2>{word[0], IrU32(ir.BitwiseOr(word[1].Value(), ir.Constant(0x00080000u)))};
    };
    const IrU32 difference(ir.ISub(exponent(numerator[1]).Value(), exponent(denominator[1]).Value()));
    const IrU1 underflow(ir.Emit(IrOpcode::SLessThan32, IrType::U1, {&difference.Value(), &ir.Constant(static_cast<std::uint32_t>(-1075))}));
    std::array<IrU32, 2> result{quotient[0], IrU32(ir.BitwiseOr(sign.Value(), magnitude(quotient[1]).Value()))};
    const auto choose = [&](IrU1 condition, const std::array<IrU32, 2>& word) {
        result = {IrU32(ir.Select(condition.Value(), word[0].Value(), result[0].Value())), IrU32(ir.Select(condition.Value(), word[1].Value(), result[1].Value()))};
    };
    choose(isNan(quotient), infinity);
    choose(underflow, zero);
    choose(IrU1(ir.LogicalOr(isInf(denominator).Value(), isZero(numerator).Value())), zero);
    choose(IrU1(ir.LogicalOr(isZero(denominator).Value(), isInf(numerator).Value())), infinity);
    choose(IrU1(ir.LogicalOr(ir.LogicalAnd(isZero(denominator).Value(), isZero(numerator).Value()), ir.LogicalAnd(isInf(denominator).Value(), isInf(numerator).Value()))), defaultNan);
    choose(isNan(denominator), quiet(denominator));
    choose(isNan(numerator), quiet(numerator));
    writeF64Result(inst.destination, ir.ConstructU64(result[0].Value(), result[1].Value()));
    return true;
}

}
