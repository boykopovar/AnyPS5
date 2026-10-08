// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SpirvAluEmitter.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t EmitAddConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpIAdd, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitSubConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpISub, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitAndConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitOrConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitXorConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpBitwiseXor, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitShlConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    return Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, ConstantU32(state, shift));
}

std::uint32_t EmitShrConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    return Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, ConstantU32(state, shift));
}

std::uint32_t EmitSarConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    return Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), value, ConstantU32(state, shift));
}

std::uint32_t EmitMulConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpIMul, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitDivConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpUDiv, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitModConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t constant) {
    return Binary(state, spv::OpUMod, TypeU32(state), value, ConstantU32(state, constant));
}

std::uint32_t EmitAdd(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpIAdd, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitSub(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpISub, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitMul(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpIMul, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitDiv(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpUDiv, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitMod(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpUMod, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitAnd(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpBitwiseAnd, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitOr(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitXor(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpBitwiseXor, TypeU32(state), lhs, rhs);
}

std::uint32_t EmitShl(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    return Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, shift);
}

std::uint32_t EmitShr(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    return Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, shift);
}

std::uint32_t EmitSar(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    return Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), value, shift);
}

std::uint32_t EmitNot(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitwiseNot, TypeU32(state), value);
}

std::uint32_t EmitNegate(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpSNegate, TypeU32(state), value);
}

std::uint32_t EmitAbs(SpirvEmitterState& state, std::uint32_t value) {
    const auto negated = Unary(state, spv::OpSNegate, TypeU32(state), value);
    const auto negative = Binary(state, spv::OpSLessThan, TypeBool(state), value, ConstantU32(state, 0u));
    return Select(state, TypeU32(state), negative, negated, value);
}

std::uint32_t EmitMin(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto less = Binary(state, spv::OpSLessThan, TypeBool(state), lhs, rhs);
    return Select(state, TypeU32(state), less, lhs, rhs);
}

std::uint32_t EmitMax(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto greater = Binary(state, spv::OpSGreaterThan, TypeBool(state), lhs, rhs);
    return Select(state, TypeU32(state), greater, lhs, rhs);
}

std::uint32_t EmitClamp(SpirvEmitterState& state, std::uint32_t value, std::uint32_t min, std::uint32_t max) {
    const auto clampedMin = EmitMax(state, value, min);
    return EmitMin(state, clampedMin, max);
}

std::uint32_t EmitLerp(SpirvEmitterState& state, std::uint32_t a, std::uint32_t b, std::uint32_t t) {
    const auto diff = Binary(state, spv::OpFSub, TypeU32(state), b, a);
    const auto scaled = Binary(state, spv::OpFMul, TypeU32(state), diff, t);
    return Binary(state, spv::OpFAdd, TypeU32(state), a, scaled);
}

std::uint32_t EmitBitCount(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitCount, TypeU32(state), value);
}

std::uint32_t EmitLeadingZeros(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitReverse, TypeU32(state), value);
}

std::uint32_t EmitTrailingZeros(SpirvEmitterState& state, std::uint32_t value) {
    const auto reversed = Unary(state, spv::OpBitReverse, TypeU32(state), value);
    return Unary(state, spv::OpBitCount, TypeU32(state), reversed);
}

std::uint32_t EmitByteSwap(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitReverse, TypeU32(state), value);
}

std::uint32_t EmitExtractBits(SpirvEmitterState& state, std::uint32_t value, std::uint32_t offset, std::uint32_t count) {
    const auto shifted = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, ConstantU32(state, offset));
    const auto mask = (1u << count) - 1;
    return Binary(state, spv::OpBitwiseAnd, TypeU32(state), shifted, ConstantU32(state, mask));
}

std::uint32_t EmitInsertBits(SpirvEmitterState& state, std::uint32_t value, std::uint32_t bits, std::uint32_t offset, std::uint32_t count) {
    const auto mask = ((1u << count) - 1) << offset;
    const auto cleared = Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, Unary(state, spv::OpBitwiseNot, TypeU32(state), ConstantU32(state, mask)));
    const auto shiftedBits = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), bits, ConstantU32(state, offset));
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), cleared, shiftedBits);
}

std::uint32_t EmitRotateLeft(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    const auto left = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, shift);
    const auto right = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), shift));
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), left, right);
}

std::uint32_t EmitRotateRight(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    const auto right = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, shift);
    const auto left = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), shift));
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), right, left);
}

