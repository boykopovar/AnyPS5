#include "IntermediateRepresentation/IrValue.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace ShaderRecompiler {

IrValue::IrValue(IrOpcode opcode, IrType type, std::uint32_t id, std::pmr::memory_resource* useArena) : opcode(opcode), type(type), id(id), hasImmediate(false), parent(nullptr), operandUses(IrUseAllocator<IrUse>(useArena)) {
    arguments.reserve(IrOpcodeOperandCount(opcode));
}

std::uint32_t IrValue::Id() const {
    return id;
}

const IrOperands& IrValue::Arguments() const {
    return arguments;
}

IrBlock* IrValue::Parent() const {
    return parent;
}

std::uint32_t IrValue::ImmediateU32() const {
    return static_cast<std::uint32_t>(immediateBits);
}

void IrValue::AddArgument(IrValue* argument) {
    arguments.push_back(argument);
    if (argument == nullptr) {
        return;
    }
    argument->operandUses.push_back(IrUse{this, arguments.size() - 1});
}

void IrValue::ReplaceArgument(std::size_t index, IrValue* argument) {
    if (index >= arguments.size()) {
        throw std::out_of_range("IrValue::ReplaceArgument index is out of range");
    }
    IrValue* previous = arguments[index];
    if (previous != nullptr) {
        const auto operandIt = !previous->operandUses.empty() && previous->operandUses.back() == IrUse{this, index}
            ? previous->operandUses.end() - 1
            : std::find_if(previous->operandUses.begin(), previous->operandUses.end(), [this, index](const IrUse& use) {
                return use.user == this && use.operand == index;
            });
        if (operandIt != previous->operandUses.end()) {
            previous->operandUses.erase(operandIt);
        }
    }
    arguments[index] = argument;
    if (argument != nullptr) {
        argument->operandUses.push_back(IrUse{this, index});
    }
}

void IrValue::ReplaceAllUsesWith(IrValue* replacement) {
    if (replacement == this) {
        return;
    }
    if (replacement != nullptr) {
        const auto reserveAdditional = [count = operandUses.size()](auto& list) {
            const auto maximum = list.max_size();
            if (count > maximum - list.size()) {
                throw std::length_error("IrValue::ReplaceAllUsesWith use list is too large");
            }
            const auto required = list.size() + count;
            if (required > list.capacity()) {
                const auto grown = list.capacity() > maximum / 2 ? maximum : list.capacity() * 2;
                list.reserve(std::max(required, grown));
            }
        };
        reserveAdditional(replacement->operandUses);
    }
    for (auto use = operandUses.rbegin(); use != operandUses.rend(); ++use) {
        use->user->arguments[use->operand] = replacement;
        if (replacement != nullptr) {
            replacement->operandUses.push_back(*use);
        }
    }
    operandUses.clear();
}

void IrValue::SetImmediateU32(std::uint32_t value) {
    immediateBits = value;
    hasImmediate = true;
}

void IrValue::SetParent(IrBlock* parent) {
    this->parent = parent;
}

bool IrValue::IsEmpty() const {
    return opcode == IrOpcode::Void;
}

bool IrValue::IsPhi() const {
    return opcode == IrOpcode::Phi;
}

bool IrValue::MayHaveSideEffects() const {
    return IrOpcodeHasSideEffects(opcode);
}

bool IrValue::HasUses() const {
    return !operandUses.empty();
}

std::size_t IrValue::UseCount() const {
    return operandUses.size();
}

std::size_t IrValue::PhiBlockCount() const {
    return phiBlocks.size();
}

IrBlock* IrValue::PhiBlock(std::size_t index) const {
    if (index >= phiBlocks.size()) {
        throw std::out_of_range("IrValue::PhiBlock index is out of range");
    }
    return phiBlocks[index];
}

const IrUseList& IrValue::OperandUses() const {
    return operandUses;
}

void IrValue::MoveUsesToHeap() {
    if (operandUses.get_allocator().Arena() == nullptr) return;
    operandUses = IrUseList(operandUses.begin(), operandUses.end(), IrUseAllocator<IrUse>{});
}

std::uint64_t IrValue::ImmediateU64() const {
    return immediateBits;
}

