#include "SpirvBackend/SpirvMemory/SpirvFormatConvert.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <bit>
#include <stdexcept>
#include <string>
#include <cmath>
#include <cstdlib>
#include <SpirvBackend/SpirvEmitterHelpers.hpp>
#include <SpirvBackend/SpirvEmitterInstructions.hpp>

namespace ShaderRecompiler
{
namespace {

    [[noreturn]] void FailEmit(const std::string& reason) {
        throw std::runtime_error("SPIR-V module emission failed: " + reason);
    }

}

std::uint32_t EmitTBufferBitcastU32ToI32(SpirvEmitterState& state, std::uint32_t value) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeI32(state), result, value);
    return result;
}

bool IsSignedFormatComponent(SpirvFormatComponentType type) {
    return type == SpirvFormatComponentType::Sint || type == SpirvFormatComponentType::Snorm || type == SpirvFormatComponentType::Sscaled;
}

std::uint32_t EmitUFloatToF32Bits(SpirvEmitterState& state, std::uint32_t raw, std::uint32_t bits) {
    const std::uint32_t mantissaBits = bits == 11u ? 6u : 5u;
    const std::uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const auto mantissa = EmitBinaryU32(state, spv::OpBitwiseAnd, raw, ConstantU32(state, mantissaMask));
    const auto exponent = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, raw, ConstantU32(state, mantissaBits)), ConstantU32(state, 0x1fu));
    const auto exponent32 = EmitAddU32(state, exponent, ConstantU32(state, 127u - 15u));
    const auto exponentBits = EmitBinaryU32(state, spv::OpShiftLeftLogical, exponent32, ConstantU32(state, 23));
    const auto mantissaBits32 = EmitBinaryU32(state, spv::OpShiftLeftLogical, mantissa, ConstantU32(state, 23u - mantissaBits));
    const auto normalBits = EmitBinaryU32(state, spv::OpBitwiseOr, exponentBits, mantissaBits32);
    const auto normal = EmitBitcastU32ToF32(state, normalBits);
    const auto specialBits = EmitBinaryU32(state, spv::OpBitwiseOr, ConstantU32(state, 0x7f800000u), mantissaBits32);
    const auto special = EmitBitcastU32ToF32(state, specialBits);
    const auto mantissaF32 = state.module.AllocateId();
    const auto subnormal = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), mantissaF32, mantissa);
    state.module.AddFunction(spv::OpFMul, TypeF32(state), subnormal, mantissaF32, ConstantF32Value(state, std::ldexp(1.0f, 1 - 15 - static_cast<int>(mantissaBits))));
    const auto zeroExponent = EmitCompareU32Constant(state, spv::OpIEqual, exponent, 0);
    const auto specialExponent = EmitCompareU32Constant(state, spv::OpIEqual, exponent, 31);
    const auto finite = EmitTBufferSelectF32(state, zeroExponent, subnormal, normal);
    const auto result = EmitTBufferSelectF32(state, specialExponent, special, finite);
    return EmitBitcastF32ToU32(state, result);
}

std::uint32_t NormalizeFormatComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t raw) {
    const auto bits = info.componentBits[component];
    switch (info.type) {
    case SpirvFormatComponentType::Uint:
    case SpirvFormatComponentType::Sint:
        return raw;
    case SpirvFormatComponentType::Uscaled: {
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), value, raw);
            return EmitBitcastF32ToU32(state, value);
    }
    case SpirvFormatComponentType::Sscaled: {
            const auto signedRaw = EmitTBufferBitcastU32ToI32(state, raw);
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpConvertSToF, TypeF32(state), value, signedRaw);
            return EmitBitcastF32ToU32(state, value);
    }
    case SpirvFormatComponentType::Unorm: {
            const auto value = state.module.AllocateId();
            const auto normalized = state.module.AllocateId();
            const auto maxValue = static_cast<float>((1u << bits) - 1u);
            state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), value, raw);
            state.module.AddFunction(spv::OpFDiv, TypeF32(state), normalized, value, ConstantF32Value(state, maxValue));
            return EmitBitcastF32ToU32(state, normalized);
    }
    case SpirvFormatComponentType::Snorm: {
            const auto signedRaw = EmitTBufferBitcastU32ToI32(state, raw);
            const auto value = state.module.AllocateId();
            const auto normalized = state.module.AllocateId();
            const auto clamped = state.module.AllocateId();
            const auto maxValue = static_cast<float>((1u << (bits - 1u)) - 1u);
            state.module.AddFunction(spv::OpConvertSToF, TypeF32(state), value, signedRaw);
            state.module.AddFunction(spv::OpFDiv, TypeF32(state), normalized, value, ConstantF32Value(state, maxValue));
            state.module.AddFunction(spv::OpExtInst, TypeF32(state), clamped, GlslStd450(state), GLSLstd450FMax, normalized, ConstantF32Value(state, -1.0f));
            return EmitBitcastF32ToU32(state, clamped);
    }
    case SpirvFormatComponentType::Float:
        if (bits == 32u) {
            return raw;
        }
        if (bits == 16u) {
            return EmitBitcastF32ToU32(state, EmitF16BitsToF32(state, raw));
        }
        return EmitUFloatToF32Bits(state, raw, bits);
    default:
        FailEmit("buffer format component type is not supported");
    }
}

