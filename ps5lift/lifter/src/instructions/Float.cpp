#include "../FunctionLifter.hpp"

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace Ps5Lift::Lifting {

llvm::Type* FunctionLifter::FloatType(const unsigned bits) {
    if (bits == 16) return _builder.getHalfTy();
    return bits == 32 ? _builder.getFloatTy() : _builder.getDoubleTy();
}

llvm::Value* FunctionLifter::AsFloats(llvm::Value* value, const unsigned element) {
    const auto bits = value->getType()->getIntegerBitWidth();
    if (bits == element) return _builder.CreateBitCast(value, FloatType(element));
    return _builder.CreateBitCast(value, llvm::FixedVectorType::get(FloatType(element), bits / element));
}

llvm::Value* FunctionLifter::FloatBits(llvm::Value* value) {
    return _builder.CreateBitCast(value, Int(value->getType()->getPrimitiveSizeInBits().getFixedValue()));
}

llvm::Type* FunctionLifter::IntegerLike(llvm::Type* type) {
    const auto element = type->getScalarSizeInBits();
    if (auto* vector = llvm::dyn_cast<llvm::FixedVectorType>(type)) return llvm::FixedVectorType::get(Int(element), vector->getNumElements());
    return Int(element);
}

llvm::Value* FunctionLifter::Quiet(llvm::Value* value) {
    const auto element = value->getType()->getScalarSizeInBits();
    auto* integerType = IntegerLike(value->getType());
    auto* bit = llvm::ConstantInt::get(integerType, std::uint64_t{1} << (element == 32 ? 22 : element == 64 ? 51 : 9));
    return _builder.CreateBitCast(_builder.CreateOr(_builder.CreateBitCast(value, integerType), bit), value->getType());
}

llvm::Value* FunctionLifter::DefaultNan(llvm::Type* type) {
    const auto element = type->getScalarSizeInBits();
    const auto bits = element == 32 ? std::uint64_t{0xffc00000} : element == 64 ? std::uint64_t{0xfff8000000000000} : std::uint64_t{0xfe00};
    return _builder.CreateBitCast(llvm::ConstantInt::get(IntegerLike(type), bits), type);
}

llvm::Value* FunctionLifter::IsNan(llvm::Value* value) {
    return _builder.CreateFCmpUNO(value, value);
}

bool FunctionLifter::ExactFloat() const {
    return _module.Options().Float == FloatModel::Exact;
}

llvm::Value* FunctionLifter::NanResult(llvm::Value* result, std::initializer_list<llvm::Value*> sources) {
    if (!ExactFloat()) return result;
    result = _builder.CreateUnaryIntrinsic(llvm::Intrinsic::arithmetic_fence, result);
    llvm::Value* value = _builder.CreateSelect(IsNan(result), DefaultNan(result->getType()), result);
    std::vector<llvm::Value*> ordered(sources);
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) value = _builder.CreateSelect(IsNan(*it), Quiet(*it), value);
    return value;
}

llvm::Value* FunctionLifter::DenormalsAreZero(llvm::Value* value) {
    if (!ExactFloat()) return value;
    const auto element = value->getType()->getScalarSizeInBits();
    auto* integerType = IntegerLike(value->getType());
    auto* bits = _builder.CreateBitCast(value, integerType);
    const auto mantissa = element == 32 ? std::uint64_t{0x7fffff} : std::uint64_t{0xfffffffffffff};
    const auto exponent = element == 32 ? std::uint64_t{0x7f800000} : std::uint64_t{0x7ff0000000000000};
    auto* zero = llvm::Constant::getNullValue(integerType);
    auto* denormal = _builder.CreateAnd(_builder.CreateICmpEQ(_builder.CreateAnd(bits, llvm::ConstantInt::get(integerType, exponent)), zero), _builder.CreateICmpNE(_builder.CreateAnd(bits, llvm::ConstantInt::get(integerType, mantissa)), zero));
    auto* mxcsr = StateLoad(_builder.getInt32Ty(), offsetof(Ps5LiftState, Mxcsr));
    llvm::Value* enabled = _builder.CreateICmpNE(_builder.CreateAnd(mxcsr, 0x40), _builder.getInt32(0));
    if (value->getType()->isVectorTy()) enabled = _builder.CreateVectorSplat(llvm::cast<llvm::FixedVectorType>(value->getType())->getNumElements(), enabled);
    auto* signOnly = _builder.CreateAnd(bits, llvm::ConstantInt::get(integerType, std::uint64_t{1} << (element - 1)));
    return _builder.CreateBitCast(_builder.CreateSelect(_builder.CreateAnd(enabled, denormal), signOnly, bits), value->getType());
}

