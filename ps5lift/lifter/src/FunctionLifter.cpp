#include "FunctionLifter.hpp"

#include "LlvmCompatibility.hpp"

#include <lifter/Hex.hpp>

#include <llvm/IR/MDBuilder.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Ps5Lift::Lifting {

[[noreturn]] void UnexpectedMnemonic(const char* function, const ZydisMnemonic mnemonic) {
    throw std::logic_error(std::string(function) + " does not lift " + ZydisMnemonicGetString(mnemonic));
}

int GprIndex(const ZydisRegister reg) {
    const auto enclosing = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg);
    if (enclosing < ZYDIS_REGISTER_RAX || enclosing > ZYDIS_REGISTER_R15) return -1;
    return enclosing - ZYDIS_REGISTER_RAX;
}

int VectorIndex(const ZydisRegister reg) {
    if (reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM15) return reg - ZYDIS_REGISTER_XMM0;
    if (reg >= ZYDIS_REGISTER_YMM0 && reg <= ZYDIS_REGISTER_YMM15) return reg - ZYDIS_REGISTER_YMM0;
    return -1;
}

int MmxIndex(const ZydisRegister reg) {
    if (reg >= ZYDIS_REGISTER_MM0 && reg <= ZYDIS_REGISTER_MM7) return reg - ZYDIS_REGISTER_MM0;
    return -1;
}

bool IsHighByte(const ZydisRegister reg) {
    return reg == ZYDIS_REGISTER_AH || reg == ZYDIS_REGISTER_CH || reg == ZYDIS_REGISTER_DH || reg == ZYDIS_REGISTER_BH;
}

bool AssignsStackPointer(const Instruction& instruction) {
    switch (instruction.Info.mnemonic) {
    case ZYDIS_MNEMONIC_ADD:
    case ZYDIS_MNEMONIC_SUB:
    case ZYDIS_MNEMONIC_AND:
        return false;
    case ZYDIS_MNEMONIC_LEA:
        if (instruction.Operands[1].mem.base == ZYDIS_REGISTER_RSP) return false;
        break;
    default:
        break;
    }
    for (unsigned i = 0; i < instruction.Info.operand_count_visible; ++i) {
        const auto& operand = instruction.Operands[i];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && operand.reg.value == ZYDIS_REGISTER_RSP && (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0) return true;
    }
    return false;
}

bool IsRegister(const ZydisDecodedOperand& operand, const ZydisRegister reg) {
    return operand.type == ZYDIS_OPERAND_TYPE_REGISTER && operand.reg.value == reg;
}

bool WritesGpr(const Instruction& instruction, const int gpr) {
    for (unsigned i = 0; i < instruction.Info.operand_count; ++i) {
        const auto& operand = instruction.Operands[i];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && GprIndex(operand.reg.value) == gpr && (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0) return true;
    }
    return false;
}

void FunctionLifter::Lift() {
    auto* entry = llvm::BasicBlock::Create(Context(), "entry", _host);
    _builder.SetInsertPoint(entry);
    _state = _host->getArg(0);
    ScanInstructions();
    for (int i = 0; i < PS5LIFT_GPR_COUNT; ++i) {
        if (_usedGprs[i]) _gpr[i] = _builder.CreateAlloca(I64(), nullptr, "gpr" + std::to_string(i));
    }
    for (int i = 0; i < PS5LIFT_FLAG_COUNT; ++i) {
        if (_usesFlags) _flags[i] = _builder.CreateAlloca(_builder.getInt1Ty(), nullptr, "flag" + std::to_string(i));
    }
    for (int i = 0; i < VectorCount; ++i) {
        if (_usedVectors[i]) _vectors[i] = _builder.CreateAlloca(Int(256), nullptr, "ymm" + std::to_string(i));
    }
    Reload();
    _entryRsp = Gpr(PS5LIFT_RSP);
    for (const auto leader : _function.Leaders) {
        if (!_function.Instructions.contains(leader)) throw std::logic_error("The block at 0x" + Hex(leader) + " in the function at 0x" + Hex(_function.Entry) + " has no instruction");
        _blocks[leader] = llvm::BasicBlock::Create(Context(), "b_" + Hex(leader), _host);
    }
    _builder.CreateBr(_blocks.at(_function.Entry));

    std::optional<std::uint64_t> expected;
    for (const auto& [address, instruction] : _function.Instructions) {
        if (expected && *expected != address && !Terminated()) {
            if (!_blocks.contains(*expected)) throw std::logic_error("The instruction before 0x" + Hex(address) + " continues at 0x" + Hex(*expected) + ", which has no block");
            _builder.CreateBr(_blocks.at(*expected));
        }
        if (_blocks.contains(address)) {
            if (!Terminated()) _builder.CreateBr(_blocks.at(address));
            _builder.SetInsertPoint(_blocks.at(address));
        } else if (!expected || *expected != address || Terminated()) {
            StartDeadBlock();
        }
        _instruction = &instruction;
        EmitInstruction(instruction);
        ++_module.Report().Instructions;
        expected = instruction.Next();
        if (!Terminated() && !_function.Instructions.contains(instruction.Next())) {
            TailCall(instruction.Next());
        }
    }
    for (auto& block : *_host) {
        if (!HasTerminator(block)) {
            _builder.SetInsertPoint(&block);
            _builder.CreateUnreachable();
        }
    }
}