namespace {

std::uint32_t Ext(SpirvEmitterState& state, std::uint32_t type, std::uint32_t op, std::uint32_t lhs, std::uint32_t rhs) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, type, result, GlslStd450(state), op, lhs, rhs);
    return result;
}

std::uint32_t FindMsb(SpirvEmitterState& state, std::uint32_t value) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeU32(state), result, GlslStd450(state), GLSLstd450FindUMsb, value);
    return result;
}

std::uint32_t UnsignedGreater(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpUGreaterThan, TypeBool(state), lhs, rhs);
}

std::uint32_t SplitSign(SpirvEmitterState& state, std::uint32_t raw, std::uint32_t& magnitude) {
    const auto negative = Binary(state, spv::OpSLessThan, TypeBool(state), EmitTBufferBitcastU32ToI32(state, raw), EmitTBufferBitcastU32ToI32(state, ConstantU32(state, 0u)));
    magnitude = EmitSelectValueU32(state, negative, Unary(state, spv::OpSNegate, TypeU32(state), raw), raw);
    return EmitSelectValueU32(state, negative, ConstantU32(state, 0x8000u), ConstantU32(state, 0u));
}

std::uint32_t NormToF16Bits(SpirvEmitterState& state, std::uint32_t value, std::uint32_t maximum) {
    const std::uint32_t width = 32u - static_cast<std::uint32_t>(std::countl_zero(maximum));
    const auto zero = EmitCompareU32Constant(state, spv::OpIEqual, value, 0u);
    const auto safe = EmitSelectValueU32(state, zero, ConstantU32(state, 1u), value);
    const auto first = EmitBinaryU32(state, spv::OpISub, ConstantU32(state, 9u + width), FindMsb(state, safe));
    const auto low = Binary(state, spv::OpULessThan, TypeBool(state), EmitBinaryU32(state, spv::OpShiftLeftLogical, safe, first), ConstantU32(state, 1024u * maximum));
    const auto adjusted = EmitSelectValueU32(state, low, EmitAddU32(state, first, ConstantU32(state, 1u)), first);
    const auto shift = Ext(state, TypeU32(state), GLSLstd450UMin, adjusted, ConstantU32(state, 24u));
    const auto scaled = EmitBinaryU32(state, spv::OpShiftLeftLogical, safe, shift);
    const auto quotient = EmitBinaryU32(state, spv::OpUDiv, scaled, ConstantU32(state, maximum));
    const auto remainder = EmitBinaryU32(state, spv::OpUMod, scaled, ConstantU32(state, maximum));
    const auto up = UnsignedGreater(state, remainder, ConstantU32(state, maximum >> 1u));
    const auto rounded = EmitAddU32(state, quotient, EmitSelectValueU32(state, up, ConstantU32(state, 1u), ConstantU32(state, 0u)));
    const auto exponent = EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitBinaryU32(state, spv::OpISub, ConstantU32(state, 25u), shift), ConstantU32(state, 10u));
    const auto bits = EmitBinaryU32(state, spv::OpISub, EmitAddU32(state, exponent, rounded), ConstantU32(state, 1024u));
    return EmitSelectValueU32(state, zero, ConstantU32(state, 0u), bits);
}