llvm::Value* FunctionLifter::FloatConstant(llvm::Type* type, const double value) {
    return llvm::ConstantFP::get(type, value);
}

llvm::Value* FunctionLifter::Arithmetic(const FloatKind kind, llvm::Value* a, llvm::Value* b) {
    switch (kind) {
    case FloatKind::Add: return NanResult(_builder.CreateFAdd(a, b), {a, b});
    case FloatKind::Sub: return NanResult(_builder.CreateFSub(a, b), {a, b});
    case FloatKind::Mul: return NanResult(_builder.CreateFMul(a, b), {a, b});
    case FloatKind::Div: return NanResult(_builder.CreateFDiv(a, b), {a, b});
    case FloatKind::Min:
    case FloatKind::Max: {
        auto* left = DenormalsAreZero(a);
        auto* right = DenormalsAreZero(b);
        auto* compare = kind == FloatKind::Min ? _builder.CreateFCmpOLT(left, right) : _builder.CreateFCmpOGT(left, right);
        return _builder.CreateSelect(compare, left, right);
    }
    case FloatKind::Sqrt: return NanResult(_builder.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, b), {b});
    case FloatKind::Reciprocal: return NanResult(_builder.CreateFDiv(FloatConstant(b->getType(), 1.0), b), {b});
    case FloatKind::ReciprocalSqrt: return NanResult(_builder.CreateFDiv(FloatConstant(b->getType(), 1.0), _builder.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, b)), {b});
    default: throw std::logic_error("not an arithmetic kind");
    }
}

llvm::Value* FunctionLifter::RoundValue(llvm::Value* value, const std::uint64_t control) {
    llvm::Intrinsic::ID intrinsic = llvm::Intrinsic::rint;
    if ((control & 4) == 0) {
        switch (control & 3) {
        case 0: intrinsic = llvm::Intrinsic::roundeven; break;
        case 1: intrinsic = llvm::Intrinsic::floor; break;
        case 2: intrinsic = llvm::Intrinsic::ceil; break;
        default: intrinsic = llvm::Intrinsic::trunc; break;
        }
    }
    return NanResult(_builder.CreateUnaryIntrinsic(intrinsic, value), {value});
}

llvm::Value* FunctionLifter::CompareMask(llvm::Value* a, llvm::Value* b, const std::uint64_t predicate) {
    static constexpr llvm::CmpInst::Predicate Predicates[16] = {
        llvm::CmpInst::FCMP_OEQ, llvm::CmpInst::FCMP_OLT, llvm::CmpInst::FCMP_OLE, llvm::CmpInst::FCMP_UNO,
        llvm::CmpInst::FCMP_UNE, llvm::CmpInst::FCMP_UGE, llvm::CmpInst::FCMP_UGT, llvm::CmpInst::FCMP_ORD,
        llvm::CmpInst::FCMP_UEQ, llvm::CmpInst::FCMP_ULT, llvm::CmpInst::FCMP_ULE, llvm::CmpInst::FCMP_FALSE,
        llvm::CmpInst::FCMP_ONE, llvm::CmpInst::FCMP_OGE, llvm::CmpInst::FCMP_OGT, llvm::CmpInst::FCMP_TRUE};
    const bool legacy = _instruction->Info.encoding == ZYDIS_INSTRUCTION_ENCODING_LEGACY;
    auto* compare = _builder.CreateFCmp(Predicates[predicate & (legacy ? 7 : 15)], a, b);
    return _builder.CreateBitCast(_builder.CreateSExt(compare, IntegerLike(a->getType())), a->getType());
}

