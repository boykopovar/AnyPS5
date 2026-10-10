#include <Testing/Test.hpp>
#include "ControlFlow/Structurizer.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Fail;
using Testing::Require;

ControlFlowGraph MakeGraph(const std::vector<std::vector<std::uint32_t>>& successors) {
    ControlFlowGraph graph;
    graph.entryBlock = 0;
    for (std::uint32_t id = 0; id < successors.size(); ++id) {
        BasicBlock block;
        block.id = id;
        block.startProgramCounter = id * 8;
        block.endProgramCounter = id * 8 + 8;
        block.instructionBegin = id * 2;
        block.instructionEnd = id * 2 + 2;
        block.successors = successors[id];
        auto& terminator = block.terminator;
        if (successors[id].empty()) {
            terminator.kind = TerminatorKind::Return;
        } else if (successors[id].size() == 1) {
            terminator.kind = TerminatorKind::Branch;
            terminator.trueBlock = successors[id][0];
        } else {
            terminator.kind = TerminatorKind::ConditionalBranch;
            terminator.condition = BranchCondition::SccNonZero;
            terminator.trueBlock = successors[id][0];
            terminator.falseBlock = successors[id][1];
        }
        graph.blocks.push_back(std::move(block));
    }
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) graph.blocks[successor].predecessors.push_back(block.id);
    }
    return graph;
}

ControlFlowGraph Structurized(const std::vector<std::vector<std::uint32_t>>& successors) {
    auto graph = MakeGraph(successors);
    Structurizer{}.Structurize(graph);
    return graph;
}

const BasicBlock* InnermostLoopHeader(const ControlFlowGraph& graph, std::uint32_t blockId) {
    const BasicBlock* innermost = nullptr;
    std::size_t innermostSize = 0;
    for (const auto& header : graph.blocks) {
        if (!header.terminator.loopHeader || !graph.Dominates(header.id, blockId) || graph.Dominates(header.terminator.mergeBlock, blockId) || blockId == header.terminator.mergeBlock) continue;
        const auto size = static_cast<std::size_t>(std::count_if(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& block) { return graph.Dominates(header.id, block.id) && !graph.Dominates(header.terminator.mergeBlock, block.id); }));
        if (innermost == nullptr || size < innermostSize) {
            innermost = &header;
            innermostSize = size;
        }
    }
    return innermost;
}

void RequireStructuredBranches(const ControlFlowGraph& graph, const char* name) {
    for (const auto& block : graph.blocks) {
        const auto& terminator = block.terminator;
        if (terminator.kind != TerminatorKind::ConditionalBranch || terminator.loopHeader || terminator.trueBlock == terminator.falseBlock) continue;
        const auto* loop = InnermostLoopHeader(graph, block.id);
        if (terminator.mergeBlock != InvalidControlFlowId) {
            if (loop != nullptr && terminator.mergeBlock == loop->terminator.continueBlock) {
                Fail(std::string(name) + ": block " + std::to_string(block.id) + " in the loop at block " + std::to_string(loop->id) + " merges at the loop's continue block " + std::to_string(terminator.mergeBlock));
            }
            continue;
        }
        const auto exits = [&](std::uint32_t target) { return loop != nullptr && (target == loop->terminator.mergeBlock || target == loop->terminator.continueBlock); };
        if (!exits(terminator.trueBlock) && !exits(terminator.falseBlock)) {
            Fail(std::string(name) + ": block " + std::to_string(block.id) + " branches to " + std::to_string(terminator.trueBlock) + "/" + std::to_string(terminator.falseBlock) + " without a merge, and neither is its loop's merge or continue");
        }
    }
}

const Case nestedSelections{"Structurize_NestedSelections_GetDistinctMergesAndStructuredBranches", [] {
    const auto nested = Structurized({{1}, {2}, {5, 3}, {5, 4}, {7}, {6, 7}, {}, {1}});
    Require(nested.FindBlock(2).terminator.mergeBlock != nested.FindBlock(3).terminator.mergeBlock,
            "the nested selections share merge block " + std::to_string(nested.FindBlock(2).terminator.mergeBlock));
    RequireStructuredBranches(nested, "nested selections");
}};

const Case loopExitThroughTail{"Structurize_LoopExitThroughTailBlock_HasStructuredBranches", [] {
    RequireStructuredBranches(Structurized({{1}, {2}, {3, 4}, {6}, {6, 5}, {1}, {}}), "a loop exit through a tail block");
}};

const Case loopWithTwoExitTails{"Structurize_LoopWithTwoExitTails_HasStructuredBranches", [] {
    RequireStructuredBranches(Structurized({{1}, {2}, {3, 4}, {7}, {5, 6}, {7, 1}, {7}, {}}), "a loop with two exit tails");
}};

const Case twoEndingExits{"Structurize_LoopWhoseTwoExitsEndTheProgram_HasStructuredBranches", [] {
    RequireStructuredBranches(Structurized({{6, 1}, {2}, {3}, {6, 4}, {2, 5}, {}, {}}), "a loop whose two exits end the program");
}};

const Case threeEndingExits{"Structurize_LoopWhoseThreeExitsEndTheProgram_HasStructuredBranches", [] {
    RequireStructuredBranches(Structurized({{1}, {2}, {5, 3}, {6, 4}, {1, 7}, {}, {}, {}}), "a loop whose three exits end the program");
}};

const Case innerEndingExit{"Structurize_InnerLoopExitEndingTheProgram_HasStructuredBranches", [] {
    RequireStructuredBranches(Structurized({{1}, {2}, {3}, {7, 4}, {2, 5}, {1, 6}, {}, {}}), "an inner loop exit that ends the program");
}};

const Case earlyReturn{"Structurize_EarlyReturnInsideSelection_HeadersDominateTheirMerges", [] {
    const auto graph = Structurized({{2, 1}, {4, 2}, {3, 5}, {5}, {}, {}});
    RequireStructuredBranches(graph, "an early return inside a selection that joins its parent's merge");
    for (const auto& block : graph.blocks) {
        const auto merge = block.terminator.mergeBlock;
        Require(merge == InvalidControlFlowId || graph.Dominates(block.id, merge),
                "an early return inside a selection: header " + std::to_string(block.id) + " does not dominate its merge " + std::to_string(merge));
    }
}};

} // namespace
