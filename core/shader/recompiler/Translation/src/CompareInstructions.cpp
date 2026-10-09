#include "Translation/CompareInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ShaderRecompiler {

namespace {

std::optional<std::uint32_t> constantBits(const IrValue& value) {
    if (value.HasImmediate()) return value.ImmediateU32();
    const IrOpcode opcode = value.Opcode();
    if (opcode != IrOpcode::BitwiseAnd32 && opcode != IrOpcode::BitwiseOr32 && opcode != IrOpcode::BitwiseXor32) return std::nullopt;
    const auto lhs = constantBits(*value.Argument(0));
    const auto rhs = constantBits(*value.Argument(1));
    if (!lhs.has_value() || !rhs.has_value()) return std::nullopt;
    if (opcode == IrOpcode::BitwiseAnd32) return *lhs & *rhs;
    if (opcode == IrOpcode::BitwiseOr32) return *lhs | *rhs;
    return *lhs ^ *rhs;
}

IrOpcode mirroredFloatCompare(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::FPOrdLessThan32: return IrOpcode::FPOrdGreaterThan32;
    case IrOpcode::FPUnordLessThan32: return IrOpcode::FPUnordGreaterThan32;
    case IrOpcode::FPOrdGreaterThan32: return IrOpcode::FPOrdLessThan32;
    case IrOpcode::FPUnordGreaterThan32: return IrOpcode::FPUnordLessThan32;
    case IrOpcode::FPOrdLessThanEqual32: return IrOpcode::FPOrdGreaterThanEqual32;
    case IrOpcode::FPUnordLessThanEqual32: return IrOpcode::FPUnordGreaterThanEqual32;
    case IrOpcode::FPOrdGreaterThanEqual32: return IrOpcode::FPOrdLessThanEqual32;
    case IrOpcode::FPUnordGreaterThanEqual32: return IrOpcode::FPUnordLessThanEqual32;
    case IrOpcode::FPOrdEqual32:
    case IrOpcode::FPUnordEqual32:
    case IrOpcode::FPOrdNotEqual32:
    case IrOpcode::FPUnordNotEqual32: return opcode;
    default: throw std::runtime_error("f32 compare against a constant: unexpected compare opcode");
    }
}

}

void TranslateCompareInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateCompareInstruction not implemented");
}

void TranslationContext::emitCompareResult(const RdnaInstruction& inst, IrU1 value, bool scalar, bool cmpx) {
    if (scalar) {
        ir.SetScc(value.Value());
        return;
    }
    IrValue& masked = ir.LogicalAnd(ir.GetExec(), value.Value());
    if (cmpx) {
        const std::array<IrU32, 2> mask = ballotMask(IrU1(masked));
        ir.SetExec(masked);
        ir.SetExecLo(mask[0].Value());
        ir.SetExecHi(mask[1].Value());
        return;
    }
    writeMask(inst.destination, IrU1(masked));
}

void TranslationContext::emitCompareConstant(const RdnaInstruction& inst, bool value, bool scalar, bool cmpx) {
    emitCompareResult(inst, IrU1(ir.ConstantBool(value)), scalar, cmpx);
}

void TranslationContext::emitIntegerCompare(const RdnaInstruction& inst, IrOpcode opcode, IrType type, bool scalar, bool cmpx) {
    IrValue* lhs = readOperand(sourceAt(inst, 0u), type);
    IrValue* rhs = readOperand(sourceAt(inst, 1u), type);
    emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {lhs, rhs})), scalar, cmpx);
}

void TranslationContext::emitInteger16Compare(const RdnaInstruction& inst, IrOpcode opcode, bool signedValue, bool cmpx) {
    const IrU32 lhs = readU16AsU32(sourceAt(inst, 0u), signedValue);
    const IrU32 rhs = readU16AsU32(sourceAt(inst, 1u), signedValue);
    emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {&lhs.Value(), &rhs.Value()})), false, cmpx);
}

void TranslationContext::emitFloatCompare(const RdnaInstruction& inst, IrOpcode opcode, bool half, bool cmpx, bool swap) {
    IrValue* lhs = nullptr;
    IrValue* rhs = nullptr;
    if (half) {
        lhs = &readF16AsF32(sourceAt(inst, 0u)).Value();
        rhs = &readF16AsF32(sourceAt(inst, 1u)).Value();
    } else {
        IrValue* lhsBits = readOperand(sourceAt(inst, 0u), IrType::U32);
        IrValue* rhsBits = readOperand(sourceAt(inst, 1u), IrType::U32);
        if (swap) std::swap(lhsBits, rhsBits);
        const auto lhsConstant = constantBits(*lhsBits);
        const auto rhsConstant = constantBits(*rhsBits);
        if ((f32DenormalFlush & 1u) != 0u && lhsConstant.has_value() != rhsConstant.has_value()) {
            const IrOpcode oriented = lhsConstant.has_value() ? mirroredFloatCompare(opcode) : opcode;
            emitCompareResult(inst, floatCompareWithConstant(oriented, IrU32(lhsConstant.has_value() ? *rhsBits : *lhsBits), lhsConstant.has_value() ? *lhsConstant : *rhsConstant), false, cmpx);
            return;
        }
        lhs = &ir.BitCastF32(flushF32Denormal(IrU32(*lhsBits)).Value());
        rhs = &ir.BitCastF32(flushF32Denormal(IrU32(*rhsBits)).Value());
        emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {lhs, rhs})), false, cmpx);
        return;
    }
    if (swap) std::swap(lhs, rhs);
    emitCompareResult(inst, IrU1(ir.Emit(opcode, IrType::U1, {lhs, rhs})), false, cmpx);
}

