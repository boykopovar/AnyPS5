#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <initializer_list>
#include <iterator>
#include <span>

namespace ShaderRecompiler {
namespace {

struct F64Bits {
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

std::uint32_t TypeF64(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeFloat, 64u);
}

std::uint32_t ConstantF64(SpirvEmitterState& state, std::uint64_t bits) {
    return state.module.Constant(spv::OpConstant, TypeF64(state), static_cast<std::uint32_t>(bits), static_cast<std::uint32_t>(bits >> 32u));
}

std::uint32_t ToF64(SpirvEmitterState& state, std::uint32_t bits) {
    return Unary(state, spv::OpBitcast, TypeF64(state), bits);
}

std::uint32_t FromF64(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitcast, TypeU64(state), value);
}

F64Bits Split(SpirvEmitterState& state, std::uint32_t bits) {
    F64Bits result{state.module.AllocateId(), state.module.AllocateId()};
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result.low, bits, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result.high, bits, 1u);
    return result;
}

std::uint32_t Join(SpirvEmitterState& state, F64Bits bits) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU64(state), result, bits.low, bits.high);
    return result;
}

F64Bits SelectBits(SpirvEmitterState& state, std::uint32_t condition, F64Bits trueValue, F64Bits falseValue) {
    return {Select(state, TypeU32(state), condition, trueValue.low, falseValue.low), Select(state, TypeU32(state), condition, trueValue.high, falseValue.high)};
}

std::uint32_t Exact(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t lhs, std::uint32_t rhs) {
    const auto result = Binary(state, opcode, TypeF64(state), lhs, rhs);
    state.module.AddAnnotation(spv::OpDecorate, result, spv::DecorationNoContraction);
    return result;
}

template<typename... TArguments>
std::uint32_t Glsl(SpirvEmitterState& state, std::uint32_t type, std::uint32_t opcode, TArguments... arguments) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, type, result, GlslStd450(state), opcode, arguments...);
    return result;
}

struct F64Class {
    F64Bits bits;
    std::uint32_t nan = 0;
    std::uint32_t signalingNan = 0;
    std::uint32_t zero = 0;
    std::uint32_t negative = 0;
    std::uint32_t infinite = 0;
};

F64Class Classify(SpirvEmitterState& state, std::uint32_t value) {
    F64Class cls;
    cls.bits = Split(state, value);
    const auto absHigh = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x7fffffffu));
    const auto aboveInf = Binary(state, spv::OpUGreaterThan, TypeBool(state), absHigh, ConstantU32(state, 0x7ff00000u));
    const auto infHigh = Binary(state, spv::OpIEqual, TypeBool(state), absHigh, ConstantU32(state, 0x7ff00000u));
    const auto lowNonzero = Binary(state, spv::OpINotEqual, TypeBool(state), cls.bits.low, ConstantU32(state, 0u));
    cls.nan = Binary(state, spv::OpLogicalOr, TypeBool(state), aboveInf, Binary(state, spv::OpLogicalAnd, TypeBool(state), infHigh, lowNonzero));
    const auto quietBit = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x00080000u));
    const auto signaling = Binary(state, spv::OpIEqual, TypeBool(state), quietBit, ConstantU32(state, 0u));
    cls.signalingNan = Binary(state, spv::OpLogicalAnd, TypeBool(state), cls.nan, signaling);
    cls.infinite = Binary(state, spv::OpLogicalAnd, TypeBool(state), infHigh, Unary(state, spv::OpLogicalNot, TypeBool(state), lowNonzero));
    const auto magnitude = Binary(state, spv::OpBitwiseOr, TypeU32(state), absHigh, cls.bits.low);
    cls.zero = Binary(state, spv::OpIEqual, TypeBool(state), magnitude, ConstantU32(state, 0u));
    cls.negative = Binary(state, spv::OpINotEqual, TypeBool(state), absHigh, cls.bits.high);
    return cls;
}

F64Bits Quiet(SpirvEmitterState& state, F64Bits bits) {
    return {bits.low, Binary(state, spv::OpBitwiseOr, TypeU32(state), bits.high, ConstantU32(state, 0x00080000u))};
}

std::uint32_t NanResult(SpirvEmitterState& state, std::uint32_t value, std::initializer_list<std::uint32_t> inputs) {
    const auto cls = Classify(state, FromF64(state, value));
    auto result = SelectBits(state, cls.nan, {ConstantU32(state, 0u), ConstantU32(state, 0xfff80000u)}, cls.bits);
    for (auto it = std::rbegin(inputs); it != std::rend(inputs); ++it) {
        const auto input = Classify(state, *it);
        result = SelectBits(state, input.nan, Quiet(state, input.bits), result);
    }
    return Join(state, result);
}

