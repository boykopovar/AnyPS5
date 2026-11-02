#include "../FunctionLifter.hpp"

#include <llvm/IR/IntrinsicsAArch64.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace Ps5Lift::Lifting {

void FunctionLifter::VectorBitwise(const llvm::Instruction::BinaryOps operation) {
    const bool threeOperand = _instruction->Info.operand_count_visible == 3;
    const auto bits = Bits(0);
    auto* a = threeOperand ? ReadAs(1, bits) : Read(0);
    auto* b = ReadAs(threeOperand ? 2 : 1, bits);
    Write(0, _builder.CreateBinOp(operation, a, b));
}

llvm::Value* FunctionLifter::AsVector(llvm::Value* value, const unsigned element) {
    const auto bits = value->getType()->getIntegerBitWidth();
    return _builder.CreateBitCast(value, llvm::FixedVectorType::get(Int(element), bits / element));
}

llvm::Value* FunctionLifter::AsInteger(llvm::Value* vector) {
    return _builder.CreateBitCast(vector, Int(vector->getType()->getPrimitiveSizeInBits().getFixedValue()));
}

llvm::Value* FunctionLifter::Splat(llvm::Value* vector, llvm::Value* scalar) {
    return _builder.CreateVectorSplat(llvm::cast<llvm::FixedVectorType>(vector->getType())->getNumElements(), scalar);
}

llvm::Value* FunctionLifter::ShuffleWithZero(llvm::Value* a, llvm::Value* b, const std::vector<int>& mask) {
    const auto count = static_cast<int>(mask.size());
    std::vector<int> picked(mask.size());
    std::vector<int> zeroed(mask.size());
    bool anyZero = false;
    for (int i = 0; i < count; ++i) {
        picked[i] = mask[i] < 0 ? 0 : mask[i];
        zeroed[i] = mask[i] < 0 ? count + i : i;
        anyZero = anyZero || mask[i] < 0;
    }
    auto* result = _builder.CreateShuffleVector(a, b, picked);
    if (!anyZero) return result;
    return _builder.CreateShuffleVector(result, llvm::Constant::getNullValue(result->getType()), zeroed);
}

llvm::Value* FunctionLifter::Elements(llvm::Value* vector, const unsigned start, const unsigned count) {
    const auto size = llvm::cast<llvm::FixedVectorType>(vector->getType())->getNumElements();
    if (start == 0 && count == size) return vector;
    std::vector<int> mask(count);
    for (unsigned i = 0; i < count; ++i) mask[i] = start + i < size ? static_cast<int>(start + i) : -1;
    return ShuffleWithZero(vector, vector, mask);
}

llvm::Value* FunctionLifter::Concatenate(llvm::Value* low, llvm::Value* high) {
    std::vector<int> mask(2 * llvm::cast<llvm::FixedVectorType>(low->getType())->getNumElements());
    for (std::size_t i = 0; i < mask.size(); ++i) mask[i] = static_cast<int>(i);
    return _builder.CreateShuffleVector(low, high, mask);
}

llvm::Value* FunctionLifter::LookUpBytes(llvm::Value* table, llvm::Value* indices) {
    const auto size = llvm::cast<llvm::FixedVectorType>(table->getType())->getNumElements();
    std::vector<llvm::Value*> arguments;
    for (unsigned start = 0; start < size; start += 16) arguments.push_back(Elements(table, start, 16));
    arguments.push_back(indices);
    return _builder.CreateIntrinsic(arguments.size() == 2 ? llvm::Intrinsic::aarch64_neon_tbl1 : llvm::Intrinsic::aarch64_neon_tbl2, {indices->getType()}, arguments);
}

llvm::Value* FunctionLifter::ShiftByCount(const llvm::Instruction::BinaryOps shift, llvm::Value* vector, llvm::Value* counts, const unsigned element) {
    auto* limit = Splat(vector, llvm::ConstantInt::get(Int(element), element - 1));
    auto* clamped = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::umin, counts, limit);
    auto* tooLarge = _builder.CreateICmpUGT(counts, limit);
    auto* zero = llvm::Constant::getNullValue(vector->getType());
    if (shift == llvm::Instruction::AShr) return _builder.CreateAShr(vector, clamped);
    return _builder.CreateSelect(tooLarge, zero, _builder.CreateBinOp(shift, vector, clamped));
}

