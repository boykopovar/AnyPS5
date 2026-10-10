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

std::uint32_t FromF32Exact(SpirvEmitterState& state, std::uint32_t value) {
    const auto u32 = TypeU32(state);
    const auto bits = Unary(state, spv::OpBitcast, u32, value);
    const auto sign = Binary(state, spv::OpBitwiseAnd, u32, bits, ConstantU32(state, 0x80000000u));
    const auto magnitude = Binary(state, spv::OpBitwiseAnd, u32, bits, ConstantU32(state, 0x7fffffffu));
    const auto subnormal = Binary(state, spv::OpULessThan, TypeBool(state), magnitude, ConstantU32(state, 0x00800000u));
    const auto scaled = Exact(state, spv::OpFMul, Unary(state, spv::OpConvertUToF, TypeF64(state), magnitude), ConstantF64(state, 0x36a0000000000000ull));
    const F64Bits normal{Binary(state, spv::OpShiftLeftLogical, u32, magnitude, ConstantU32(state, 29u)), Binary(state, spv::OpIAdd, u32, Binary(state, spv::OpShiftRightLogical, u32, magnitude, ConstantU32(state, 3u)), ConstantU32(state, 896u << 20u))};
    const F64Bits special{normal.low, Binary(state, spv::OpBitwiseOr, u32, Binary(state, spv::OpShiftRightLogical, u32, Binary(state, spv::OpBitwiseAnd, u32, magnitude, ConstantU32(state, 0x007fffffu)), ConstantU32(state, 3u)), ConstantU32(state, 0x7ff00000u))};
    const auto nonFinite = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), magnitude, ConstantU32(state, 0x7f800000u));
    const auto unsignedBits = SelectBits(state, nonFinite, special, Split(state, FromF64(state, Select(state, TypeF64(state), subnormal, scaled, ToF64(state, Join(state, normal))))));
    return ToF64(state, Join(state, {unsignedBits.low, Binary(state, spv::OpBitwiseOr, u32, unsignedBits.high, sign)}));
}

std::uint32_t RoundToPrecision(SpirvEmitterState& state, std::uint32_t value, std::uint32_t fractionBits, std::uint32_t smallestStep) {
    const auto u32 = TypeU32(state);
    const auto bits = Split(state, FromF64(state, value));
    const auto exponent = Unary(state, spv::OpBitcast, TypeI32(state), ExponentField(state, bits.high));
    const auto step = Glsl(state, TypeI32(state), GLSLstd450SMax, Binary(state, spv::OpISub, TypeI32(state), exponent, ConstantI32(state, static_cast<std::int32_t>(fractionBits))), ConstantI32(state, static_cast<std::int32_t>(smallestStep)));
    const auto sign = Binary(state, spv::OpBitwiseAnd, u32, bits.high, ConstantU32(state, 0x80000000u));
    const auto magicExponent = Binary(state, spv::OpShiftLeftLogical, u32, Unary(state, spv::OpBitcast, u32, Binary(state, spv::OpIAdd, TypeI32(state), step, ConstantI32(state, 52))), ConstantU32(state, 20u));
    const auto magic = ToF64(state, Join(state, {ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, u32, sign, magicExponent)}));
    const auto rounded = Split(state, FromF64(state, Exact(state, spv::OpFSub, Exact(state, spv::OpFAdd, value, magic), magic)));
    return ToF64(state, Join(state, {rounded.low, Binary(state, spv::OpBitwiseOr, u32, Binary(state, spv::OpBitwiseAnd, u32, rounded.high, ConstantU32(state, 0x7fffffffu)), sign)}));
}

