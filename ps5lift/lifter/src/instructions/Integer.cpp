#include "../FunctionLifter.hpp"

#include <llvm/IR/MDBuilder.h>

#include <bit>

namespace Ps5Lift::Lifting {

void FunctionLifter::Shift(const ZydisMnemonic mnemonic) {
    const auto bits = Bits(0);
    auto* value = Read(0);
    auto* count = _builder.CreateAnd(ReadAs(1, 8), _builder.getInt8(bits == 64 ? 63 : 31));
    auto* zero = _builder.CreateICmpEQ(count, _builder.getInt8(0));
    auto* wideCount = _builder.CreateZExt(count, Int(128));
    auto* below = _builder.CreateSelect(zero, llvm::ConstantInt::get(Int(128), 0), _builder.CreateSub(wideCount, llvm::ConstantInt::get(Int(128), 1)));
    llvm::Value* result = nullptr;
    llvm::Value* carry = nullptr;
    llvm::Value* overflow = nullptr;
    const bool rotate = mnemonic == ZYDIS_MNEMONIC_ROL || mnemonic == ZYDIS_MNEMONIC_ROR;
    if (mnemonic == ZYDIS_MNEMONIC_SHL) {
        auto* wide = _builder.CreateShl(_builder.CreateZExt(value, Int(128)), wideCount);
        result = _builder.CreateTrunc(wide, Int(bits));
        carry = _builder.CreateTrunc(_builder.CreateLShr(wide, bits), _builder.getInt1Ty());
        overflow = _builder.CreateXor(SignBit(result), carry);
    } else if (mnemonic == ZYDIS_MNEMONIC_SHR) {
        auto* wide = _builder.CreateZExt(value, Int(128));
        result = _builder.CreateTrunc(_builder.CreateLShr(wide, wideCount), Int(bits));
        carry = _builder.CreateTrunc(_builder.CreateLShr(wide, below), _builder.getInt1Ty());
        overflow = SignBit(value);
    } else if (mnemonic == ZYDIS_MNEMONIC_SAR) {
        auto* wide = _builder.CreateSExt(value, Int(128));
        result = _builder.CreateTrunc(_builder.CreateAShr(wide, wideCount), Int(bits));
        carry = _builder.CreateTrunc(_builder.CreateAShr(wide, below), _builder.getInt1Ty());
        overflow = _builder.getFalse();
    } else if (rotate) {
        auto* amount = _builder.CreateZExtOrTrunc(_builder.CreateURem(count, _builder.getInt8(bits)), Int(bits));
        const auto intrinsic = mnemonic == ZYDIS_MNEMONIC_ROL ? llvm::Intrinsic::fshl : llvm::Intrinsic::fshr;
        result = _builder.CreateIntrinsic(intrinsic, {Int(bits)}, {value, value, amount});
        if (mnemonic == ZYDIS_MNEMONIC_ROL) {
            carry = _builder.CreateTrunc(result, _builder.getInt1Ty());
            overflow = _builder.CreateXor(SignBit(result), carry);
        } else {
            carry = SignBit(result);
            overflow = _builder.CreateXor(carry, _builder.CreateTrunc(_builder.CreateLShr(result, bits - 2), _builder.getInt1Ty()));
        }
    } else {
        UnexpectedMnemonic("Shift", mnemonic);
    }
    Write(0, result);
    SetFlag(PS5LIFT_CF, _builder.CreateSelect(zero, Flag(PS5LIFT_CF), carry));
    SetFlag(PS5LIFT_OF, _builder.CreateSelect(zero, Flag(PS5LIFT_OF), overflow));
    if (rotate) return;
    SetFlag(PS5LIFT_ZF, _builder.CreateSelect(zero, Flag(PS5LIFT_ZF), _builder.CreateICmpEQ(result, llvm::ConstantInt::get(result->getType(), 0))));
    SetFlag(PS5LIFT_SF, _builder.CreateSelect(zero, Flag(PS5LIFT_SF), SignBit(result)));
    SetFlag(PS5LIFT_PF, _builder.CreateSelect(zero, Flag(PS5LIFT_PF), Parity(result)));
}

void FunctionLifter::ShiftDouble(const ZydisMnemonic mnemonic) {
    const auto bits = Bits(0);
    if (bits == 16) throw UnsupportedInstruction("16-bit");
    auto* value = Read(0);
    auto* count = _builder.CreateAnd(ReadAs(2, 8), _builder.getInt8(bits == 64 ? 63 : 31));
    auto* zero = _builder.CreateICmpEQ(count, _builder.getInt8(0));
    auto* amount = _builder.CreateZExt(count, Int(bits));
    auto* wideValue = _builder.CreateZExt(value, Int(128));
    auto* wideCount = _builder.CreateZExt(count, Int(128));
    llvm::Value* result = nullptr;
    llvm::Value* carry = nullptr;
    if (mnemonic == ZYDIS_MNEMONIC_SHLD) {
        result = _builder.CreateIntrinsic(llvm::Intrinsic::fshl, {Int(bits)}, {value, ReadAs(1, bits), amount});
        carry = _builder.CreateTrunc(_builder.CreateLShr(_builder.CreateShl(wideValue, wideCount), bits), _builder.getInt1Ty());
    } else if (mnemonic == ZYDIS_MNEMONIC_SHRD) {
        result = _builder.CreateIntrinsic(llvm::Intrinsic::fshr, {Int(bits)}, {ReadAs(1, bits), value, amount});
        auto* below = _builder.CreateSelect(zero, llvm::ConstantInt::get(Int(128), 0), _builder.CreateSub(wideCount, llvm::ConstantInt::get(Int(128), 1)));
        carry = _builder.CreateTrunc(_builder.CreateLShr(wideValue, below), _builder.getInt1Ty());
    } else {
        UnexpectedMnemonic("ShiftDouble", mnemonic);
    }
    Write(0, result);
    SetFlag(PS5LIFT_CF, _builder.CreateSelect(zero, Flag(PS5LIFT_CF), carry));
    SetFlag(PS5LIFT_OF, _builder.CreateSelect(zero, Flag(PS5LIFT_OF), _builder.CreateXor(SignBit(result), SignBit(value))));
    SetFlag(PS5LIFT_ZF, _builder.CreateSelect(zero, Flag(PS5LIFT_ZF), _builder.CreateICmpEQ(result, llvm::ConstantInt::get(result->getType(), 0))));
    SetFlag(PS5LIFT_SF, _builder.CreateSelect(zero, Flag(PS5LIFT_SF), SignBit(result)));
    SetFlag(PS5LIFT_PF, _builder.CreateSelect(zero, Flag(PS5LIFT_PF), Parity(result)));
}

void FunctionLifter::AddWithFlagCarry(const int flag) {
    auto* sum = _builder.CreateIntrinsic(llvm::Intrinsic::uadd_with_overflow, {Int(Bits(0))}, {Read(0), ReadAs(1, Bits(0))});
    auto* total = _builder.CreateIntrinsic(llvm::Intrinsic::uadd_with_overflow, {Int(Bits(0))}, {_builder.CreateExtractValue(sum, 0), _builder.CreateZExt(Flag(flag), Int(Bits(0)))});
    Write(0, _builder.CreateExtractValue(total, 0));
    SetFlag(flag, _builder.CreateOr(_builder.CreateExtractValue(sum, 1), _builder.CreateExtractValue(total, 1)));
}

llvm::Value* FunctionLifter::LowAccumulator(const unsigned bits) {
    return _builder.CreateTrunc(Gpr(PS5LIFT_RAX), Int(bits));
}

void FunctionLifter::WriteAccumulatorPair(const unsigned bits, llvm::Value* low, llvm::Value* high) {
    if (bits == 8) {
        auto* ax = _builder.CreateOr(_builder.CreateZExt(low, _builder.getInt16Ty()), _builder.CreateShl(_builder.CreateZExt(high, _builder.getInt16Ty()), 8));
        return WriteRegister(ZYDIS_REGISTER_AX, ax);
    }
    const auto rax = bits == 64 ? ZYDIS_REGISTER_RAX : bits == 32 ? ZYDIS_REGISTER_EAX : ZYDIS_REGISTER_AX;
    const auto rdx = bits == 64 ? ZYDIS_REGISTER_RDX : bits == 32 ? ZYDIS_REGISTER_EDX : ZYDIS_REGISTER_DX;
    WriteRegister(rax, low);
    WriteRegister(rdx, high);
}

llvm::Value* FunctionLifter::AccumulatorPair(const unsigned bits) {
    if (bits == 8) return _builder.CreateTrunc(Gpr(PS5LIFT_RAX), _builder.getInt16Ty());
    auto* low = _builder.CreateZExt(LowAccumulator(bits), Int(bits * 2));
    auto* high = _builder.CreateZExt(_builder.CreateTrunc(Gpr(PS5LIFT_RDX), Int(bits)), Int(bits * 2));
    return _builder.CreateOr(low, _builder.CreateShl(high, bits));
}

void FunctionLifter::Multiply(const bool isSigned) {
    const auto bits = Bits(0);
    auto* source = Read(0);
    auto* a = LowAccumulator(bits);
    auto* wideA = isSigned ? _builder.CreateSExt(a, Int(bits * 2)) : _builder.CreateZExt(a, Int(bits * 2));
    auto* wideB = isSigned ? _builder.CreateSExt(source, Int(bits * 2)) : _builder.CreateZExt(source, Int(bits * 2));
    auto* product = _builder.CreateMul(wideA, wideB);
    auto* low = _builder.CreateTrunc(product, Int(bits));
    auto* high = _builder.CreateTrunc(_builder.CreateLShr(product, bits), Int(bits));
    WriteAccumulatorPair(bits, low, high);
    auto* overflow = isSigned ? _builder.CreateICmpNE(_builder.CreateSExt(low, Int(bits * 2)), product) : _builder.CreateICmpNE(high, llvm::ConstantInt::get(high->getType(), 0));
    SetFlag(PS5LIFT_CF, overflow);
    SetFlag(PS5LIFT_OF, overflow);
    ResultFlags(low);
}

void FunctionLifter::Divide(const bool isSigned) {
    const auto bits = Bits(0);
    auto* type = Int(bits);
    auto* wide = Int(bits * 2);
    auto* divisor = Read(0);
    auto* dividend = AccumulatorPair(bits);
    auto* low = _builder.CreateTrunc(dividend, type);
    auto* high = _builder.CreateTrunc(_builder.CreateLShr(dividend, bits), type);
    auto* minusOne = _builder.CreateICmpEQ(divisor, llvm::ConstantInt::getAllOnesValue(type));
    const auto block = [&](const char* name) { return llvm::BasicBlock::Create(Context(), name, _host); };
    auto* nonzero = block("divide_nonzero");
    auto* narrow = block("divide_narrow");
    auto* wideDivide = block("divide_wide");
    auto* fault = block("divide_error");
    auto* done = block("divide_done");
    const auto unlikely = llvm::MDBuilder(Context()).createBranchWeights(1, 1u << 20);
    _builder.CreateCondBr(_builder.CreateICmpEQ(divisor, llvm::ConstantInt::get(type, 0)), fault, nonzero, unlikely);

    _builder.SetInsertPoint(nonzero);
    auto* extended = isSigned ? _builder.CreateAShr(low, bits - 1) : llvm::ConstantInt::get(type, 0);
    _builder.CreateCondBr(_builder.CreateICmpEQ(high, extended), narrow, wideDivide);

    _builder.SetInsertPoint(narrow);
    if (isSigned) {
        auto* overflow = _builder.CreateAnd(minusOne, _builder.CreateICmpEQ(low, llvm::ConstantInt::get(Context(), llvm::APInt::getSignedMinValue(bits))));
        auto* divide = block("divide_narrow_signed");
        _builder.CreateCondBr(overflow, fault, divide, unlikely);
        _builder.SetInsertPoint(divide);
    }
    auto* narrowQuotient = isSigned ? _builder.CreateSDiv(low, divisor) : _builder.CreateUDiv(low, divisor);
    auto* narrowRemainder = isSigned ? _builder.CreateSRem(low, divisor) : _builder.CreateURem(low, divisor);
    auto* narrowEnd = _builder.GetInsertBlock();
    _builder.CreateBr(done);

    _builder.SetInsertPoint(wideDivide);
    llvm::Value* wideQuotient = nullptr;
    llvm::Value* wideRemainder = nullptr;
    llvm::Value* fits = nullptr;
    if (isSigned) {
        auto* safeDivisor = _builder.CreateSelect(minusOne, llvm::ConstantInt::get(wide, 1), _builder.CreateSExt(divisor, wide));
        wideQuotient = _builder.CreateSelect(minusOne, _builder.CreateNeg(dividend), _builder.CreateSDiv(dividend, safeDivisor));
        wideRemainder = _builder.CreateSelect(minusOne, llvm::ConstantInt::get(wide, 0), _builder.CreateSRem(dividend, safeDivisor));
        fits = _builder.CreateICmpEQ(_builder.CreateSExt(_builder.CreateTrunc(wideQuotient, type), wide), wideQuotient);
    } else {
        fits = _builder.CreateICmpULT(high, divisor);
        auto* divide = block("divide_wide_unsigned");
        _builder.CreateCondBr(fits, divide, fault);
        _builder.SetInsertPoint(divide);
        auto* wideDivisor = _builder.CreateZExt(divisor, wide);
        wideQuotient = _builder.CreateUDiv(dividend, wideDivisor);
        wideRemainder = _builder.CreateURem(dividend, wideDivisor);
        fits = nullptr;
    }
    auto* quotientBits = _builder.CreateTrunc(wideQuotient, type);
    auto* remainderBits = _builder.CreateTrunc(wideRemainder, type);
    auto* wideEnd = _builder.GetInsertBlock();
    if (fits != nullptr) _builder.CreateCondBr(fits, done, fault);
    else _builder.CreateBr(done);

    _builder.SetInsertPoint(fault);
    Flush();
    auto callee = _module.Runtime("ps5lift_divide_error", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64()}, false));
    _builder.CreateCall(callee, {_state, _builder.getInt64(_instruction->Address)})->setDoesNotReturn();
    _builder.CreateUnreachable();

    _builder.SetInsertPoint(done);
    auto* quotient = _builder.CreatePHI(type, 2);
    quotient->addIncoming(narrowQuotient, narrowEnd);
    quotient->addIncoming(quotientBits, wideEnd);
    auto* remainder = _builder.CreatePHI(type, 2);
    remainder->addIncoming(narrowRemainder, narrowEnd);
    remainder->addIncoming(remainderBits, wideEnd);
    WriteAccumulatorPair(bits, quotient, remainder);
}