IrU1 TranslationContext::floatCompareWithConstant(IrOpcode opcode, IrU32 variableBits, std::uint32_t constant) {
    IrValue& value = ir.BitCastF32(variableBits.Value());
    const auto compare = [&](IrOpcode compareOpcode, IrValue& lhs, std::uint32_t rhs) { return IrU1(ir.Emit(compareOpcode, IrType::U1, {&lhs, &ir.BitCastF32(ir.Constant(rhs))})); };
    if ((constant & 0x7f800000u) != 0u) return compare(opcode, value, constant);
    IrValue& magnitude = ir.BitCastF32(ir.BitwiseAnd(variableBits.Value(), ir.Constant(0x7fffffffu)));
    constexpr std::uint32_t MinNormal = 0x00800000u;
    constexpr std::uint32_t NegativeMinNormal = 0x80800000u;
    switch (opcode) {
    case IrOpcode::FPOrdLessThan32: return compare(IrOpcode::FPOrdLessThanEqual32, value, NegativeMinNormal);
    case IrOpcode::FPUnordLessThan32: return compare(IrOpcode::FPUnordLessThanEqual32, value, NegativeMinNormal);
    case IrOpcode::FPOrdLessThanEqual32: return compare(IrOpcode::FPOrdLessThan32, value, MinNormal);
    case IrOpcode::FPUnordLessThanEqual32: return compare(IrOpcode::FPUnordLessThan32, value, MinNormal);
    case IrOpcode::FPOrdGreaterThan32: return compare(IrOpcode::FPOrdGreaterThanEqual32, value, MinNormal);
    case IrOpcode::FPUnordGreaterThan32: return compare(IrOpcode::FPUnordGreaterThanEqual32, value, MinNormal);
    case IrOpcode::FPOrdGreaterThanEqual32: return compare(IrOpcode::FPOrdGreaterThan32, value, NegativeMinNormal);
    case IrOpcode::FPUnordGreaterThanEqual32: return compare(IrOpcode::FPUnordGreaterThan32, value, NegativeMinNormal);
    case IrOpcode::FPOrdEqual32: return compare(IrOpcode::FPOrdLessThan32, magnitude, MinNormal);
    case IrOpcode::FPUnordEqual32: return compare(IrOpcode::FPUnordLessThan32, magnitude, MinNormal);
    case IrOpcode::FPOrdNotEqual32: return compare(IrOpcode::FPOrdGreaterThanEqual32, magnitude, MinNormal);
    case IrOpcode::FPUnordNotEqual32: return compare(IrOpcode::FPUnordGreaterThanEqual32, magnitude, MinNormal);
    default: throw std::runtime_error("f32 compare against a constant: unexpected compare opcode");
    }
}

void TranslationContext::emitInteger64Order(const RdnaInstruction& inst, bool signedValue, bool swap, bool negate, bool cmpx) {
    IrValue* lhs = readOperand(sourceAt(inst, swap ? 1u : 0u), IrType::U64);
    IrValue* rhs = readOperand(sourceAt(inst, swap ? 0u : 1u), IrType::U64);
    IrValue& less = ir.Emit(signedValue ? IrOpcode::SLessThan64 : IrOpcode::ULessThan64, IrType::U1, {lhs, rhs});
    emitCompareResult(inst, IrU1(negate ? ir.LogicalNot(less) : less), false, cmpx);
}

void TranslationContext::emitFloatOrderedCompare(const RdnaInstruction& inst, bool ordered, bool half, bool cmpx) {
    IrValue* lhs = half ? &readF16AsF32(sourceAt(inst, 0u)).Value() : readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* rhs = half ? &readF16AsF32(sourceAt(inst, 1u)).Value() : readOperand(sourceAt(inst, 1u), IrType::F32);
    IrValue& lhsNan = ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {lhs});
    IrValue& rhsNan = ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {rhs});
    IrValue& unordered = ir.LogicalOr(lhsNan, rhsNan);
    IrValue& result = ordered ? ir.LogicalNot(unordered) : unordered;
    emitCompareResult(inst, IrU1(result), false, cmpx);
}

void TranslationContext::emitFloatClassCompare(const RdnaInstruction& inst, bool cmpx) {
    IrValue* value = readOperand(sourceAt(inst, 0u), IrType::F32);
    IrValue* mask = readOperand(sourceAt(inst, 1u), IrType::U32);
    emitCompareResult(inst, IrU1(ir.Emit(IrOpcode::FPCmpClass32, IrType::U1, {value, mask})), false, cmpx);
}