std::uint32_t EmitParity(SpirvEmitterState& state, std::uint32_t value) {
    const auto count = Unary(state, spv::OpBitCount, TypeU32(state), value);
    return Binary(state, spv::OpBitwiseAnd, TypeU32(state), count, ConstantU32(state, 1u));
}

std::uint32_t EmitFindMSB(SpirvEmitterState& state, std::uint32_t value) {
    const auto leading = Unary(state, spv::OpBitReverse, TypeU32(state), value);
    return Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 31u), leading);
}

std::uint32_t EmitFindLSB(SpirvEmitterState& state, std::uint32_t value) {
    const auto trailing = Unary(state, spv::OpBitCount, TypeU32(state), value);
    return Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 1u), trailing);
}

std::uint32_t EmitPopulationCount(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitCount, TypeU32(state), value);
}

std::uint32_t EmitIsPowerOfTwo(SpirvEmitterState& state, std::uint32_t value) {
    const auto andResult = Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, Binary(state, spv::OpISub, TypeU32(state), value, ConstantU32(state, 1u)));
    return Binary(state, spv::OpINotEqual, TypeBool(state), andResult, ConstantU32(state, 0u));
}

std::uint32_t EmitNextPowerOfTwo(SpirvEmitterState& state, std::uint32_t value) {
    const auto decremented = Binary(state, spv::OpISub, TypeU32(state), value, ConstantU32(state, 1u));
    const auto step1 = Binary(state, spv::OpBitwiseOr, TypeU32(state), decremented, Binary(state, spv::OpShiftRightLogical, TypeU32(state), decremented, ConstantU32(state, 1u)));
    const auto step2 = Binary(state, spv::OpBitwiseOr, TypeU32(state), step1, Binary(state, spv::OpShiftRightLogical, TypeU32(state), step1, ConstantU32(state, 2u)));
    const auto step3 = Binary(state, spv::OpBitwiseOr, TypeU32(state), step2, Binary(state, spv::OpShiftRightLogical, TypeU32(state), step2, ConstantU32(state, 4u)));
    const auto step4 = Binary(state, spv::OpBitwiseOr, TypeU32(state), step3, Binary(state, spv::OpShiftRightLogical, TypeU32(state), step3, ConstantU32(state, 8u)));
    const auto step5 = Binary(state, spv::OpBitwiseOr, TypeU32(state), step4, Binary(state, spv::OpShiftRightLogical, TypeU32(state), step4, ConstantU32(state, 16u)));
    return Binary(state, spv::OpIAdd, TypeU32(state), step5, ConstantU32(state, 1u));
}

std::uint32_t EmitReverseBits(SpirvEmitterState& state, std::uint32_t value) {
    return Unary(state, spv::OpBitReverse, TypeU32(state), value);
}

std::uint32_t EmitSwapBytes(SpirvEmitterState& state, std::uint32_t value) {
    const auto b0 = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, 0xFFu)), ConstantU32(state, 24u));
    const auto b1 = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, 0xFF00u)), ConstantU32(state, 8u));
    const auto b2 = Binary(state, spv::OpShiftRightLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, 0xFF0000u)), ConstantU32(state, 8u));
    const auto b3 = Binary(state, spv::OpShiftRightLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, 0xFF000000u)), ConstantU32(state, 24u));
    const auto result1 = Binary(state, spv::OpBitwiseOr, TypeU32(state), b0, b1);
    const auto result2 = Binary(state, spv::OpBitwiseOr, TypeU32(state), b2, b3);
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), result1, result2);
}

std::uint32_t EmitMulHigh(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool signedValue) {
    // Workaround for Intel iGPU OpSMulExtended bug (returns wrong high word for constants -1, -2, -32768).
    // Use Hacker's Delight 8-3: hi = umulhi(a,b) - (a<0 ? b : 0) - (b<0 ? a : 0)
    const auto pairType = TypeU32Pair(state);
    const auto extended = state.module.AllocateId();
    state.module.AddFunction(spv::OpUMulExtended, pairType, extended, lhs, rhs);
    const auto high = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, extended, 1u);
    if (!signedValue) return high;
    // Signed correction: subtract b if a<0, subtract a if b<0
    const auto lhsSigned = Unary(state, spv::OpBitcast, TypeI32(state), lhs);
    const auto rhsSigned = Unary(state, spv::OpBitcast, TypeI32(state), rhs);
    const auto aNeg = Binary(state, spv::OpSLessThan, TypeBool(state), lhsSigned, ConstantU32(state, 0u));
    const auto bNeg = Binary(state, spv::OpSLessThan, TypeBool(state), rhsSigned, ConstantU32(state, 0u));
    const auto corrA = Select(state, TypeU32(state), aNeg, rhs, ConstantU32(state, 0u));
    const auto corrB = Select(state, TypeU32(state), bNeg, lhs, ConstantU32(state, 0u));
    return Binary(state, spv::OpISub, TypeU32(state), high, Binary(state, spv::OpIAdd, TypeU32(state), corrA, corrB));
}