void FunctionLifter::SignedMultiply() {
    if (_instruction->Info.operand_count_visible == 1) return Multiply(true);
    const auto bits = Bits(0);
    auto* a = _instruction->Info.operand_count_visible == 3 ? Read(1) : Read(0);
    auto* b = _instruction->Info.operand_count_visible == 3 ? ReadAs(2, bits) : Read(1);
    auto* product = _builder.CreateMul(_builder.CreateSExt(a, Int(bits * 2)), _builder.CreateSExt(b, Int(bits * 2)));
    auto* result = _builder.CreateTrunc(product, Int(bits));
    auto* overflow = _builder.CreateICmpNE(_builder.CreateSExt(result, Int(bits * 2)), product);
    Write(0, result);
    SetFlag(PS5LIFT_CF, overflow);
    SetFlag(PS5LIFT_OF, overflow);
    ResultFlags(result);
}

void FunctionLifter::BitScan(const ZydisMnemonic mnemonic) {
    auto* source = Read(1);
    auto* type = source->getType();
    const auto bits = type->getIntegerBitWidth();
    auto* isZero = _builder.CreateICmpEQ(source, llvm::ConstantInt::get(type, 0));
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_BSF:
    case ZYDIS_MNEMONIC_BSR: {
        llvm::Value* index = _builder.CreateBinaryIntrinsic(llvm::Intrinsic::cttz, source, _builder.getTrue());
        if (mnemonic == ZYDIS_MNEMONIC_BSR) index = _builder.CreateSub(llvm::ConstantInt::get(type, bits - 1), _builder.CreateBinaryIntrinsic(llvm::Intrinsic::ctlz, source, _builder.getTrue()));
        Write(0, _builder.CreateSelect(isZero, Read(0), index));
        SetFlag(PS5LIFT_ZF, isZero);
        return;
    }
    case ZYDIS_MNEMONIC_TZCNT:
    case ZYDIS_MNEMONIC_LZCNT: {
        const auto intrinsic = mnemonic == ZYDIS_MNEMONIC_TZCNT ? llvm::Intrinsic::cttz : llvm::Intrinsic::ctlz;
        auto* result = _builder.CreateBinaryIntrinsic(intrinsic, source, _builder.getFalse());
        Write(0, result);
        SetFlag(PS5LIFT_CF, isZero);
        SetFlag(PS5LIFT_ZF, _builder.CreateICmpEQ(result, llvm::ConstantInt::get(type, 0)));
        return;
    }
    case ZYDIS_MNEMONIC_POPCNT: {
        Write(0, _builder.CreateUnaryIntrinsic(llvm::Intrinsic::ctpop, source));
        SetFlag(PS5LIFT_ZF, isZero);
        for (const auto flag : {PS5LIFT_CF, PS5LIFT_PF, PS5LIFT_AF, PS5LIFT_SF, PS5LIFT_OF}) SetFlag(flag, _builder.getFalse());
        return;
    }
    default: UnexpectedMnemonic("BitScan", mnemonic);
    }
}