std::uint32_t IntegerToF16Bits(SpirvEmitterState& state, std::uint32_t value) {
    const auto zero = EmitCompareU32Constant(state, spv::OpIEqual, value, 0u);
    const auto msb = FindMsb(state, EmitSelectValueU32(state, zero, ConstantU32(state, 1u), value));
    const auto wide = UnsignedGreater(state, msb, ConstantU32(state, 10u));
    const auto left = EmitSelectValueU32(state, wide, ConstantU32(state, 0u), EmitBinaryU32(state, spv::OpISub, ConstantU32(state, 10u), msb));
    const auto right = EmitSelectValueU32(state, wide, EmitBinaryU32(state, spv::OpISub, msb, ConstantU32(state, 10u)), ConstantU32(state, 0u));
    const auto quotient = EmitBinaryU32(state, spv::OpShiftRightLogical, EmitBinaryU32(state, spv::OpShiftLeftLogical, value, left), right);
    const auto unit = EmitBinaryU32(state, spv::OpShiftLeftLogical, ConstantU32(state, 1u), right);
    const auto remainder = EmitBinaryU32(state, spv::OpBitwiseAnd, value, EmitBinaryU32(state, spv::OpISub, unit, ConstantU32(state, 1u)));
    const auto half = EmitBinaryU32(state, spv::OpShiftRightLogical, unit, ConstantU32(state, 1u));
    const auto odd = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, quotient, 1u), 0u);
    const auto tie = EmitLogicalAndBool(state, Binary(state, spv::OpIEqual, TypeBool(state), remainder, half), odd);
    const auto up = EmitLogicalAndBool(state, wide, EmitLogicalOrBool(state, UnsignedGreater(state, remainder, half), tie));
    const auto rounded = EmitAddU32(state, quotient, EmitSelectValueU32(state, up, ConstantU32(state, 1u), ConstantU32(state, 0u)));
    const auto exponent = EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitAddU32(state, msb, ConstantU32(state, 15u)), ConstantU32(state, 10u));
    const auto bits = EmitBinaryU32(state, spv::OpISub, EmitAddU32(state, exponent, rounded), ConstantU32(state, 1024u));
    return EmitSelectValueU32(state, zero, ConstantU32(state, 0u), Ext(state, TypeU32(state), GLSLstd450UMin, bits, ConstantU32(state, 0x7bffu)));
}

std::uint32_t F32BitsToSmallFloatRtz(SpirvEmitterState& state, std::uint32_t bits, std::uint32_t mantissaBits, bool signedFormat) {
    const std::uint32_t infinity = 0x1fu << mantissaBits;
    const auto negative = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, bits, 0x80000000u), 0u);
    const auto sign = signedFormat ? EmitSelectValueU32(state, negative, ConstantU32(state, 1u << (5u + mantissaBits)), ConstantU32(state, 0u)) : ConstantU32(state, 0u);
    const auto exponent = EmitAndConstant(state, EmitBinaryU32(state, spv::OpShiftRightLogical, bits, ConstantU32(state, 23u)), 0xffu);
    const auto mantissa = EmitAndConstant(state, bits, 0x7fffffu);
    const auto top = EmitBinaryU32(state, spv::OpShiftRightLogical, mantissa, ConstantU32(state, 23u - mantissaBits));
    const auto lostPayload = EmitLogicalAndBool(state, EmitCompareU32Constant(state, spv::OpIEqual, top, 0u), EmitCompareU32Constant(state, spv::OpINotEqual, mantissa, 0u));
    const auto special = EmitOrU32(state, ConstantU32(state, infinity), EmitSelectValueU32(state, lostPayload, ConstantU32(state, 1u), top));
    const auto biased = Binary(state, spv::OpISub, TypeI32(state), EmitTBufferBitcastU32ToI32(state, exponent), EmitTBufferBitcastU32ToI32(state, ConstantU32(state, 112u)));
    const auto atLeast = [&](std::uint32_t limit) {
        return Binary(state, spv::OpSGreaterThanEqual, TypeBool(state), biased, EmitTBufferBitcastU32ToI32(state, ConstantU32(state, limit)));
    };
    const auto biasedU = Unary(state, spv::OpBitcast, TypeU32(state), biased);
    const auto normal = EmitOrU32(state, EmitBinaryU32(state, spv::OpShiftLeftLogical, biasedU, ConstantU32(state, mantissaBits)), top);
    const auto subnormalShift = EmitBinaryU32(state, spv::OpISub, ConstantU32(state, 24u - mantissaBits), biasedU);
    const auto subnormal = EmitBinaryU32(state, spv::OpShiftRightLogical, EmitOrU32(state, mantissa, ConstantU32(state, 0x800000u)), Ext(state, TypeU32(state), GLSLstd450UMin, subnormalShift, ConstantU32(state, 31u)));
    auto magnitude = EmitSelectValueU32(state, atLeast(static_cast<std::uint32_t>(-static_cast<std::int32_t>(mantissaBits))), subnormal, ConstantU32(state, 0u));
    magnitude = EmitSelectValueU32(state, atLeast(1u), normal, magnitude);
    magnitude = EmitSelectValueU32(state, atLeast(31u), ConstantU32(state, infinity - 1u), magnitude);
    const auto isSpecial = EmitCompareU32Constant(state, spv::OpIEqual, exponent, 0xffu);
    magnitude = EmitSelectValueU32(state, isSpecial, special, magnitude);
    if (!signedFormat) {
        const auto nan = EmitLogicalAndBool(state, isSpecial, EmitCompareU32Constant(state, spv::OpINotEqual, mantissa, 0u));
        magnitude = EmitSelectValueU32(state, EmitLogicalAndBool(state, negative, EmitLogicalNotBool(state, nan)), ConstantU32(state, 0u), magnitude);
    }
    return EmitOrU32(state, sign, magnitude);
}