std::uint16_t IrValue::ImmediateF16Bits() const {
    return static_cast<std::uint16_t>(immediateBits);
}

float IrValue::ImmediateF32() const {
    return std::bit_cast<float>(static_cast<std::uint32_t>(immediateBits));
}

bool IrValue::ImmediateBool() const {
    return immediateBits != 0u;
}

std::uint8_t IrValue::ImmediateU8() const {
    return static_cast<std::uint8_t>(immediateBits);
}

std::uint16_t IrValue::ImmediateU16() const {
    return static_cast<std::uint16_t>(immediateBits);
}

GuestRegister IrValue::Register() const {
    return registerValue;
}

void IrValue::SetImmediateU64(std::uint64_t value) {
    immediateBits = value;
    hasImmediate = true;
}

void IrValue::SetImmediateF16Bits(std::uint16_t bits) {
    immediateBits = bits;
    hasImmediate = true;
}

void IrValue::SetImmediateF32(float value) {
    immediateBits = std::bit_cast<std::uint32_t>(value);
    hasImmediate = true;
}

void IrValue::SetImmediateBool(bool value) {
    immediateBits = value ? 1u : 0u;
    hasImmediate = true;
}

void IrValue::SetImmediateU8(std::uint8_t value) {
    immediateBits = value;
    hasImmediate = true;
}

void IrValue::SetImmediateU16(std::uint16_t value) {
    immediateBits = value;
    hasImmediate = true;
}

void IrValue::SetRegister(const GuestRegister& reg) {
    registerValue = reg;
}

void IrValue::AddPhiOperand(IrBlock* predecessor, IrValue* value) {
    if (!IsPhi()) {
        throw std::runtime_error("IrValue::AddPhiOperand called on a non-Phi value");
    }
    if (predecessor == nullptr) {
        throw std::invalid_argument("IrValue::AddPhiOperand predecessor cannot be null");
    }
    phiBlocks.push_back(predecessor);
    AddArgument(value);
}

void IrValue::SetDetachedOperands(std::vector<IrValue*> arguments, std::vector<IrBlock*> phiBlocks) {
    if (!this->arguments.empty() || !this->phiBlocks.empty()) {
        throw std::runtime_error("IrValue::SetDetachedOperands called on a value with operands");
    }
    if (IsPhi() ? phiBlocks.size() != arguments.size() : !phiBlocks.empty()) {
        throw std::runtime_error("IrValue::SetDetachedOperands phi blocks do not match the arguments");
    }
    this->arguments = std::move(arguments);
    this->phiBlocks = std::move(phiBlocks);
}

void IrValue::ReplaceOpcode(IrOpcode opcode) {
    this->opcode = opcode;
}

void IrValue::Invalidate() {
    if (!operandUses.empty()) {
        throw std::runtime_error("IrValue::Invalidate called on a value that still has uses");
    }
    for (std::size_t index = 0; index < arguments.size(); index++) {
        ReplaceArgument(index, nullptr);
    }
    opcode = IrOpcode::Unreachable;
}

void IrValue::ReplaceUsesWith(IrValue* replacement, bool preserve) {
    if (replacement == this) {
        return;
    }
    if (preserve) {
        for (std::size_t index = 0; index < arguments.size(); index++) {
            ReplaceArgument(index, nullptr);
        }
        arguments.clear();
        opcode = IrOpcode::Identity;
        AddArgument(replacement);
        return;
    }
    ReplaceAllUsesWith(replacement);
    Invalidate();
}

bool IrValue::operator==(const IrValue& other) const {
    if (this == &other) {
        return true;
    }
    if (opcode != other.opcode || type != other.type || flags != other.flags || hasImmediate != other.hasImmediate) {
        return false;
    }
    if (hasImmediate && immediateBits != other.immediateBits) {
        return false;
    }
    if (!(registerValue == other.registerValue)) {
        return false;
    }
    if (arguments.size() != other.arguments.size()) {
        return false;
    }
    for (std::size_t index = 0; index < arguments.size(); index++) {
        if (arguments[index] != other.arguments[index]) {
            return false;
        }
    }
    return true;
}

bool IrUse::operator==(const IrUse& other) const {
    return user == other.user && operand == other.operand;
}

}