void FunctionLifter::BitTest(const ZydisMnemonic mnemonic) {
    const auto bits = Bits(0);
    auto* type = Int(bits);
    auto* offset = ReadAs(1, bits);
    auto* mask = _builder.CreateShl(llvm::ConstantInt::get(type, 1), _builder.CreateAnd(offset, bits - 1));
    llvm::Value* value = nullptr;
    if (Operand(0).type != ZYDIS_OPERAND_TYPE_MEMORY) {
        value = Read(0);
        if (mnemonic != ZYDIS_MNEMONIC_BT) Write(0, BitUpdate(mnemonic, value, mask));
    } else if (Locked()) {
        const auto operation = mnemonic == ZYDIS_MNEMONIC_BTS ? llvm::AtomicRMWInst::Or : mnemonic == ZYDIS_MNEMONIC_BTR ? llvm::AtomicRMWInst::And : llvm::AtomicRMWInst::Xor;
        value = AtomicUpdate(BitStringAddress(offset), operation, mnemonic == ZYDIS_MNEMONIC_BTR ? _builder.CreateNot(mask) : mask);
    } else {
        auto* address = BitStringAddress(offset);
        const auto ordered = NeedsOrdering(Operand(0), bits);
        value = Load(address, bits, ordered);
        if (mnemonic != ZYDIS_MNEMONIC_BT) Store(address, BitUpdate(mnemonic, value, mask), ordered);
    }
    SetFlag(PS5LIFT_CF, _builder.CreateICmpNE(_builder.CreateAnd(value, mask), llvm::ConstantInt::get(type, 0)));
}