llvm::Value* FunctionLifter::Pairs(llvm::Value* vector, const unsigned count, const unsigned offset) {
    std::vector<int> picked(count);
    for (unsigned i = 0; i < count; ++i) picked[i] = static_cast<int>(2 * i + offset);
    return _builder.CreateShuffleVector(vector, vector, picked);
}

llvm::Value* FunctionLifter::Saturate(llvm::Value* wide, const unsigned element, const bool isSigned) {
    auto* type = wide->getType();
    const auto wideBits = llvm::cast<llvm::FixedVectorType>(type)->getElementType()->getIntegerBitWidth();
    const auto low = isSigned ? llvm::APInt::getSignedMinValue(element).sext(wideBits) : llvm::APInt::getZero(wideBits);
    const auto high = isSigned ? llvm::APInt::getSignedMaxValue(element).sext(wideBits) : llvm::APInt::getMaxValue(element).zext(wideBits);
    auto* clamped = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::smax, wide, llvm::ConstantInt::get(type, low));
    clamped = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::smin, clamped, llvm::ConstantInt::get(type, high));
    return _builder.CreateTrunc(clamped, llvm::FixedVectorType::get(Int(element), llvm::cast<llvm::FixedVectorType>(type)->getNumElements()));
}

void FunctionLifter::MoveMask(const unsigned element) {
    auto* source = Read(1);
    auto* negative = _builder.CreateICmpSLT(AsVector(source, element), llvm::Constant::getNullValue(AsVector(source, element)->getType()));
    auto* bits = _builder.CreateBitCast(negative, Int(source->getType()->getIntegerBitWidth() / element));
    Write(0, _builder.CreateZExt(bits, Int(Bits(0))));
}

void FunctionLifter::TestFlags(llvm::Value* a, llvm::Value* b) {
    auto* zero = llvm::ConstantInt::get(a->getType(), 0);
    SetFlag(PS5LIFT_ZF, _builder.CreateICmpEQ(_builder.CreateAnd(a, b), zero));
    SetFlag(PS5LIFT_CF, _builder.CreateICmpEQ(_builder.CreateAnd(_builder.CreateNot(a), b), zero));
    for (const auto flag : {PS5LIFT_PF, PS5LIFT_AF, PS5LIFT_SF, PS5LIFT_OF}) SetFlag(flag, _builder.getFalse());
}

void FunctionLifter::VectorTest() {
    auto* a = Read(0);
    TestFlags(a, ReadAs(1, a->getType()->getIntegerBitWidth()));
}

void FunctionLifter::VectorTestSigns(const unsigned element) {
    auto* a = Read(0);
    auto* b = ReadAs(1, a->getType()->getIntegerBitWidth());
    const auto bits = a->getType()->getIntegerBitWidth();
    llvm::APInt signs(bits, 0);
    for (unsigned i = element - 1; i < bits; i += element) signs.setBit(i);
    auto* mask = llvm::ConstantInt::get(Context(), signs);
    auto* signsA = _builder.CreateAnd(a, mask);
    auto* signsB = _builder.CreateAnd(b, mask);
    TestFlags(signsA, signsB);
}

void FunctionLifter::VectorAbsolute(const unsigned element) {
    const auto width = Bits(0);
    Write(0, AsInteger(_builder.CreateBinaryIntrinsic(llvm::Intrinsic::abs, AsVector(ReadAs(1, width), element), _builder.getFalse())));
}

void FunctionLifter::VectorExtend(const llvm::Instruction::CastOps extension, const unsigned element, const unsigned sourceElement) {
    const auto width = Bits(0);
    const unsigned count = width / element;
    auto* source = Read(1);
    auto* narrow = _builder.CreateZExtOrTrunc(source, Int(count * sourceElement));
    auto* vector = AsVector(narrow, sourceElement);
    auto* target = llvm::FixedVectorType::get(Int(element), count);
    Write(0, AsInteger(_builder.CreateCast(extension, vector, target)));
}

void FunctionLifter::ShiftBytes(const llvm::Instruction::BinaryOps shift) {
    const auto width = Bits(0);
    const unsigned lanes = width / std::min(width, 128u);
    const unsigned count = width / 8;
    const int base = Base(2);
    auto* source = AsVector(ReadAs(base, width), 8);
    const auto amount = static_cast<int>(std::min<std::uint64_t>(Immediate(base + 1), 16));
    std::vector<int> mask(count);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        for (int i = 0; i < 16; ++i) {
            const int from = shift == llvm::Instruction::Shl ? i - amount : i + amount;
            mask[lane * 16 + i] = from >= 0 && from < 16 ? static_cast<int>(lane * 16) + from : -1;
        }
    }
    Write(0, AsInteger(ShuffleWithZero(source, source, mask)));
}