llvm::Value* FunctionLifter::FusedMultiplyAdd(const FloatSpec& spec, llvm::Value* x1, llvm::Value* x2, llvm::Value* x3) {
    llvm::Value* left = nullptr;
    llvm::Value* right = nullptr;
    llvm::Value* addend = nullptr;
    if (spec.Order == 132) {
        left = x1;
        right = x3;
        addend = x2;
    } else if (spec.Order == 213) {
        left = x2;
        right = x1;
        addend = x3;
    } else {
        left = x2;
        right = x3;
        addend = x1;
    }
    auto* type = left->getType();
    auto* product = spec.NegateProduct ? _builder.CreateFNeg(left) : left;
    auto* add = _builder.CreateIntrinsic(llvm::Intrinsic::fma, {type}, {product, right, addend});
    auto* subtract = _builder.CreateIntrinsic(llvm::Intrinsic::fma, {type}, {product, right, _builder.CreateFNeg(addend)});
    llvm::Value* result = spec.SubtractAddend ? subtract : add;
    if (spec.Alternate != 0) {
        const auto count = llvm::cast<llvm::FixedVectorType>(type)->getNumElements();
        std::vector<int> mask(count);
        for (unsigned i = 0; i < count; ++i) {
            const bool even = i % 2 == 0;
            const bool useSubtract = spec.Alternate == 1 ? even : !even;
            mask[i] = static_cast<int>(useSubtract ? i : count + i);
        }
        result = _builder.CreateShuffleVector(subtract, add, mask);
    }
    return NanResult(result, {left, right, addend});
}

llvm::Value* FunctionLifter::LoadScalarFloat(const int index, const unsigned element) {
    return _builder.CreateBitCast(ReadAs(index, element), FloatType(element));
}

void FunctionLifter::FloatArithmetic(const FloatSpec& spec) {
    const auto element = spec.Element;
    const auto kind = spec.Kind;
    const bool unary = kind == FloatKind::Sqrt || kind == FloatKind::Reciprocal || kind == FloatKind::ReciprocalSqrt;
    if (spec.Scalar) {
        int first = 0;
        int second = 1;
        if (kind == FloatKind::Fma) {
            auto* destination = AsFloats(ReadVector(0, 128), element);
            auto* x1 = _builder.CreateExtractElement(destination, _builder.getInt64(0));
            auto* x2 = _builder.CreateExtractElement(AsFloats(ReadVector(1, 128), element), _builder.getInt64(0));
            auto* x3 = LoadScalarFloat(2, element);
            Write(0, FloatBits(_builder.CreateInsertElement(destination, FusedMultiplyAdd(spec, x1, x2, x3), _builder.getInt64(0))));
            return;
        }
        const bool withImmediate = kind == FloatKind::Round || kind == FloatKind::Compare;
        first = Base(withImmediate ? 3 : 2);
        second = first + 1;
        auto* source = AsFloats(ReadVector(first, 128), element);
        auto* a = _builder.CreateExtractElement(source, _builder.getInt64(0));
        auto* b = LoadScalarFloat(second, element);
        llvm::Value* value = nullptr;
        if (kind == FloatKind::Round) value = RoundValue(b, Immediate(second + 1));
        else if (kind == FloatKind::Compare) value = CompareMask(a, b, Immediate(second + 1));
        else value = Arithmetic(kind, a, b);
        Write(0, FloatBits(_builder.CreateInsertElement(source, value, _builder.getInt64(0))));
        return;
    }

    const auto width = Bits(0);
    const unsigned count = width / element;
    const unsigned perLane = 128 / element;
    if (kind == FloatKind::Fma) {
        Write(0, FloatBits(FusedMultiplyAdd(spec, AsFloats(ReadAs(0, width), element), AsFloats(ReadAs(1, width), element), AsFloats(ReadAs(2, width), element))));
        return;
    }
    if (unary || kind == FloatKind::Round) {
        auto* source = AsFloats(ReadAs(1, width), element);
        Write(0, FloatBits(kind == FloatKind::Round ? RoundValue(source, Immediate(2)) : Arithmetic(kind, source, source)));
        return;
    }
    const int base = Base(kind == FloatKind::Compare ? 3 : 2);
    auto* a = AsFloats(ReadAs(base, width), element);
    auto* b = AsFloats(ReadAs(base + 1, width), element);
    llvm::Value* result = nullptr;
    switch (kind) {
    case FloatKind::Compare:
        result = CompareMask(a, b, Immediate(base + 2));
        break;
    case FloatKind::AddSub: {
        auto* sum = Arithmetic(FloatKind::Add, a, b);
        auto* difference = Arithmetic(FloatKind::Sub, a, b);
        std::vector<int> mask(count);
        for (unsigned i = 0; i < count; ++i) mask[i] = static_cast<int>(i % 2 == 0 ? i : count + i);
        result = _builder.CreateShuffleVector(difference, sum, mask);
        break;
    }
    case FloatKind::HorizontalAdd:
    case FloatKind::HorizontalSub: {
        const auto [left, right] = HorizontalOperands(a, b, count, perLane);
        result = Arithmetic(kind == FloatKind::HorizontalAdd ? FloatKind::Add : FloatKind::Sub, left, right);
        break;
    }
    default:
        result = Arithmetic(kind, a, b);
        break;
    }
    Write(0, FloatBits(result));
}