std::uint32_t NormStoreBits(SpirvEmitterState& state, std::uint32_t bits, std::uint32_t maximum, bool signedFormat) {
    const auto value = EmitBitcastU32ToF32(state, bits);
    const auto magnitudeBits = EmitAndConstant(state, bits, 0x7fffffffu);
    const auto magnitude = EmitBitcastU32ToF32(state, magnitudeBits);
    const auto exponent = EmitAndConstant(state, EmitBinaryU32(state, spv::OpShiftRightLogical, bits, ConstantU32(state, 23u)), 0xffu);
    const auto denormal = EmitCompareU32Constant(state, spv::OpIEqual, exponent, 0u);
    const auto mantissa = EmitOrU32(state, EmitAndConstant(state, bits, 0x7fffffu), EmitSelectValueU32(state, denormal, ConstantU32(state, 0u), ConstantU32(state, 0x800000u)));
    const auto shift = EmitSelectValueU32(state, denormal, ConstantU32(state, 149u), EmitBinaryU32(state, spv::OpISub, ConstantU32(state, 150u), exponent));
    const auto extra = EmitBinaryU32(state, spv::OpISub, shift, ConstantU32(state, 24u));
    const auto product = state.module.AllocateId();
    state.module.AddFunction(spv::OpUMulExtended, state.module.Type(spv::OpTypeStruct, TypeU32(state), TypeU32(state)), product, mantissa, ConstantU32(state, maximum));
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, product, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, product, 1u);
    const auto integer = EmitOrU32(state, EmitBinaryU32(state, spv::OpShiftRightLogical, low, ConstantU32(state, 24u)), EmitBinaryU32(state, spv::OpShiftLeftLogical, high, ConstantU32(state, 8u)));
    const auto fraction = EmitAndConstant(state, low, 0xffffffu);
    const auto fixed = EmitOrU32(state, EmitBinaryU32(state, spv::OpShiftLeftLogical, integer, ConstantU32(state, 16u)), EmitBinaryU32(state, spv::OpShiftRightLogical, fraction, ConstantU32(state, 8u)));
    const auto sticky = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, fraction, 0xffu), 0u);
    const auto safeExtra = Ext(state, TypeU32(state), GLSLstd450UMin, extra, ConstantU32(state, 16u));
    const auto quotient = EmitBinaryU32(state, spv::OpShiftRightLogical, EmitBinaryU32(state, spv::OpShiftRightLogical, fixed, ConstantU32(state, 16u)), safeExtra);
    const auto remainder = EmitBinaryU32(state, spv::OpISub, fixed, EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitBinaryU32(state, spv::OpShiftLeftLogical, quotient, ConstantU32(state, 16u)), safeExtra));
    const auto half = EmitBinaryU32(state, spv::OpShiftLeftLogical, ConstantU32(state, 1u << 15u), safeExtra);
    const auto odd = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, quotient, 1u), 0u);
    const auto tie = EmitLogicalAndBool(state, Binary(state, spv::OpIEqual, TypeBool(state), remainder, half), EmitLogicalOrBool(state, sticky, odd));
    const auto up = EmitLogicalOrBool(state, UnsignedGreater(state, remainder, half), tie);
    const auto rounded = EmitAddU32(state, quotient, EmitSelectValueU32(state, up, ConstantU32(state, 1u), ConstantU32(state, 0u)));
    auto result = EmitSelectValueU32(state, UnsignedGreater(state, extra, ConstantU32(state, 16u)), ConstantU32(state, 0u), rounded);
    const auto full = Binary(state, spv::OpFOrdGreaterThanEqual, TypeBool(state), magnitude, ConstantF32Value(state, 1.0f));
    result = EmitSelectValueU32(state, full, ConstantU32(state, maximum), result);
    const auto positive = Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), signedFormat ? magnitude : value, ConstantF32Value(state, 0.0f));
    result = EmitSelectValueU32(state, positive, result, ConstantU32(state, 0u));
    if (!signedFormat) {
        return result;
    }
    const auto negative = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, bits, 0x80000000u), 0u);
    return EmitSelectValueU32(state, negative, Unary(state, spv::OpSNegate, TypeU32(state), result), result);
}

}

