#include "ControlFlow/ControlFlowGraph.hpp"
#include "ControlFlow/ControlFlowHelpers.hpp"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

std::string toHexString(std::uint32_t value) {
    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return std::string(buffer);
}

bool contains(const std::vector<std::uint32_t>& values, std::uint32_t value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::vector<std::uint32_t> intersectSorted(const std::vector<std::uint32_t>& first, const std::vector<std::uint32_t>& second) {
    std::vector<std::uint32_t> result;
    std::set_intersection(first.begin(), first.end(), second.begin(), second.end(), std::back_inserter(result));
    return result;
}

void addUnique(std::vector<std::uint32_t>& values, std::uint32_t value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void sortUnique(std::vector<std::uint32_t>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::uint32_t remapId(std::uint32_t id, const std::vector<std::uint32_t>& idMap) {
    return id != InvalidControlFlowId && id < idMap.size() ? idMap[id] : id;
}

void remapIds(std::vector<std::uint32_t>& values, const std::vector<std::uint32_t>& idMap) {
    for (auto& value : values) {
        value = remapId(value, idMap);
    }
    sortUnique(values);
}

void rebuildPredecessors(ControlFlowGraph& graph) {
    for (auto& block : graph.blocks) {
        block.predecessors.clear();
        sortUnique(block.successors);
    }
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) {
            addUnique(graph.blocks[successor].predecessors, block.id);
        }
    }
    for (auto& block : graph.blocks) {
        sortUnique(block.predecessors);
    }
}

namespace {

std::string joinIds(const std::vector<std::uint32_t>& values) {
    std::string text;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            text += ",";
        }
        text += std::to_string(values[i]);
    }
    return text;
}

}

const BasicBlock& ControlFlowGraph::FindBlock(std::uint32_t blockId) const {
    if (blockId < blocks.size() && blocks[blockId].id == blockId) {
        return blocks[blockId];
    }
    for (const auto& block : blocks) {
        if (block.id == blockId) {
            return block;
        }
    }
    throw std::invalid_argument("control flow graph has no block with id " + std::to_string(blockId));
}

BasicBlock& ControlFlowGraph::FindBlock(std::uint32_t blockId) {
    return const_cast<BasicBlock&>(static_cast<const ControlFlowGraph*>(this)->FindBlock(blockId));
}

const BasicBlock& ControlFlowGraph::FindBlockByProgramCounter(std::uint32_t programCounter) const {
    for (const auto& block : blocks) {
        if (block.startProgramCounter == programCounter) {
            return block;
        }
    }
    throw std::invalid_argument("control flow graph has no block starting at program counter " + toHexString(programCounter));
}

BasicBlock& ControlFlowGraph::FindBlockByProgramCounter(std::uint32_t programCounter) {
    return const_cast<BasicBlock&>(static_cast<const ControlFlowGraph*>(this)->FindBlockByProgramCounter(programCounter));
}

bool ControlFlowGraph::Dominates(std::uint32_t dominator, std::uint32_t blockId) const {
    const auto& dominators = FindBlock(blockId).dominators;
    return std::binary_search(dominators.begin(), dominators.end(), dominator);
}

bool ControlFlowGraph::PostDominates(std::uint32_t postDominator, std::uint32_t blockId) const {
    const auto& postDominators = FindBlock(blockId).postDominators;
    return std::binary_search(postDominators.begin(), postDominators.end(), postDominator);
}

std::uint32_t ControlFlowGraph::FindNearestCommonPostDominator(std::uint32_t firstBlock, std::uint32_t secondBlock) const {
    const auto& first = FindBlock(firstBlock);
    const auto& second = FindBlock(secondBlock);
    const auto common = intersectSorted(first.postDominators, second.postDominators);
    if (common.empty()) {
        return InvalidControlFlowId;
    }
    for (const auto candidate : common) {
        bool nearest = true;
        for (const auto other : common) {
            if (other != candidate && !PostDominates(other, candidate)) {
                nearest = false;
                break;
            }
        }
        if (nearest) {
            return candidate;
        }
    }
    throw std::logic_error("blocks " + std::to_string(firstBlock) + " and " + std::to_string(secondBlock) + " have no unique nearest common post-dominator");
}

std::string BranchConditionToString(BranchCondition condition) {
    switch (condition) {
        case BranchCondition::Always: return "always";
        case BranchCondition::SccZero: return "scc0";
        case BranchCondition::SccNonZero: return "scc1";
        case BranchCondition::VccZero: return "vccz";
        case BranchCondition::VccNonZero: return "vccnz";
        case BranchCondition::ExecZero: return "execz";
        case BranchCondition::ExecNonZero: return "execnz";
        case BranchCondition::ScalarInstruction: return "scalar_instruction";
        case BranchCondition::IndirectTarget: return "indirect_target";
        case BranchCondition::GotoVariable: return "goto_variable";
        case BranchCondition::Unknown: return "unknown";
    }
    throw std::invalid_argument("unsupported branch condition for string conversion");
}

