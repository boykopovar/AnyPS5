#include "IntermediateRepresentation/IrProgram.hpp"
#include <algorithm>
#include <bit>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ShaderRecompiler {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

template<typename TNode>
class IdIndex {
public:
    bool emplace(const TNode* node, std::size_t index) {
        const auto id = static_cast<std::size_t>(node->Id());
        if (id >= nodes.size()) {
            nodes.resize(id + 1u, nullptr);
            indices.resize(id + 1u, 0u);
        }
        if (nodes[id] == nullptr) {
            nodes[id] = node;
            indices[id] = index;
            return true;
        }
        if (nodes[id] == node) {
            return false;
        }
        return others.emplace(node, index).second;
    }

    [[nodiscard]] bool contains(const TNode* node) const {
        return owns(node) || others.contains(node);
    }

    [[nodiscard]] std::size_t at(const TNode* node) const {
        return owns(node) ? indices[node->Id()] : others.at(node);
    }

private:
    [[nodiscard]] bool owns(const TNode* node) const {
        const auto id = static_cast<std::size_t>(node->Id());
        return id < nodes.size() && nodes[id] == node;
    }

    std::vector<const TNode*> nodes;
    std::vector<std::size_t> indices;
    std::unordered_map<const TNode*, std::size_t> others;
};

bool isRegisterStatePseudo(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::GetThreadBitScalarRegister:
        case IrOpcode::SetThreadBitScalarRegister:
        case IrOpcode::GetScalarMaskTag:
        case IrOpcode::SetScalarMaskTag:
        case IrOpcode::GetScalarRegister:
        case IrOpcode::SetScalarRegister:
        case IrOpcode::GetVectorRegister:
        case IrOpcode::SetVectorRegister:
        case IrOpcode::GetGotoVariable:
        case IrOpcode::SetGotoVariable:
        case IrOpcode::GetScc:
        case IrOpcode::SetScc:
        case IrOpcode::GetExec:
        case IrOpcode::SetExec:
        case IrOpcode::GetExecLo:
        case IrOpcode::SetExecLo:
        case IrOpcode::GetExecHi:
        case IrOpcode::SetExecHi:
        case IrOpcode::GetVcc:
        case IrOpcode::SetVcc:
        case IrOpcode::GetVccLo:
        case IrOpcode::SetVccLo:
        case IrOpcode::GetVccHi:
        case IrOpcode::SetVccHi:
        case IrOpcode::GetM0:
        case IrOpcode::SetM0:
            return true;
        default:
            return false;
    }
}

bool isRuntimeRead(IrOpcode opcode) {
    return opcode == IrOpcode::ReadConstBuffer || AddressOpcodeInfoOf(opcode).access == AddressAccess::Read;
}

bool sameValue(const IrValue* left, const IrValue* right) {
    if (left == right) {
        return true;
    }
    if (left == nullptr || right == nullptr) {
        return false;
    }
    if (left->HasImmediate() && right->HasImmediate()) {
        return left->Type() == right->Type() && left->ImmediateU64() == right->ImmediateU64();
    }
    return false;
}

bool equivalentValueImpl(const IrResourcePlan& program, const IrValue* left, const IrValue* right, std::vector<std::pair<const IrValue*, const IrValue*>>& visited) {
    if (left != nullptr) {
        left = left->Resolve();
    }
    if (right != nullptr) {
        right = right->Resolve();
    }
    if (sameValue(left, right)) {
        return true;
    }
    if (left == nullptr || right == nullptr) {
        return false;
    }
    if (left->HasImmediate() || right->HasImmediate() || left->Type() != right->Type()) {
        return false;
    }
    if (left->Type() == IrType::ScalarReg || left->Type() == IrType::VectorReg) {
        return left->Register() == right->Register();
    }
    if (left->Opcode() != right->Opcode() || left->ArgumentCount() != right->ArgumentCount()) {
        return false;
    }
    const auto entry = std::make_pair(left, right);
    if (std::find(visited.begin(), visited.end(), entry) != visited.end()) {
        return true;
    }
    visited.push_back(entry);
    if (isRuntimeRead(left->Opcode())) {
        const auto leftIndex = left->Flags<MemoryFlags>().index;
        const auto rightIndex = right->Flags<MemoryFlags>().index;
        if (leftIndex >= program.memoryInfo.size() || rightIndex >= program.memoryInfo.size() || !(program.memoryInfo[leftIndex] == program.memoryInfo[rightIndex])) {
            return false;
        }
    } else if (left->Flags<std::uint64_t>() != right->Flags<std::uint64_t>()) {
        return false;
    }
    for (std::size_t index = 0; index < left->ArgumentCount(); index++) {
        if (left->Opcode() == IrOpcode::Phi && left->PhiBlock(index) != right->PhiBlock(index)) {
            return false;
        }
        if (!equivalentValueImpl(program, left->Argument(index), right->Argument(index), visited)) {
            return false;
        }
    }
    return true;
}