llvm::Value* FunctionLifter::ConvertNan(llvm::Value* source, llvm::Value* converted) {
    if (!ExactFloat()) return converted;
    const auto from = source->getType()->getScalarSizeInBits();
    const auto to = converted->getType()->getScalarSizeInBits();
    const auto mantissa = [](const unsigned bits) { return bits == 16 ? 10u : bits == 32 ? 23u : 52u; };
    auto* integerSource = _builder.CreateBitCast(source, IntegerLike(source->getType()));
    auto* targetInteger = IntegerLike(converted->getType());
    auto* sign = _builder.CreateICmpSLT(integerSource, llvm::Constant::getNullValue(integerSource->getType()));
    auto* payload = _builder.CreateAnd(integerSource, llvm::ConstantInt::get(integerSource->getType(), (std::uint64_t{1} << mantissa(from)) - 1));
    llvm::Value* moved = nullptr;
    if (mantissa(to) >= mantissa(from)) moved = _builder.CreateShl(_builder.CreateZExt(payload, targetInteger), mantissa(to) - mantissa(from));
    else moved = _builder.CreateTrunc(_builder.CreateLShr(payload, mantissa(from) - mantissa(to)), targetInteger);
    const auto exponentAndQuiet = ((std::uint64_t{1} << (to - 1 - mantissa(to))) - 1) << mantissa(to) | (std::uint64_t{1} << (mantissa(to) - 1));
    auto* signBit = _builder.CreateSelect(sign, llvm::ConstantInt::get(targetInteger, std::uint64_t{1} << (to - 1)), llvm::Constant::getNullValue(targetInteger));
    auto* nan = _builder.CreateOr(_builder.CreateOr(moved, llvm::ConstantInt::get(targetInteger, exponentAndQuiet)), signBit);
    return _builder.CreateSelect(IsNan(source), _builder.CreateBitCast(nan, converted->getType()), converted);
}