void FunctionLifter::VectorShift(const llvm::Instruction::BinaryOps shift, const unsigned element) {
    const auto width = Bits(0);
    const int base = Base(2);
    auto* vector = AsVector(ReadAs(base, width), element);
    llvm::Value* amount = nullptr;
    if (Operand(base + 1).type == ZYDIS_OPERAND_TYPE_IMMEDIATE) amount = _builder.getInt64(Immediate(base + 1) & 0xff);
    else amount = _builder.CreateTrunc(ReadAs(base + 1, 128), I64());
    auto* clampedScalar = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::umin, amount, _builder.getInt64(element));
    Write(0, AsInteger(ShiftByCount(shift, vector, Splat(vector, _builder.CreateTrunc(clampedScalar, Int(element))), element)));
}

FunctionLifter::VectorOperands FunctionLifter::ReadVectorOperands(const int base, const unsigned width, const unsigned element) {
    auto* a = ReadAs(base, width);
    auto* b = _builder.CreateZExtOrTrunc(Read(base + 1), Int(width));
    return {a, b, AsVector(a, element), AsVector(b, element)};
}

void FunctionLifter::VectorBinary(const VectorKind kind, const unsigned element) {
    using K = VectorKind;
    const auto width = Bits(0);
    const unsigned laneBits = std::min(width, 128u);
    const unsigned lanes = width / laneBits;
    const unsigned count = width / element;
    const unsigned perLane = laneBits / element;
    const auto [a, b, va, vb] = ReadVectorOperands(Base(2), width, element);
    llvm::Value* result = nullptr;
    switch (kind) {
    case K::Add: result = _builder.CreateAdd(va, vb); break;
    case K::Sub: result = _builder.CreateSub(va, vb); break;
    case K::AddSignedSaturate: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::sadd_sat, va, vb); break;
    case K::AddUnsignedSaturate: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::uadd_sat, va, vb); break;
    case K::SubSignedSaturate: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::ssub_sat, va, vb); break;
    case K::SubUnsignedSaturate: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::usub_sat, va, vb); break;
    case K::MultiplyLow: result = _builder.CreateMul(va, vb); break;
    case K::MinSigned: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::smin, va, vb); break;
    case K::MaxSigned: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::smax, va, vb); break;
    case K::MinUnsigned: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::umin, va, vb); break;
    case K::MaxUnsigned: result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::umax, va, vb); break;
    case K::CompareEqual: result = _builder.CreateSExt(_builder.CreateICmpEQ(va, vb), va->getType()); break;
    case K::CompareGreater: result = _builder.CreateSExt(_builder.CreateICmpSGT(va, vb), va->getType()); break;
    case K::AndNot: result = _builder.CreateAnd(_builder.CreateNot(va), vb); break;
    case K::MultiplyHighSigned:
    case K::MultiplyHighUnsigned: {
        auto* wide = llvm::FixedVectorType::get(Int(element * 2), count);
        const bool isSigned = kind == K::MultiplyHighSigned;
        auto* product = _builder.CreateMul(isSigned ? _builder.CreateSExt(va, wide) : _builder.CreateZExt(va, wide), isSigned ? _builder.CreateSExt(vb, wide) : _builder.CreateZExt(vb, wide));
        result = _builder.CreateTrunc(_builder.CreateLShr(product, element), va->getType());
        break;
    }
    case K::MultiplyEvenUnsigned:
    case K::MultiplyEvenSigned: {
        auto* shift = Splat(va, _builder.getInt64(32));
        auto* left = kind == K::MultiplyEvenUnsigned ? _builder.CreateAnd(va, Splat(va, _builder.getInt64(0xffffffff))) : _builder.CreateAShr(_builder.CreateShl(va, shift), shift);
        auto* right = kind == K::MultiplyEvenUnsigned ? _builder.CreateAnd(vb, Splat(vb, _builder.getInt64(0xffffffff))) : _builder.CreateAShr(_builder.CreateShl(vb, shift), shift);
        result = _builder.CreateMul(left, right);
        break;
    }
    case K::MultiplyHighRoundScale: {
        auto* wide = llvm::FixedVectorType::get(Int(32), count);
        auto* product = _builder.CreateMul(_builder.CreateSExt(va, wide), _builder.CreateSExt(vb, wide));
        auto* scaled = _builder.CreateAdd(_builder.CreateAShr(product, 14), llvm::ConstantInt::get(wide, 1));
        result = _builder.CreateTrunc(_builder.CreateLShr(scaled, 1), va->getType());
        break;
    }
    case K::MultiplyAddWords: {
        auto* words = llvm::FixedVectorType::get(Int(32), count * 2);
        auto* product = _builder.CreateMul(_builder.CreateSExt(AsVector(a, 16), words), _builder.CreateSExt(AsVector(b, 16), words));
        result = _builder.CreateAdd(Pairs(product, count, 0), Pairs(product, count, 1));
        break;
    }
    case K::MultiplyAddBytes: {
        auto* bytes = llvm::FixedVectorType::get(Int(16), count * 2);
        auto* product = _builder.CreateMul(_builder.CreateZExt(AsVector(a, 8), bytes), _builder.CreateSExt(AsVector(b, 8), bytes));
        result = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::sadd_sat, Pairs(product, count, 0), Pairs(product, count, 1));
        break;
    }
    case K::HorizontalAdd:
    case K::HorizontalAddSaturate:
    case K::HorizontalSub:
    case K::HorizontalSubSaturate: {
        const auto [first, second] = HorizontalOperands(va, vb, count, perLane);
        if (kind == K::HorizontalAdd) result = _builder.CreateAdd(first, second);
        else if (kind == K::HorizontalSub) result = _builder.CreateSub(first, second);
        else result = _builder.CreateBinaryIntrinsic(kind == K::HorizontalAddSaturate ? llvm::Intrinsic::sadd_sat : llvm::Intrinsic::ssub_sat, first, second);
        break;
    }
    case K::SumAbsoluteDifferences: {
        auto* words = llvm::FixedVectorType::get(Int(16), count * 8);
        auto* difference = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::abs, _builder.CreateSub(_builder.CreateZExt(AsVector(a, 8), words), _builder.CreateZExt(AsVector(b, 8), words)),
                                                          _builder.getFalse());
        result = llvm::Constant::getNullValue(va->getType());
        for (unsigned i = 0; i < count; ++i) {
            std::vector<int> block(8);
            for (unsigned j = 0; j < 8; ++j) block[j] = static_cast<int>(8 * i + j);
            auto* sum = _builder.CreateAddReduce(_builder.CreateShuffleVector(difference, difference, block));
            result = _builder.CreateInsertElement(result, _builder.CreateZExt(sum, Int(64)), _builder.getInt64(i));
        }
        break;
    }
    case K::Average: {
        auto* wide = llvm::FixedVectorType::get(Int(element * 2), count);
        auto* sum = _builder.CreateAdd(_builder.CreateAdd(_builder.CreateZExt(va, wide), _builder.CreateZExt(vb, wide)), llvm::ConstantInt::get(wide, 1));
        result = _builder.CreateTrunc(_builder.CreateLShr(sum, 1), va->getType());
        break;
    }
    case K::UnpackLow:
    case K::UnpackHigh: {
        std::vector<int> mask(count);
        for (unsigned lane = 0; lane < lanes; ++lane) {
            for (unsigned i = 0; i < perLane / 2; ++i) {
                const auto from = lane * perLane + (kind == K::UnpackHigh ? perLane / 2 : 0) + i;
                mask[lane * perLane + 2 * i] = static_cast<int>(from);
                mask[lane * perLane + 2 * i + 1] = static_cast<int>(count + from);
            }
        }
        result = _builder.CreateShuffleVector(va, vb, mask);
        break;
    }
    case K::PackSignedSaturate:
    case K::PackUnsignedSaturate: {
        auto* wa = AsVector(a, element * 2);
        auto* wb = AsVector(b, element * 2);
        auto* na = Saturate(wa, element, kind == K::PackSignedSaturate);
        auto* nb = Saturate(wb, element, kind == K::PackSignedSaturate);
        const unsigned half = perLane / 2;
        const unsigned narrowCount = count / 2;
        std::vector<int> mask(count);
        for (unsigned lane = 0; lane < lanes; ++lane) {
            for (unsigned i = 0; i < half; ++i) {
                mask[lane * perLane + i] = static_cast<int>(lane * half + i);
                mask[lane * perLane + half + i] = static_cast<int>(narrowCount + lane * half + i);
            }
        }
        result = _builder.CreateShuffleVector(na, nb, mask);
        break;
    }
    case K::ShuffleBytes: {
        if (_module.Triple().isAArch64()) {
            auto* indices = _builder.CreateAnd(vb, Splat(vb, _builder.getInt8(0x80 | (perLane - 1))));
            result = LookUpBytes(Elements(va, 0, perLane), Elements(indices, 0, perLane));
            if (lanes == 2) result = Concatenate(result, LookUpBytes(Elements(va, perLane, perLane), Elements(indices, perLane, perLane)));
            break;
        }
        result = llvm::Constant::getNullValue(va->getType());
        for (unsigned i = 0; i < count; ++i) {
            auto* selector = _builder.CreateExtractElement(vb, _builder.getInt64(i));
            auto* index = _builder.CreateAdd(_builder.CreateZExt(_builder.CreateAnd(selector, perLane - 1), I64()), _builder.getInt64((i / perLane) * perLane));
            auto* picked = _builder.CreateExtractElement(va, index);
            auto* value = _builder.CreateSelect(_builder.CreateICmpSLT(selector, _builder.getInt8(0)), _builder.getInt8(0), picked);
            result = _builder.CreateInsertElement(result, value, _builder.getInt64(i));
        }
        break;
    }
    case K::PermuteDwords: {
        if (_module.Triple().isAArch64()) {
            auto* dwords = _builder.CreateAnd(va, Splat(va, _builder.getInt32(count - 1)));
            auto* indices = AsVector(AsInteger(_builder.CreateAdd(_builder.CreateMul(dwords, Splat(va, _builder.getInt32(0x04040404))), Splat(va, _builder.getInt32(0x03020100)))), 8);
            auto* table = AsVector(b, 8);
            result = Concatenate(LookUpBytes(table, Elements(indices, 0, 16)), LookUpBytes(table, Elements(indices, 16, 16)));
            break;
        }
        result = llvm::Constant::getNullValue(va->getType());
        for (unsigned i = 0; i < count; ++i) {
            auto* index = _builder.CreateAnd(_builder.CreateExtractElement(va, _builder.getInt64(i)), count - 1);
            result = _builder.CreateInsertElement(result, _builder.CreateExtractElement(vb, index), _builder.getInt64(i));
        }
        break;
    }
    }
    Write(0, AsInteger(result));
}