std::uint32_t F32BitsOfRounded(SpirvEmitterState& state, std::uint32_t value) {
    const auto u32 = TypeU32(state);
    const auto bits = Split(state, FromF64(state, value));
    const auto sign = Binary(state, spv::OpBitwiseAnd, u32, bits.high, ConstantU32(state, 0x80000000u));
    const auto exponent = ExponentField(state, bits.high);
    const auto fraction = Binary(state, spv::OpBitwiseOr, u32, Binary(state, spv::OpShiftLeftLogical, u32, Binary(state, spv::OpBitwiseAnd, u32, bits.high, ConstantU32(state, 0x000fffffu)), ConstantU32(state, 3u)), Binary(state, spv::OpShiftRightLogical, u32, bits.low, ConstantU32(state, 29u)));
    const auto normal = Binary(state, spv::OpBitwiseOr, u32, Binary(state, spv::OpShiftLeftLogical, u32, Binary(state, spv::OpISub, u32, exponent, ConstantU32(state, 896u)), ConstantU32(state, 23u)), fraction);
    const auto magnitude = ToF64(state, Join(state, {bits.low, Binary(state, spv::OpBitwiseAnd, u32, bits.high, ConstantU32(state, 0x7fffffffu))}));
    const auto subnormal = Unary(state, spv::OpConvertFToU, u32, Exact(state, spv::OpFMul, magnitude, ConstantF64(state, 0x4940000000000000ull)));
    auto result = Select(state, u32, Binary(state, spv::OpULessThan, TypeBool(state), exponent, ConstantU32(state, 897u)), subnormal, normal);
    result = Select(state, u32, Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), exponent, ConstantU32(state, 1151u)), ConstantU32(state, 0x7f800000u), result);
    return Binary(state, spv::OpBitwiseOr, u32, result, sign);
}

std::uint32_t NonFinite32(SpirvEmitterState& state, std::uint32_t bits) {
    const auto exponent = Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits, ConstantU32(state, 0x7f800000u));
    return Binary(state, spv::OpIEqual, TypeBool(state), exponent, ConstantU32(state, 0x7f800000u));
}

std::uint32_t Nan32(SpirvEmitterState& state, std::uint32_t bits) {
    return Binary(state, spv::OpUGreaterThan, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), bits, ConstantU32(state, 0x7fffffffu)), ConstantU32(state, 0x7f800000u));
}

std::uint32_t FlushDenormal32(SpirvEmitterState& state, std::uint32_t value, bool flush, std::uint32_t smallestNormal) {
    if (!flush) return value;
    const auto u32 = TypeU32(state);
    const auto bits = Unary(state, spv::OpBitcast, u32, value);
    const auto magnitude = Binary(state, spv::OpBitwiseAnd, u32, bits, ConstantU32(state, 0x7fffffffu));
    const auto denormal = Binary(state, spv::OpLogicalAnd, TypeBool(state), Binary(state, spv::OpINotEqual, TypeBool(state), magnitude, ConstantU32(state, 0u)), Binary(state, spv::OpULessThan, TypeBool(state), magnitude, ConstantU32(state, smallestNormal)));
    return Unary(state, spv::OpBitcast, TypeF32(state), Select(state, u32, denormal, Binary(state, spv::OpBitwiseAnd, u32, bits, ConstantU32(state, 0x80000000u)), bits));
}

