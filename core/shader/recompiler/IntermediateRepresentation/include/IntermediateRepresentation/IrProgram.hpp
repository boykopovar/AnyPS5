#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRPROGRAM_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRPROGRAM_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

class IrValuePool {
public:
    IrValuePool() = default;
    IrValuePool(const IrValuePool&) = delete;
    IrValuePool& operator=(const IrValuePool&) = delete;
    IrValuePool(IrValuePool&& other) noexcept : live(std::exchange(other.live, {})), freeSlots(std::exchange(other.freeSlots, {})), chunks(std::exchange(other.chunks, {})), chunkUsed(std::exchange(other.chunkUsed, 0)), chunkCapacity(std::exchange(other.chunkCapacity, 0)) {}
    IrValuePool& operator=(IrValuePool&& other) noexcept {
        if (this != &other) {
            release();
            live = std::exchange(other.live, {});
            freeSlots = std::exchange(other.freeSlots, {});
            chunks = std::exchange(other.chunks, {});
            chunkUsed = std::exchange(other.chunkUsed, 0);
            chunkCapacity = std::exchange(other.chunkCapacity, 0);
        }
        return *this;
    }
    ~IrValuePool() { release(); }

    IrValue& Create(IrOpcode opcode, IrType type, std::uint32_t id, std::pmr::memory_resource* useArena) {
        live.push_back(nullptr);
        const bool reuse = !freeSlots.empty();
        if (!reuse && chunkUsed == chunkCapacity) {
            try {
                grow();
            } catch (...) {
                live.pop_back();
                throw;
            }
        }
        IrValue* slot = reuse ? freeSlots.back() : chunks.back() + chunkUsed;
        IrValue* value = nullptr;
        try {
            value = new (slot) IrValue(opcode, type, id, useArena);
        } catch (...) {
            live.pop_back();
            throw;
        }
        if (reuse) freeSlots.pop_back();
        else ++chunkUsed;
        live.back() = value;
        return *value;
    }
    template<typename TPredicate>
    std::size_t EraseIf(TPredicate predicate) {
        const auto dead = std::count_if(live.begin(), live.end(), [&](IrValue* value) { return predicate(*value); });
        if (dead == 0) return 0;
        freeSlots.reserve(freeSlots.size() + static_cast<std::size_t>(dead));
        const auto removed = std::erase_if(live, [&](IrValue* value) {
            if (!predicate(*value)) return false;
            value->~IrValue();
            freeSlots.push_back(value);
            return true;
        });
        live.shrink_to_fit();
        return removed;
    }
    [[nodiscard]] std::vector<IrValue*>::const_iterator begin() const { return live.begin(); }
    [[nodiscard]] std::vector<IrValue*>::const_iterator end() const { return live.end(); }

private:
    void grow() {
        const std::size_t size = chunkCapacity == 0 ? 64 : std::min<std::size_t>(chunkCapacity * 2, 4096);
        chunks.reserve(chunks.size() + 1);
        chunks.push_back(static_cast<IrValue*>(::operator new(size * sizeof(IrValue), std::align_val_t{alignof(IrValue)})));
        chunkUsed = 0;
        chunkCapacity = size;
    }
    void release() {
        for (IrValue* value : live) value->~IrValue();
        for (IrValue* chunk : chunks) ::operator delete(chunk, std::align_val_t{alignof(IrValue)});
        live.clear();
        freeSlots.clear();
        chunks.clear();
        chunkUsed = 0;
        chunkCapacity = 0;
    }
    std::vector<IrValue*> live;
    std::vector<IrValue*> freeSlots;
    std::vector<IrValue*> chunks;
    std::size_t chunkUsed = 0;
    std::size_t chunkCapacity = 0;
};

class IrProgram {
public:
    [[nodiscard]] std::vector<std::unique_ptr<IrBlock>>& Blocks();
    [[nodiscard]] const std::vector<std::unique_ptr<IrBlock>>& Blocks() const;
    [[nodiscard]] IrBlock& EntryBlock() const;
    [[nodiscard]] ShaderInfo& Info();
    [[nodiscard]] const ShaderInfo& Info() const;
    [[nodiscard]] std::uint32_t WaveSize() const;

    void SetWaveSize(std::uint32_t waveSize);
    [[nodiscard]] IrBlock& CreateBlock();
    [[nodiscard]] IrValue& CreateValue(IrOpcode opcode, IrType type);

    IrResourcePlan& Resources();
    const IrResourcePlan& Resources() const;
    IrProgramMetadata& Metadata();
    const IrProgramMetadata& Metadata() const;
    [[nodiscard]] CompiledShaderInfo CompiledInfo() const&;
    CompiledShaderInfo TakeCompiledInfo() &&;
    IrValue& CreateValue(IrOpcode opcode, IrType type, std::uint64_t flags);
    void SetEntryBlock(IrBlock& block);
    std::size_t ReclaimUnusedValues();
    void ReleaseUseArena();
    [[nodiscard]] std::vector<IrBlock*>& BlockOrder();
    [[nodiscard]] const std::vector<IrBlock*>& BlockOrder() const;

private:
    std::shared_ptr<std::pmr::memory_resource> instructionNodes;
    std::vector<std::unique_ptr<IrBlock>> blocks;
    IrUseArena useArena;
    std::vector<IrBlock*> blockOrder;
    IrValuePool values;
    IrResourcePlan resourcePlan;
    IrProgramMetadata metadata;
    IrBlock* entryBlock = nullptr;
    std::uint32_t waveSize = 64;
    std::uint32_t nextValueId = 0;
    std::uint32_t nextBlockId = 0;
};

std::string ProgramToString(const IrProgram& program);
void ValidateProgram(const IrProgram& program, bool requireSsa);
void ResolveControlFlowIdentities(IrProgram& program);
bool EquivalentValue(const IrResourcePlan& program, const IrValue* left, const IrValue* right);
IrValue* ResolveInvariantPhi(const IrResourcePlan& program, IrValue* value);
bool IsAddressResourceKind(ResourceKind kind);
PositionExportComponent DecodePositionExportComponent(std::uint32_t control, std::uint32_t positionIndex, std::uint32_t component);
std::uint32_t NativeBinding(IrShaderStage stage, DescriptorBindingKind kind);
ImageResourceClass ImageBindingResourceClass(DescriptorBindingKind kind);
std::uint32_t ImageBindingIndex(DescriptorBindingKind kind);
DescriptorBindingKind DescriptorBindingForImage(const ImageResource& image);

}

#endif