std::string FailureKindToString(FailureKind kind) {
    switch (kind) {
        case FailureKind::None: return "None";
        case FailureKind::InvalidInput: return "InvalidInput";
        case FailureKind::UnsupportedInstruction: return "UnsupportedInstruction";
        case FailureKind::InvalidBranchTarget: return "InvalidBranchTarget";
        case FailureKind::MissingFallthrough: return "MissingFallthrough";
        case FailureKind::InvalidLabel: return "InvalidLabel";
        case FailureKind::IrreducibleControlFlow: return "IrreducibleControlFlow";
        case FailureKind::StructuredControlFlow: return "StructuredControlFlow";
    }
    throw std::invalid_argument("unsupported failure kind for string conversion");
}

std::string GraphToString(const ControlFlowGraph& graph) {
    std::string text;
    text += "entryBlock=" + std::to_string(graph.entryBlock);
    text += " irreducible=" + std::to_string(graph.irreducible ? 1u : 0u);
    text += " unsupported=" + std::to_string(graph.unsupported ? 1u : 0u);
    text += " failure=" + FailureKindToString(graph.failureKind);
    text += " failureBlock=" + std::to_string(graph.failureBlock);
    text += "\n";
    if (!graph.unsupportedReason.empty()) {
        text += "unsupportedReason=" + graph.unsupportedReason + "\n";
    }

    for (const auto& block : graph.blocks) {
        text += "block_" + std::to_string(block.id);
        text += " pc=" + toHexString(block.startProgramCounter);
        text += " end=" + toHexString(block.endProgramCounter);
        text += " inst=[" + std::to_string(block.instructionBegin) + "," + std::to_string(block.instructionEnd) + ")";
        text += " words=" + std::to_string(block.estimatedSpirvWords) + "\n";
        text += "  predecessors=[" + joinIds(block.predecessors) + "]";
        text += " successors=[" + joinIds(block.successors) + "]\n";
        text += "  dominators=[" + joinIds(block.dominators) + "]";
        text += " postDominators=[" + joinIds(block.postDominators) + "]\n";
        text += "  terminator=" + std::to_string(static_cast<std::uint32_t>(block.terminator.kind));
        text += " condition=" + BranchConditionToString(block.terminator.condition);
        text += " true=" + std::to_string(block.terminator.trueBlock);
        text += " false=" + std::to_string(block.terminator.falseBlock);
        text += " merge=" + std::to_string(block.terminator.mergeBlock);
        text += " continue=" + std::to_string(block.terminator.continueBlock);
        text += " loopHeader=" + std::to_string(block.terminator.loopHeader ? 1u : 0u);
        text += " indirectSgpr=" + std::to_string(block.terminator.indirectPcSgpr);
        text += " indirectSelector=" + std::to_string(block.terminator.indirectSelectorCode);
        text += " indirectTargets=[" + joinIds(block.terminator.indirectTargets) + "]";
        text += " selectorValues=[" + joinIds(block.terminator.indirectSelectorValues) + "]";
        text += " gotoVariable=" + std::to_string(block.terminator.gotoVariable);
        text += " gotoValue=" + std::to_string(block.terminator.gotoValue);
        text += "\n";
    }

    for (const auto& edge : graph.backEdges) {
        text += "backedge " + std::to_string(edge.sourceBlock) + " -> " + std::to_string(edge.targetBlock);
        text += " natural=" + std::to_string(edge.natural ? 1u : 0u) + "\n";
    }

    for (const auto& loop : graph.naturalLoops) {
        text += "loop header=" + std::to_string(loop.headerBlock);
        text += " latch=" + std::to_string(loop.latchBlock);
        text += " merge=" + std::to_string(loop.mergeBlock);
        text += " continue=" + std::to_string(loop.continueBlock);
        text += " body=[" + joinIds(loop.blocks) + "]";
        text += " exits=[" + joinIds(loop.exitBlocks) + "]\n";
    }

    for (const auto& component : graph.components) {
        text += "scc blocks=[" + joinIds(component.blocks) + "]";
        text += " entries=[" + joinIds(component.entryBlocks) + "]";
        text += " irreducible=" + std::to_string(component.irreducible ? 1u : 0u) + "\n";
    }

    return text;
}

}