std::uint32_t MinMax(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue) {
    const auto a = Classify(state, lhs);
    const auto b = Classify(state, rhs);
    const auto ordered = Binary(state, maxValue ? spv::OpFOrdGreaterThan : spv::OpFOrdLessThan, TypeBool(state), ToF64(state, lhs), ToF64(state, rhs));
    auto result = SelectBits(state, ordered, a.bits, b.bits);
    const auto bothZero = Binary(state, spv::OpLogicalAnd, TypeBool(state), a.zero, b.zero);
    const auto zero = maxValue ? SelectBits(state, a.negative, b.bits, a.bits) : SelectBits(state, a.negative, a.bits, b.bits);
    result = SelectBits(state, bothZero, zero, result);
    result = SelectBits(state, a.nan, b.bits, result);
    result = SelectBits(state, b.nan, a.bits, result);
    result = SelectBits(state, b.signalingNan, Quiet(state, b.bits), result);
    result = SelectBits(state, a.signalingNan, Quiet(state, a.bits), result);
    return Join(state, result);
}

std::uint32_t Trunc(SpirvEmitterState& state, std::uint32_t value) {
    const auto cls = Classify(state, value);
    const auto exponent = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), cls.bits.high, ConstantU32(state, 20u)), ConstantU32(state, 0x7ffu));
    const auto fractionBits = Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 1075u), exponent);
    const auto highShift = Binary(state, spv::OpISub, TypeU32(state), Glsl(state, TypeU32(state), GLSLstd450UMax, fractionBits, ConstantU32(state, 32u)), ConstantU32(state, 32u));
    const auto lowShift = Glsl(state, TypeU32(state), GLSLstd450UMin, fractionBits, ConstantU32(state, 31u));
    const auto wideFraction = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), fractionBits, ConstantU32(state, 32u));
    const auto lowMask = Select(state, TypeU32(state), wideFraction, ConstantU32(state, 0u), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), ConstantU32(state, 0xffffffffu), lowShift));
    const auto highMask = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), ConstantU32(state, 0xffffffffu), highShift);
    const F64Bits masked{Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.low, lowMask), Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, highMask)};
    const F64Bits signedZero{ConstantU32(state, 0u), Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x80000000u))};
    const auto belowOne = Binary(state, spv::OpULessThan, TypeBool(state), exponent, ConstantU32(state, 1023u));
    const auto integral = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), exponent, ConstantU32(state, 1075u));
    auto result = SelectBits(state, belowOne, signedZero, masked);
    result = SelectBits(state, integral, cls.bits, result);
    return ToF64(state, Join(state, SelectBits(state, cls.nan, Quiet(state, cls.bits), result)));
}

std::uint32_t Floor(SpirvEmitterState& state, std::uint32_t bits) {
    const auto truncated = Trunc(state, bits);
    const auto below = Binary(state, spv::OpFOrdLessThan, TypeBool(state), ToF64(state, bits), truncated);
    return Select(state, TypeF64(state), below, Exact(state, spv::OpFSub, truncated, ConstantF64(state, 0x3ff0000000000000ull)), truncated);
}

std::uint32_t ExponentField(SpirvEmitterState& state, std::uint32_t high) {
    return Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), high, ConstantU32(state, 20u)), ConstantU32(state, 0x7ffu));
}

F64Bits WithExponent(SpirvEmitterState& state, F64Bits bits, std::uint32_t biased) {
    const auto mantissa = Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits.high, ConstantU32(state, 0x800fffffu));
    return {bits.low, Binary(state, spv::OpBitwiseOr, TypeU32(state), mantissa, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), biased, ConstantU32(state, 20u)))};
}

std::uint32_t PowerOfTwo(SpirvEmitterState& state, std::uint32_t exponent) {
    const auto biased = Unary(state, spv::OpBitcast, TypeU32(state), Binary(state, spv::OpIAdd, TypeI32(state), exponent, ConstantI32(state, 1023)));
    return ToF64(state, Join(state, {ConstantU32(state, 0u), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), biased, ConstantU32(state, 20u))}));
}

struct F64Pair {
    std::uint32_t high = 0;
    std::uint32_t low = 0;
};

F64Pair TwoSum(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto sum = Exact(state, spv::OpFAdd, lhs, rhs);
    const auto rhsPart = Exact(state, spv::OpFSub, sum, lhs);
    const auto lhsPart = Exact(state, spv::OpFSub, sum, rhsPart);
    return {sum, Exact(state, spv::OpFAdd, Exact(state, spv::OpFSub, lhs, lhsPart), Exact(state, spv::OpFSub, rhs, rhsPart))};
}