llvm::Value* FunctionLifter::BitUpdate(const ZydisMnemonic mnemonic, llvm::Value* value, llvm::Value* mask) {
    if (mnemonic == ZYDIS_MNEMONIC_BTS) return _builder.CreateOr(value, mask);
    if (mnemonic == ZYDIS_MNEMONIC_BTR) return _builder.CreateAnd(value, _builder.CreateNot(mask));
    return _builder.CreateXor(value, mask);
}

llvm::Value* FunctionLifter::BitStringAddress(llvm::Value* offset) {
    auto* address = Address(Operand(0));
    if (Operand(1).type != ZYDIS_OPERAND_TYPE_REGISTER) return address;
    if (_instruction->Info.address_width != 64) throw UnsupportedInstruction("32-bit bit string addresses");
    const auto bits = Bits(0);
    auto* unit = _builder.CreateSExt(_builder.CreateAShr(offset, std::countr_zero(bits)), I64());
    return _builder.CreateAdd(address, _builder.CreateShl(unit, std::countr_zero(bits / 8)));
}

llvm::Value* FunctionLifter::AtomicUpdate(const llvm::AtomicRMWInst::BinOp operation, llvm::Value* operand) {
    return AtomicUpdate(Address(Operand(0)), operation, operand);
}

llvm::Value* FunctionLifter::AtomicUpdate(llvm::Value* address, const llvm::AtomicRMWInst::BinOp operation, llvm::Value* operand) {
    auto* pointer = _builder.CreateIntToPtr(address, _module.Pointer());
    return _builder.CreateAtomicRMW(operation, pointer, operand, llvm::MaybeAlign(operand->getType()->getIntegerBitWidth() / 8), llvm::AtomicOrdering::SequentiallyConsistent);
}

