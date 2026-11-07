#include "../FunctionLifter.hpp"

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

namespace Ps5Lift::Lifting {

void FunctionLifter::StringOp(const Ps5LiftStringOperation operation, const unsigned width) {
    if (_instruction->Info.address_width != 64) throw UnsupportedInstruction("32-bit string addresses");
    llvm::Value* sourceBase = _builder.getInt64(0);
    if (operation == PS5LIFT_STRING_MOVS) {
        const auto& source = Operand(1);
        if (source.type != ZYDIS_OPERAND_TYPE_MEMORY || source.mem.base != ZYDIS_REGISTER_RSI) throw std::logic_error("movs without its rsi source");
        if (source.mem.segment == ZYDIS_REGISTER_FS) sourceBase = StateLoad(I64(), offsetof(Ps5LiftState, FsBase));
        if (source.mem.segment == ZYDIS_REGISTER_GS) sourceBase = StateLoad(I64(), offsetof(Ps5LiftState, GsBase));
    }
    const bool ordered = _module.Options().Memory == MemoryModel::Tso;
    Flush();
    if (ordered) _builder.CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
    auto callee = _module.Runtime("ps5lift_string_op", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), _builder.getInt32Ty(), _builder.getInt32Ty(), _builder.getInt32Ty(), I64()}, false));
    const bool repeat = (_instruction->Info.attributes & ZYDIS_ATTRIB_HAS_REP) != 0;
    _builder.CreateCall(callee, {_state, _builder.getInt32(static_cast<std::uint32_t>(operation)), _builder.getInt32(width), _builder.getInt32(repeat ? 1 : 0), sourceBase});
    if (ordered) _builder.CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
    for (const int index : {PS5LIFT_RCX, PS5LIFT_RSI, PS5LIFT_RDI}) {
        if (_gpr[index]) _builder.CreateStore(StateLoad(I64(), offsetof(Ps5LiftState, Gpr) + 8 * static_cast<std::size_t>(index)), _gpr[index]);
    }
}

void FunctionLifter::ReadString(const StringRead kind, const unsigned width) {
    if (_instruction->Info.address_width != 64) throw UnsupportedInstruction("32-bit string addresses");
    const bool load = kind == StringRead::Load;
    const bool scan = kind == StringRead::Scan;
    const auto string = [&](const ZydisDecodedOperand& operand, const ZydisRegister base) {
        return operand.type == ZYDIS_OPERAND_TYPE_MEMORY && operand.mem.base == base && operand.size == width * 8;
    };
    const bool accumulator = Operand(0).type == ZYDIS_OPERAND_TYPE_REGISTER && Operand(0).size == width * 8;
    if (!(kind == StringRead::Compare ? string(Operand(0), ZYDIS_REGISTER_RSI) && string(Operand(1), ZYDIS_REGISTER_RDI)
                                      : accumulator && string(Operand(1), scan ? ZYDIS_REGISTER_RDI : ZYDIS_REGISTER_RSI)))
        throw std::logic_error("string instruction without its implicit operands");
    const auto attributes = _instruction->Info.attributes;
    const bool repeat = (attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE | ZYDIS_ATTRIB_HAS_REPNE)) != 0;
    const auto step = [&] { return _builder.CreateSelect(Flag(PS5LIFT_DF), _builder.getInt64(-static_cast<std::int64_t>(width)), _builder.getInt64(width)); };
    const auto element = [&] {
        if (load) {
            Write(0, Read(1));
            SetGpr(PS5LIFT_RSI, _builder.CreateAdd(Gpr(PS5LIFT_RSI), step()));
            return;
        }
        Sub(Read(0), Read(1), _builder.getFalse());
        if (!scan) SetGpr(PS5LIFT_RSI, _builder.CreateAdd(Gpr(PS5LIFT_RSI), step()));
        SetGpr(PS5LIFT_RDI, _builder.CreateAdd(Gpr(PS5LIFT_RDI), step()));
    };
    if (!repeat) return element();
    auto* body = llvm::BasicBlock::Create(Context(), "string", _host);
    auto* done = llvm::BasicBlock::Create(Context(), "string_done", _host);
    _builder.CreateCondBr(_builder.CreateICmpEQ(Gpr(PS5LIFT_RCX), _builder.getInt64(0)), done, body);
    _builder.SetInsertPoint(body);
    element();
    auto* remaining = _builder.CreateSub(Gpr(PS5LIFT_RCX), _builder.getInt64(1));
    SetGpr(PS5LIFT_RCX, remaining);
    llvm::Value* again = _builder.CreateICmpNE(remaining, _builder.getInt64(0));
    if (!load && (attributes & ZYDIS_ATTRIB_HAS_REPNE)) again = _builder.CreateAnd(again, _builder.CreateNot(Flag(PS5LIFT_ZF)));
    else if (!load) again = _builder.CreateAnd(again, Flag(PS5LIFT_ZF));
    _builder.CreateCondBr(again, body, done);
    _builder.SetInsertPoint(done);
}