std::string formatHex32(std::uint32_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return stream.str();
}

std::string formatHex16(std::uint16_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw(4) << std::setfill('0') << value;
    return stream.str();
}

std::string formatHex64(std::uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

std::string valueToString(const std::map<const IrValue*, std::size_t>& ids, const IrValue* value) {
    if (value == nullptr) {
        return "<null>";
    }
    const auto found = ids.find(value);
    if (found != ids.end()) {
        return "%" + std::to_string(found->second);
    }
    switch (value->Type()) {
        case IrType::ScalarReg:
            return "s" + std::to_string(value->Register().index);
        case IrType::VectorReg:
            return "v" + std::to_string(value->Register().index);
        case IrType::Bool:
            return value->ImmediateBool() ? "true" : "false";
        case IrType::U8:
            return std::to_string(value->ImmediateU8()) + "u8";
        case IrType::U16:
            return std::to_string(value->ImmediateU16()) + "u16";
        case IrType::U32:
            return formatHex32(value->ImmediateU32());
        case IrType::U64:
            return formatHex64(value->ImmediateU64());
        case IrType::F16:
            return "f16(" + formatHex16(value->ImmediateF16Bits()) + ")";
        case IrType::F32:
            return std::to_string(value->ImmediateF32()) + "f";
        default:
            return "<" + TypeToString(value->Type()) + ">";
    }
}

}

std::vector<std::unique_ptr<IrBlock>>& IrProgram::Blocks() {
    return blocks;
}

const std::vector<std::unique_ptr<IrBlock>>& IrProgram::Blocks() const {
    return blocks;
}

const std::vector<std::unique_ptr<IrValue>>& IrProgram::Values() const {
    return values;
}

IrBlock& IrProgram::EntryBlock() const {
    if (entryBlock == nullptr) {
        throw std::runtime_error("IrProgram::EntryBlock has not been set");
    }
    return *entryBlock;
}

ShaderInfo& IrProgram::Info() {
    return resourcePlan.info;
}

const ShaderInfo& IrProgram::Info() const {
    return resourcePlan.info;
}

std::uint32_t IrProgram::WaveSize() const {
    return waveSize;
}

void IrProgram::SetWaveSize(std::uint32_t waveSize) {
    this->waveSize = waveSize;
}

IrBlock& IrProgram::CreateBlock() {
    auto block = std::make_unique<IrBlock>(nextBlockId);
    nextBlockId++;
    IrBlock& created = *block;
    blocks.push_back(std::move(block));
    return created;
}

IrValue& IrProgram::CreateValue(IrOpcode opcode, IrType type) {
    auto value = std::make_unique<IrValue>(opcode, type, nextValueId);
    nextValueId++;
    IrValue& created = *value;
    values.push_back(std::move(value));
    return created;
}

IrResourcePlan& IrProgram::Resources() {
    return resourcePlan;
}

const IrResourcePlan& IrProgram::Resources() const {
    return resourcePlan;
}

IrProgramMetadata& IrProgram::Metadata() {
    return metadata;
}

const IrProgramMetadata& IrProgram::Metadata() const {
    return metadata;
}

CompiledShaderInfo IrProgram::TakeCompiledInfo() && {
    CompiledShaderInfo result;
    result.stage = resourcePlan.stage;
    result.shaderHash = resourcePlan.shaderHash;
    result.waveSize = waveSize;
    result.userDataBase = resourcePlan.userDataBase;
    result.userDataCount = resourcePlan.userDataCount;
    result.scratchDwords = resourcePlan.info.scratchDwords;
    result.info = std::move(resourcePlan.info);
    result.bindings = std::move(metadata.bindings);
    for (const auto& output : result.info.outputs) {
        if (output.kind == StageOutputKind::Parameter && output.index < 32u) {
            result.paramExportMask |= 1u << output.index;
        }
    }
    return result;
}

IrValue& IrProgram::CreateValue(IrOpcode opcode, IrType type, std::uint64_t flags) {
    IrValue& created = CreateValue(opcode, type);
    created.SetFlags(flags);
    return created;
}