bool FunctionLifter::LockedMemory() const {
    return Locked() && Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY;
}

void FunctionLifter::Exchange() {
    if (Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY || Operand(1).type == ZYDIS_OPERAND_TYPE_MEMORY) {
        const int memory = Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY ? 0 : 1;
        const int reg = 1 - memory;
        auto* old = _builder.CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, MemoryPointer(memory), Read(reg), llvm::MaybeAlign(Bits(reg) / 8), llvm::AtomicOrdering::SequentiallyConsistent);
        Write(reg, old);
        return;
    }
    auto* a = Read(0);
    auto* b = Read(1);
    Write(0, b);
    Write(1, a);
}

void FunctionLifter::ExchangeAdd() {
    auto* source = Read(1);
    llvm::Value* old = nullptr;
    if (Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY) {
        old = AtomicUpdate(llvm::AtomicRMWInst::Add, source);
        Add(old, source, _builder.getFalse());
        Write(1, old);
    } else {
        old = Read(0);
        auto* sum = Add(old, source, _builder.getFalse());
        Write(1, old);
        Write(0, sum);
    }
}

void FunctionLifter::CompareExchange() {
    const auto bits = Bits(0);
    auto* expected = LowAccumulator(bits);
    auto* desired = Read(1);
    llvm::Value* old = nullptr;
    if (Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY) {
        auto* exchange = _builder.CreateAtomicCmpXchg(MemoryPointer(0), expected, desired, llvm::MaybeAlign(bits / 8), llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
        old = _builder.CreateExtractValue(exchange, 0);
    } else {
        old = Read(0);
        Write(0, _builder.CreateSelect(_builder.CreateICmpEQ(old, expected), desired, old));
    }
    Sub(expected, old, _builder.getFalse());
    auto* equal = _builder.CreateICmpEQ(old, expected);
    if (bits == 32) return SetGpr(PS5LIFT_RAX, _builder.CreateSelect(equal, Gpr(PS5LIFT_RAX), _builder.CreateZExt(old, I64())));
    WriteRegister(bits == 64 ? ZYDIS_REGISTER_RAX : bits == 16 ? ZYDIS_REGISTER_AX : ZYDIS_REGISTER_AL, old);
}

void FunctionLifter::CompareExchangeWide() {
    const auto bits = Bits(0);
    const auto half = bits / 2;
    const auto pair = [&](const int high, const int low) {
        auto* upper = _builder.CreateZExt(_builder.CreateTrunc(Gpr(high), Int(half)), Int(bits));
        return _builder.CreateOr(_builder.CreateShl(upper, half), _builder.CreateZExt(_builder.CreateTrunc(Gpr(low), Int(half)), Int(bits)));
    };
    auto* exchange = _builder.CreateAtomicCmpXchg(MemoryPointer(0), pair(PS5LIFT_RDX, PS5LIFT_RAX), pair(PS5LIFT_RCX, PS5LIFT_RBX), llvm::MaybeAlign(bits / 8),
                                                  llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
    auto* old = _builder.CreateExtractValue(exchange, 0);
    auto* matched = _builder.CreateExtractValue(exchange, 1);
    SetFlag(PS5LIFT_ZF, matched);
    auto* low = _builder.CreateZExt(_builder.CreateTrunc(old, Int(half)), I64());
    auto* high = _builder.CreateZExt(_builder.CreateTrunc(_builder.CreateLShr(old, half), Int(half)), I64());
    SetGpr(PS5LIFT_RAX, _builder.CreateSelect(matched, Gpr(PS5LIFT_RAX), low));
    SetGpr(PS5LIFT_RDX, _builder.CreateSelect(matched, Gpr(PS5LIFT_RDX), high));
}

void FunctionLifter::SignExtendAccumulator(const ZydisMnemonic mnemonic) {
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_CBW: WriteRegister(ZYDIS_REGISTER_AX, _builder.CreateSExt(LowAccumulator(8), _builder.getInt16Ty())); return;
    case ZYDIS_MNEMONIC_CWDE: WriteRegister(ZYDIS_REGISTER_EAX, _builder.CreateSExt(LowAccumulator(16), _builder.getInt32Ty())); return;
    case ZYDIS_MNEMONIC_CDQE: SetGpr(PS5LIFT_RAX, _builder.CreateSExt(LowAccumulator(32), I64())); return;
    case ZYDIS_MNEMONIC_CWD: WriteRegister(ZYDIS_REGISTER_DX, _builder.CreateAShr(LowAccumulator(16), 15)); return;
    case ZYDIS_MNEMONIC_CDQ: WriteRegister(ZYDIS_REGISTER_EDX, _builder.CreateAShr(LowAccumulator(32), 31)); return;
    case ZYDIS_MNEMONIC_CQO: SetGpr(PS5LIFT_RDX, _builder.CreateAShr(Gpr(PS5LIFT_RAX), 63)); return;
    default: UnexpectedMnemonic("SignExtendAccumulator", mnemonic);
    }
}

void FunctionLifter::BitManipulation(const ZydisMnemonic mnemonic) {
    const auto bits = Bits(0);
    auto* type = Int(bits);
    auto* zero = llvm::ConstantInt::get(type, 0);
    auto* countMask = llvm::ConstantInt::get(type, bits - 1);
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_ANDN: {
        auto* result = _builder.CreateAnd(_builder.CreateNot(Read(1)), Read(2));
        Write(0, result);
        LogicFlags(result);
        return;
    }
    case ZYDIS_MNEMONIC_SHLX:
    case ZYDIS_MNEMONIC_SHRX:
    case ZYDIS_MNEMONIC_SARX: {
        auto* count = _builder.CreateAnd(ReadAs(2, bits), countMask);
        auto* value = Read(1);
        auto* result = mnemonic == ZYDIS_MNEMONIC_SHLX ? _builder.CreateShl(value, count) : mnemonic == ZYDIS_MNEMONIC_SHRX ? _builder.CreateLShr(value, count) : _builder.CreateAShr(value, count);
        Write(0, result);
        return;
    }
    case ZYDIS_MNEMONIC_RORX: {
        auto* count = _builder.CreateAnd(ReadAs(2, bits), countMask);
        auto* value = Read(1);
        Write(0, _builder.CreateIntrinsic(llvm::Intrinsic::fshr, {type}, {value, value, count}));
        return;
    }
    case ZYDIS_MNEMONIC_BEXTR: {
        auto* control = ReadAs(2, bits);
        auto* start = _builder.CreateAnd(control, llvm::ConstantInt::get(type, 0xff));
        auto* length = _builder.CreateAnd(_builder.CreateLShr(control, 8), llvm::ConstantInt::get(type, 0xff));
        auto* wide = _builder.CreateZExt(Read(1), Int(512));
        auto* shifted = _builder.CreateLShr(wide, _builder.CreateZExt(start, Int(512)));
        auto* mask = _builder.CreateSub(_builder.CreateShl(llvm::ConstantInt::get(Int(512), 1), _builder.CreateZExt(length, Int(512))), llvm::ConstantInt::get(Int(512), 1));
        auto* result = _builder.CreateTrunc(_builder.CreateAnd(shifted, mask), type);
        Write(0, result);
        LogicFlags(result);
        return;
    }
    case ZYDIS_MNEMONIC_BZHI: {
        auto* index = _builder.CreateAnd(ReadAs(2, bits), llvm::ConstantInt::get(type, 0xff));
        auto* value = Read(1);
        auto* inRange = _builder.CreateICmpULT(index, llvm::ConstantInt::get(type, bits));
        auto* mask = _builder.CreateSub(_builder.CreateShl(llvm::ConstantInt::get(type, 1), _builder.CreateAnd(index, countMask)), llvm::ConstantInt::get(type, 1));
        auto* result = _builder.CreateSelect(inRange, _builder.CreateAnd(value, mask), value);
        Write(0, result);
        LogicFlags(result);
        SetFlag(PS5LIFT_CF, _builder.CreateNot(inRange));
        return;
    }
    case ZYDIS_MNEMONIC_BLSR:
    case ZYDIS_MNEMONIC_BLSI:
    case ZYDIS_MNEMONIC_BLSMSK: {
        auto* value = Read(1);
        auto* minusOne = _builder.CreateSub(value, llvm::ConstantInt::get(type, 1));
        llvm::Value* result = nullptr;
        if (mnemonic == ZYDIS_MNEMONIC_BLSR) result = _builder.CreateAnd(value, minusOne);
        else if (mnemonic == ZYDIS_MNEMONIC_BLSI) result = _builder.CreateAnd(value, _builder.CreateNeg(value));
        else result = _builder.CreateXor(value, minusOne);
        Write(0, result);
        ResultFlags(result);
        SetFlag(PS5LIFT_OF, _builder.getFalse());
        if (mnemonic == ZYDIS_MNEMONIC_BLSI) SetFlag(PS5LIFT_CF, _builder.CreateICmpNE(value, zero));
        else SetFlag(PS5LIFT_CF, _builder.CreateICmpEQ(value, zero));
        if (mnemonic == ZYDIS_MNEMONIC_BLSMSK) SetFlag(PS5LIFT_ZF, _builder.getFalse());
        return;
    }
    case ZYDIS_MNEMONIC_MULX: {
        auto* product = _builder.CreateMul(_builder.CreateZExt(Read(2), Int(bits * 2)), _builder.CreateZExt(LowRdx(bits), Int(bits * 2)));
        auto* low = _builder.CreateTrunc(product, type);
        auto* high = _builder.CreateTrunc(_builder.CreateLShr(product, bits), type);
        Write(1, low);
        Write(0, high);
        return;
    }
    default: UnexpectedMnemonic("BitManipulation", mnemonic);
    }
}

