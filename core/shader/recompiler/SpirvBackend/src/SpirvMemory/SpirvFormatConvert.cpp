#include "SpirvBackend/SpirvMemory/SpirvFormatConvert.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
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
