#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBLOCK_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBLOCK_HPP

#include "IntermediateRepresentation/IrValue.hpp"
#include <cstdint>
#include <array>
#include <list>
#include <memory>
#include <memory_resource>
#include <vector>

namespace ShaderRecompiler {

template<std::size_t Size>
class IrSsaValues {
public:
    IrValue*& operator[](std::size_t index) {
        if (storage == nullptr) storage = std::make_unique<std::array<IrValue*, Size>>();
        return (*storage)[index];
    }

    IrValue* operator[](std::size_t index) const { return values()[index]; }
    bool HasStorage() const { return storage != nullptr; }
    void fill(IrValue* value) {
        if (value == nullptr) { storage.reset(); return; }
        if (storage == nullptr) storage = std::make_unique<std::array<IrValue*, Size>>();
        storage->fill(value);
    }
    auto begin() const { return values().begin(); }
    auto end() const { return values().end(); }

private:
    const std::array<IrValue*, Size>& values() const {
        static const std::array<IrValue*, Size> empty{};
        return storage != nullptr ? *storage : empty;
    }
    std::unique_ptr<std::array<IrValue*, Size>> storage;
};

using IrInstructionList = std::pmr::list<IrValue*>;

class IrBlock {
public:
    explicit IrBlock(std::uint32_t id);
    IrBlock(std::uint32_t id, std::shared_ptr<std::pmr::memory_resource> instructionNodes);
    IrBlock(const IrBlock& other) = delete;
    IrBlock& operator=(const IrBlock& other) = delete;
    IrBlock(IrBlock&& other) = delete;
    IrBlock& operator=(IrBlock&& other) = delete;

    [[nodiscard]] std::uint32_t Id() const;
    [[nodiscard]] IrInstructionList& Instructions();
    [[nodiscard]] const IrInstructionList& Instructions() const;
    [[nodiscard]] std::vector<IrBlock*>& Predecessors();
    [[nodiscard]] std::vector<IrBlock*>& Successors();

    void AppendInstruction(IrValue* value);
    void InsertInstructionBefore(IrValue* position, IrValue* value);
    void RemoveInstruction(IrValue* value);
    void AddPredecessor(IrBlock* block);
    void AddSuccessor(IrBlock* block);

    IrSsaValues<NumScalarRegs> ssaScalarValues{};
    IrSsaValues<NumScalarRegs> ssaThreadBitScalarValues{};
    IrSsaValues<NumScalarRegs> ssaScalarMaskTags{};
    IrSsaValues<NumVectorRegs> ssaVectorValues{};

    [[nodiscard]] const std::vector<IrBlock*>& Predecessors() const;
    [[nodiscard]] const std::vector<IrBlock*>& Successors() const;
    void SsaSeal();
    [[nodiscard]] bool IsSsaSealed() const;
    void AddBranch(IrBlock* block);
    [[nodiscard]] bool Empty() const;

private:
    std::shared_ptr<std::pmr::memory_resource> instructionNodes;
    bool ssaSealed = false;
    std::uint32_t id;
    IrInstructionList instructions;
    std::vector<IrBlock*> predecessors;
    std::vector<IrBlock*> successors;
};

}

#endif