std::uint32_t SplitHigh(SpirvEmitterState& state, std::uint32_t value) {
    const auto spread = Exact(state, spv::OpFMul, value, ConstantF64(state, 0x41a0000002000000ull));
    return Exact(state, spv::OpFSub, spread, Exact(state, spv::OpFSub, spread, value));
}

F64Pair TwoProduct(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto product = Exact(state, spv::OpFMul, lhs, rhs);
    const auto lhsHigh = SplitHigh(state, lhs);
    const auto rhsHigh = SplitHigh(state, rhs);
    const auto lhsLow = Exact(state, spv::OpFSub, lhs, lhsHigh);
    const auto rhsLow = Exact(state, spv::OpFSub, rhs, rhsHigh);
    auto error = Exact(state, spv::OpFSub, Exact(state, spv::OpFMul, lhsHigh, rhsHigh), product);
    error = Exact(state, spv::OpFAdd, error, Exact(state, spv::OpFMul, lhsHigh, rhsLow));
    error = Exact(state, spv::OpFAdd, error, Exact(state, spv::OpFMul, lhsLow, rhsHigh));
    return {product, Exact(state, spv::OpFAdd, error, Exact(state, spv::OpFMul, lhsLow, rhsLow))};
}

std::uint32_t AddRoundToOdd(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto sum = TwoSum(state, lhs, rhs);
    const auto rounded = Split(state, FromF64(state, sum.high));
    const auto error = Split(state, FromF64(state, sum.low));
    const auto inexact = Binary(state, spv::OpFUnordNotEqual, TypeBool(state), sum.low, ConstantF64(state, 0u));
    const auto signs = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpBitwiseXor, TypeU32(state), rounded.high, error.high), ConstantU32(state, 0x80000000u));
    const auto awayFromZero = Binary(state, spv::OpINotEqual, TypeBool(state), signs, ConstantU32(state, 0u));
    const auto borrow = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), rounded.low, ConstantU32(state, 0u)), ConstantU32(state, 1u), ConstantU32(state, 0u));
    const F64Bits decremented{Binary(state, spv::OpISub, TypeU32(state), rounded.low, ConstantU32(state, 1u)), Binary(state, spv::OpISub, TypeU32(state), rounded.high, borrow)};
    const auto truncated = SelectBits(state, awayFromZero, decremented, rounded);
    const F64Bits odd{Binary(state, spv::OpBitwiseOr, TypeU32(state), truncated.low, ConstantU32(state, 1u)), truncated.high};
    return ToF64(state, Join(state, SelectBits(state, inexact, odd, rounded)));
}