void IrProgram::SetEntryBlock(IrBlock& block) {
    const auto found = std::find_if(blocks.begin(), blocks.end(), [&block](const std::unique_ptr<IrBlock>& candidate) {
        return candidate.get() == &block;
    });
    if (found == blocks.end()) {
        throw std::runtime_error("IrProgram::SetEntryBlock block does not belong to this program");
    }
    entryBlock = &block;
}

std::vector<IrBlock*>& IrProgram::BlockOrder() {
    return blockOrder;
}

const std::vector<IrBlock*>& IrProgram::BlockOrder() const {
    return blockOrder;
}

std::string ProgramToString(const IrProgram& program) {
    std::map<const IrValue*, std::size_t> ids;
    std::size_t nextId = 1;
    for (const auto* block : program.BlockOrder()) {
        for (const auto* inst : block->Instructions()) {
            ids.emplace(inst, nextId++);
        }
    }
    std::string text;
    const auto& blockOrder = program.BlockOrder();
    const auto& blockInfo = program.Metadata().blockInfo;
    for (std::size_t blockIndex = 0; blockIndex < blockOrder.size(); blockIndex++) {
        text += "Block $" + std::to_string(blockIndex) + " pc=" + formatHex32(blockInfo[blockIndex].startPc) + ".." + formatHex32(blockInfo[blockIndex].endPc) + "\n";
        for (const auto* inst : blockOrder[blockIndex]->Instructions()) {
            if (inst->Type() != IrType::Void) {
                text += "  %" + std::to_string(ids.at(inst)) + " = " + std::string(IrOpcodeName(inst->Opcode()));
            } else {
                text += "          " + std::string(IrOpcodeName(inst->Opcode()));
            }
            for (std::size_t index = 0; index < inst->ArgumentCount(); index++) {
                text += index == 0 ? " " : ", ";
                if (inst->Opcode() == IrOpcode::Phi) {
                    const auto predecessor = std::find(blockOrder.begin(), blockOrder.end(), inst->PhiBlock(index));
                    text += "[" + valueToString(ids, inst->Argument(index)) + ", $" + std::to_string(std::distance(blockOrder.begin(), predecessor)) + "]";
                } else {
                    text += valueToString(ids, inst->Argument(index));
                }
            }
            text += " (" + TypeToString(inst->Type()) + "; uses=" + std::to_string(inst->UseCount()) + ")\n";
        }
    }
    return text;
}