std::uint32_t EmitD16FormatComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t raw) {
    const auto bits = info.componentBits[component];
    if (info.type != SpirvFormatComponentType::Float && bits > 16u) {
        if (info.type == SpirvFormatComponentType::Uint) return Ext(state, TypeU32(state), GLSLstd450UMin, raw, ConstantU32(state, 0xffffu));
        if (info.type == SpirvFormatComponentType::Sint) {
            const auto clamped = Ext(state, TypeI32(state), GLSLstd450SMax, EmitTBufferBitcastU32ToI32(state, raw), EmitTBufferBitcastU32ToI32(state, ConstantU32(state, 0xffff8000u)));
            const auto limited = Ext(state, TypeI32(state), GLSLstd450SMin, clamped, EmitTBufferBitcastU32ToI32(state, ConstantU32(state, 0x7fffu)));
            return EmitAndConstant(state, Unary(state, spv::OpBitcast, TypeU32(state), limited), 0xffffu);
        }
        FailEmit("D16 conversion of a component wider than 16 bits is not supported");
    }
    std::uint32_t magnitude = raw;
    switch (info.type) {
    case SpirvFormatComponentType::Uint:
    case SpirvFormatComponentType::Sint:
        return EmitAndConstant(state, raw, 0xffffu);
    case SpirvFormatComponentType::Unorm:
        return NormToF16Bits(state, raw, (1u << bits) - 1u);
    case SpirvFormatComponentType::Snorm: {
        const auto sign = SplitSign(state, raw, magnitude);
        const std::uint32_t maximum = (1u << (bits - 1u)) - 1u;
        return EmitOrU32(state, sign, NormToF16Bits(state, Ext(state, TypeU32(state), GLSLstd450UMin, magnitude, ConstantU32(state, maximum)), maximum));
    }
    case SpirvFormatComponentType::Uscaled:
        return IntegerToF16Bits(state, raw);
    case SpirvFormatComponentType::Sscaled: {
        const auto sign = SplitSign(state, raw, magnitude);
        return EmitOrU32(state, sign, IntegerToF16Bits(state, magnitude));
    }
    case SpirvFormatComponentType::Float:
        if (bits == 16u) return EmitAndConstant(state, raw, 0xffffu);
        return F32BitsToSmallFloatRtz(state, NormalizeFormatComponent(state, info, component, raw), 10u, true);
    default:
        FailEmit("buffer format component type is not supported");
    }
}