std::uint32_t Interpolate(SpirvEmitterState& state, std::uint32_t delta, std::uint32_t coordinate, std::uint32_t base, bool halfResult, std::uint32_t mode) {
    const auto u32 = TypeU32(state);
    const bool quietNans = halfResult || (mode & InterpolationQuiet) != 0u;
    const bool flush32 = (mode & InterpolationFlush32) != 0u;
    const bool flush16 = (mode & InterpolationFlush16) != 0u;
    delta = FlushDenormal32(state, delta, flush16, 0x38800000u);
    coordinate = FlushDenormal32(state, coordinate, flush32, 0x00800000u);
    base = halfResult ? FlushDenormal32(state, base, flush32, 0x00800000u) : FlushDenormal32(state, base, flush16, 0x38800000u);
    const auto product = Exact(state, spv::OpFMul, FromF32Exact(state, delta), FromF32Exact(state, coordinate));
    const auto sum = AddRoundToOdd(state, product, FromF32Exact(state, base));
    auto rounded = halfResult ? RoundToPrecision(state, sum, 10u, 999u) : RoundToPrecision(state, sum, 23u, 874u);
    if (halfResult) {
        const auto bits = Split(state, FromF64(state, rounded));
        const auto sign = Binary(state, spv::OpBitwiseAnd, u32, bits.high, ConstantU32(state, 0x80000000u));
        const auto overflow = Binary(state, spv::OpUGreaterThan, TypeBool(state), Binary(state, spv::OpBitwiseAnd, u32, bits.high, ConstantU32(state, 0x7fffffffu)), ConstantU32(state, 0x40f00000u));
        rounded = ToF64(state, Join(state, SelectBits(state, overflow, {ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, u32, sign, ConstantU32(state, 0x40f00000u))}, bits)));
    }
    const auto finite = F32BitsOfRounded(state, rounded);
    const auto inputs = std::array{Unary(state, spv::OpBitcast, u32, delta), Unary(state, spv::OpBitcast, u32, coordinate), Unary(state, spv::OpBitcast, u32, base)};
    const auto unbounded = Split(state, FromF64(state, Exact(state, spv::OpFAdd, product, FromF32Exact(state, base))));
    auto special = Select(state, u32, Classify(state, Join(state, unbounded)).nan, ConstantU32(state, 0xffc00000u), Binary(state, spv::OpBitwiseOr, u32, Binary(state, spv::OpBitwiseAnd, u32, unbounded.high, ConstantU32(state, 0x80000000u)), ConstantU32(state, 0x7f800000u)));
    auto nonFinite = NonFinite32(state, inputs[0]);
    for (const auto input : {inputs[1], inputs[2]}) nonFinite = Binary(state, spv::OpLogicalOr, TypeBool(state), nonFinite, NonFinite32(state, input));
    const auto zero = [&](std::uint32_t bits) { return Binary(state, spv::OpIEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, u32, bits, ConstantU32(state, 0x7fffffffu)), ConstantU32(state, 0u)); };
    const auto infinite = [&](std::uint32_t bits) { return Binary(state, spv::OpIEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, u32, bits, ConstantU32(state, 0x7fffffffu)), ConstantU32(state, 0x7f800000u)); };
    const auto invalidProduct = Binary(state, spv::OpLogicalOr, TypeBool(state), Binary(state, spv::OpLogicalAnd, TypeBool(state), infinite(inputs[0]), zero(inputs[1])), Binary(state, spv::OpLogicalAnd, TypeBool(state), zero(inputs[0]), infinite(inputs[1])));
    const auto quiet = [&](std::uint32_t bits) { return quietNans ? Binary(state, spv::OpBitwiseOr, u32, bits, ConstantU32(state, 0x00400000u)) : bits; };
    special = Select(state, u32, Binary(state, spv::OpLogicalAnd, TypeBool(state), Nan32(state, inputs[2]), Unary(state, spv::OpLogicalNot, TypeBool(state), invalidProduct)), quiet(inputs[2]), special);
    special = Select(state, u32, Nan32(state, inputs[1]), quiet(inputs[1]), special);
    special = Select(state, u32, Nan32(state, inputs[0]), quiet(inputs[0]), special);
    const auto result = Unary(state, spv::OpBitcast, TypeF32(state), Select(state, u32, nonFinite, special, finite));
    return halfResult ? FlushDenormal32(state, result, flush16, 0x38800000u) : FlushDenormal32(state, result, flush32, 0x00800000u);
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
    const auto both = [&](std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, spv::OpLogicalAnd, TypeBool(state), lhs, rhs); };
    const auto either = [&](std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, spv::OpLogicalOr, TypeBool(state), lhs, rhs); };
    const auto negate = [&](std::uint32_t value) { return Unary(state, spv::OpLogicalNot, TypeBool(state), value); };
    struct Normalized {
        F64Bits bits;
        std::uint32_t exponent = 0;
    };
    const auto normalize = [&](const F64Class& value, std::uint32_t bits, std::uint32_t field) {
        const auto subnormal = isField(field, 0u);
        const auto widened = Split(state, FromF64(state, Exact(state, spv::OpFMul, ToF64(state, bits), ConstantF64(state, 0x43f0000000000000ull))));
        const auto normal = SelectBits(state, subnormal, widened, value.bits);
        const auto bias = Select(state, TypeI32(state), subnormal, ConstantI32(state, 64), ConstantI32(state, 0));
        return Normalized{normal, Binary(state, spv::OpISub, TypeI32(state), signedInt(ExponentField(state, normal.high)), bias)};
    };
    const auto normalA = normalize(a, arg0, exponentA);
    const auto normalB = normalize(b, arg1, exponentB);
    const auto normalC = normalize(c, arg2, exponentC);
    const auto product = Binary(state, spv::OpISub, TypeI32(state), Binary(state, spv::OpIAdd, TypeI32(state), normalA.exponent, normalB.exponent), ConstantI32(state, 2046));
    const auto distance = Binary(state, spv::OpISub, TypeI32(state), Binary(state, spv::OpISub, TypeI32(state), normalC.exponent, ConstantI32(state, 1023)), product);
    const auto far = both(Binary(state, spv::OpSGreaterThanEqual, TypeBool(state), distance, ConstantI32(state, 60)), negate(c.zero));
    const auto tiny = Binary(state, spv::OpSLessThan, TypeBool(state), distance, ConstantI32(state, -150));
    const auto clamped = Glsl(state, TypeI32(state), GLSLstd450SClamp, distance, ConstantI32(state, -150), ConstantI32(state, 60));
    const auto sign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), c.bits.high, ConstantU32(state, 0x80000000u));
    const F64Bits tinyBits{ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, (1023u - 150u) << 20u))};
    const auto rebiased = Unary(state, spv::OpBitcast, TypeU32(state), Binary(state, spv::OpIAdd, TypeI32(state), clamped, ConstantI32(state, 1023)));
    const auto scaledC = SelectBits(state, c.zero, c.bits, SelectBits(state, tiny, tinyBits, WithExponent(state, normalC.bits, rebiased)));

    const auto exact = TwoProduct(state, ToF64(state, Join(state, WithExponent(state, normalA.bits, ConstantU32(state, 1023u)))), ToF64(state, Join(state, WithExponent(state, normalB.bits, ConstantU32(state, 1023u)))));
    const auto sum = TwoSum(state, ToF64(state, Join(state, scaledC)), exact.high);
    const auto remainder = TwoSum(state, sum.low, exact.low);
    const auto odd = AddRoundToOdd(state, sum.low, exact.low);
    const auto rounded = TwoSum(state, sum.high, odd);
    const auto residual = Exact(state, spv::OpFAdd, rounded.low, Exact(state, spv::OpFAdd, Exact(state, spv::OpFSub, remainder.high, odd), remainder.low));
    const auto productSign = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpBitwiseXor, TypeU32(state), a.bits.high, b.bits.high), ConstantU32(state, 0x80000000u));
    const auto productNegative = Binary(state, spv::OpINotEqual, TypeBool(state), productSign, ConstantU32(state, 0u));
    const auto above = either(both(far, negate(productNegative)), both(negate(far), Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), residual, ConstantF64(state, 0u))));
    const auto below = either(both(far, productNegative), both(negate(far), Binary(state, spv::OpFOrdLessThan, TypeBool(state), residual, ConstantF64(state, 0u))));

    auto value = SelectBits(state, far, normalC.bits, Split(state, FromF64(state, rounded.high)));
    const auto farTotal = Binary(state, spv::OpISub, TypeI32(state), Binary(state, spv::OpIAdd, TypeI32(state), normalC.exponent, scale), signedInt(ExponentField(state, normalC.bits.high)));
    const auto total = Select(state, TypeI32(state), far, farTotal, Binary(state, spv::OpIAdd, TypeI32(state), product, scale));
    const auto resultExponent = [&](F64Bits bits) { return Binary(state, spv::OpIAdd, TypeI32(state), signedInt(ExponentField(state, bits.high)), total); };
    const auto magnitude = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value.high, ConstantU32(state, 0x7fffffffu)), value.low);
    const auto zero = Binary(state, spv::OpIEqual, TypeBool(state), magnitude, ConstantU32(state, 0u));
    const auto unrounded = resultExponent(value);
    const auto shift = Binary(state, spv::OpISub, TypeI32(state), ConstantI32(state, 1), unrounded);
    const auto dropped = Unary(state, spv::OpBitcast, TypeU32(state), Glsl(state, TypeI32(state), GLSLstd450SClamp, shift, ConstantI32(state, 1), ConstantI32(state, 54)));
    const auto isHalf = [&](std::uint32_t word, std::uint32_t bits) {
        const auto mask = Binary(state, spv::OpShiftRightLogical, TypeU32(state), ConstantU32(state, 0xffffffffu), Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), bits));
        const auto half = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), ConstantU32(state, 1u), Binary(state, spv::OpISub, TypeU32(state), bits, ConstantU32(state, 1u)));
        return Binary(state, spv::OpIEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), word, mask), half);
    };
    const auto lowHalf = isHalf(value.low, Glsl(state, TypeU32(state), GLSLstd450UMin, dropped, ConstantU32(state, 32u)));
    const auto significandHigh = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value.high, ConstantU32(state, 0x000fffffu)), ConstantU32(state, 0x00100000u));
    const auto highBits = Binary(state, spv::OpISub, TypeU32(state), Glsl(state, TypeU32(state), GLSLstd450UMax, dropped, ConstantU32(state, 33u)), ConstantU32(state, 32u));
    const auto highHalf = both(Binary(state, spv::OpIEqual, TypeBool(state), value.low, ConstantU32(state, 0u)), isHalf(significandHigh, highBits));
    const auto wide = Binary(state, spv::OpUGreaterThan, TypeBool(state), dropped, ConstantU32(state, 32u));
    const auto onHalf = either(both(wide, highHalf), both(negate(wide), lowHalf));
    const auto inRange = both(Binary(state, spv::OpSLessThanEqual, TypeBool(state), unrounded, ConstantI32(state, 0)), Binary(state, spv::OpSLessThanEqual, TypeBool(state), shift, ConstantI32(state, 54)));
    const auto midpoint = both(both(negate(zero), inRange), both(onHalf, either(above, below)));
    const auto negative = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value.high, ConstantU32(state, 0x80000000u)), ConstantU32(state, 0u));
    const auto away = either(both(above, negate(negative)), both(below, negative));
    const auto carry = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), value.low, ConstantU32(state, 0xffffffffu)), ConstantU32(state, 1u), ConstantU32(state, 0u));
    const auto borrow = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), value.low, ConstantU32(state, 0u)), ConstantU32(state, 1u), ConstantU32(state, 0u));
    const F64Bits increased{Binary(state, spv::OpIAdd, TypeU32(state), value.low, ConstantU32(state, 1u)), Binary(state, spv::OpIAdd, TypeU32(state), value.high, carry)};
    const F64Bits decreased{Binary(state, spv::OpISub, TypeU32(state), value.low, ConstantU32(state, 1u)), Binary(state, spv::OpISub, TypeU32(state), value.high, borrow)};
    value = SelectBits(state, midpoint, SelectBits(state, away, increased, decreased), value);

    const auto exponent = resultExponent(value);
    const auto normalResult = WithExponent(state, value, Unary(state, spv::OpBitcast, TypeU32(state), Glsl(state, TypeI32(state), GLSLstd450SClamp, exponent, ConstantI32(state, 1), ConstantI32(state, 2046))));
    const auto lowest = PowerOfTwo(state, Glsl(state, TypeI32(state), GLSLstd450SMax, Binary(state, spv::OpISub, TypeI32(state), exponent, ConstantI32(state, 1)), ConstantI32(state, -60)));
    const auto subnormalResult = Split(state, FromF64(state, Exact(state, spv::OpFMul, ToF64(state, Join(state, WithExponent(state, value, ConstantU32(state, 1u)))), lowest)));
    const F64Bits infinity{ConstantU32(state, 0u), Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value.high, ConstantU32(state, 0x80000000u)), ConstantU32(state, 0x7ff00000u))};
    auto finite = SelectBits(state, Binary(state, spv::OpSGreaterThanEqual, TypeBool(state), exponent, ConstantI32(state, 1)), normalResult, subnormalResult);
    finite = SelectBits(state, Binary(state, spv::OpSGreaterThanEqual, TypeBool(state), exponent, ConstantI32(state, 2047)), infinity, finite);
    finite = SelectBits(state, zero, value, finite);

    const auto power = PowerOfTwo(state, scale);
    auto fallback = Split(state, FromF64(state, Exact(state, spv::OpFMul, Exact(state, spv::OpFAdd, Exact(state, spv::OpFMul, ToF64(state, arg0), ToF64(state, arg1)), ToF64(state, arg2)), power)));
    const auto finiteProduct = Binary(state, spv::OpLogicalAnd, TypeBool(state), Unary(state, spv::OpLogicalNot, TypeBool(state), isField(exponentA, 0x7ffu)), Unary(state, spv::OpLogicalNot, TypeBool(state), isField(exponentB, 0x7ffu)));
    fallback = SelectBits(state, Binary(state, spv::OpLogicalAnd, TypeBool(state), c.infinite, finiteProduct), c.bits, fallback);
    fallback = SelectBits(state, Classify(state, Join(state, fallback)).nan, {ConstantU32(state, 0u), ConstantU32(state, 0xfff80000u)}, fallback);
    const auto special = any({a.zero, b.zero, isField(exponentA, 0x7ffu), isField(exponentB, 0x7ffu), isField(exponentC, 0x7ffu)});

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