void ValidateProgram(const IrProgram& program, bool requireSsa) {
    const auto& blockOrder = program.BlockOrder();
    const auto& blockStorage = program.Blocks();
    const auto& blockInfo = program.Metadata().blockInfo;
    if (blockOrder.size() != blockInfo.size() || blockOrder.size() != blockStorage.size()) {
        fail("value IR block storage is inconsistent");
    }
    IdIndex<IrBlock> blockIndices;
    std::unordered_map<std::uint32_t, const IrBlock*> blocksById;
    IdIndex<IrValue> instructionPositions;
    for (std::size_t blockIndex = 0; blockIndex < blockOrder.size(); blockIndex++) {
        const auto* block = blockOrder[blockIndex];
        if (block == nullptr || blockStorage[blockIndex] == nullptr || block != blockStorage[blockIndex].get()) {
            fail("value IR block pointer is inconsistent");
        }
        if (!blockIndices.emplace(block, blockIndex)) {
            fail("value IR block pointer is duplicated");
        }
        if (blockInfo[blockIndex].id == InvalidControlFlowId) {
            fail("value IR block uses the reserved exit id");
        }
        if (!blocksById.emplace(blockInfo[blockIndex].id, block).second) {
            fail("value IR block id is duplicated");
        }
        std::size_t position = 0;
        for (const auto* inst : block->Instructions()) {
            if (!instructionPositions.emplace(inst, position++)) {
                fail("value IR instruction is duplicated");
            }
        }
    }
    if (!blockOrder.empty() && !blockOrder.front()->Predecessors().empty()) {
        fail("value IR entry block has a predecessor");
    }

    for (std::size_t blockIndex = 0; blockIndex < blockOrder.size(); blockIndex++) {
        const auto* block = blockOrder[blockIndex];
        std::unordered_set<const IrBlock*> predecessors;
        for (const auto* predecessor : block->Predecessors()) {
            if (predecessor == nullptr || !blockIndices.contains(predecessor)) {
                fail("value IR block has a foreign predecessor");
            }
            if (!predecessors.insert(predecessor).second) {
                fail("value IR block predecessor is duplicated");
            }
            if (std::find(predecessor->Successors().begin(), predecessor->Successors().end(), block) == predecessor->Successors().end()) {
                fail("value IR predecessor edge is not reciprocal");
            }
        }

        std::unordered_set<const IrBlock*> successors;
        for (const auto* successor : block->Successors()) {
            if (successor == nullptr || !blockIndices.contains(successor)) {
                fail("value IR block has a foreign successor");
            }
            if (!successors.insert(successor).second) {
                fail("value IR block successor is duplicated");
            }
            if (std::find(successor->Predecessors().begin(), successor->Predecessors().end(), block) == successor->Predecessors().end()) {
                fail("value IR successor edge is not reciprocal");
            }
        }

        std::unordered_set<const IrBlock*> expectedSuccessors;
        const auto addTarget = [&](std::uint32_t id) {
            const auto found = blocksById.find(id);
            if (found == blocksById.end()) {
                return false;
            }
            expectedSuccessors.insert(found->second);
            return true;
        };
        const auto& terminator = blockInfo[blockIndex].terminator;
        const auto validateControlValue = [&](IrValue* value, IrType type) {
            if (value == nullptr || value->Type() != type) {
                return false;
            }
            return value->Parent() == nullptr || instructionPositions.contains(value);
        };
        switch (terminator.kind) {
            case TerminatorKind::Branch:
                if (!addTarget(terminator.trueBlock)) {
                    fail("value IR branch target is missing");
                }
                break;
            case TerminatorKind::ConditionalBranch:
                if (!addTarget(terminator.trueBlock) || !addTarget(terminator.falseBlock)) {
                    fail("value IR conditional branch target is missing");
                }
                if (!validateControlValue(blockInfo[blockIndex].condition, IrType::Bool)) {
                    fail("value IR conditional branch condition is invalid");
                }
                break;
            case TerminatorKind::IndirectBranch: {
                if (!validateControlValue(blockInfo[blockIndex].indirectTarget, IrType::U32)) {
                    fail("value IR indirect branch selector is invalid");
                }
                std::unordered_set<std::uint32_t> indirectTargets;
                for (const auto target : terminator.indirectTargets) {
                    if (!indirectTargets.insert(target).second) {
                        fail("value IR indirect branch target is duplicated");
                    }
                    if (!addTarget(target)) {
                        fail("value IR indirect branch target is missing");
                    }
                }
                if (terminator.indirectSelectorValues.size() != terminator.indirectSelectorTargets.size()) {
                    fail("value IR indirect selector table is inconsistent");
                }
                for (const auto target : terminator.indirectSelectorTargets) {
                    const auto found = blocksById.find(target);
                    if (found == blocksById.end() || !expectedSuccessors.contains(found->second)) {
                        fail("value IR indirect selector target is not a CFG successor");
                    }
                }
                break;
            }
            case TerminatorKind::Return:
            case TerminatorKind::Unsupported:
                break;
        }
        if ((terminator.mergeBlock != InvalidControlFlowId && !blocksById.contains(terminator.mergeBlock)) || (terminator.continueBlock != InvalidControlFlowId && !blocksById.contains(terminator.continueBlock))) {
            fail("value IR structured control target is missing");
        }
        if (successors != expectedSuccessors) {
            fail("value IR terminator and successor graph disagree");
        }

        bool sawNonPhi = false;
        for (const auto* inst : block->Instructions()) {
            if (inst->Parent() != block) {
                fail("value IR instruction has the wrong parent block");
            }
            if (inst->IsPhi()) {
                if (sawNonPhi) {
                    fail("value IR Phi appears after a non-Phi instruction");
                }
                if (inst->PhiBlockCount() != inst->ArgumentCount()) {
                    fail("value IR Phi parent table is inconsistent");
                }
                if (inst->ArgumentCount() == 0) {
                    fail("value IR Phi has no incoming values");
                }
                std::unordered_set<const IrBlock*> incomingBlocks;
                for (std::size_t argIndex = 0; argIndex < inst->ArgumentCount(); argIndex++) {
                    const auto* predecessor = inst->PhiBlock(argIndex);
                    if (predecessor == nullptr || !blockIndices.contains(predecessor) || !predecessors.contains(predecessor)) {
                        fail("value IR Phi has a foreign or non-predecessor parent");
                    }
                    if (!incomingBlocks.insert(predecessor).second) {
                        fail("value IR Phi parent is duplicated");
                    }
                    const auto* argument = inst->Argument(argIndex);
                    if (argument == nullptr) {
                        fail("value IR Phi has an empty argument");
                    }
                    if (argument->Type() != inst->Type()) {
                        fail("value IR Phi incoming type does not match its result");
                    }
                }
                if (incomingBlocks != predecessors) {
                    fail("value IR Phi does not cover every predecessor");
                }
            } else {
                sawNonPhi = true;
            }
            if (requireSsa && isRegisterStatePseudo(inst->Opcode())) {
                fail("register-state pseudo " + std::string(IrOpcodeName(inst->Opcode())) + " survived SSA rewrite");
            }
            if (inst->Opcode() != IrOpcode::Phi && inst->Opcode() != IrOpcode::Identity && inst->Type() == IrType::Opaque) {
                fail("untyped opcode " + std::string(IrOpcodeName(inst->Opcode())) + " survived translation");
            }
            const bool fixedSignature = inst->Opcode() != IrOpcode::Phi && inst->Opcode() != IrOpcode::Identity;
            if (fixedSignature && inst->ArgumentCount() != IrOpcodeOperandCount(inst->Opcode())) {
                fail(std::string(IrOpcodeName(inst->Opcode())) + " has " + std::to_string(inst->ArgumentCount()) + " arguments, expected " + std::to_string(IrOpcodeOperandCount(inst->Opcode())));
            }
            if (inst->Opcode() == IrOpcode::ReadConstBuffer) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid memory-info index");
                }
                const auto& memory = program.Resources().memoryInfo[memoryIndex];
                if (memory.kind != ResourceKind::ScalarBuffer) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid scalar-memory resource kind");
                }
                const bool validGroupWidth = memory.componentCount == 1u || memory.componentCount == 2u || memory.componentCount == 4u || memory.componentCount == 8u || memory.componentCount == 16u;
                if (memory.dataBits != 32u || memory.dataDwords != 1u || !validGroupWidth || memory.componentIndex >= memory.componentCount) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has inconsistent scalar-memory metadata");
                }
            }
            const auto addressInfo = AddressOpcodeInfoOf(inst->Opcode());
            if (addressInfo.access != AddressAccess::None) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid memory-info index");
                }
                const auto& memory = program.Resources().memoryInfo[memoryIndex];
                if (!IsAddressResourceKind(memory.kind) || (memory.kind == ResourceKind::ScalarAddress && inst->Opcode() != IrOpcode::LoadAddressU32)) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid address resource kind");
                }
                const bool scalarAddress = memory.kind == ResourceKind::ScalarAddress;
                const bool validGroupWidth = scalarAddress ? (memory.componentCount == 1u || memory.componentCount == 2u || memory.componentCount == 4u || memory.componentCount == 8u || memory.componentCount == 16u) : (memory.componentCount >= 1u && memory.componentCount <= 4u);
                const bool validWideLayout = addressInfo.components == 1u || (memory.componentIndex == 0u && memory.componentCount == addressInfo.components);
                if (memory.dataBits != addressInfo.dataBits || memory.dataDwords != addressInfo.components || !validGroupWidth || !validWideLayout || memory.componentIndex >= memory.componentCount || memory.sampler != 0u) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has inconsistent address-memory metadata");
                }
            }
            const auto bufferComponents = BufferComponentCount(inst->Opcode());
            if (bufferComponents != 0u) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid memory-info index");
                }
                const auto& memory = program.Resources().memoryInfo[memoryIndex];
                if (memory.kind != ResourceKind::Buffer && memory.kind != ResourceKind::ScalarBuffer) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has a non-buffer resource kind");
                }
                if (bufferComponents > 1u && (memory.kind != ResourceKind::Buffer || memory.dataBits != 32u || memory.dataDwords != bufferComponents || memory.componentIndex != 0u)) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has inconsistent native-wide metadata");
                }
                if (bufferComponents == 1u && (inst->Opcode() == IrOpcode::LoadBufferU32 || inst->Opcode() == IrOpcode::StoreBufferU32) && memory.dataDwords != 1u) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " retains scalar-sibling width metadata");
                }
            }
            const auto sharedComponents = SharedComponentCount(inst->Opcode());
            if (sharedComponents != 0u) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid memory-info index");
                }
                const auto& memory = program.Resources().memoryInfo[memoryIndex];
                if ((memory.kind != ResourceKind::Lds && memory.kind != ResourceKind::Gds) || memory.resource != 0u || memory.sampler != 0u || memory.componentCount == 0u || memory.componentIndex >= memory.componentCount) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has invalid shared-memory metadata");
                }
                std::uint32_t expectedBits = 32u;
                if (inst->Opcode() == IrOpcode::LoadSharedU8 || inst->Opcode() == IrOpcode::WriteSharedU8) {
                    expectedBits = 8u;
                } else if (inst->Opcode() == IrOpcode::LoadSharedU16 || inst->Opcode() == IrOpcode::WriteSharedU16) {
                    expectedBits = 16u;
                }
                if (memory.dataBits != expectedBits || memory.dataDwords != sharedComponents || (sharedComponents > 1u && memory.componentIndex != 0u)) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has inconsistent shared-memory width");
                }
            }
            const auto imageInfo = ImageOpcodeInfoOf(inst->Opcode());
            if (imageInfo.access != ImageAccess::None) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid memory-info index");
                }
                const auto& memory = program.Resources().memoryInfo[memoryIndex];
                if (memory.kind != ResourceKind::Image || imageInfo.resourceClass == ImageResourceClass::None) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has invalid image-memory metadata");
                }
            }
            if (inst->Opcode() == IrOpcode::SetAttribute && inst->Flags<ExportFlags>().index >= program.Metadata().exportInfo.size()) {
                fail("SetAttribute has an invalid export-info index");
            }
            std::uint32_t compositeComponents = 0u;
            switch (inst->Opcode()) {
                case IrOpcode::CompositeExtractU64:
                    compositeComponents = 2u;
                    break;
                case IrOpcode::CompositeExtractU32x2:
                    compositeComponents = 2u;
                    break;
                case IrOpcode::CompositeExtractU32x3:
                    compositeComponents = 3u;
                    break;
                case IrOpcode::CompositeExtractU32x4:
                    compositeComponents = 4u;
                    break;
                default:
                    break;
            }
            if (compositeComponents != 0u) {
                const auto* index = inst->Argument(1);
                if (index == nullptr || !index->HasImmediate() || index->Type() != IrType::U32 || index->ImmediateU32() >= compositeComponents) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an invalid component index");
                }
            }
            for (std::size_t argIndex = 0; argIndex < inst->ArgumentCount(); argIndex++) {
                const auto* arg = inst->Argument(argIndex);
                if (arg == nullptr) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " has an empty argument");
                }
                if (fixedSignature && arg->Type() != IrOpcodeArgumentType(inst->Opcode(), argIndex)) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " argument " + std::to_string(argIndex) + " has type " + TypeToString(arg->Type()) + ", expected " + TypeToString(IrOpcodeArgumentType(inst->Opcode(), argIndex)));
                }
                if (arg->Parent() != nullptr && !instructionPositions.contains(arg)) {
                    fail("value IR argument has a foreign definition");
                }
                const auto& uses = arg->OperandUses();
                const auto use = std::find_if(uses.begin(), uses.end(), [&](const IrUse& candidate) {
                    return candidate.user == inst && candidate.operand == argIndex;
                });
                if (use == uses.end()) {
                    fail(std::string(IrOpcodeName(inst->Opcode())) + " argument " + std::to_string(argIndex) + " is absent from " + std::string(IrOpcodeName(arg->Opcode())) + " reverse uses");
                }
            }
        }
    }

    if (blockOrder.empty()) {
        return;
    }

    std::vector<bool> reachable(blockOrder.size(), false);
    std::vector<std::size_t> pending{0u};
    while (!pending.empty()) {
        const auto blockIndex = pending.back();
        pending.pop_back();
        if (reachable[blockIndex]) {
            continue;
        }
        reachable[blockIndex] = true;
        for (const auto* successor : blockOrder[blockIndex]->Successors()) {
            pending.push_back(blockIndices.at(successor));
        }
    }
    if (!std::all_of(reachable.begin(), reachable.end(), [](bool value) { return value; })) {
        fail("value IR contains an unreachable block");
    }

    std::vector<std::vector<bool>> dominators(blockOrder.size(), std::vector<bool>(blockOrder.size(), true));
    dominators.front().assign(blockOrder.size(), false);
    dominators.front().front() = true;
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t blockIndex = 1; blockIndex < blockOrder.size(); blockIndex++) {
            std::vector<bool> next(blockOrder.size(), true);
            for (const auto* predecessor : blockOrder[blockIndex]->Predecessors()) {
                const auto predecessorIndex = blockIndices.at(predecessor);
                for (std::size_t candidate = 0; candidate < next.size(); candidate++) {
                    next[candidate] = next[candidate] && dominators[predecessorIndex][candidate];
                }
            }
            next[blockIndex] = true;
            if (next != dominators[blockIndex]) {
                dominators[blockIndex] = std::move(next);
                changed = true;
            }
        }
    }

    const auto dominates = [&](const IrBlock* definition, const IrBlock* use) {
        return dominators[blockIndices.at(use)][blockIndices.at(definition)];
    };
    const auto controlDominates = [&](IrValue* value, const IrBlock* use) {
        return value == nullptr || value->Parent() == nullptr || value->Parent() == use || dominates(value->Parent(), use);
    };

    for (std::size_t blockIndex = 0; blockIndex < blockOrder.size(); blockIndex++) {
        const auto* block = blockOrder[blockIndex];
        for (const auto* inst : block->Instructions()) {
            for (std::size_t argIndex = 0; argIndex < inst->ArgumentCount(); argIndex++) {
                const auto* definition = inst->Argument(argIndex);
                if (definition == nullptr || definition->Parent() == nullptr) {
                    continue;
                }
                if (inst->Opcode() == IrOpcode::Phi) {
                    const auto* predecessor = inst->PhiBlock(argIndex);
                    if (definition->Parent() != predecessor && !dominates(definition->Parent(), predecessor)) {
                        fail("value IR Phi incoming definition does not dominate its edge");
                    }
                } else if (definition->Parent() == block) {
                    if (instructionPositions.at(definition) >= instructionPositions.at(inst)) {
                        fail("value IR instruction uses a same-block definition before it");
                    }
                } else if (!dominates(definition->Parent(), block)) {
                    fail("value IR instruction definition does not dominate its use");
                }
            }
        }
        const auto& info = blockInfo[blockIndex];
        if (info.condition != nullptr && !controlDominates(info.condition, block)) {
            fail("value IR branch condition definition does not dominate its use");
        }
        if (info.indirectTarget != nullptr && !controlDominates(info.indirectTarget, block)) {
            fail("value IR indirect selector definition does not dominate its use");
        }
    }
}