llvm::ConstantInt* FunctionLifter::Const(const unsigned bits, const std::uint64_t value) {
    return llvm::ConstantInt::get(Context(), llvm::APInt(bits, bits >= 64 ? value : value & ((std::uint64_t{1} << bits) - 1)));
}

llvm::Value* FunctionLifter::StateField(const std::size_t offset) {
    return _builder.CreateConstGEP1_64(_builder.getInt8Ty(), _state, offset);
}

llvm::Align FunctionLifter::StateAlignment(const std::size_t offset) {
    return llvm::commonAlignment(llvm::Align(alignof(Ps5LiftState)), offset);
}

llvm::Value* FunctionLifter::StateLoad(llvm::Type* type, const std::size_t offset) {
    auto* load = _builder.CreateAlignedLoad(type, StateField(offset), StateAlignment(offset));
    load->setMetadata(llvm::LLVMContext::MD_tbaa, _module.StateTag());
    return load;
}

void FunctionLifter::StateStore(llvm::Value* value, const std::size_t offset) {
    _builder.CreateAlignedStore(value, StateField(offset), StateAlignment(offset))->setMetadata(llvm::LLVMContext::MD_tbaa, _module.StateTag());
}

std::size_t FunctionLifter::VectorOffset(const int index) {
    return offsetof(Ps5LiftState, Vector) + 32 * static_cast<std::size_t>(index);
}

void FunctionLifter::ScanInstructions() {
    RegisterSet named = RegisterSet{1} << PS5LIFT_RSP;
    _framePointer = KeepsFramePointer();
    for (const auto& [address, instruction] : _function.Instructions) {
        if (HandsOutFrameAddress(instruction)) _frameEscapes = true;
        const auto* flags = instruction.Info.cpu_flags;
        if (flags != nullptr && (flags->tested | flags->modified | flags->set_0 | flags->set_1 | flags->undefined) != 0) _usesFlags = true;
        if (AssignsStackPointer(instruction)) _assignsStackPointer = true;
        named |= InstructionEffects(instruction).Named;
    }
    if ((named & ~_module.Functions().at(_function.Entry).Effects.Named) != 0) throw std::logic_error("The function at 0x" + Hex(_function.Entry) + " names registers its effects leave out");
    for (int i = 0; i < PS5LIFT_GPR_COUNT; ++i) _usedGprs[i] = (named >> i & 1) != 0;
    for (int i = 0; i < VectorCount; ++i) _usedVectors[i] = (named >> (FirstVectorBit + i) & 1) != 0;
}

bool FunctionLifter::KeepsFramePointer() const {
    const auto& instructions = _function.Instructions;
    const auto push = instructions.find(_function.Entry);
    if (push == instructions.end() || push->second.Info.mnemonic != ZYDIS_MNEMONIC_PUSH || !IsRegister(push->second.Operands[0], ZYDIS_REGISTER_RBP)) return false;
    const auto move = instructions.find(push->second.Next());
    if (move == instructions.end() || move->second.Info.mnemonic != ZYDIS_MNEMONIC_MOV || !IsRegister(move->second.Operands[0], ZYDIS_REGISTER_RBP) || !IsRegister(move->second.Operands[1], ZYDIS_REGISTER_RSP)) return false;
    for (const auto& [address, instruction] : instructions) {
        const auto mnemonic = instruction.Info.mnemonic;
        const bool restores = mnemonic == ZYDIS_MNEMONIC_LEAVE || (mnemonic == ZYDIS_MNEMONIC_POP && IsRegister(instruction.Operands[0], ZYDIS_REGISTER_RBP));
        if (address != move->first && !restores && WritesGpr(instruction, PS5LIFT_RBP)) return false;
    }
    return true;
}

bool FunctionLifter::IsFrameRegister(const ZydisRegister reg) const {
    const auto gpr = GprIndex(reg);
    return gpr == PS5LIFT_RSP || (_framePointer && gpr == PS5LIFT_RBP);
}

bool FunctionLifter::HandsOutFrameAddress(const Instruction& instruction) const {
    const auto& info = instruction.Info;
    if (info.mnemonic == ZYDIS_MNEMONIC_ENTER) return true;
    if (_framePointer && instruction.Address == _function.Entry) return false;
    bool reads = false;
    for (unsigned i = 0; i < info.operand_count_visible; ++i) {
        const auto& operand = instruction.Operands[i];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && IsFrameRegister(operand.reg.value) && (operand.actions & ZYDIS_OPERAND_ACTION_MASK_READ) != 0) reads = true;
        if (info.mnemonic == ZYDIS_MNEMONIC_LEA && operand.type == ZYDIS_OPERAND_TYPE_MEMORY && (IsFrameRegister(operand.mem.base) || IsFrameRegister(operand.mem.index))) reads = true;
    }
    if (!reads) return false;
    for (unsigned i = 0; i < info.operand_count; ++i) {
        const auto& operand = instruction.Operands[i];
        if ((operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) == 0) continue;
        if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY) return true;
        const auto reg = operand.reg.value;
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && !IsFrameRegister(reg) && (GprIndex(reg) >= 0 || VectorIndex(reg) >= 0 || MmxIndex(reg) >= 0)) return true;
    }
    return false;
}