std::uint32_t FusedMultiplyAdd(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, std::uint32_t scale) {
    const auto a = Classify(state, arg0);
    const auto b = Classify(state, arg1);
    const auto c = Classify(state, arg2);
    const auto exponentA = ExponentField(state, a.bits.high);
    const auto exponentB = ExponentField(state, b.bits.high);
    const auto exponentC = ExponentField(state, c.bits.high);
    const auto isField = [&](std::uint32_t field, std::uint32_t value) {
        return Binary(state, spv::OpIEqual, TypeBool(state), field, ConstantU32(state, value));
    };
    const auto any = [&](std::initializer_list<std::uint32_t> conditions) {
        auto result = *conditions.begin();
        for (auto it = conditions.begin() + 1; it != conditions.end(); ++it) result = Binary(state, spv::OpLogicalOr, TypeBool(state), result, *it);
        return result;
    };

    const auto signedInt = [&](std::uint32_t value) { return Unary(state, spv::OpBitcast, TypeI32(state), value); };
    const auto product = Binary(state, spv::OpISub, TypeI32(state), signedInt(Binary(state, spv::OpIAdd, TypeU32(state), exponentA, exponentB)), ConstantI32(state, 2046));
    const auto distance = Binary(state, spv::OpISub, TypeI32(state), Binary(state, spv::OpISub, TypeI32(state), signedInt(exponentC), ConstantI32(state, 1023)), product);
    const auto far = Binary(state, spv::OpLogicalAnd, TypeBool(state), Binary(state, spv::OpSGreaterThanEqual, TypeBool(state), distance, ConstantI32(state, 60)), Unary(state, spv::OpLogicalNot, TypeBool(state), c.zero));
    const auto tiny = Binary(state, spv::OpSLessThan, TypeBool(state), distance, ConstantI32(state, -150));
    const auto clamped = Glsl(state, TypeI32(state), GLSLstd450SClamp, distance, ConstantI32(state, -150), ConstantI32(state, 60));
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), c.bits.high, ConstantU32(state, 0x80000000u));
    const F64Bits tinyBits{ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, (1023u - 150u) << 20u))};
    const auto rebiased = Unary(state, spv::OpBitcast, TypeU32(state), Binary(state, spv::OpIAdd, TypeI32(state), clamped, ConstantI32(state, 1023)));
    const auto scaledC = SelectBits(state, c.zero, c.bits, SelectBits(state, tiny, tinyBits, WithExponent(state, c.bits, rebiased)));

    const auto exact = TwoProduct(state, ToF64(state, Join(state, WithExponent(state, a.bits, ConstantU32(state, 1023u)))), ToF64(state, Join(state, WithExponent(state, b.bits, ConstantU32(state, 1023u)))));
    const auto sum = TwoSum(state, ToF64(state, Join(state, scaledC)), exact.high);
    const auto scaled = Exact(state, spv::OpFAdd, sum.high, AddRoundToOdd(state, sum.low, exact.low));
    const auto power = PowerOfTwo(state, scale);
    const auto total = Glsl(state, TypeI32(state), GLSLstd450SClamp, Binary(state, spv::OpIAdd, TypeI32(state), product, scale), ConstantI32(state, -2044), ConstantI32(state, 2046));
    const auto half = Binary(state, spv::OpSDiv, TypeI32(state), total, ConstantI32(state, 2));
    const auto rescaled = Exact(state, spv::OpFMul, Exact(state, spv::OpFMul, scaled, PowerOfTwo(state, half)), PowerOfTwo(state, Binary(state, spv::OpISub, TypeI32(state), total, half)));
    const auto farValue = Split(state, FromF64(state, Exact(state, spv::OpFMul, ToF64(state, arg2), power)));
    const auto finite = SelectBits(state, far, farValue, Split(state, FromF64(state, rescaled)));

    auto fallback = Split(state, FromF64(state, Exact(state, spv::OpFMul, Exact(state, spv::OpFAdd, Exact(state, spv::OpFMul, ToF64(state, arg0), ToF64(state, arg1)), ToF64(state, arg2)), power)));
    const auto finiteProduct = Binary(state, spv::OpLogicalAnd, TypeBool(state), Unary(state, spv::OpLogicalNot, TypeBool(state), isField(exponentA, 0x7ffu)), Unary(state, spv::OpLogicalNot, TypeBool(state), isField(exponentB, 0x7ffu)));
    fallback = SelectBits(state, Binary(state, spv::OpLogicalAnd, TypeBool(state), c.infinite, finiteProduct), c.bits, fallback);
    fallback = SelectBits(state, Classify(state, Join(state, fallback)).nan, {ConstantU32(state, 0u), ConstantU32(state, 0xfff80000u)}, fallback);
    const auto subnormalC = Binary(state, spv::OpLogicalAnd, TypeBool(state), isField(exponentC, 0u), Unary(state, spv::OpLogicalNot, TypeBool(state), c.zero));
    const auto special = any({isField(exponentA, 0u), isField(exponentB, 0u), isField(exponentA, 0x7ffu), isField(exponentB, 0x7ffu), isField(exponentC, 0x7ffu), subnormalC});

    auto result = SelectBits(state, special, fallback, finite);
    result = SelectBits(state, c.nan, Quiet(state, c.bits), result);
    result = SelectBits(state, b.nan, Quiet(state, b.bits), result);
    result = SelectBits(state, a.nan, Quiet(state, a.bits), result);
    return Join(state, result);
}

}

std::uint32_t EmitFPAdd64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return NanResult(state, Exact(state, spv::OpFAdd, ToF64(state, arg0), ToF64(state, arg1)), {arg0, arg1});
}

std::uint32_t EmitFPMul64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return NanResult(state, Exact(state, spv::OpFMul, ToF64(state, arg0), ToF64(state, arg1)), {arg0, arg1});
}

std::uint32_t EmitFPFma64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return FusedMultiplyAdd(state, arg0, arg1, arg2, ConstantI32(state, 0));
}

std::uint32_t EmitFPFmaScale64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, std::uint32_t arg3) {
    return FusedMultiplyAdd(state, arg0, arg1, arg2, Unary(state, spv::OpBitcast, TypeI32(state), arg3));
}

std::uint32_t EmitFPMin64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return MinMax(state, arg0, arg1, false);
}