llvm::Value* FunctionLifter::SingleToHalf(llvm::Value* source, const unsigned mode) {
    auto* type = llvm::cast<llvm::FixedVectorType>(source->getType());
    auto* i32 = llvm::FixedVectorType::get(Int(32), type->getNumElements());
    auto* x = _builder.CreateBitCast(DenormalsAreZero(source), i32);
    const auto constant = [&](const std::uint32_t value) { return llvm::ConstantInt::get(i32, value); };
    auto* sign = _builder.CreateAnd(_builder.CreateLShr(x, 16), constant(0x8000));
    auto* negative = _builder.CreateICmpNE(sign, constant(0));
    auto* magnitude = _builder.CreateAnd(x, constant(0x7fffffff));
    auto* exponent = _builder.CreateLShr(magnitude, 23);
    auto* implicit = _builder.CreateSelect(_builder.CreateICmpEQ(exponent, constant(0)), constant(0), constant(0x800000));
    auto* mantissa = _builder.CreateOr(_builder.CreateAnd(magnitude, constant(0x7fffff)), implicit);
    auto* subnormal = _builder.CreateICmpULT(magnitude, constant(0x38800000));
    auto* rawShift = _builder.CreateSub(constant(126), exponent);
    auto* shift = _builder.CreateSelect(subnormal, _builder.CreateBinaryIntrinsic(llvm::Intrinsic::umin, rawShift, constant(25)), constant(13));
    auto* base = _builder.CreateSelect(subnormal, _builder.CreateLShr(mantissa, shift), _builder.CreateLShr(_builder.CreateSub(magnitude, constant(0x38000000)), 13));
    auto* remainderMask = _builder.CreateSub(_builder.CreateShl(constant(1), shift), constant(1));
    auto* remainder = _builder.CreateAnd(_builder.CreateSelect(subnormal, mantissa, magnitude), remainderMask);
    auto* half = _builder.CreateShl(constant(1), _builder.CreateSub(shift, constant(1)));
    auto* inexact = _builder.CreateICmpNE(remainder, constant(0));
    llvm::Value* increment = nullptr;
    switch (mode) {
    case 0:
        increment = _builder.CreateOr(_builder.CreateICmpUGT(remainder, half), _builder.CreateAnd(_builder.CreateICmpEQ(remainder, half), _builder.CreateICmpNE(_builder.CreateAnd(base, constant(1)), constant(0))));
        break;
    case 1:
        increment = _builder.CreateAnd(inexact, negative);
        break;
    case 2:
        increment = _builder.CreateAnd(inexact, _builder.CreateNot(negative));
        break;
    default:
        increment = llvm::Constant::getNullValue(llvm::FixedVectorType::get(_builder.getInt1Ty(), type->getNumElements()));
        break;
    }
    auto* rounded = _builder.CreateAdd(base, _builder.CreateZExt(increment, i32));
    const bool infinityPositive = mode == 0 || mode == 2;
    const bool infinityNegative = mode == 0 || mode == 1;
    auto* overflowValue = _builder.CreateSelect(negative, constant(infinityNegative ? 0x7c00 : 0x7bff), constant(infinityPositive ? 0x7c00 : 0x7bff));
    auto* overflow = _builder.CreateOr(_builder.CreateICmpUGE(magnitude, constant(0x47800000)), _builder.CreateICmpUGT(rounded, constant(0x7bff)));
    llvm::Value* finite = _builder.CreateSelect(overflow, overflowValue, rounded);
    auto* nan = _builder.CreateOr(constant(0x7e00), _builder.CreateLShr(_builder.CreateAnd(magnitude, constant(0x7fffff)), 13));
    auto* isNan = _builder.CreateICmpUGT(magnitude, constant(0x7f800000));
    auto* isInfinity = _builder.CreateICmpEQ(magnitude, constant(0x7f800000));
    finite = _builder.CreateSelect(isInfinity, constant(0x7c00), finite);
    finite = _builder.CreateSelect(isNan, nan, finite);
    auto* result = _builder.CreateOr(finite, sign);
    return _builder.CreateTrunc(result, llvm::FixedVectorType::get(Int(16), type->getNumElements()));
}