std::uint32_t EmitFormatStoreComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t data) {
    const auto bits = info.componentBits[component];
    const std::uint32_t mask = bits >= 32u ? 0xffffffffu : (1u << bits) - 1u;
    switch (info.type) {
    case SpirvFormatComponentType::Uint:
        return bits >= 32u ? data : Ext(state, TypeU32(state), GLSLstd450UMin, data, ConstantU32(state, mask));
    case SpirvFormatComponentType::Sint: {
        if (bits >= 32u) return data;
        const auto low = EmitTBufferBitcastU32ToI32(state, ConstantU32(state, ~(mask >> 1u)));
        const auto high = EmitTBufferBitcastU32ToI32(state, ConstantU32(state, mask >> 1u));
        const auto clamped = Ext(state, TypeI32(state), GLSLstd450SMin, Ext(state, TypeI32(state), GLSLstd450SMax, EmitTBufferBitcastU32ToI32(state, data), low), high);
        return EmitAndConstant(state, Unary(state, spv::OpBitcast, TypeU32(state), clamped), mask);
    }
    case SpirvFormatComponentType::Unorm:
    case SpirvFormatComponentType::Uscaled:
        return NormStoreBits(state, data, mask, false);
    case SpirvFormatComponentType::Snorm:
    case SpirvFormatComponentType::Sscaled:
        return EmitAndConstant(state, NormStoreBits(state, data, mask >> 1u, true), mask);
    case SpirvFormatComponentType::Float:
        if (bits == 32u) return data;
        if (bits == 16u) return F32BitsToSmallFloatRtz(state, data, 10u, true);
        if (bits == 11u || bits == 10u) return F32BitsToSmallFloatRtz(state, data, bits - 5u, false);
        FailEmit("buffer float format component width is not supported for stores");
    default:
        FailEmit("buffer format component type is not supported");
    }
}

std::uint32_t EmitD16StoreComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t half) {
    switch (info.type) {
    case SpirvFormatComponentType::Uint:
        return EmitFormatStoreComponent(state, info, component, half);
    case SpirvFormatComponentType::Sint:
        return EmitFormatStoreComponent(state, info, component, Unary(state, spv::OpBitcast, TypeU32(state), Binary(state, spv::OpShiftRightArithmetic, TypeI32(state), EmitTBufferBitcastU32ToI32(state, EmitBinaryU32(state, spv::OpShiftLeftLogical, half, ConstantU32(state, 16u))), EmitTBufferBitcastU32ToI32(state, ConstantU32(state, 16u)))));
    case SpirvFormatComponentType::Float:
        if (info.componentBits[component] == 16u) return half;
        break;
    default:
        break;
    }
    const auto sign = EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitAndConstant(state, half, 0x8000u), ConstantU32(state, 16u));
    const auto exponent = EmitAndConstant(state, EmitBinaryU32(state, spv::OpShiftRightLogical, half, ConstantU32(state, 10u)), 0x1fu);
    const auto mantissa = EmitAndConstant(state, half, 0x3ffu);
    const auto normal = EmitOrU32(state, EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitAddU32(state, exponent, ConstantU32(state, 112u)), ConstantU32(state, 23u)), EmitBinaryU32(state, spv::OpShiftLeftLogical, mantissa, ConstantU32(state, 13u)));
    const auto special = EmitOrU32(state, ConstantU32(state, 0x7f800000u), EmitBinaryU32(state, spv::OpShiftLeftLogical, mantissa, ConstantU32(state, 13u)));
    const auto msb = FindMsb(state, EmitSelectValueU32(state, EmitCompareU32Constant(state, spv::OpIEqual, mantissa, 0u), ConstantU32(state, 1u), mantissa));
    const auto subnormalExponent = EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitAddU32(state, msb, ConstantU32(state, 103u)), ConstantU32(state, 23u));
    const auto subnormalMantissa = EmitAndConstant(state, EmitBinaryU32(state, spv::OpShiftLeftLogical, mantissa, EmitBinaryU32(state, spv::OpISub, ConstantU32(state, 23u), msb)), 0x7fffffu);
    const auto subnormal = EmitSelectValueU32(state, EmitCompareU32Constant(state, spv::OpIEqual, mantissa, 0u), ConstantU32(state, 0u), EmitOrU32(state, subnormalExponent, subnormalMantissa));
    auto magnitude = EmitSelectValueU32(state, EmitCompareU32Constant(state, spv::OpIEqual, exponent, 0u), subnormal, normal);
    magnitude = EmitSelectValueU32(state, EmitCompareU32Constant(state, spv::OpIEqual, exponent, 31u), special, magnitude);
    return EmitFormatStoreComponent(state, info, component, EmitOrU32(state, sign, magnitude));
}