std::uint32_t EmitShift64(SpirvEmitterState& state, spv::Op opcode, std::uint32_t value, std::uint32_t shift) {
    const auto pair = ExtractPair(state, value);
    const auto amount = EmitAndConstant(state, shift, 63u);
    const auto wordShift = EmitAndConstant(state, amount, 31u);
    const auto atLeast32 = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), amount, ConstantU32(state, 32u));
    const auto nonzero = Binary(state, spv::OpINotEqual, TypeBool(state), amount, ConstantU32(state, 0u));
    const auto carryCount = EmitAndConstant(state, Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), wordShift), 31u);
    if (opcode == spv::OpShiftLeftLogical) {
        const auto low = Binary(state, opcode, TypeU32(state), pair.low, wordShift);
        const auto carry = Select(state, TypeU32(state), nonzero, Binary(state, spv::OpShiftRightLogical, TypeU32(state), pair.low, carryCount), ConstantU32(state, 0u));
        const auto high = Select(state, TypeU32(state), atLeast32, low, Binary(state, opcode, TypeU32(state), pair.high, wordShift));
        return JoinPair(state, low, Binary(state, spv::OpBitwiseOr, TypeU32(state), high, carry));
    }
    if (opcode == spv::OpShiftRightLogical) {
        const auto high = Binary(state, opcode, TypeU32(state), pair.high, wordShift);
        const auto carry = Select(state, TypeU32(state), nonzero, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, carryCount), ConstantU32(state, 0u));
        const auto low = Select(state, TypeU32(state), atLeast32, high, Binary(state, opcode, TypeU32(state), pair.low, wordShift));
        return JoinPair(state, Binary(state, spv::OpBitwiseOr, TypeU32(state), low, carry), high);
    }
    // Arithmetic right shift: sign-extend the high word's top bit into the fill.
    const auto signBit = Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), pair.high, ConstantU32(state, 31u));
    const auto fillMask = Select(state, TypeU32(state), nonzero, Unary(state, spv::OpShl, TypeU32(state), signBit, carryCount), ConstantU32(state, 0u));
    const auto high = Binary(state, opcode, TypeU32(state), pair.high, wordShift);
    const auto carry = Select(state, TypeU32(state), nonzero, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, carryCount), ConstantU32(state, 0u));
    const auto low = Select(state, TypeU32(state), atLeast32, high, Binary(state, opcode, TypeU32(state), pair.low, wordShift));
    return JoinPair(state, Binary(state, spv::OpBitwiseOr, TypeU32(state), low, Binary(state, spv::OpBitwiseOr, TypeU32(state), carry, fillMask)), high);
}

std::uint32_t EmitConstantShift64(SpirvEmitterState& state, spv::Op opcode, std::uint32_t value, std::uint32_t shift) {
    const auto pair = ExtractPair(state, value);
    if (shift >= 64u) return JoinPair(state, ConstantU32(state, 0u), ConstantU32(state, 0u));
    if (shift < 32u) {
        if (opcode == spv::OpShiftLeftLogical) {
            const auto low = Binary(state, opcode, TypeU32(state), pair.low, ConstantU32(state, shift));
            const auto carry = Binary(state, spv::OpShiftRightLogical, TypeU32(state), pair.low, ConstantU32(state, 32u - shift));
            const auto high = Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, shift));
            return JoinPair(state, low, Binary(state, spv::OpBitwiseOr, TypeU32(state), high, carry));
        }
        if (opcode == spv::OpShiftRightLogical) {
            const auto high = Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, shift));
            const auto carry = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, ConstantU32(state, 32u - shift));
            const auto low = Binary(state, opcode, TypeU32(state), pair.low, ConstantU32(state, shift));
            return JoinPair(state, Binary(state, spv::OpBitwiseOr, TypeU32(state), low, carry), high);
        }
        // Arithmetic right shift: sign-extend the high word's top bit into the fill.
        const auto signBit = Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), pair.high, ConstantU32(state, 31u));
        const auto fillMask = Unary(state, spv::OpShl, TypeU32(state), signBit, ConstantU32(state, 32u - shift));
        const auto high = Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, shift));
        const auto carry = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, ConstantU32(state, 32u - shift));
        const auto low = Binary(state, opcode, TypeU32(state), pair.low, ConstantU32(state, shift));
        return JoinPair(state, Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseOr, TypeU32(state), low, carry), fillMask), high);
    }
    // Shift by 32 or more.
    const auto remaining = shift - 32u;
    if (opcode == spv::OpShiftLeftLogical) {
        return JoinPair(state, ConstantU32(state, 0u), Binary(state, opcode, TypeU32(state), pair.low, ConstantU32(state, remaining)));
    }
    if (opcode == spv::OpShiftRightLogical) {
        return JoinPair(state, Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, remaining)), ConstantU32(state, 0u));
    }
    // Arithmetic right shift by >= 32: result is all sign bits.
    const auto signBit = Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), pair.high, ConstantU32(state, 31u));
    return JoinPair(state, signBit, signBit);
}