llvm::Value* FunctionLifter::FloatToInteger(llvm::Value* value, const unsigned bits, const bool truncate) {
    auto* rounded = _builder.CreateUnaryIntrinsic(truncate ? llvm::Intrinsic::trunc : llvm::Intrinsic::rint, value);
    auto* lower = FloatConstant(value->getType(), -std::ldexp(1.0, static_cast<int>(bits) - 1));
    auto* upper = FloatConstant(value->getType(), std::ldexp(1.0, static_cast<int>(bits) - 1));
    auto* inRange = _builder.CreateAnd(_builder.CreateFCmpOGE(rounded, lower), _builder.CreateFCmpOLT(rounded, upper));
    auto* targetType = value->getType()->isVectorTy() ? static_cast<llvm::Type*>(llvm::FixedVectorType::get(Int(bits), llvm::cast<llvm::FixedVectorType>(value->getType())->getNumElements())) : Int(bits);
    auto* safe = _builder.CreateSelect(inRange, rounded, llvm::Constant::getNullValue(value->getType()));
    auto* converted = _builder.CreateFPToSI(safe, targetType);
    return _builder.CreateSelect(inRange, converted, llvm::ConstantInt::get(targetType, std::uint64_t{1} << (bits - 1)));
}

void FunctionLifter::WriteVectorLow(const int index, llvm::Value* vector, const unsigned width) {
    auto* bits = FloatBits(vector);
    Write(index, _builder.CreateZExtOrTrunc(bits, Int(width)));
}