std::uint32_t EmitFPMax64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return MinMax(state, arg0, arg1, true);
}

std::uint32_t EmitFPSaturate64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto value = ToF64(state, arg0);
    const auto positive = Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), value, ConstantF64(state, 0u));
    const auto belowOne = Binary(state, spv::OpFOrdLessThan, TypeBool(state), value, ConstantF64(state, 0x3ff0000000000000ull));
    const auto upper = Select(state, TypeF64(state), belowOne, value, ConstantF64(state, 0x3ff0000000000000ull));
    return FromF64(state, Select(state, TypeF64(state), positive, upper, ConstantF64(state, 0u)));
}

std::uint32_t EmitFPLdexp64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    const auto cls = Classify(state, arg0);
    const auto exponentOf = [&](std::uint32_t high) {
        return Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), high, ConstantU32(state, 20u)), ConstantU32(state, 0x7ffu));
    };
    const auto subnormal = Binary(state, spv::OpIEqual, TypeBool(state), exponentOf(cls.bits.high), ConstantU32(state, 0u));
    const auto scaled = Exact(state, spv::OpFMul, ToF64(state, arg0), ConstantF64(state, 0x43f0000000000000ull));
    const auto normal = Split(state, FromF64(state, Select(state, TypeF64(state), subnormal, scaled, ToF64(state, arg0))));
    const auto bias = Select(state, TypeU32(state), subnormal, ConstantU32(state, 1022u + 64u), ConstantU32(state, 1022u));
    const auto exponent = Glsl(state, TypeI32(state), GLSLstd450SClamp, Unary(state, spv::OpBitcast, TypeI32(state), arg1), ConstantI32(state, -2200), ConstantI32(state, 2200));
    const auto shift = Binary(state, spv::OpISub, TypeU32(state), exponentOf(normal.high), bias);
    const auto total = Glsl(state, TypeI32(state), GLSLstd450SClamp, Binary(state, spv::OpIAdd, TypeI32(state), Unary(state, spv::OpBitcast, TypeI32(state), shift), exponent), ConstantI32(state, -1100), ConstantI32(state, 1100));
    const auto mantissa = Join(state, {normal.low, Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), normal.high, ConstantU32(state, 0x800fffffu)), ConstantU32(state, 1022u << 20u))});
    const auto power = [&](std::uint32_t value) {
        const auto biased = Binary(state, spv::OpIAdd, TypeI32(state), value, ConstantI32(state, 1023));
        const auto high = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Unary(state, spv::OpBitcast, TypeU32(state), biased), ConstantU32(state, 20u));
        return ToF64(state, Join(state, {ConstantU32(state, 0u), high}));
    };
    const auto first = Glsl(state, TypeI32(state), GLSLstd450SClamp, total, ConstantI32(state, -1021), ConstantI32(state, 1023));
    const auto second = Binary(state, spv::OpISub, TypeI32(state), total, first);
    const auto exact = Exact(state, spv::OpFMul, ToF64(state, mantissa), power(first));
    const auto result = Split(state, FromF64(state, Exact(state, spv::OpFMul, exact, power(Glsl(state, TypeI32(state), GLSLstd450SMax, second, ConstantI32(state, -60))))));
    const auto special = Binary(state, spv::OpIEqual, TypeBool(state), exponentOf(cls.bits.high), ConstantU32(state, 0x7ffu));
    const auto passthrough = Binary(state, spv::OpLogicalOr, TypeBool(state), cls.zero, special);
    return Join(state, SelectBits(state, passthrough, SelectBits(state, cls.nan, Quiet(state, cls.bits), cls.bits), result));
}

std::uint32_t EmitFPRoundEven64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cls = Classify(state, arg0);
    const auto value = ToF64(state, arg0);
    const auto magnitude = Glsl(state, TypeF64(state), GLSLstd450FAbs, value);
    const auto shift = ConstantF64(state, 0x4330000000000000ull);
    const auto rounded = Split(state, FromF64(state, Exact(state, spv::OpFSub, Exact(state, spv::OpFAdd, magnitude, shift), shift)));
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x80000000u));
    const F64Bits signedRounded{rounded.low, Binary(state, spv::OpBitwiseOr, TypeU32(state), rounded.high, sign)};
    const auto integral = Binary(state, spv::OpFOrdGreaterThanEqual, TypeBool(state), magnitude, shift);
    const auto result = SelectBits(state, integral, cls.bits, signedRounded);
    return Join(state, SelectBits(state, cls.nan, Quiet(state, cls.bits), result));
}

