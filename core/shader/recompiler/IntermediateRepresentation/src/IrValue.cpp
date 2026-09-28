#include "IntermediateRepresentation/IrValue.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace ShaderRecompiler {

IrValue::IrValue(IrOpcode opcode, IrType type, std::uint32_t id) : opcode(opcode), type(type), id(id), hasImmediate(false), parent(nullptr) {
}

IrOpcode IrValue::Opcode() const {
    return opcode;
}

IrType IrValue::Type() const {
    return type;
}

std::uint32_t IrValue::Id() const {
    return id;
}

const std::vector<IrValue*>& IrValue::Arguments() const {
    return arguments;
}

const std::vector<IrValue*>& IrValue::Uses() const {
    return uses;
}

IrBlock* IrValue::Parent() const {
    return parent;
}

bool IrValue::HasImmediate() const {
    return hasImmediate;
}

std::uint32_t IrValue::ImmediateU32() const {
    return static_cast<std::uint32_t>(immediateBits);
}

void IrValue::AddArgument(IrValue* argument) {
    arguments.push_back(argument);
    if (argument == nullptr) {
        return;
    }
    argument->uses.push_back(this);
    argument->operandUses.push_back(IrUse{this, arguments.size() - 1});
}

void IrValue::ReplaceArgument(std::size_t index, IrValue* argument) {
    if (index >= arguments.size()) {
        throw std::out_of_range("IrValue::ReplaceArgument index is out of range");
    }
    IrValue* previous = arguments[index];
    if (previous != nullptr) {
        const auto useIt = std::find(previous->uses.begin(), previous->uses.end(), this);
        if (useIt != previous->uses.end()) {
            previous->uses.erase(useIt);
        }
        const auto operandIt = std::find_if(previous->operandUses.begin(), previous->operandUses.end(), [this, index](const IrUse& use) {
            return use.user == this && use.operand == index;
        });
        if (operandIt != previous->operandUses.end()) {
            previous->operandUses.erase(operandIt);
        }
    }
    arguments[index] = argument;
    if (argument != nullptr) {
        argument->uses.push_back(this);
        argument->operandUses.push_back(IrUse{this, index});
    }
}

void IrValue::ReplaceAllUsesWith(IrValue* replacement) {
    if (replacement == this) {
        return;
    }
    while (!operandUses.empty()) {
        const IrUse use = operandUses.back();
        use.user->ReplaceArgument(use.operand, replacement);
    }
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

bool IrValue::IsIdentity() const {
    return opcode == IrOpcode::Identity;
}

bool IrValue::IsPhi() const {
    return opcode == IrOpcode::Phi;
}

IrValue* IrValue::Resolve() const {
    const IrValue* current = this;
    while (current->IsIdentity()) {
        if (current->arguments.empty() || current->arguments.front() == nullptr) {
            throw std::runtime_error("IrValue::Resolve encountered a malformed Identity value");
        }
        current = current->arguments.front();
    }
    return const_cast<IrValue*>(current);
}

bool IrValue::MayHaveSideEffects() const {
    return IrOpcodeHasSideEffects(opcode);
}

bool IrValue::HasUses() const {
    return !uses.empty();
}

std::size_t IrValue::UseCount() const {
    return uses.size();
}

std::size_t IrValue::ArgumentCount() const {
    return arguments.size();
}

std::size_t IrValue::PhiBlockCount() const {
    return phiBlocks.size();
}

IrValue* IrValue::Argument(std::size_t index) const {
    if (index >= arguments.size()) {
        throw std::out_of_range("IrValue::Argument index is out of range");
    }
    return arguments[index];
}

IrBlock* IrValue::PhiBlock(std::size_t index) const {
    if (index >= phiBlocks.size()) {
        throw std::out_of_range("IrValue::PhiBlock index is out of range");
    }
    return phiBlocks[index];
}

const std::vector<IrUse>& IrValue::OperandUses() const {
    return operandUses;
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

void IrValue::ReplaceOpcode(IrOpcode opcode) {
    this->opcode = opcode;
}

void IrValue::Invalidate() {
    if (!uses.empty()) {
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
