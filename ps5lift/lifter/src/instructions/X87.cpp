#include "../FunctionLifter.hpp"

namespace Ps5Lift::Lifting {

llvm::Value* FunctionLifter::CallX87(const std::uint32_t operation, const std::uint32_t format, llvm::Value* address, const std::uint32_t index) {
    const auto* memory = address ? X87Memory() : nullptr;
    const bool ordered = memory != nullptr && _module.Options().Memory == MemoryModel::Tso && !InPrivateFrame(*memory) && !InReadOnlySegment(*memory, memory->size);
    if (ordered) _builder.CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
    auto callee = _module.Runtime("ps5lift_x87", llvm::FunctionType::get(_builder.getInt32Ty(), {_module.Pointer(), _builder.getInt32Ty(), _builder.getInt32Ty(), I64(), _builder.getInt32Ty()}, false));
    auto* result = _builder.CreateCall(callee, {_state, _builder.getInt32(operation), _builder.getInt32(format), address ? address : _builder.getInt64(0), _builder.getInt32(index)});
    if (ordered) _builder.CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
    return result;
}

bool FunctionLifter::IsStackRegister(const ZydisDecodedOperand& operand) {
    return operand.type == ZYDIS_OPERAND_TYPE_REGISTER && operand.reg.value >= ZYDIS_REGISTER_ST0 && operand.reg.value <= ZYDIS_REGISTER_ST7;
}

std::uint32_t FunctionLifter::StackIndex(const ZydisDecodedOperand& operand) {
    return static_cast<std::uint32_t>(operand.reg.value - ZYDIS_REGISTER_ST0);
}

const ZydisDecodedOperand* FunctionLifter::X87Memory() const {
    for (unsigned i = 0; i < _instruction->Info.operand_count_visible; ++i) {
        if (Operand(static_cast<int>(i)).type == ZYDIS_OPERAND_TYPE_MEMORY) return &Operand(static_cast<int>(i));
    }
    return nullptr;
}

std::uint32_t FunctionLifter::X87Format(const bool integer) const {
    const auto* memory = X87Memory();
    if (memory == nullptr) return PS5LIFT_X87_REGISTER;
    switch (memory->size) {
    case 16: return integer ? PS5LIFT_X87_INTEGER16 : throw UnsupportedInstruction("16-bit x87 float");
    case 32: return integer ? PS5LIFT_X87_INTEGER32 : PS5LIFT_X87_FLOAT32;
    case 64: return integer ? PS5LIFT_X87_INTEGER64 : PS5LIFT_X87_FLOAT64;
    case 80: return PS5LIFT_X87_FLOAT80;
    default: throw UnsupportedInstruction("x87 operand size");
    }
}

std::uint32_t FunctionLifter::X87Index(const std::uint32_t fallback) const {
    std::uint32_t index = fallback;
    bool found = false;
    for (unsigned i = 0; i < _instruction->Info.operand_count_visible; ++i) {
        const auto& operand = Operand(static_cast<int>(i));
        if (!IsStackRegister(operand)) continue;
        if (StackIndex(operand) != 0 || !found) index = StackIndex(operand);
        found = found || StackIndex(operand) != 0;
    }
    return index;
}

bool FunctionLifter::X87ToRegister() const {
    return _instruction->Info.operand_count_visible >= 2 && IsStackRegister(Operand(0)) && StackIndex(Operand(0)) != 0;
}

void FunctionLifter::X87CompareFlags(llvm::Value* result) {
    auto* unordered = _builder.CreateICmpEQ(result, _builder.getInt32(PS5LIFT_X87_UNORDERED));
    ComparisonFlags(unordered, [&] { return _builder.CreateOr(unordered, _builder.CreateICmpEQ(result, _builder.getInt32(PS5LIFT_X87_EQUAL))); },
                    [&] { return _builder.CreateOr(unordered, _builder.CreateICmpEQ(result, _builder.getInt32(PS5LIFT_X87_LESS))); });
}

llvm::Value* FunctionLifter::X87Address() {
    const auto* memory = X87Memory();
    return memory ? Address(*memory) : nullptr;
}

void FunctionLifter::X87Arithmetic(const std::uint32_t operation, const bool integer, const bool pop) {
    std::uint32_t format = X87Format(integer);
    if (format == PS5LIFT_X87_REGISTER && (X87ToRegister() || pop)) format |= PS5LIFT_X87_TO_REGISTER;
    if (pop) format |= PS5LIFT_X87_POP;
    CallX87(operation, format, X87Address(), X87Index(1));
}

void FunctionLifter::X87StoreStatus() {
    Write(0, _builder.CreateTrunc(CallX87(PS5LIFT_X87_STORE_STATUS, 0, nullptr, 0), _builder.getInt16Ty()));
}

void FunctionLifter::X87Environment(const bool store) {
    if (Operand(0).size != 224) throw UnsupportedInstruction("16-bit environment");
    CallX87(store ? PS5LIFT_X87_STORE_ENVIRONMENT : PS5LIFT_X87_LOAD_ENVIRONMENT, 0, X87Address(), 0);
}

void FunctionLifter::X87SaveState() {
    Flush();
    CallX87(PS5LIFT_X87_SAVE_STATE, 0, X87Address(), 0);
}

void FunctionLifter::X87RestoreState() {
    Flush();
    CallX87(PS5LIFT_X87_RESTORE_STATE, 0, X87Address(), 0);
    Reload();
}

void FunctionLifter::X87ConditionalMove(const ZydisMnemonic mnemonic) {
    llvm::Value* condition = nullptr;
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_FCMOVB: condition = Flag(PS5LIFT_CF); break;
    case ZYDIS_MNEMONIC_FCMOVE: condition = Flag(PS5LIFT_ZF); break;
    case ZYDIS_MNEMONIC_FCMOVBE: condition = _builder.CreateOr(Flag(PS5LIFT_CF), Flag(PS5LIFT_ZF)); break;
    case ZYDIS_MNEMONIC_FCMOVU: condition = Flag(PS5LIFT_PF); break;
    case ZYDIS_MNEMONIC_FCMOVNB: condition = _builder.CreateNot(Flag(PS5LIFT_CF)); break;
    case ZYDIS_MNEMONIC_FCMOVNE: condition = _builder.CreateNot(Flag(PS5LIFT_ZF)); break;
    case ZYDIS_MNEMONIC_FCMOVNBE: condition = _builder.CreateNot(_builder.CreateOr(Flag(PS5LIFT_CF), Flag(PS5LIFT_ZF))); break;
    case ZYDIS_MNEMONIC_FCMOVNU: condition = _builder.CreateNot(Flag(PS5LIFT_PF)); break;
    default: UnexpectedMnemonic("X87ConditionalMove", mnemonic);
    }
    auto* move = llvm::BasicBlock::Create(Context(), "fcmov", _host);
    auto* after = llvm::BasicBlock::Create(Context(), "fcmov_done", _host);
    _builder.CreateCondBr(condition, move, after);
    _builder.SetInsertPoint(move);
    CallX87(PS5LIFT_X87_MOVE_REGISTER, 0, nullptr, X87Index(1));
    _builder.CreateBr(after);
    _builder.SetInsertPoint(after);
}

}