std::uint32_t EmitFPFloor64(SpirvEmitterState& state, std::uint32_t arg0) {
    return FromF64(state, Floor(state, arg0));
}

std::uint32_t EmitFPCeil64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto truncated = Trunc(state, arg0);
    const auto above = Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), ToF64(state, arg0), truncated);
    return FromF64(state, Select(state, TypeF64(state), above, Exact(state, spv::OpFAdd, truncated, ConstantF64(state, 0x3ff0000000000000ull)), truncated));
}

std::uint32_t EmitFPTrunc64(SpirvEmitterState& state, std::uint32_t arg0) {
    return FromF64(state, Trunc(state, arg0));
}

std::uint32_t EmitFPFract64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto value = ToF64(state, arg0);
    const auto fraction = Exact(state, spv::OpFSub, value, Floor(state, arg0));
    const auto belowOne = ConstantF64(state, 0x3fefffffffffffffull);
    const auto overflow = Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), fraction, belowOne);
    return NanResult(state, Select(state, TypeF64(state), overflow, belowOne, fraction), {arg0});
}

std::uint32_t EmitFPFrexpMant64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cls = Classify(state, arg0);
    const auto split = Glsl(state, state.module.Type(spv::OpTypeStruct, TypeF64(state), TypeI32(state)), GLSLstd450FrexpStruct, ToF64(state, arg0));
    const auto mantissa = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeF64(state), mantissa, split, 0u);
    const auto exponentBits = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x7ff00000u));
    const auto special = Binary(state, spv::OpIEqual, TypeBool(state), exponentBits, ConstantU32(state, 0x7ff00000u));
    const auto passthrough = SelectBits(state, cls.nan, Quiet(state, cls.bits), cls.bits);
    return Join(state, SelectBits(state, special, passthrough, Split(state, FromF64(state, mantissa))));
}

std::uint32_t EmitFPFrexpExp64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto bits = Split(state, arg0);
    const auto split = Glsl(state, state.module.Type(spv::OpTypeStruct, TypeF64(state), TypeI32(state)), GLSLstd450FrexpStruct, ToF64(state, arg0));
    const auto exponent = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeI32(state), exponent, split, 1u);
    const auto exponentBits = Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits.high, ConstantU32(state, 0x7ff00000u));
    const auto special = Binary(state, spv::OpIEqual, TypeBool(state), exponentBits, ConstantU32(state, 0x7ff00000u));
    return Select(state, TypeU32(state), special, ConstantU32(state, 0u), Unary(state, spv::OpBitcast, TypeU32(state), exponent));
}

std::uint32_t EmitFPRcp64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cls = Classify(state, arg0);
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x80000000u));
    const auto quotient = Split(state, FromF64(state, Exact(state, spv::OpFDiv, ConstantF64(state, 0x3ff0000000000000ull), ToF64(state, arg0))));
    auto result = SelectBits(state, cls.zero, {ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, 0x7ff00000u))}, quotient);
    result = SelectBits(state, cls.infinite, {ConstantU32(state, 0u), sign}, result);
    return Join(state, SelectBits(state, cls.nan, Quiet(state, cls.bits), result));
}

std::uint32_t EmitFPRsq64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cls = Classify(state, arg0);
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x80000000u));
    const auto root = Glsl(state, TypeF64(state), GLSLstd450Sqrt, ToF64(state, arg0));
    auto result = Split(state, FromF64(state, Exact(state, spv::OpFDiv, ConstantF64(state, 0x3ff0000000000000ull), root)));
    result = SelectBits(state, cls.negative, {ConstantU32(state, 0u), ConstantU32(state, 0xfff80000u)}, result);
    result = SelectBits(state, cls.zero, {ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, 0x7ff00000u))}, result);
    result = SelectBits(state, cls.infinite, SelectBits(state, cls.negative, result, {ConstantU32(state, 0u), ConstantU32(state, 0u)}), result);
    return Join(state, SelectBits(state, cls.nan, Quiet(state, cls.bits), result));
}

std::uint32_t EmitFPSqrt64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cls = Classify(state, arg0);
    auto result = Split(state, FromF64(state, Glsl(state, TypeF64(state), GLSLstd450Sqrt, ToF64(state, arg0))));
    result = SelectBits(state, cls.negative, {ConstantU32(state, 0u), ConstantU32(state, 0xfff80000u)}, result);
    const auto passthrough = Binary(state, spv::OpLogicalOr, TypeBool(state), cls.zero, Binary(state, spv::OpLogicalAnd, TypeBool(state), cls.infinite, Unary(state, spv::OpLogicalNot, TypeBool(state), cls.negative)));
    result = SelectBits(state, passthrough, cls.bits, result);
    return Join(state, SelectBits(state, cls.nan, Quiet(state, cls.bits), result));
}