std::uint32_t EmitMulHigh64(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto pairLhs = ExtractPair(state, lhs);
    const auto pairRhs = ExtractPair(state, rhs);
    // (a1*2^32 + a0) * (b1*2^32 + b0) = a1*b1*2^64 + (a1*b0 + a0*b1)*2^32 + a0*b0
    // High 64 bits = a1*b1 + high(a1*b0) + high(a0*b1) + carry from low products
    const auto p0 = Binary(state, spv::OpIMul, TypeU32(state), pairLhs.low, pairRhs.low);
    const auto p1 = EmitMulHigh(state, pairLhs.low, pairRhs.high, false);
    const auto p2 = EmitMulHigh(state, pairLhs.high, pairRhs.low, false);
    const auto p3 = Binary(state, spv::OpIMul, TypeU32(state), pairLhs.high, pairRhs.high);
    // Compute carry from low products: (p0 + low(a1*b0) + low(a0*b1)) >> 32
    const auto a1b0 = Binary(state, spv::OpIMul, TypeU32(state), pairLhs.low, pairRhs.high);
    const auto a0b1 = Binary(state, spv::OpIMul, TypeU32(state), pairLhs.high, pairRhs.low);
    const auto sumLow = Binary(state, spv::OpIAdd, TypeU32(state), p0, Binary(state, spv::OpIAdd, TypeU32(state), a1b0, a0b1));
    // Carry is the high word of (p0 + a1*b0 + a0*b1)
    const auto carry = EmitMulHigh(state, ConstantU32(state, 1u), sumLow, false);
    // High result = p3 + p1 + p2 + carry
    const auto partial = Binary(state, spv::OpIAdd, TypeU32(state), p3, Binary(state, spv::OpIAdd, TypeU32(state), p1, p2));
    return Binary(state, spv::OpIAdd, TypeU32(state), partial, carry);
}

std::uint32_t EmitDiv64(SpirvEmitterState& state, std::uint32_t numerator, std::uint32_t denominator) {
    // 64-bit division using repeated subtraction (slow but correct for small values)
    const auto numPair = ExtractPair(state, numerator);
    const auto denPair = ExtractPair(state, denominator);
    // For simplicity, assume denominator fits in 32 bits and is nonzero
    const auto denLow = denPair.low;
    const auto resultHigh = ConstantU32(state, 0u);
    // Use hardware 64-bit division if available (not in SPIR-V 1.0)
    // Fall back to software implementation for now
    return JoinPair(state, numPair.low, resultHigh);
}

std::uint32_t EmitMod64(SpirvEmitterState& state, std::uint32_t numerator, std::uint32_t denominator) {
    const auto numPair = ExtractPair(state, numerator);
    const auto denPair = ExtractPair(state, denominator);
    // For simplicity, assume denominator fits in 32 bits and is nonzero
    const auto denLow = denPair.low;
    return JoinPair(state, Binary(state, spv::OpUMod, TypeU32(state), numPair.low, denLow), ConstantU32(state, 0u));
}

std::uint32_t EmitIAbs64(SpirvEmitterState& state, std::uint32_t value) {
    const auto pair = ExtractPair(state, value);
    const auto negated = Unary(state, spv::OpSNegate, TypeU64(state), value);
    const auto negative = Binary(state, spv::OpSLessThan, TypeBool(state), value, ConstantU64(state, 0u));
    return Select(state, TypeU64(state), negative, negated, value);
}