IrU1 TranslationContext::float64IsNan(const std::array<IrU32, 2>& bits) {
    IrValue& magnitude = ir.ConstructU64(bits[0].Value(), ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x7fffffffu)));
    return IrU1(ir.Emit(IrOpcode::UGreaterThan64, IrType::U1, {&magnitude, &ir.ConstantU64(0x7ff0000000000000ull)}));
}

IrU64 TranslationContext::float64OrderKey(const std::array<IrU32, 2>& bits) {
    IrValue& magnitude = ir.ConstructU64(bits[0].Value(), ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x7fffffffu)));
    IrValue& signWord = ir.ShiftRightArithmetic(bits[1].Value(), ir.Constant(31u));
    IrValue& signMask = ir.ConstructU64(signWord, signWord);
    IrValue& negativeMagnitude = ir.Emit(IrOpcode::BitwiseAnd64, IrType::U64, {&magnitude, &signMask});
    IrValue& doubled = ir.Emit(IrOpcode::ShiftLeftLogical64, IrType::U64, {&negativeMagnitude, &ir.Constant(1u)});
    return IrU64(ir.Emit(IrOpcode::ISub64, IrType::U64, {&magnitude, &doubled}));
}

void TranslationContext::emitFloat64Compare(const RdnaInstruction& inst, bool less, bool equal, bool greater, bool unordered, bool cmpx) {
    const std::array<IrU32, 2> lhs = readF64Bits(sourceAt(inst, 0u));
    const std::array<IrU32, 2> rhs = readF64Bits(sourceAt(inst, 1u));
    const IrU1 lhsNan = float64IsNan(lhs);
    const IrU1 rhsNan = float64IsNan(rhs);
    IrValue& isUnordered = ir.LogicalOr(lhsNan.Value(), rhsNan.Value());
    const IrU64 lhsKey = float64OrderKey(lhs);
    const IrU64 rhsKey = float64OrderKey(rhs);
    IrValue* relation = nullptr;
    const auto include = [&](IrValue& term) {
        relation = relation == nullptr ? &term : &ir.LogicalOr(*relation, term);
    };
    if (less) {
        include(ir.Emit(IrOpcode::SLessThan64, IrType::U1, {&lhsKey.Value(), &rhsKey.Value()}));
    }
    if (equal) {
        include(ir.Emit(IrOpcode::IEqual64, IrType::U1, {&lhsKey.Value(), &rhsKey.Value()}));
    }
    if (greater) {
        include(ir.Emit(IrOpcode::SLessThan64, IrType::U1, {&rhsKey.Value(), &lhsKey.Value()}));
    }
    if (relation == nullptr) {
        relation = &ir.ConstantBool(false);
    }
    IrValue& result = unordered ? ir.LogicalOr(isUnordered, *relation) : ir.LogicalAnd(ir.LogicalNot(isUnordered), *relation);
    emitCompareResult(inst, IrU1(result), false, cmpx);
}

void TranslationContext::emitFloat64ClassCompare(const RdnaInstruction& inst, bool cmpx) {
    const RdnaOperand& maskOperand = sourceAt(inst, 1u);
    if (maskOperand.absolute || maskOperand.negate) {
        throw std::runtime_error("TranslationContext::emitFloat64ClassCompare class mask does not take source modifiers");
    }
    const std::array<IrU32, 2> bits = readF64Bits(sourceAt(inst, 0u));
    const IrU32 mask = readU32(maskOperand);
    IrValue& zero = ir.Constant(0u);
    IrValue& negative = ir.INotEqual(ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x80000000u)), zero);
    IrValue& exponent = ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x7ff00000u));
    IrValue& fraction = ir.BitwiseOr(ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x000fffffu)), bits[0].Value());
    IrValue& fractionNonZero = ir.INotEqual(fraction, zero);
    IrValue& quiet = ir.INotEqual(ir.BitwiseAnd(bits[1].Value(), ir.Constant(0x00080000u)), zero);
    IrValue& nan = ir.Select(quiet, ir.Constant(1u), zero);
    IrValue& infinity = ir.Select(negative, ir.Constant(2u), ir.Constant(9u));
    IrValue& normal = ir.Select(negative, ir.Constant(3u), ir.Constant(8u));
    IrValue& denormal = ir.Select(negative, ir.Constant(4u), ir.Constant(7u));
    IrValue& signedZero = ir.Select(negative, ir.Constant(5u), ir.Constant(6u));
    IrValue& exponentMax = ir.Select(fractionNonZero, nan, infinity);
    IrValue& exponentZero = ir.Select(fractionNonZero, denormal, signedZero);
    IrValue& finite = ir.Select(ir.IEqual(exponent, zero), exponentZero, normal);
    IrValue& index = ir.Select(ir.IEqual(exponent, ir.Constant(0x7ff00000u)), exponentMax, finite);
    IrValue& selected = ir.BitwiseAnd(ir.ShiftRightLogical(mask.Value(), index), ir.Constant(1u));
    emitCompareResult(inst, IrU1(ir.INotEqual(selected, zero)), false, cmpx);
}

void TranslateCompareInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateCompareInstruction not implemented");
}

}