void FunctionLifter::VectorShiftVariable(const llvm::Instruction::BinaryOps shift, const unsigned element) {
    const auto width = Bits(0);
    const auto [a, b, va, vb] = ReadVectorOperands(Base(2), width, element);
    Write(0, AsInteger(ShiftByCount(shift, va, vb, element)));
}

std::pair<llvm::Value*, llvm::Value*> FunctionLifter::HorizontalOperands(llvm::Value* a, llvm::Value* b, const unsigned count, const unsigned perLane) {
    std::vector<int> first(count);
    std::vector<int> second(count);
    const unsigned half = perLane / 2;
    for (unsigned lane = 0; lane < count / perLane; ++lane) {
        for (unsigned i = 0; i < perLane; ++i) {
            const unsigned from = (i < half ? 0 : count) + lane * perLane + 2 * (i % half);
            first[lane * perLane + i] = static_cast<int>(from);
            second[lane * perLane + i] = static_cast<int>(from + 1);
        }
    }
    return {_builder.CreateShuffleVector(a, b, first), _builder.CreateShuffleVector(a, b, second)};
}

void FunctionLifter::MinimumPosition() {
    auto* words = AsVector(ReadAs(1, 128), 16);
    auto* minimum = _builder.CreateUnaryIntrinsic(llvm::Intrinsic::vector_reduce_umin, words);
    auto* matches = _builder.CreateBitCast(_builder.CreateICmpEQ(words, _builder.CreateVectorSplat(8, minimum)), _builder.getInt8Ty());
    auto* index = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::cttz, matches, _builder.getTrue());
    Write(0, _builder.CreateOr(_builder.CreateZExt(minimum, Int(128)), _builder.CreateShl(_builder.CreateZExt(index, Int(128)), 16)));
}

}