llvm::Value* FunctionLifter::LowRdx(const unsigned bits) {
    return _builder.CreateTrunc(Gpr(PS5LIFT_RDX), Int(bits));
}

void FunctionLifter::Addition() {
    if (LockedMemory()) {
        auto* source = ReadAs(1, Bits(0));
        Add(AtomicUpdate(llvm::AtomicRMWInst::Add, source), source, _builder.getFalse());
        return;
    }
    Write(0, Add(Read(0), ReadAs(1, Bits(0)), _builder.getFalse()));
}

void FunctionLifter::CarryArithmetic(const bool add) {
    auto* source = ReadAs(1, Bits(0));
    auto* carry = Flag(PS5LIFT_CF);
    if (LockedMemory()) {
        auto* old = AtomicUpdate(add ? llvm::AtomicRMWInst::Add : llvm::AtomicRMWInst::Sub, _builder.CreateAdd(source, _builder.CreateZExt(carry, source->getType())));
        if (add) Add(old, source, carry);
        else Sub(old, source, carry);
        return;
    }
    auto* value = Read(0);
    Write(0, add ? Add(value, source, carry) : Sub(value, source, carry));
}

void FunctionLifter::Subtraction() {
    if (LockedMemory()) {
        auto* source = ReadAs(1, Bits(0));
        Sub(AtomicUpdate(llvm::AtomicRMWInst::Sub, source), source, _builder.getFalse());
        return;
    }
    Write(0, Sub(Read(0), ReadAs(1, Bits(0)), _builder.getFalse()));
}