void FunctionLifter::Convert(const ZydisMnemonic mnemonic) {
    const bool scalarSource = mnemonic == ZYDIS_MNEMONIC_CVTSI2SS || mnemonic == ZYDIS_MNEMONIC_VCVTSI2SS || mnemonic == ZYDIS_MNEMONIC_CVTSI2SD || mnemonic == ZYDIS_MNEMONIC_VCVTSI2SD
        || mnemonic == ZYDIS_MNEMONIC_CVTSS2SD || mnemonic == ZYDIS_MNEMONIC_VCVTSS2SD || mnemonic == ZYDIS_MNEMONIC_CVTSD2SS || mnemonic == ZYDIS_MNEMONIC_VCVTSD2SS;
    if (scalarSource) {
        const int first = Base(2);
        const auto target = mnemonic == ZYDIS_MNEMONIC_CVTSI2SS || mnemonic == ZYDIS_MNEMONIC_VCVTSI2SS || mnemonic == ZYDIS_MNEMONIC_CVTSD2SS || mnemonic == ZYDIS_MNEMONIC_VCVTSD2SS ? 32u : 64u;
        auto* destination = AsFloats(ReadVector(first, 128), target);
        llvm::Value* value = nullptr;
        if (mnemonic == ZYDIS_MNEMONIC_CVTSI2SS || mnemonic == ZYDIS_MNEMONIC_VCVTSI2SS || mnemonic == ZYDIS_MNEMONIC_CVTSI2SD || mnemonic == ZYDIS_MNEMONIC_VCVTSI2SD) {
            value = _builder.CreateSIToFP(Read(first + 1), FloatType(target));
        } else {
            auto* source = LoadScalarFloat(first + 1, target == 32 ? 64 : 32);
            value = ConvertNan(source, target == 32 ? _builder.CreateFPTrunc(source, FloatType(32)) : _builder.CreateFPExt(source, FloatType(64)));
        }
        Write(0, FloatBits(_builder.CreateInsertElement(destination, value, _builder.getInt64(0))));
        return;
    }
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_CVTTSS2SI: case ZYDIS_MNEMONIC_VCVTTSS2SI: case ZYDIS_MNEMONIC_CVTSS2SI: case ZYDIS_MNEMONIC_VCVTSS2SI:
    case ZYDIS_MNEMONIC_CVTTSD2SI: case ZYDIS_MNEMONIC_VCVTTSD2SI: case ZYDIS_MNEMONIC_CVTSD2SI: case ZYDIS_MNEMONIC_VCVTSD2SI: {
        const bool isDouble = mnemonic == ZYDIS_MNEMONIC_CVTTSD2SI || mnemonic == ZYDIS_MNEMONIC_VCVTTSD2SI || mnemonic == ZYDIS_MNEMONIC_CVTSD2SI || mnemonic == ZYDIS_MNEMONIC_VCVTSD2SI;
        const bool truncate = mnemonic == ZYDIS_MNEMONIC_CVTTSS2SI || mnemonic == ZYDIS_MNEMONIC_VCVTTSS2SI || mnemonic == ZYDIS_MNEMONIC_CVTTSD2SI || mnemonic == ZYDIS_MNEMONIC_VCVTTSD2SI;
        Write(0, FloatToInteger(LoadScalarFloat(1, isDouble ? 64 : 32), Bits(0), truncate));
        return;
    }
    case ZYDIS_MNEMONIC_CVTDQ2PS: case ZYDIS_MNEMONIC_VCVTDQ2PS: {
        const auto width = Bits(0);
        Write(0, FloatBits(_builder.CreateSIToFP(AsVector(ReadAs(1, width), 32), llvm::FixedVectorType::get(FloatType(32), width / 32))));
        return;
    }
    case ZYDIS_MNEMONIC_CVTTPS2DQ: case ZYDIS_MNEMONIC_VCVTTPS2DQ: case ZYDIS_MNEMONIC_CVTPS2DQ: case ZYDIS_MNEMONIC_VCVTPS2DQ: {
        const auto width = Bits(0);
        const bool truncate = mnemonic == ZYDIS_MNEMONIC_CVTTPS2DQ || mnemonic == ZYDIS_MNEMONIC_VCVTTPS2DQ;
        Write(0, AsInteger(FloatToInteger(AsFloats(ReadAs(1, width), 32), 32, truncate)));
        return;
    }
    case ZYDIS_MNEMONIC_CVTDQ2PD: case ZYDIS_MNEMONIC_VCVTDQ2PD: {
        const auto count = Bits(0) / 64;
        auto* source = AsVector(_builder.CreateZExtOrTrunc(ReadRegisterOrMemory(1, count * 32), Int(count * 32)), 32);
        Write(0, FloatBits(_builder.CreateSIToFP(source, llvm::FixedVectorType::get(FloatType(64), count))));
        return;
    }
    case ZYDIS_MNEMONIC_CVTPS2PD: case ZYDIS_MNEMONIC_VCVTPS2PD: {
        const auto count = Bits(0) / 64;
        auto* source = AsFloats(_builder.CreateZExtOrTrunc(ReadRegisterOrMemory(1, count * 32), Int(count * 32)), 32);
        Write(0, FloatBits(ConvertNan(source, _builder.CreateFPExt(source, llvm::FixedVectorType::get(FloatType(64), count)))));
        return;
    }
    case ZYDIS_MNEMONIC_CVTPD2PS: case ZYDIS_MNEMONIC_VCVTPD2PS:
    case ZYDIS_MNEMONIC_CVTPD2DQ: case ZYDIS_MNEMONIC_VCVTPD2DQ: case ZYDIS_MNEMONIC_CVTTPD2DQ: case ZYDIS_MNEMONIC_VCVTTPD2DQ: {
        const auto sourceBits = Bits(1);
        const auto count = sourceBits / 64;
        auto* source = AsFloats(ReadAs(1, sourceBits), 64);
        llvm::Value* converted = nullptr;
        if (mnemonic == ZYDIS_MNEMONIC_CVTPD2PS || mnemonic == ZYDIS_MNEMONIC_VCVTPD2PS) converted = ConvertNan(source, _builder.CreateFPTrunc(source, llvm::FixedVectorType::get(FloatType(32), count)));
        else converted = FloatToInteger(source, 32, mnemonic == ZYDIS_MNEMONIC_CVTTPD2DQ || mnemonic == ZYDIS_MNEMONIC_VCVTTPD2DQ);
        WriteVectorLow(0, converted, 128);
        return;
    }
    case ZYDIS_MNEMONIC_VCVTPH2PS: {
        const auto count = Bits(0) / 32;
        auto* source = _builder.CreateBitCast(_builder.CreateZExtOrTrunc(ReadRegisterOrMemory(1, count * 16), Int(count * 16)), llvm::FixedVectorType::get(FloatType(16), count));
        Write(0, FloatBits(ConvertNan(source, _builder.CreateFPExt(source, llvm::FixedVectorType::get(FloatType(32), count)))));
        return;
    }
    case ZYDIS_MNEMONIC_VCVTPS2PH: {
        const auto control = Immediate(2);
        const auto sourceBits = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, Operand(1).reg.value);
        const auto count = sourceBits / 32;
        auto* source = AsFloats(ReadRegister(Operand(1).reg.value), 32);
        llvm::Value* bits = nullptr;
        if (control & 4) bits = FloatBits(ConvertNan(source, _builder.CreateFPTrunc(source, llvm::FixedVectorType::get(FloatType(16), count))));
        else bits = AsInteger(SingleToHalf(source, static_cast<unsigned>(control & 3)));
        if (Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY) Write(0, bits);
        else WriteRegister(Operand(0).reg.value, _builder.CreateZExt(bits, Int(128)));
        return;
    }
    default:
        UnexpectedMnemonic("Convert", mnemonic);
    }
}

