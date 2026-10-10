#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRPROGRAM_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRPROGRAM_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace ShaderRecompiler {

class IrProgram {
public:
    [[nodiscard]] std::vector<std::unique_ptr<IrBlock>>& Blocks();
    [[nodiscard]] const std::vector<std::unique_ptr<IrBlock>>& Blocks() const;
    [[nodiscard]] const std::vector<std::unique_ptr<IrValue>>& Values() const;
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
    CompiledShaderInfo TakeCompiledInfo() &&;
    IrValue& CreateValue(IrOpcode opcode, IrType type, std::uint64_t flags);
    void SetEntryBlock(IrBlock& block);
    [[nodiscard]] std::vector<IrBlock*>& BlockOrder();
    [[nodiscard]] const std::vector<IrBlock*>& BlockOrder() const;

private:
    std::vector<std::unique_ptr<IrBlock>> blocks;
    std::vector<IrBlock*> blockOrder;
    std::vector<std::unique_ptr<IrValue>> values;
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