void ResolveControlFlowIdentities(IrProgram& program) {
    for (auto& info : program.Metadata().blockInfo) {
        if (info.condition != nullptr) {
            info.condition = info.condition->Resolve();
        }
        if (info.indirectTarget != nullptr) {
            info.indirectTarget = info.indirectTarget->Resolve();
        }
    }
}

bool EquivalentValue(const IrResourcePlan& program, const IrValue* left, const IrValue* right) {
    std::vector<std::pair<const IrValue*, const IrValue*>> visited;
    return equivalentValueImpl(program, left, right, visited);
}

IrValue* ResolveInvariantPhi(const IrResourcePlan& program, IrValue* value) {
    if (value == nullptr) {
        throw std::runtime_error("ResolveInvariantPhi value cannot be null");
    }
    value = value->Resolve();
    if (!value->IsPhi()) {
        return value;
    }
    IrValue* invariant = nullptr;
    std::vector<IrValue*> pending{value};
    std::unordered_set<const IrValue*> visited;
    while (!pending.empty()) {
        auto* raw = pending.back();
        pending.pop_back();
        if (raw == nullptr) {
            throw std::runtime_error("ResolveInvariantPhi encountered a null phi operand");
        }
        auto* current = raw->Resolve();
        if (current->IsPhi()) {
            if (!visited.insert(current).second) {
                continue;
            }
            for (std::size_t index = 0; index < current->ArgumentCount(); index++) {
                pending.push_back(current->Argument(index));
            }
            continue;
        }
        if (invariant == nullptr) {
            invariant = current;
        } else if (!EquivalentValue(program, invariant, current)) {
            return nullptr;
        }
    }
    return invariant;
}