bool FunctionLifter::CrossesCalls(const int flag) const {
    return flag == PS5LIFT_DF || _module.Options().PreserveFlags;
}

void FunctionLifter::Flush(const RegisterSet registers) {
    for (int i = 0; i < PS5LIFT_GPR_COUNT; ++i) {
        if (_gpr[i] && (registers >> i & 1) != 0) StateStore(_builder.CreateLoad(I64(), _gpr[i]), offsetof(Ps5LiftState, Gpr) + 8 * i);
    }
    for (int i = 0; i < PS5LIFT_FLAG_COUNT; ++i) {
        if (_flags[i] && CrossesCalls(i)) StateStore(_builder.CreateZExt(_builder.CreateLoad(_builder.getInt1Ty(), _flags[i]), _builder.getInt8Ty()), offsetof(Ps5LiftState, Flags) + i);
    }
    for (int i = 0; i < VectorCount; ++i) {
        if (_vectors[i] && (registers >> (FirstVectorBit + i) & 1) != 0) StateStore(_builder.CreateLoad(Int(256), _vectors[i]), VectorOffset(i));
    }
}

void FunctionLifter::Reload(const RegisterSet registers) {
    for (int i = 0; i < PS5LIFT_GPR_COUNT; ++i) {
        if (_gpr[i] && (registers >> i & 1) != 0) _builder.CreateStore(StateLoad(I64(), offsetof(Ps5LiftState, Gpr) + 8 * i), _gpr[i]);
    }
    for (int i = 0; i < PS5LIFT_FLAG_COUNT; ++i) {
        if (_flags[i] && CrossesCalls(i)) _builder.CreateStore(_builder.CreateTrunc(StateLoad(_builder.getInt8Ty(), offsetof(Ps5LiftState, Flags) + i), _builder.getInt1Ty()), _flags[i]);
        else if (_flags[i]) _builder.CreateStore(_builder.getFalse(), _flags[i]);
    }
    for (int i = 0; i < VectorCount; ++i) {
        if (_vectors[i] && (registers >> (FirstVectorBit + i) & 1) != 0) _builder.CreateStore(StateLoad(Int(256), VectorOffset(i)), _vectors[i]);
    }
}

bool FunctionLifter::Terminated() const {
    return HasTerminator(*_builder.GetInsertBlock());
}

void FunctionLifter::StartDeadBlock() {
    _builder.SetInsertPoint(llvm::BasicBlock::Create(Context(), "dead", _host));
}

llvm::Value* FunctionLifter::Gpr(const int index) {
    if (_gpr[index]) return _builder.CreateLoad(I64(), _gpr[index]);
    return StateLoad(I64(), offsetof(Ps5LiftState, Gpr) + 8 * static_cast<std::size_t>(index));
}

void FunctionLifter::SetGpr(const int index, llvm::Value* value) {
    if (_gpr[index]) _builder.CreateStore(value, _gpr[index]);
    else StateStore(value, offsetof(Ps5LiftState, Gpr) + 8 * static_cast<std::size_t>(index));
}

llvm::Value* FunctionLifter::Flag(const int flag) {
    if (_flags[flag]) return _builder.CreateLoad(_builder.getInt1Ty(), _flags[flag]);
    return _builder.CreateTrunc(StateLoad(_builder.getInt8Ty(), offsetof(Ps5LiftState, Flags) + static_cast<std::size_t>(flag)), _builder.getInt1Ty());
}

void FunctionLifter::SetFlag(const int flag, llvm::Value* value) {
    if (_flags[flag]) _builder.CreateStore(value, _flags[flag]);
    else StateStore(_builder.CreateZExt(value, _builder.getInt8Ty()), offsetof(Ps5LiftState, Flags) + static_cast<std::size_t>(flag));
}

llvm::Value* FunctionLifter::Vector(const int index) {
    if (_vectors[index]) return _builder.CreateLoad(Int(256), _vectors[index]);
    return StateLoad(Int(256), VectorOffset(index));
}

void FunctionLifter::SetVector(const int index, llvm::Value* value) {
    if (_vectors[index]) _builder.CreateStore(value, _vectors[index]);
    else StateStore(value, VectorOffset(index));
}

void FunctionLifter::WriteVector(const int index, llvm::Value* value, const bool zeroUpper) {
    const auto bits = value->getType()->getIntegerBitWidth();
    if (bits == 256) return SetVector(index, value);
    auto* wide = _builder.CreateZExt(value, Int(256));
    if (zeroUpper) return SetVector(index, wide);
    auto* keep = llvm::ConstantInt::get(Context(), llvm::APInt::getHighBitsSet(256, 256 - bits));
    SetVector(index, _builder.CreateOr(_builder.CreateAnd(Vector(index), keep), wide));
}