std::uint32_t EmitFPDot2F32F16(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    const auto u32 = TypeU32(state);
    const auto op = [&](spv::Op opcode, std::uint32_t lhs, std::uint32_t value) { return Binary(state, opcode, u32, lhs, ConstantU32(state, value)); };
    const auto test = [&](spv::Op opcode, std::uint32_t lhs, std::uint32_t value) { return Binary(state, opcode, TypeBool(state), lhs, ConstantU32(state, value)); };
    const auto logical = [&](spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, opcode, TypeBool(state), lhs, rhs); };
    const auto differ = [&](std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, spv::OpINotEqual, TypeBool(state), lhs, rhs); };
    const auto pick = [&](std::uint32_t condition, std::uint32_t trueValue, std::uint32_t falseValue) { return Select(state, u32, condition, trueValue, falseValue); };
    struct Half {
        std::uint32_t bits = 0;
        std::uint32_t sign = 0;
        std::uint32_t nan = 0;
        std::uint32_t infinite = 0;
        std::uint32_t zero = 0;
        std::uint32_t value = 0;
    };
    const auto half = [&](std::uint32_t packed, std::uint32_t shift) {
        Half result;
        result.bits = op(spv::OpBitwiseAnd, op(spv::OpShiftRightLogical, packed, shift), 0xffffu);
        const auto magnitude = op(spv::OpBitwiseAnd, result.bits, 0x7fffu);
        result.sign = op(spv::OpShiftLeftLogical, op(spv::OpBitwiseAnd, result.bits, 0x8000u), 16u);
        result.nan = test(spv::OpUGreaterThan, magnitude, 0x7c00u);
        result.infinite = test(spv::OpIEqual, magnitude, 0x7c00u);
        result.zero = test(spv::OpIEqual, magnitude, 0u);
        const auto subnormal = Exact(state, spv::OpFMul, Unary(state, spv::OpConvertUToF, TypeF64(state), magnitude), ConstantF64(state, 0x3e70000000000000ull));
        const auto normal = ToF64(state, Join(state, {ConstantU32(state, 0u), op(spv::OpIAdd, op(spv::OpShiftLeftLogical, magnitude, 10u), 1008u << 20u)}));
        const auto unsignedBits = Split(state, FromF64(state, Select(state, TypeF64(state), test(spv::OpULessThan, magnitude, 0x400u), subnormal, normal)));
        result.value = ToF64(state, Join(state, {unsignedBits.low, Binary(state, spv::OpBitwiseOr, u32, unsignedBits.high, result.sign)}));
        return result;
    };
    const auto aLow = half(arg0, 0u);
    const auto bLow = half(arg1, 0u);
    const auto aHigh = half(arg0, 16u);
    const auto bHigh = half(arg1, 16u);

    const auto magnitudeC = op(spv::OpBitwiseAnd, arg2, 0x7fffffffu);
    const auto signC = op(spv::OpBitwiseAnd, arg2, 0x80000000u);
    const auto nanC = test(spv::OpUGreaterThan, magnitudeC, 0x7f800000u);
    const auto infiniteC = test(spv::OpIEqual, magnitudeC, 0x7f800000u);
    const auto flushedC = test(spv::OpULessThan, magnitudeC, 0x00800000u);
    const auto highC = pick(flushedC, ConstantU32(state, 0u), op(spv::OpIAdd, op(spv::OpShiftRightLogical, magnitudeC, 3u), 896u << 20u));
    const auto lowC = pick(flushedC, ConstantU32(state, 0u), op(spv::OpShiftLeftLogical, magnitudeC, 29u));
    const auto valueC = ToF64(state, Join(state, {lowC, Binary(state, spv::OpBitwiseOr, u32, highC, signC)}));

    const auto productLow = Exact(state, spv::OpFMul, aLow.value, bLow.value);
    const auto productHigh = Exact(state, spv::OpFMul, aHigh.value, bHigh.value);
    const auto upper = TwoSum(state, productHigh, valueC);
    const auto total = TwoSum(state, productLow, upper.high);
    const auto sum = Split(state, FromF64(state, AddRoundToOdd(state, total.high, AddRoundToOdd(state, total.low, upper.low))));
    const auto exponent = op(spv::OpBitwiseAnd, op(spv::OpShiftRightLogical, sum.high, 20u), 0x7ffu);
    const auto mantissa = Binary(state, spv::OpBitwiseOr, u32, op(spv::OpShiftLeftLogical, op(spv::OpBitwiseAnd, sum.high, 0x000fffffu), 3u), op(spv::OpShiftRightLogical, sum.low, 29u));
    const auto base = Binary(state, spv::OpIAdd, u32, op(spv::OpShiftLeftLogical, op(spv::OpISub, exponent, 896u), 23u), mantissa);
    const auto rest = op(spv::OpBitwiseAnd, sum.low, 0x1fffffffu);
    const auto tie = logical(spv::OpLogicalAnd, test(spv::OpIEqual, rest, 0x10000000u), test(spv::OpINotEqual, op(spv::OpBitwiseAnd, base, 1u), 0u));
    const auto roundUp = logical(spv::OpLogicalOr, test(spv::OpUGreaterThan, rest, 0x10000000u), tie);
    auto result = Binary(state, spv::OpBitwiseOr, u32, op(spv::OpBitwiseAnd, sum.high, 0x80000000u), Binary(state, spv::OpIAdd, u32, base, pick(roundUp, ConstantU32(state, 1u), ConstantU32(state, 0u))));

    const auto signLow = Binary(state, spv::OpBitwiseXor, u32, aLow.sign, bLow.sign);
    const auto signHigh = Binary(state, spv::OpBitwiseXor, u32, aHigh.sign, bHigh.sign);
    const auto zeroSign = Binary(state, spv::OpBitwiseAnd, u32, Binary(state, spv::OpBitwiseAnd, u32, signLow, signHigh), signC);
    const auto zero = test(spv::OpIEqual, Binary(state, spv::OpBitwiseOr, u32, op(spv::OpBitwiseAnd, sum.high, 0x7fffffffu), sum.low), 0u);
    result = pick(zero, zeroSign, result);

    const auto notZero = [&](const Half& value) { return Unary(state, spv::OpLogicalNot, TypeBool(state), value.zero); };
    const auto infiniteProduct = [&](const Half& lhs, const Half& rhs) {
        return logical(spv::OpLogicalOr, logical(spv::OpLogicalAnd, lhs.infinite, notZero(rhs)), logical(spv::OpLogicalAnd, rhs.infinite, notZero(lhs)));
    };
    const auto invalidProduct = [&](const Half& lhs, const Half& rhs) {
        return logical(spv::OpLogicalOr, logical(spv::OpLogicalAnd, lhs.infinite, rhs.zero), logical(spv::OpLogicalAnd, rhs.infinite, lhs.zero));
    };
    const auto infiniteLow = infiniteProduct(aLow, bLow);
    const auto infiniteHigh = infiniteProduct(aHigh, bHigh);
    auto invalid = logical(spv::OpLogicalOr, invalidProduct(aLow, bLow), invalidProduct(aHigh, bHigh));
    invalid = logical(spv::OpLogicalOr, invalid, logical(spv::OpLogicalAnd, logical(spv::OpLogicalAnd, infiniteLow, infiniteHigh), differ(signLow, signHigh)));
    invalid = logical(spv::OpLogicalOr, invalid, logical(spv::OpLogicalAnd, logical(spv::OpLogicalAnd, infiniteLow, infiniteC), differ(signLow, signC)));
    invalid = logical(spv::OpLogicalOr, invalid, logical(spv::OpLogicalAnd, logical(spv::OpLogicalAnd, infiniteHigh, infiniteC), differ(signHigh, signC)));
    const auto infinite = logical(spv::OpLogicalOr, logical(spv::OpLogicalOr, infiniteLow, infiniteHigh), infiniteC);
    const auto infiniteSign = pick(infiniteLow, signLow, pick(infiniteHigh, signHigh, signC));
    result = pick(infinite, op(spv::OpBitwiseOr, infiniteSign, 0x7f800000u), result);
    result = pick(invalid, ConstantU32(state, 0xffc00000u), result);

    const auto quietHalf = [&](const Half& value) {
        return Binary(state, spv::OpBitwiseOr, u32, op(spv::OpBitwiseOr, value.sign, 0x7fc00000u), op(spv::OpShiftLeftLogical, op(spv::OpBitwiseAnd, value.bits, 0x3ffu), 13u));
    };
    result = pick(nanC, op(spv::OpBitwiseOr, arg2, 0x00400000u), result);
    result = pick(bHigh.nan, quietHalf(bHigh), result);
    result = pick(aHigh.nan, quietHalf(aHigh), result);
    result = pick(bLow.nan, quietHalf(bLow), result);
    return pick(aLow.nan, quietHalf(aLow), result);
}

std::uint32_t EmitFPInterpolateF32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, const IrValue* mode) {
    return Interpolate(state, arg0, arg1, arg2, false, mode->ImmediateU32());
}

std::uint32_t EmitFPInterpolateF16(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, const IrValue* mode) {
    return Interpolate(state, arg0, arg1, arg2, true, mode->ImmediateU32());
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