bool IsAddressResourceKind(ResourceKind kind) {
    return kind == ResourceKind::ScalarAddress || kind == ResourceKind::Flat || kind == ResourceKind::Global || kind == ResourceKind::Scratch;
}

PositionExportComponent DecodePositionExportComponent(std::uint32_t control, std::uint32_t positionIndex, std::uint32_t component) {
    PositionExportComponent result;
    if (positionIndex == 0u || component >= 4u) {
        return result;
    }
    std::uint32_t slot = positionIndex - 1u;
    std::uint32_t vector = 3u;
    for (std::uint32_t index = 0; index < 3u; index++) {
        if ((control & (1u << (21u + index))) != 0u) {
            if (slot == 0u) {
                vector = index;
                break;
            }
            slot--;
        }
    }
    if (vector == 3u) {
        return result;
    }
    if (vector == 0u) {
        result.pointSize = component == 0u && (control & (1u << 16u)) != 0u;
        result.layer = component == 2u && (control & (1u << 18u)) != 0u;
        result.viewport = component == 2u && (control & (1u << 19u)) != 0u;
        return result;
    }
    const auto scalar = (vector - 1u) * 4u + component;
    const auto lower = (1u << scalar) - 1u;
    const auto clip = control & 0xffu;
    const auto cull = (control >> 8u) & 0xffu;
    if ((clip & (1u << scalar)) != 0u) {
        result.clipDistance = static_cast<std::uint32_t>(std::popcount(clip & lower));
    }
    if ((cull & (1u << scalar)) != 0u) {
        result.cullDistance = static_cast<std::uint32_t>(std::popcount(cull & lower));
    }
    return result;
}