std::size_t FunctionLifter::MmxOffset(const int index) const {
    return offsetof(Ps5LiftState, X87Registers) + 16 * static_cast<std::size_t>(index);
}

bool FunctionLifter::UsesMmx() const {
    for (std::uint8_t i = 0; i < _instruction->Info.operand_count; ++i) {
        if (Operand(i).type == ZYDIS_OPERAND_TYPE_REGISTER && MmxIndex(Operand(i).reg.value) >= 0) return true;
    }
    return false;
}

void FunctionLifter::EnterMmx() {
    constexpr std::uint16_t Top = 7 << 11;
    StateStore(_builder.CreateAnd(StateLoad(_builder.getInt16Ty(), offsetof(Ps5LiftState, X87Status)), _builder.getInt16(static_cast<std::uint16_t>(~Top))), offsetof(Ps5LiftState, X87Status));
    StateStore(_builder.getInt8(0xff), offsetof(Ps5LiftState, X87Valid));
}

llvm::Value* FunctionLifter::ReadRegister(const ZydisRegister reg) {
    if (reg == ZYDIS_REGISTER_RIP) return _builder.getInt64(_instruction->Next());
    if (const auto mmx = MmxIndex(reg); mmx >= 0) return StateLoad(I64(), MmxOffset(mmx));
    const auto vector = VectorIndex(reg);
    if (vector >= 0) return _builder.CreateTrunc(Vector(vector), Int(ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, reg)));
    const auto index = GprIndex(reg);
    if (index < 0) throw UnsupportedInstruction(std::string("register ") + ZydisRegisterGetString(reg));
    const auto width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, reg);
    auto* value = Gpr(index);
    if (width == 64) return value;
    if (IsHighByte(reg)) value = _builder.CreateLShr(value, 8);
    return _builder.CreateTrunc(value, Int(width));
}

void FunctionLifter::WriteRegister(const ZydisRegister reg, llvm::Value* value) {
    if (const auto mmx = MmxIndex(reg); mmx >= 0) {
        StateStore(_builder.CreateZExtOrTrunc(value, I64()), MmxOffset(mmx));
        StateStore(_builder.getInt16(0xffff), MmxOffset(mmx) + 8);
        return;
    }
    const auto vector = VectorIndex(reg);
    if (vector >= 0) {
        const bool vex = _instruction->Info.encoding == ZYDIS_INSTRUCTION_ENCODING_VEX;
        WriteVector(vector, value, vex && value->getType()->getIntegerBitWidth() == 128);
        return;
    }
    const auto index = GprIndex(reg);
    if (index < 0) throw UnsupportedInstruction(std::string("register ") + ZydisRegisterGetString(reg));
    const auto width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, reg);
    if (width == 64) return SetGpr(index, value);
    auto* extended = _builder.CreateZExt(value, I64());
    if (width == 32) return SetGpr(index, extended);
    if (IsHighByte(reg)) return SetGpr(index, _builder.CreateOr(_builder.CreateAnd(Gpr(index), ~std::uint64_t{0xff00}), _builder.CreateShl(extended, 8)));
    const auto mask = width == 16 ? ~std::uint64_t{0xffff} : ~std::uint64_t{0xff};
    SetGpr(index, _builder.CreateOr(_builder.CreateAnd(Gpr(index), mask), extended));
}

llvm::Value* FunctionLifter::Address(const ZydisDecodedOperand& operand, const bool applySegment) {
    if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY) throw std::logic_error("Address of a non-memory operand");
    const auto width = _instruction->Info.address_width;
    if (width != 64 && width != 32) throw UnsupportedInstruction("16-bit addressing");
    llvm::Value* address = Const(width, static_cast<std::uint64_t>(operand.mem.disp.value));
    if (operand.mem.base != ZYDIS_REGISTER_NONE) address = _builder.CreateAdd(address, ReadRegister(operand.mem.base));
    if (operand.mem.index != ZYDIS_REGISTER_NONE) address = _builder.CreateAdd(address, _builder.CreateMul(ReadRegister(operand.mem.index), Const(width, operand.mem.scale)));
    address = _builder.CreateZExt(address, I64());
    if (applySegment && operand.mem.segment == ZYDIS_REGISTER_FS) address = _builder.CreateAdd(address, StateLoad(I64(), offsetof(Ps5LiftState, FsBase)));
    if (applySegment && operand.mem.segment == ZYDIS_REGISTER_GS) address = _builder.CreateAdd(address, StateLoad(I64(), offsetof(Ps5LiftState, GsBase)));
    return address;
}

bool FunctionLifter::NeedsOrdering(const ZydisDecodedOperand& operand, const unsigned bits) const {
    if (_module.Options().Memory != MemoryModel::Tso || bits > 64) return false;
    return !InPrivateFrame(operand) && !InReadOnlySegment(operand, bits);
}