void FunctionLifter::CompareScalar(const unsigned element) {
    auto* a = LoadScalarFloat(0, element);
    auto* b = LoadScalarFloat(1, element);
    ComparisonFlags(_builder.CreateFCmpUNO(a, b), [&] { return _builder.CreateFCmpUEQ(a, b); }, [&] { return _builder.CreateFCmpULT(a, b); });
}

void FunctionLifter::DotProduct(const unsigned element) {
    const int base = Base(3);
    const auto width = Bits(0);
    const auto control = Immediate(base + 2);
    const unsigned count = width / element;
    const unsigned perLane = 128 / element;
    auto* a = AsFloats(ReadAs(base, width), element);
    auto* b = AsFloats(ReadAs(base + 1, width), element);
    auto* type = a->getType();
    auto* zero = llvm::Constant::getNullValue(type);
    std::vector<llvm::Constant*> selected(count);
    for (unsigned i = 0; i < count; ++i) selected[i] = _builder.getInt1(((control >> (4 + i % perLane)) & 1) != 0);
    auto* chosen = llvm::ConstantVector::get(selected);
    auto* products = Arithmetic(FloatKind::Mul, _builder.CreateSelect(chosen, a, zero), _builder.CreateSelect(chosen, b, zero));
    llvm::Value* result = zero;
    for (unsigned lane = 0; lane < count; lane += perLane) {
        const auto product = [&](const unsigned i) { return _builder.CreateExtractElement(products, _builder.getInt64(lane + i)); };
        const auto add = [&](llvm::Value* a, llvm::Value* b) { return Arithmetic(FloatKind::Add, a, b); };
        auto* sum = perLane == 4 ? add(add(product(0), product(1)), add(product(2), product(3))) : add(product(0), product(1));
        for (unsigned i = 0; i < perLane; ++i) {
            if ((control >> i) & 1) result = _builder.CreateInsertElement(result, sum, _builder.getInt64(lane + i));
        }
    }
    Write(0, FloatBits(result));
}

void FunctionLifter::LoadMxcsr() {
    StateStore(Read(0), offsetof(Ps5LiftState, Mxcsr));
    _builder.CreateCall(_module.Runtime("ps5lift_apply_mxcsr", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer()}, false)), {_state});
}

void FunctionLifter::StoreMxcsr() {
    auto callee = _module.Runtime("ps5lift_read_mxcsr", llvm::FunctionType::get(_builder.getInt32Ty(), {_module.Pointer()}, false));
    Write(0, _builder.CreateCall(callee, {_state}));
}

}
