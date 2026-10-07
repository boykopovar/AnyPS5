#include "IntermediateRepresentation/IrBlock.hpp"
#include <algorithm>
#include <stdexcept>

namespace ShaderRecompiler {

IrBlock::IrBlock(std::uint32_t id) : id(id) {}

std::uint32_t IrBlock::Id() const { return id; }

std::list<IrValue*>& IrBlock::Instructions() { return instructions; }

const std::list<IrValue*>& IrBlock::Instructions() const { return instructions; }

std::vector<IrBlock*>& IrBlock::Predecessors() { return predecessors; }

std::vector<IrBlock*>& IrBlock::Successors() { return successors; }

void IrBlock::AppendInstruction(IrValue* value) {
    if (value == nullptr) {
        throw std::invalid_argument("IrBlock::AppendInstruction value cannot be null");
    }
    if (value->Parent() != nullptr) {
        throw std::runtime_error("IrBlock::AppendInstruction value already belongs to a block");
    }
    instructions.push_back(value);
    value->SetParent(this);
}

void IrBlock::InsertInstructionBefore(IrValue* position, IrValue* value) {
    if (value == nullptr) {
        throw std::invalid_argument("IrBlock::InsertInstructionBefore value cannot be null");
    }
    if (value->Parent() != nullptr) {
        throw std::runtime_error("IrBlock::InsertInstructionBefore value already belongs to a block");
    }
    if (position == nullptr) {
        instructions.push_front(value);
        value->SetParent(this);
        return;
    }
    const auto it = std::find(instructions.begin(), instructions.end(), position);
    if (it == instructions.end()) {
        throw std::runtime_error("IrBlock::InsertInstructionBefore position is not in this block");
    }
    instructions.insert(it, value);
    value->SetParent(this);
}

void IrBlock::RemoveInstruction(IrValue* value) {
    if (value == nullptr) {
        throw std::invalid_argument("IrBlock::RemoveInstruction value cannot be null");
    }
    const auto it = std::find(instructions.begin(), instructions.end(), value);
    if (it == instructions.end()) {
        throw std::runtime_error("IrBlock::RemoveInstruction value is not in this block");
    }
    instructions.erase(it);
    value->SetParent(nullptr);
}

void IrBlock::AddPredecessor(IrBlock* block) {
    if (block == nullptr) {
        throw std::invalid_argument("IrBlock::AddPredecessor block cannot be null");
    }
    predecessors.push_back(block);
}

void IrBlock::AddSuccessor(IrBlock* block) {
    if (block == nullptr) {
        throw std::invalid_argument("IrBlock::AddSuccessor block cannot be null");
    }
    successors.push_back(block);
}

const std::vector<IrBlock*>& IrBlock::Predecessors() const { return predecessors; }

const std::vector<IrBlock*>& IrBlock::Successors() const { return successors; }

void IrBlock::SsaSeal() {
    if (ssaSealed) {
        throw std::runtime_error("IrBlock::SsaSeal called on an already sealed block");
    }
    ssaSealed = true;
}

bool IrBlock::IsSsaSealed() const { return ssaSealed; }

void IrBlock::AddBranch(IrBlock* block) {
    if (block == nullptr) {
        throw std::invalid_argument("IrBlock::AddBranch block cannot be null");
    }
    AddSuccessor(block);
    block->AddPredecessor(this);
}

bool IrBlock::Empty() const { return instructions.empty(); }

}