bool FunctionLifter::InPrivateFrame(const ZydisDecodedOperand& operand) const {
    if (_frameEscapes || operand.mem.segment == ZYDIS_REGISTER_FS || operand.mem.segment == ZYDIS_REGISTER_GS) return false;
    return IsFrameRegister(operand.mem.base);
}

bool FunctionLifter::InReadOnlySegment(const ZydisDecodedOperand& operand, const unsigned bits) const {
    if (operand.mem.base != ZYDIS_REGISTER_RIP || operand.mem.index != ZYDIS_REGISTER_NONE || operand.mem.segment == ZYDIS_REGISTER_FS || operand.mem.segment == ZYDIS_REGISTER_GS) return false;
    return _module.Image().IsReadOnly(_instruction->Next() + static_cast<std::uint64_t>(operand.mem.disp.value), bits / 8);
}

llvm::Value* FunctionLifter::Load(llvm::Value* address, const unsigned bits, const bool ordered) {
    if (!ordered) return GuestLoad(address, bits);
    if (bits == 8) return Ordered(GuestLoad(address, bits), llvm::AtomicOrdering::Acquire);
    const auto split = SplitOnAlignment(address, bits);
    _builder.SetInsertPoint(split.Aligned);
    auto* aligned = Ordered(GuestLoad(address, bits), llvm::AtomicOrdering::Acquire);
    _builder.CreateBr(split.Done);
    _builder.SetInsertPoint(split.Unaligned);
    auto* unaligned = GuestLoad(address, bits);
    _builder.CreateFence(llvm::AtomicOrdering::Acquire);
    _builder.CreateBr(split.Done);
    _builder.SetInsertPoint(split.Done);
    auto* value = _builder.CreatePHI(Int(bits), 2);
    value->addIncoming(aligned, split.Aligned);
    value->addIncoming(unaligned, split.Unaligned);
    return value;
}

void FunctionLifter::Store(llvm::Value* address, llvm::Value* value, const bool ordered) {
    const auto bits = value->getType()->getIntegerBitWidth();
    if (!ordered) {
        GuestStore(address, value);
    } else if (bits == 8) {
        Ordered(GuestStore(address, value), llvm::AtomicOrdering::Release);
    } else {
        const auto split = SplitOnAlignment(address, bits);
        _builder.SetInsertPoint(split.Aligned);
        Ordered(GuestStore(address, value), llvm::AtomicOrdering::Release);
        _builder.CreateBr(split.Done);
        _builder.SetInsertPoint(split.Unaligned);
        _builder.CreateFence(llvm::AtomicOrdering::Release);
        GuestStore(address, value);
        _builder.CreateBr(split.Done);
        _builder.SetInsertPoint(split.Done);
    }
}

llvm::LoadInst* FunctionLifter::GuestLoad(llvm::Value* address, const unsigned bits) {
    auto* load = _builder.CreateAlignedLoad(Int(bits), _builder.CreateIntToPtr(address, _module.Pointer()), llvm::Align(1));
    load->setMetadata(llvm::LLVMContext::MD_tbaa, _module.GuestTag());
    return load;
}

llvm::StoreInst* FunctionLifter::GuestStore(llvm::Value* address, llvm::Value* value) {
    auto* store = _builder.CreateAlignedStore(value, _builder.CreateIntToPtr(address, _module.Pointer()), llvm::Align(1));
    store->setMetadata(llvm::LLVMContext::MD_tbaa, _module.GuestTag());
    return store;
}

FunctionLifter::AlignmentSplit FunctionLifter::SplitOnAlignment(llvm::Value* address, const unsigned bits) {
    const AlignmentSplit split{llvm::BasicBlock::Create(Context(), "aligned", _host), llvm::BasicBlock::Create(Context(), "unaligned", _host),
        llvm::BasicBlock::Create(Context(), "ordered", _host)};
    auto* misaligned = _builder.CreateICmpNE(_builder.CreateAnd(address, _builder.getInt64(bits / 8 - 1)), _builder.getInt64(0));
    _builder.CreateCondBr(misaligned, split.Unaligned, split.Aligned, llvm::MDBuilder(Context()).createBranchWeights(1, 1u << 20));
    return split;
}

llvm::Value* FunctionLifter::Read(const int index) {
    const auto& operand = Operand(index);
    switch (operand.type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        auto* value = ReadRegister(operand.reg.value);
        if (VectorIndex(operand.reg.value) >= 0 && operand.size < value->getType()->getIntegerBitWidth()) return _builder.CreateTrunc(value, Int(operand.size));
        return value;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY:
        return Load(Address(operand), operand.size, NeedsOrdering(operand, operand.size));
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        const bool widen = index > 0 && Operand(0).type != ZYDIS_OPERAND_TYPE_IMMEDIATE && Operand(0).size > operand.size;
        return Const(widen ? Operand(0).size : operand.size, operand.imm.value.u);
    }
    default:
        throw UnsupportedInstruction("operand type");
    }
}