void FunctionLifter::StringCompare(const bool explicitLengths, const bool index) {
    const auto control = static_cast<std::uint32_t>(Immediate(2)) & 0x7f;
    const unsigned elements = (control & 1) ? 8 : 16;
    auto* first = ReadAs(0, 128);
    auto* second = ReadAs(1, 128);
    const auto half = [&](llvm::Value* value, const unsigned part) { return _builder.CreateTrunc(_builder.CreateLShr(value, 64 * part), I64()); };
    const auto length = [&](std::initializer_list<ZydisRegister> candidates) { return _builder.CreateSExt(ReadRegister(HiddenRegister(candidates)), I64()); };
    auto* zero = _builder.getInt64(0);
    auto* firstLength = explicitLengths ? length({ZYDIS_REGISTER_EAX, ZYDIS_REGISTER_RAX}) : zero;
    auto* secondLength = explicitLengths ? length({ZYDIS_REGISTER_EDX, ZYDIS_REGISTER_RDX}) : zero;
    auto* type = llvm::FunctionType::get(_builder.getInt32Ty(), {I64(), I64(), I64(), I64(), I64(), I64(), _builder.getInt32Ty()}, false);
    auto* call = _builder.CreateCall(_module.Runtime("ps5lift_string_compare", type),
                                     {half(first, 0), half(first, 1), half(second, 0), half(second, 1), firstLength, secondLength,
                                      _builder.getInt32(control | (explicitLengths ? 0u : static_cast<std::uint32_t>(PS5LIFT_STRING_IMPLICIT)))});
    call->setDoesNotAccessMemory();
    call->setDoesNotThrow();
    auto* matches = _builder.CreateAnd(call, 0xffff);
    SetFlag(PS5LIFT_CF, _builder.CreateICmpNE(matches, _builder.getInt32(0)));
    SetFlag(PS5LIFT_OF, _builder.CreateTrunc(matches, _builder.getInt1Ty()));
    SetFlag(PS5LIFT_ZF, _builder.CreateICmpNE(_builder.CreateAnd(call, PS5LIFT_STRING_SHORT_SECOND), _builder.getInt32(0)));
    SetFlag(PS5LIFT_SF, _builder.CreateICmpNE(_builder.CreateAnd(call, PS5LIFT_STRING_SHORT_FIRST), _builder.getInt32(0)));
    SetFlag(PS5LIFT_AF, _builder.getFalse());
    SetFlag(PS5LIFT_PF, _builder.getFalse());
    if (index) {
        const auto counter = HiddenRegister({ZYDIS_REGISTER_ECX, ZYDIS_REGISTER_RCX});
        auto* position = (control & 0x40) ? _builder.CreateSub(_builder.getInt32(31), _builder.CreateBinaryIntrinsic(llvm::Intrinsic::ctlz, matches, _builder.getFalse()))
                                          : _builder.CreateBinaryIntrinsic(llvm::Intrinsic::cttz, matches, _builder.getFalse());
        position = _builder.CreateSelect(_builder.CreateICmpEQ(matches, _builder.getInt32(0)), _builder.getInt32(elements), position);
        WriteRegister(counter, _builder.CreateZExt(position, Int(ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, counter))));
        return;
    }
    llvm::Value* mask = _builder.CreateZExt(matches, Int(128));
    if (control & 0x40) {
        auto* bits = _builder.CreateBitCast(_builder.CreateTrunc(matches, Int(elements)), llvm::FixedVectorType::get(_builder.getInt1Ty(), elements));
        mask = _builder.CreateBitCast(_builder.CreateSExt(bits, llvm::FixedVectorType::get(Int(128 / elements), elements)), Int(128));
    }
    WriteRegister(ZYDIS_REGISTER_XMM0, mask);
}

}