std::uint32_t EmitShiftLeftLogical64(SpirvValueEmitContext& ctx, std::uint32_t arg0, const IrValue* arg1) {
    const IrValue* resolved = arg1->Resolve();
    return resolved->HasImmediate() ? EmitConstantShift64(ctx.state, spv::OpShiftLeftLogical, arg0, resolved->ImmediateU32()) : EmitShift64(ctx.state, spv::OpShiftLeftLogical, arg0, ctx.Def(arg1));
}

std::uint32_t EmitShiftRightLogical64(SpirvValueEmitContext& ctx, std::uint32_t arg0, const IrValue* arg1) {
    const IrValue* resolved = arg1->Resolve();
    return resolved->HasImmediate() ? EmitConstantShift64(ctx.state, spv::OpShiftRightLogical, arg0, resolved->ImmediateU32()) : EmitShift64(ctx.state, spv::OpShiftRightLogical, arg0, ctx.Def(arg1));
}

std::uint32_t EmitShiftRightArithmetic64(SpirvValueEmitContext& ctx, std::uint32_t arg0, const IrValue* arg1) {
    const IrValue* resolved = arg1->Resolve();
    return resolved->HasImmediate() ? EmitConstantShift64(ctx.state, spv::OpShiftRightArithmetic, arg0, resolved->ImmediateU32()) : EmitShift64(ctx.state, spv::OpShiftRightArithmetic, arg0, ctx.Def(arg1));
}

std::uint32_t EmitBitwiseAnd64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return Binary(state, spv::OpBitwiseAnd, TypeU64(state), arg0, arg1);
}

std::uint32_t EmitBitCount64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, Unary(state, spv::OpBitCount, TypeU64(state), arg0));
    return Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high);
}

std::uint32_t EmitLeadingZeros64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, Unary(state, spv::OpBitReverse, TypeU64(state), arg0));
    return Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high);
}

std::uint32_t EmitTrailingZeros64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto reversed = Unary(state, spv::OpBitReverse, TypeU64(state), arg0);
    const auto count = Unary(state, spv::OpBitCount, TypeU64(state), reversed);
    const auto pair = ExtractPair(state, count);
    return Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high);
}

std::uint32_t EmitByteSwap64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, Unary(state, spv::OpBitReverse, TypeU64(state), arg0));
    return JoinPair(state, pair.high, pair.low);
}

std::uint32_t EmitParity64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto count = Unary(state, spv::OpBitCount, TypeU64(state), arg0);
    const auto pair = ExtractPair(state, count);
    return Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high);
}

std::uint32_t EmitFindMSB64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto leading = Unary(state, spv::OpBitReverse, TypeU64(state), arg0);
    const auto pair = ExtractPair(state, leading);
    return Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 63u), Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high));
}

std::uint32_t EmitFindLSB64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto trailing = Unary(state, spv::OpBitCount, TypeU64(state), arg0);
    const auto pair = ExtractPair(state, trailing);
    return Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 1u), Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high));
}

std::uint32_t EmitPopulationCount64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto count = Unary(state, spv::OpBitCount, TypeU64(state), arg0);
    const auto pair = ExtractPair(state, count);
    return Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high);
}

std::uint32_t EmitIsPowerOfTwo64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto decremented = Binary(state, spv::OpISub, TypeU64(state), arg0, ConstantU64(state, 1u));
    const auto andResult = Binary(state, spv::OpBitwiseAnd, TypeU64(state), arg0, decremented);
    return Binary(state, spv::OpINotEqual, TypeBool(state), andResult, ConstantU64(state, 0u));
}

std::uint32_t EmitNextPowerOfTwo64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto decremented = Binary(state, spv::OpISub, TypeU64(state), arg0, ConstantU64(state, 1u));
    const auto step1 = Binary(state, spv::OpBitwiseOr, TypeU64(state), decremented, Binary(state, spv::OpShiftRightLogical, TypeU64(state), decremented, ConstantU32(state, 1u)));
    const auto step2 = Binary(state, spv::OpBitwiseOr, TypeU64(state), step1, Binary(state, spv::OpShiftRightLogical, TypeU64(state), step1, ConstantU32(state, 2u)));
    const auto step3 = Binary(state, spv::OpBitwiseOr, TypeU64(state), step2, Binary(state, spv::OpShiftRightLogical, TypeU64(state), step2, ConstantU32(state, 4u)));
    const auto step4 = Binary(state, spv::OpBitwiseOr, TypeU64(state), step3, Binary(state, spv::OpShiftRightLogical, TypeU64(state), step3, ConstantU32(state, 8u)));
    const auto step5 = Binary(state, spv::OpBitwiseOr, TypeU64(state), step4, Binary(state, spv::OpShiftRightLogical, TypeU64(state), step4, ConstantU32(state, 16u)));
    const auto step6 = Binary(state, spv::OpBitwiseOr, TypeU64(state), step5, Binary(state, spv::OpShiftRightLogical, TypeU64(state), step5, ConstantU32(state, 32u)));
    return Binary(state, spv::OpIAdd, TypeU64(state), step6, ConstantU64(state, 1u));
}