llvm::Value* FunctionLifter::ReadVector(const int index, const unsigned bits) {
    const auto& operand = Operand(index);
    if (operand.type != ZYDIS_OPERAND_TYPE_REGISTER) return ReadAs(index, bits);
    return _builder.CreateZExtOrTrunc(ReadRegister(operand.reg.value), Int(bits));
}

llvm::Value* FunctionLifter::ReadAs(const int index, const unsigned bits) {
    auto* value = Read(index);
    const auto width = value->getType()->getIntegerBitWidth();
    if (width == bits) return value;
    return width > bits ? _builder.CreateTrunc(value, Int(bits)) : _builder.CreateZExt(value, Int(bits));
}

void FunctionLifter::Write(const int index, llvm::Value* value) {
    const auto& operand = Operand(index);
    if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER) return WriteRegister(operand.reg.value, value);
    if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY) return Store(Address(operand), value, NeedsOrdering(operand, value->getType()->getIntegerBitWidth()));
    throw UnsupportedInstruction("write to operand type");
}

llvm::Value* FunctionLifter::SignBit(llvm::Value* value) {
    return _builder.CreateICmpSLT(value, llvm::ConstantInt::get(value->getType(), 0));
}

llvm::Value* FunctionLifter::Parity(llvm::Value* value) {
    auto* low = _builder.CreateTrunc(value, _builder.getInt8Ty());
    auto* count = _builder.CreateUnaryIntrinsic(llvm::Intrinsic::ctpop, low);
    return _builder.CreateICmpEQ(_builder.CreateAnd(count, 1), _builder.getInt8(0));
}

void FunctionLifter::ResultFlags(llvm::Value* result) {
    SetFlag(PS5LIFT_ZF, _builder.CreateICmpEQ(result, llvm::ConstantInt::get(result->getType(), 0)));
    SetFlag(PS5LIFT_SF, SignBit(result));
    SetFlag(PS5LIFT_PF, Parity(result));
}

void FunctionLifter::AdjustFlag(llvm::Value* a, llvm::Value* b, llvm::Value* result) {
    auto* bits = _builder.CreateXor(_builder.CreateXor(a, b), result);
    SetFlag(PS5LIFT_AF, _builder.CreateICmpNE(_builder.CreateAnd(bits, llvm::ConstantInt::get(a->getType(), 0x10)), llvm::ConstantInt::get(a->getType(), 0)));
}

void FunctionLifter::LogicFlags(llvm::Value* result) {
    ResultFlags(result);
    SetFlag(PS5LIFT_CF, _builder.getFalse());
    SetFlag(PS5LIFT_OF, _builder.getFalse());
    SetFlag(PS5LIFT_AF, _builder.getFalse());
}

llvm::Value* FunctionLifter::Add(llvm::Value* a, llvm::Value* b, llvm::Value* carry, const bool setCarry) {
    const auto bits = a->getType()->getIntegerBitWidth();
    auto* wide = _builder.CreateAdd(_builder.CreateAdd(_builder.CreateZExt(a, Int(bits + 1)), _builder.CreateZExt(b, Int(bits + 1))), _builder.CreateZExt(carry, Int(bits + 1)));
    auto* result = _builder.CreateTrunc(wide, Int(bits));
    if (setCarry) SetFlag(PS5LIFT_CF, _builder.CreateTrunc(_builder.CreateLShr(wide, bits), _builder.getInt1Ty()));
    SetFlag(PS5LIFT_OF, SignBit(_builder.CreateAnd(_builder.CreateXor(a, result), _builder.CreateXor(b, result))));
    AdjustFlag(a, b, result);
    ResultFlags(result);
    return result;
}

llvm::Value* FunctionLifter::Sub(llvm::Value* a, llvm::Value* b, llvm::Value* borrow, const bool setCarry) {
    const auto bits = a->getType()->getIntegerBitWidth();
    auto* subtrahend = _builder.CreateAdd(_builder.CreateZExt(b, Int(bits + 1)), _builder.CreateZExt(borrow, Int(bits + 1)));
    auto* result = _builder.CreateSub(_builder.CreateSub(a, b), _builder.CreateZExt(borrow, Int(bits)));
    if (setCarry) SetFlag(PS5LIFT_CF, _builder.CreateICmpULT(_builder.CreateZExt(a, Int(bits + 1)), subtrahend));
    SetFlag(PS5LIFT_OF, SignBit(_builder.CreateAnd(_builder.CreateXor(a, b), _builder.CreateXor(a, result))));
    AdjustFlag(a, b, result);
    ResultFlags(result);
    return result;
}