std::uint32_t EmitFPTrigPreop64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    static constexpr std::array<std::uint32_t, 40> TwoOverPi{
        0xa2f9836eu, 0x4e441529u, 0xfc2757d1u, 0xf534ddc0u, 0xdb629599u, 0x3c439041u, 0xfe5163abu, 0xdebbc561u,
        0xb7246e3au, 0x424dd2e0u, 0x06492eeau, 0x09d1921cu, 0xfe1deb1cu, 0xb129a73eu, 0xe88235f5u, 0x2ebb4484u,
        0xe99c7026u, 0xb45f7e41u, 0x3991d639u, 0x835339f4u, 0x9c845f8bu, 0xbdf9283bu, 0x1ff897ffu, 0xde05980fu,
        0xef2f118bu, 0x5a0a6d1fu, 0x6d367ecfu, 0x27cb09b7u, 0x4f463f66u, 0x9e5fea2du, 0x7527bac7u, 0xebe5f17bu,
        0x3d0739f7u, 0x8a5292eau, 0x6bfb5fb1u, 0x1f8d5d08u, 0x56033046u, 0x00000000u, 0x00000000u, 0x00000000u,
    };
    std::array<std::uint32_t, TwoOverPi.size()> words{};
    for (std::size_t index = 0; index < words.size(); ++index) words[index] = ConstantU32(state, TwoOverPi[index]);
    const auto arrayType = state.module.Type(spv::OpTypeArray, TypeU32(state), ConstantU32(state, static_cast<std::uint32_t>(words.size())));
    const auto table = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, arrayType), spv::StorageClassPrivate);
    state.module.AddFunction(spv::OpStore, table, state.module.Constant(spv::OpConstantComposite, arrayType, std::span<const std::uint32_t>(words)));
    const auto load = [&](std::uint32_t index) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassPrivate, TypeU32(state)), pointer, table, index);
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };

    const auto bits = Split(state, arg0);
    const auto exponent = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), bits.high, ConstantU32(state, 20u)), ConstantU32(state, 0x7ffu));
    const auto extra = Binary(state, spv::OpISub, TypeU32(state), Glsl(state, TypeU32(state), GLSLstd450UMax, exponent, ConstantU32(state, 1077u)), ConstantU32(state, 1077u));
    const auto segment = Binary(state, spv::OpIMul, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), arg1, ConstantU32(state, 31u)), ConstantU32(state, 53u));
    const auto shift = Glsl(state, TypeU32(state), GLSLstd450UMin, Binary(state, spv::OpIAdd, TypeU32(state), segment, extra), ConstantU32(state, 1201u));
    const auto word = Binary(state, spv::OpShiftRightLogical, TypeU32(state), shift, ConstantU32(state, 5u));
    const auto offset = Binary(state, spv::OpBitwiseAnd, TypeU32(state), shift, ConstantU32(state, 31u));
    const auto aligned = Binary(state, spv::OpIEqual, TypeBool(state), offset, ConstantU32(state, 0u));
    const auto rest = Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), offset);
    const auto funnel = [&](std::uint32_t upper, std::uint32_t lower) {
        const auto merged = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), upper, offset), Binary(state, spv::OpShiftRightLogical, TypeU32(state), lower, rest));
        return Select(state, TypeU32(state), aligned, upper, merged);
    };
    const auto first = load(word);
    const auto second = load(Binary(state, spv::OpIAdd, TypeU32(state), word, ConstantU32(state, 1u)));
    const auto third = load(Binary(state, spv::OpIAdd, TypeU32(state), word, ConstantU32(state, 2u)));
    const auto high = Unary(state, spv::OpConvertUToF, TypeF64(state), funnel(first, second));
    const auto low = Unary(state, spv::OpConvertUToF, TypeF64(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), funnel(second, third), ConstantU32(state, 11u)));
    const auto mantissa = Exact(state, spv::OpFAdd, Exact(state, spv::OpFMul, high, ConstantF64(state, 0x4140000000000000ull)), low);

    const auto large = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), exponent, ConstantU32(state, 1968u));
    const auto bias = Select(state, TypeU32(state), large, ConstantU32(state, 1023u + 600u - 53u + 128u), ConstantU32(state, 1023u + 600u - 53u));
    const auto scaleExponent = Binary(state, spv::OpISub, TypeU32(state), bias, shift);
    const auto scale = ToF64(state, Join(state, {ConstantU32(state, 0u), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), scaleExponent, ConstantU32(state, 20u))}));
    return FromF64(state, Exact(state, spv::OpFMul, Exact(state, spv::OpFMul, mantissa, scale), ConstantF64(state, 0x1a70000000000000ull)));
}

