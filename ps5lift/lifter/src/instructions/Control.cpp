#include "../FunctionLifter.hpp"

#include <optional>
#include <set>

namespace Ps5Lift::Lifting {

llvm::Value* FunctionLifter::Loop(const ZydisMnemonic mnemonic) {
    const auto counter = _instruction->Info.address_width == 64 ? ZYDIS_REGISTER_RCX : ZYDIS_REGISTER_ECX;
    auto* count = _builder.CreateSub(ReadRegister(counter), llvm::ConstantInt::get(Int(_instruction->Info.address_width), 1));
    WriteRegister(counter, count);
    auto* running = _builder.CreateICmpNE(count, llvm::ConstantInt::get(count->getType(), 0));
    if (mnemonic == ZYDIS_MNEMONIC_LOOPE) return _builder.CreateAnd(running, Flag(PS5LIFT_ZF));
    if (mnemonic == ZYDIS_MNEMONIC_LOOPNE) return _builder.CreateAnd(running, _builder.CreateNot(Flag(PS5LIFT_ZF)));
    return running;
}

void FunctionLifter::Branch(llvm::Value* condition) {
    const auto target = *DirectTarget(*_instruction);
    _builder.CreateCondBr(condition, Target(target), Target(_instruction->Next()));
}

llvm::Value* FunctionLifter::CounterIsZero(const unsigned bits) {
    return _builder.CreateICmpEQ(_builder.CreateTrunc(Gpr(PS5LIFT_RCX), Int(bits)), Const(bits, 0));
}

void FunctionLifter::ConditionalMove(const Condition condition) {
    Write(0, _builder.CreateSelect(Test(condition), Read(1), Read(0)));
}

void FunctionLifter::SetCondition(const Condition condition) {
    Write(0, _builder.CreateZExt(Test(condition), _builder.getInt8Ty()));
}

void FunctionLifter::Leave() {
    if (_instruction->Info.operand_width != 64) throw UnsupportedInstruction("16-bit");
    SetGpr(PS5LIFT_RSP, Gpr(PS5LIFT_RBP));
    SetGpr(PS5LIFT_RBP, Pop());
}

void FunctionLifter::Call() {
    auto target = DirectTarget(*_instruction);
    if (!target) target = ImportTarget(_module.Image(), *_instruction);
    auto* destination = target ? nullptr : Read(0);
    Push(_builder.getInt64(_instruction->Next()));
    if (target) {
        auto* callee = _module.Guest(*target);
        CallGuest(callee, _module.Functions().at(*target).Effects);
    } else {
        CallGuest(DispatchCallee().getCallee(), {EveryRegister, EveryRegister}, {destination});
    }
}

void FunctionLifter::Jump() {
    const auto target = DirectTarget(*_instruction);
    if (const auto import = target ? std::nullopt : ImportTarget(_module.Image(), *_instruction)) {
        TailCall(*import);
        return;
    }
    if (target) {
        _builder.CreateBr(Target(*target));
        return;
    }
    auto* destination = Read(0);
    const auto table = _function.JumpTables.find(_instruction->Address);
    auto* fallback = llvm::BasicBlock::Create(Context(), "dispatch", _host);
    if (table != _function.JumpTables.end()) {
        auto* selector = _builder.CreateSwitch(destination, fallback, static_cast<unsigned>(table->second.size()));
        std::set<std::uint64_t> seen;
        for (const auto target : table->second) {
            if (seen.insert(target).second) selector->addCase(_builder.getInt64(target), Target(target));
        }
    } else {
        _builder.CreateBr(fallback);
    }
    _builder.SetInsertPoint(fallback);
    if (_function.ClosedJumpTables.contains(_instruction->Address)) {
        auto left = _module.Runtime("ps5lift_left_jump_table", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64()}, false));
        _builder.CreateCall(left, {_state, _builder.getInt64(_instruction->Address)})->setDoesNotReturn();
        _builder.CreateUnreachable();
        return;
    }
    Flush();
    auto resolve = _module.Runtime("ps5lift_resolve_jump", llvm::FunctionType::get(_module.Pointer(), {_module.Pointer(), I64()}, false));
    JumpTo(_builder.CreateCall(resolve, {_state, destination}));
}

void FunctionLifter::Return() {
    auto* rsp = Gpr(PS5LIFT_RSP);
    auto* popped = _builder.CreateAdd(rsp, _builder.getInt64(8 + (_instruction->Info.operand_count_visible == 1 ? Operand(0).imm.value.u : 0)));
    if (_assignsStackPointer) ResumeIfStackSwitched(rsp, popped);
    StateStore(rsp, offsetof(Ps5LiftState, ReturnSlot));
    SetGpr(PS5LIFT_RSP, popped);
    Flush();
    _builder.CreateRetVoid();
}

void FunctionLifter::Interrupt() {
    Flush();
    auto callee = _module.Runtime("ps5lift_interrupt", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64(), _builder.getInt32Ty()}, false));
    _builder.CreateCall(callee, {_state, _builder.getInt64(_instruction->Address), _builder.getInt32(static_cast<std::uint32_t>(Immediate(0)))})->setDoesNotReturn();
    _builder.CreateUnreachable();
}

}