llvm::Value* FunctionLifter::Test(const Condition condition) {
    switch (condition) {
    case Condition::O: return Flag(PS5LIFT_OF);
    case Condition::NO: return _builder.CreateNot(Flag(PS5LIFT_OF));
    case Condition::B: return Flag(PS5LIFT_CF);
    case Condition::AE: return _builder.CreateNot(Flag(PS5LIFT_CF));
    case Condition::E: return Flag(PS5LIFT_ZF);
    case Condition::NE: return _builder.CreateNot(Flag(PS5LIFT_ZF));
    case Condition::BE: return _builder.CreateOr(Flag(PS5LIFT_CF), Flag(PS5LIFT_ZF));
    case Condition::A: return _builder.CreateNot(_builder.CreateOr(Flag(PS5LIFT_CF), Flag(PS5LIFT_ZF)));
    case Condition::S: return Flag(PS5LIFT_SF);
    case Condition::NS: return _builder.CreateNot(Flag(PS5LIFT_SF));
    case Condition::P: return Flag(PS5LIFT_PF);
    case Condition::NP: return _builder.CreateNot(Flag(PS5LIFT_PF));
    case Condition::L: return _builder.CreateICmpNE(Flag(PS5LIFT_SF), Flag(PS5LIFT_OF));
    case Condition::GE: return _builder.CreateICmpEQ(Flag(PS5LIFT_SF), Flag(PS5LIFT_OF));
    case Condition::LE: return _builder.CreateOr(Flag(PS5LIFT_ZF), _builder.CreateICmpNE(Flag(PS5LIFT_SF), Flag(PS5LIFT_OF)));
    case Condition::G: return _builder.CreateAnd(_builder.CreateNot(Flag(PS5LIFT_ZF)), _builder.CreateICmpEQ(Flag(PS5LIFT_SF), Flag(PS5LIFT_OF)));
    }
    throw std::logic_error("Unknown condition");
}

void FunctionLifter::Push(llvm::Value* value) {
    auto* rsp = _builder.CreateSub(Gpr(PS5LIFT_RSP), _builder.getInt64(value->getType()->getIntegerBitWidth() / 8));
    Store(rsp, value, false);
    SetGpr(PS5LIFT_RSP, rsp);
}

llvm::Value* FunctionLifter::Pop(const unsigned bits) {
    auto* rsp = Gpr(PS5LIFT_RSP);
    auto* value = Load(rsp, bits, false);
    SetGpr(PS5LIFT_RSP, _builder.CreateAdd(rsp, _builder.getInt64(bits / 8)));
    return value;
}

void FunctionLifter::CallGuest(llvm::Value* callee, const RegisterEffects& effects, llvm::ArrayRef<llvm::Value*> extra) {
    Flush(effects.Named);
    StateStore(_builder.getInt64(_instruction->Next()), offsetof(Ps5LiftState, Rip));
    std::vector<llvm::Value*> arguments{_state};
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    if (auto* function = llvm::dyn_cast<llvm::Function>(callee)) _builder.CreateCall(function, arguments);
    else _builder.CreateCall(_module.GuestType(), callee, arguments);
    auto* returned = llvm::BasicBlock::Create(Context(), "returned", _host);
    auto* unwinding = _builder.CreateICmpNE(StateLoad(_builder.getInt8Ty(), offsetof(Ps5LiftState, Unwinding)), _builder.getInt8(0));
    _builder.CreateCondBr(unwinding, UnwindBlock(), returned, llvm::MDBuilder(Context()).createBranchWeights(1, 1u << 20));
    _builder.SetInsertPoint(returned);
    Reload(effects.Written);
}

void FunctionLifter::ResumeIfStackSwitched(llvm::Value* rsp, llvm::Value* popped) {
    auto* switched = llvm::BasicBlock::Create(Context(), "switched_stack", _host);
    auto* own = llvm::BasicBlock::Create(Context(), "own_stack", _host);
    _builder.CreateCondBr(_builder.CreateICmpEQ(rsp, _entryRsp), own, switched, llvm::MDBuilder(Context()).createBranchWeights(1u << 20, 1));
    _builder.SetInsertPoint(switched);
    StateStore(Load(rsp, 64, false), offsetof(Ps5LiftState, ResumeRip));
    StateStore(popped, offsetof(Ps5LiftState, ResumeRsp));
    StateStore(_builder.getInt8(1), offsetof(Ps5LiftState, Unwinding));
    SetGpr(PS5LIFT_RSP, popped);
    Flush();
    _builder.CreateBr(UnwindBlock());
    _builder.SetInsertPoint(own);
}