std::uint32_t EmitConvertF32F64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto converted = Unary(state, spv::OpBitcast, TypeU32(state), Unary(state, spv::OpFConvert, TypeF32(state), ToF64(state, arg0)));
    const auto cls = Classify(state, arg0);
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x80000000u));
    const auto payloadHigh = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), cls.bits.high, ConstantU32(state, 0x000fffffu)), ConstantU32(state, 3u));
    const auto payloadLow = Binary(state, spv::OpShiftRightLogical, TypeU32(state), cls.bits.low, ConstantU32(state, 29u));
    const auto nan = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, 0x7fc00000u)), Binary(state, spv::OpBitwiseOr, TypeU32(state), payloadHigh, payloadLow));
    return Unary(state, spv::OpBitcast, TypeF32(state), Select(state, TypeU32(state), cls.nan, nan, converted));
}

std::uint32_t EmitConvertF64F32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto converted = Split(state, FromF64(state, Unary(state, spv::OpFConvert, TypeF64(state), arg0)));
    const auto bits = Unary(state, spv::OpBitcast, TypeU32(state), arg0);
    const auto nan = Binary(state, spv::OpUGreaterThan, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits, ConstantU32(state, 0x7fffffffu)), ConstantU32(state, 0x7f800000u));
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits, ConstantU32(state, 0x80000000u));
    const auto mantissa = Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits, ConstantU32(state, 0x007fffffu));
    const auto high = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, 0x7ff80000u)), Binary(state, spv::OpShiftRightLogical, TypeU32(state), mantissa, ConstantU32(state, 3u)));
    const auto low = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), mantissa, ConstantU32(state, 29u));
    return Join(state, SelectBits(state, nan, {low, high}, converted));
}

std::uint32_t EmitConvertF64S32(SpirvEmitterState& state, std::uint32_t arg0) {
    return FromF64(state, Unary(state, spv::OpConvertSToF, TypeF64(state), Unary(state, spv::OpBitcast, TypeI32(state), arg0)));
}

std::uint32_t EmitConvertF64U32(SpirvEmitterState& state, std::uint32_t arg0) {
    return FromF64(state, Unary(state, spv::OpConvertUToF, TypeF64(state), arg0));
}

std::uint32_t EmitConvertS32F64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto value = ToF64(state, arg0);
    const auto low = Binary(state, spv::OpFOrdLessThanEqual, TypeBool(state), value, ConstantF64(state, 0xc1e0000000000000ull));
    const auto high = Binary(state, spv::OpFOrdGreaterThanEqual, TypeBool(state), value, ConstantF64(state, 0x41e0000000000000ull));
    const auto ordered = Binary(state, spv::OpFOrdEqual, TypeBool(state), value, value);
    const auto safe = Select(state, TypeF64(state), ordered, value, ConstantF64(state, 0u));
    const auto clamped = Trunc(state, FromF64(state, Select(state, TypeF64(state), Binary(state, spv::OpLogicalOr, TypeBool(state), low, high), ConstantF64(state, 0u), safe)));
    const auto converted = Unary(state, spv::OpBitcast, TypeU32(state), Unary(state, spv::OpConvertFToS, TypeI32(state), clamped));
    const auto saturated = Select(state, TypeU32(state), high, ConstantU32(state, 0x7fffffffu), converted);
    return Select(state, TypeU32(state), low, ConstantU32(state, 0x80000000u), saturated);
}

std::uint32_t EmitConvertU32F64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto value = ToF64(state, arg0);
    const auto low = Binary(state, spv::OpFUnordLessThanEqual, TypeBool(state), value, ConstantF64(state, 0u));
    const auto high = Binary(state, spv::OpFOrdGreaterThanEqual, TypeBool(state), value, ConstantF64(state, 0x41f0000000000000ull));
    const auto clamped = Trunc(state, FromF64(state, Select(state, TypeF64(state), Binary(state, spv::OpLogicalOr, TypeBool(state), low, high), ConstantF64(state, 0u), value)));
    const auto converted = Unary(state, spv::OpConvertFToU, TypeU32(state), clamped);
    const auto saturated = Select(state, TypeU32(state), high, ConstantU32(state, 0xffffffffu), converted);
    return Select(state, TypeU32(state), low, ConstantU32(state, 0u), saturated);
}

}