void EmitDeviceAtomicMemoryBarrier(SpirvEmitterState& state) {
    // GCN atomics imply no fence; this device-scope barrier is the acquire side of lock/publish
    // patterns (atomic, then loads of data other waves wrote). It also makes every non-returning
    // atomic wait for its completion. Experiment switch: APS5_NO_ATOMIC_BARRIER=1 drops it.
    static const bool disabled = std::getenv("APS5_NO_ATOMIC_BARRIER") != nullptr;
    if (disabled) return;
    const auto semantics = spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsUniformMemoryMask;
    state.module.AddFunction(spv::OpMemoryBarrier, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, semantics));
}

std::uint32_t EmitFloatAtomicReplacement(SpirvEmitterState& state, std::uint32_t old, std::uint32_t source, bool maxValue) {
    struct OrderedBits {
        std::uint32_t nan;
        std::uint32_t zero;
        std::uint32_t key;
    };
    const auto classify = [&](std::uint32_t bits) {
        const auto cls = EmitClassifyF32Bits(state, bits);
        const auto negative = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, bits, 0x80000000u), 0u);
        const auto negativeKey = state.module.AllocateId();
        state.module.AddFunction(spv::OpNot, TypeU32(state), negativeKey, bits);
        const auto positiveKey = EmitBinaryU32(state, spv::OpBitwiseXor, bits, ConstantU32(state, 0x80000000u));
        return OrderedBits{cls.nan, cls.zero, EmitSelectValueU32(state, negative, negativeKey, positiveKey)};
    };
    const auto sourceClass = classify(source);
    const auto oldClass = classify(old);
    const auto unordered = EmitLogicalOrBool(state, EmitLogicalOrBool(state, sourceClass.nan, oldClass.nan), EmitLogicalAndBool(state, sourceClass.zero, oldClass.zero));
    const auto compare = state.module.AllocateId();
    state.module.AddFunction(maxValue ? spv::OpUGreaterThan : spv::OpULessThan, TypeBool(state), compare, sourceClass.key, oldClass.key);
    return EmitSelectValueU32(state, EmitLogicalAndBool(state, EmitLogicalNotBool(state, unordered), compare), source, old);
}

std::uint32_t EmitDsSwizzleTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t control) {
    if ((control & 0xc000u) == 0xc000u) {
        const std::uint32_t mask = control & 0x1fu;
        const std::uint32_t rotate = (control >> 5u) & 0x1fu;
        const std::uint32_t rotateDelta = (control & 0x400u) != 0u ? ((32u - rotate) & 0x1fu) : rotate;
        const auto lane = EmitAndConstant(state, subid, 31);
        const auto rotatedSum = EmitAddU32(state, lane, ConstantU32(state, rotateDelta));
        const auto rotated = EmitAndConstant(state, rotatedSum, 31);
        const auto kept = EmitAndConstant(state, lane, mask);
        const auto moved = EmitAndConstant(state, rotated, (~mask) & 31u);
        const auto combined = EmitOrU32(state, kept, moved);
        const auto base = EmitAndConstant(state, subid, 0xffffffe0u);
        return EmitOrU32(state, base, combined);
    }
    if ((control & 0x8000u) != 0u) {
        const auto lane2 = state.module.AllocateId();
        const auto shift = state.module.AllocateId();
        const auto perm0 = state.module.AllocateId();
        const auto perm = state.module.AllocateId();
        const auto base = state.module.AllocateId();
        const auto target = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane2, subid, ConstantU32(state, 3));
        state.module.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), shift, lane2, ConstantU32(state, 1));
        state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), perm0, ConstantU32(state, control), shift);
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), perm, perm0, ConstantU32(state, 3));
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), base, subid, ConstantU32(state, 0xfffffffcu));
        state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, base, perm);
        return target;
    }
    const auto lane = state.module.AllocateId();
    const auto masked = state.module.AllocateId();
    const auto ored = state.module.AllocateId();
    const auto xored = state.module.AllocateId();
    const auto base = state.module.AllocateId();
    const auto target = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid, ConstantU32(state, 31));
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), masked, lane, ConstantU32(state, control & 0x1fu));
    state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), ored, masked, ConstantU32(state, (control >> 5u) & 0x1fu));
    state.module.AddFunction(spv::OpBitwiseXor, TypeU32(state), xored, ored, ConstantU32(state, (control >> 10u) & 0x1fu));
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), base, subid, ConstantU32(state, 0xffffffe0u));
    state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, base, xored);
    return target;
}
}