llvm::BasicBlock* FunctionLifter::UnwindBlock() {
    if (_unwind) return _unwind;
    const llvm::IRBuilderBase::InsertPointGuard guard(_builder);
    _unwind = llvm::BasicBlock::Create(Context(), "unwind", _host);
    _builder.SetInsertPoint(_unwind);
    if (_function.ResumePoints.empty()) {
        _builder.CreateRetVoid();
        return _unwind;
    }
    auto* resume = llvm::BasicBlock::Create(Context(), "resume", _host);
    auto* leave = llvm::BasicBlock::Create(Context(), "leave", _host);
    auto* target = StateLoad(I64(), offsetof(Ps5LiftState, ResumeRsp));
    _builder.CreateCondBr(_builder.CreateICmpUGE(_entryRsp, target), resume, leave);
    _builder.SetInsertPoint(leave);
    _builder.CreateRetVoid();
    _builder.SetInsertPoint(resume);
    auto* unknown = llvm::BasicBlock::Create(Context(), "unknown_resume", _host);
    auto* selector = _builder.CreateSwitch(StateLoad(I64(), offsetof(Ps5LiftState, ResumeRip)), unknown, static_cast<unsigned>(_function.ResumePoints.size()));
    for (const auto point : _function.ResumePoints) {
        auto* block = llvm::BasicBlock::Create(Context(), "resume_" + Hex(point), _host);
        selector->addCase(_builder.getInt64(point), block);
        _builder.SetInsertPoint(block);
        StateStore(_builder.getInt8(0), offsetof(Ps5LiftState, Unwinding));
        Reload();
        _builder.CreateBr(_blocks.at(point));
    }
    _builder.SetInsertPoint(unknown);
    auto callee = _module.Runtime("ps5lift_unknown_resume", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64(), I64()}, false));
    _builder.CreateCall(callee, {_state, _builder.getInt64(_function.Entry), StateLoad(I64(), offsetof(Ps5LiftState, ResumeRip))})->setDoesNotReturn();
    _builder.CreateUnreachable();
    return _unwind;
}

llvm::FunctionCallee FunctionLifter::DispatchCallee() {
    return _module.Runtime("ps5lift_dispatch", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64()}, false));
}

void FunctionLifter::TailCall(const std::uint64_t target) {
    if (!_module.Functions().contains(target)) throw std::runtime_error("Control leaves 0x" + Hex(_function.Entry) + " for undiscovered 0x" + Hex(target));
    Flush();
    _builder.CreateCall(_module.Guest(target), {_state})->setTailCallKind(llvm::CallInst::TCK_MustTail);
    _builder.CreateRetVoid();
}

llvm::BasicBlock* FunctionLifter::Target(const std::uint64_t target) {
    if (_blocks.contains(target)) return _blocks.at(target);
    auto* current = _builder.GetInsertBlock();
    auto* block = llvm::BasicBlock::Create(Context(), "tail_" + Hex(target), _host);
    _builder.SetInsertPoint(block);
    TailCall(target);
    _builder.SetInsertPoint(current);
    return block;
}

void FunctionLifter::Unsupported(const std::string& what) {
    const std::string mnemonic = ZydisMnemonicGetString(_instruction->Info.mnemonic);
    ++_module.Report().Unsupported[mnemonic + (what.empty() ? "" : " (" + what + ")")];
    Flush();
    auto callee = _module.Runtime("ps5lift_unsupported", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64(), _module.Pointer()}, false));
    auto* call = _builder.CreateCall(callee, {_state, _builder.getInt64(_instruction->Address), _builder.CreateGlobalString(mnemonic)});
    call->setDoesNotReturn();
    _builder.CreateUnreachable();
}

void FunctionLifter::Trap() {
    Flush();
    auto callee = _module.Runtime("ps5lift_trap", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64()}, false));
    _builder.CreateCall(callee, {_state, _builder.getInt64(_instruction->Address)})->setDoesNotReturn();
    _builder.CreateUnreachable();
}

bool FunctionLifter::Locked() const {
    return (_instruction->Info.attributes & ZYDIS_ATTRIB_HAS_LOCK) != 0;
}

llvm::Value* FunctionLifter::MemoryPointer(const int index) {
    return _builder.CreateIntToPtr(Address(Operand(index)), _module.Pointer());
}

std::uint64_t FunctionLifter::Immediate(const int index) const {
    const auto& operand = Operand(index);
    if (operand.type != ZYDIS_OPERAND_TYPE_IMMEDIATE) throw UnsupportedInstruction("expected an immediate");
    return operand.imm.value.u;
}

int FunctionLifter::Base(const int legacyVisible) const {
    return _instruction->Info.operand_count_visible > legacyVisible ? 1 : 0;
}

ZydisRegister FunctionLifter::HiddenRegister(std::initializer_list<ZydisRegister> candidates) const {
    for (std::uint8_t i = _instruction->Info.operand_count_visible; i < _instruction->Info.operand_count; ++i) {
        const auto& operand = Operand(i);
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && std::find(candidates.begin(), candidates.end(), operand.reg.value) != candidates.end()) return operand.reg.value;
    }
    throw std::logic_error(std::string(ZydisMnemonicGetString(_instruction->Info.mnemonic)) + " at 0x" + Hex(_instruction->Address) + " has no such hidden register operand");
}

llvm::Value* FunctionLifter::ReadRegisterOrMemory(const int index, const unsigned bits) {
    const auto& operand = Operand(index);
    if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY) return Load(Address(operand), bits, NeedsOrdering(operand, bits));
    return _builder.CreateZExtOrTrunc(ReadRegister(operand.reg.value), Int(bits));
}

void FunctionLifter::EmitInstruction(const Instruction& instruction) {
    try {
        EmitSemantics(instruction.Info.mnemonic);
    } catch (const UnsupportedInstruction& error) {
        Unsupported(error.what());
    }
}

}