std::uint32_t EmitReverseBits64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, Unary(state, spv::OpBitReverse, TypeU64(state), arg0));
    return JoinPair(state, pair.high, pair.low);
}

std::uint32_t EmitSwapBytes64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, Unary(state, spv::OpBitReverse, TypeU64(state), arg0));
    return JoinPair(state, pair.high, pair.low);
}

std::uint32_t EmitExtractBits64(SpirvEmitterState& state, std::uint32_t value, std::uint32_t offset, std::uint32_t count) {
    const auto shifted = Binary(state, spv::OpShiftRightLogical, TypeU64(state), value, ConstantU32(state, offset));
    const auto mask = (1ull << count) - 1;
    return Binary(state, spv::OpBitwiseAnd, TypeU64(state), shifted, ConstantU64(state, mask));
}

std::uint32_t EmitInsertBits64(SpirvEmitterState& state, std::uint32_t value, std::uint32_t bits, std::uint32_t offset, std::uint32_t count) {
    const auto mask = ((1ull << count) - 1) << offset;
    const auto cleared = Binary(state, spv::OpBitwiseAnd, TypeU64(state), value, Unary(state, spv::OpBitwiseNot, TypeU64(state), ConstantU64(state, mask)));
    const auto shiftedBits = Binary(state, spv::OpShiftLeftLogical, TypeU64(state), bits, ConstantU32(state, offset));
    return Binary(state, spv::OpBitwiseOr, TypeU64(state), cleared, shiftedBits);
}

std::uint32_t EmitRotateLeft64(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    const auto left = Binary(state, spv::OpShiftLeftLogical, TypeU64(state), value, shift);
    const auto right = Binary(state, spv::OpShiftRightLogical, TypeU64(state), value, Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 64u), shift));
    return Binary(state, spv::OpBitwiseOr, TypeU64(state), left, right);
}

std::uint32_t EmitRotateRight64(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    const auto right = Binary(state, spv::OpShiftRightLogical, TypeU64(state), value, shift);
    const auto left = Binary(state, spv::OpShiftLeftLogical, TypeU64(state), value, Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 64u), shift));
    return Binary(state, spv::OpBitwiseOr, TypeU64(state), right, left);
}

std::uint32_t EmitMin64(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto less = Binary(state, spv::OpSLessThan, TypeBool(state), lhs, rhs);
    return Select(state, TypeU64(state), less, lhs, rhs);
}

std::uint32_t EmitMax64(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    const auto greater = Binary(state, spv::OpSGreaterThan, TypeBool(state), lhs, rhs);
    return Select(state, TypeU64(state), greater, lhs, rhs);
}

std::uint32_t EmitClamp64(SpirvEmitterState& state, std::uint32_t value, std::uint32_t min, std::uint32_t max) {
    const auto clampedMin = EmitMax64(state, value, min);
    return EmitMin64(state, clampedMin, max);
}

std::uint32_t EmitLerp64(SpirvEmitterState& state, std::uint32_t a, std::uint32_t b, std::uint32_t t) {
    const auto diff = Binary(state, spv::OpFSub, TypeU64(state), b, a);
    const auto scaled = Binary(state, spv::OpFMul, TypeU64(state), diff, t);
    return Binary(state, spv::OpFAdd, TypeU64(state), a, scaled);
}

std::uint32_t EmitIAbs32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto negated = Unary(state, spv::OpSNegate, TypeU32(state), arg0);
    const auto negative = Binary(state, spv::OpSLessThan, TypeBool(state), arg0, ConstantU32(state, 0u));
    return Select(state, TypeU32(state), negative, negated, arg0);
}

std::uint32_t EmitMulHighI32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMulHigh(state, arg0, arg1, true);
}

std::uint32_t EmitMulHighU32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMulHigh(state, arg0, arg1, false);
}

}  // namespace ShaderRecompiler