void FunctionLifter::Logic(const llvm::Instruction::BinaryOps operation, const llvm::AtomicRMWInst::BinOp atomic) {
    if (LockedMemory()) {
        auto* source = ReadAs(1, Bits(0));
        LogicFlags(_builder.CreateBinOp(operation, AtomicUpdate(atomic, source), source));
        return;
    }
    auto* result = _builder.CreateBinOp(operation, Read(0), ReadAs(1, Bits(0)));
    LogicFlags(result);
    Write(0, result);
}

void FunctionLifter::Increment() {
    if (LockedMemory()) {
        auto* one = Const(Bits(0), 1);
        Add(AtomicUpdate(llvm::AtomicRMWInst::Add, one), one, _builder.getFalse(), false);
        return;
    }
    Write(0, Add(Read(0), Const(Bits(0), 1), _builder.getFalse(), false));
}

void FunctionLifter::Decrement() {
    if (LockedMemory()) {
        auto* one = Const(Bits(0), 1);
        Sub(AtomicUpdate(llvm::AtomicRMWInst::Sub, one), one, _builder.getFalse(), false);
        return;
    }
    Write(0, Sub(Read(0), Const(Bits(0), 1), _builder.getFalse(), false));
}

void FunctionLifter::Negation() {
    auto* zero = llvm::ConstantInt::get(Int(Bits(0)), 0);
    auto* value = LockedMemory() ? AtomicLoop(Bits(0), [&](llvm::Value* old) { return _builder.CreateSub(zero, old); }) : Read(0);
    auto* result = Sub(zero, value, _builder.getFalse());
    SetFlag(PS5LIFT_CF, _builder.CreateICmpNE(value, zero));
    if (!LockedMemory()) Write(0, result);
}

void FunctionLifter::Complement() {
    if (LockedMemory()) {
        AtomicUpdate(llvm::AtomicRMWInst::Xor, llvm::ConstantInt::getAllOnesValue(Int(Bits(0))));
        return;
    }
    Write(0, _builder.CreateNot(Read(0)));
}

void FunctionLifter::ByteSwap() {
    if (Bits(0) == 16) throw UnsupportedInstruction("16-bit");
    Write(0, _builder.CreateUnaryIntrinsic(llvm::Intrinsic::bswap, Read(0)));
}

}