std::uint32_t NativeBinding(IrShaderStage stage, DescriptorBindingKind kind) {
    const std::uint32_t group = stage == IrShaderStage::Pixel ? 1u : stage == IrShaderStage::TessellationControl ? 2u : stage == IrShaderStage::TessellationEvaluation ? 3u : 0u;
    return RuntimeAbi::BindingNumber(static_cast<RuntimeAbi::Stage>(group), kind);
}

ImageResourceClass ImageBindingResourceClass(DescriptorBindingKind kind) {
    const auto value = static_cast<std::uint32_t>(kind);
    if (value >= FirstImageBinding && value < FirstStorageImageBinding) {
        return ImageResourceClass::Sampled;
    }
    if (value >= FirstStorageImageBinding && value < static_cast<std::uint32_t>(DescriptorBindingKind::Samplers)) {
        return ImageResourceClass::Storage;
    }
    return ImageResourceClass::None;
}

std::uint32_t ImageBindingIndex(DescriptorBindingKind kind) {
    return static_cast<std::uint32_t>(kind) - FirstImageBinding;
}

DescriptorBindingKind DescriptorBindingForImage(const ImageResource& image) {
    constexpr std::uint32_t sampledFloatBinding = 1u;
    constexpr std::uint32_t sampledUintBinding = 8u;
    constexpr std::uint32_t sampledSintBinding = 15u;
    constexpr std::uint32_t storageFloatBinding = FirstStorageImageBinding;
    constexpr std::uint32_t storageUintBinding = storageFloatBinding + 7u;
    constexpr std::uint32_t atomicUintBinding = storageUintBinding + 7u;
    constexpr std::uint32_t atomic64UintBinding = atomicUintBinding + 7u;

    std::uint32_t base = 0u;
    if (image.resourceClass == ImageResourceClass::Sampled) {
        if (image.atomic) {
            fail("DescriptorBindingForImage sampled image cannot be atomic");
        }
        switch (image.numericClass) {
            case IrTextureNumericClass::Float:
                base = image.depthCompare ? FirstComparisonImageBinding : sampledFloatBinding;
                break;
            case IrTextureNumericClass::Uint:
                base = sampledUintBinding;
                break;
            case IrTextureNumericClass::Sint:
                base = sampledSintBinding;
                break;
            case IrTextureNumericClass::Unsupported:
            default:
                fail("DescriptorBindingForImage sampled image has an unsupported numeric class");
        }
        if (image.depthCompare && image.numericClass != IrTextureNumericClass::Float) {
            fail("DescriptorBindingForImage depth-compare image must be float");
        }
    } else if (image.resourceClass == ImageResourceClass::Storage) {
        if (image.atomic) {
            if (image.numericClass != IrTextureNumericClass::Uint) {
                fail("DescriptorBindingForImage atomic image must be uint");
            }
            base = image.atomic64 ? atomic64UintBinding : atomicUintBinding;
        } else {
            switch (image.numericClass) {
                case IrTextureNumericClass::Float:
                    base = storageFloatBinding;
                    break;
                case IrTextureNumericClass::Uint:
                    base = storageUintBinding;
                    break;
                case IrTextureNumericClass::Sint:
                case IrTextureNumericClass::Unsupported:
                default:
                    fail("DescriptorBindingForImage storage image has an unsupported numeric class");
            }
        }
    } else {
        fail("DescriptorBindingForImage image has no resource class");
    }

    std::uint32_t dimension = 0u;
    switch (image.dimension) {
        case RdnaImageDimension::Dim1D:
            break;
        case RdnaImageDimension::Dim1DArray:
            dimension = 1u;
            break;
        case RdnaImageDimension::Dim2D:
            dimension = 2u;
            break;
        case RdnaImageDimension::Dim2DArray:
            dimension = 3u;
            break;
        case RdnaImageDimension::Dim2DMsaa:
            dimension = 4u;
            break;
        case RdnaImageDimension::Dim2DMsaaArray:
            dimension = 5u;
            break;
        case RdnaImageDimension::Dim3D:
            dimension = 6u;
            break;
        case RdnaImageDimension::Unknown:
        default:
            fail("DescriptorBindingForImage image has an unknown dimension");
    }
    return static_cast<DescriptorBindingKind>(base + dimension);
}

}
