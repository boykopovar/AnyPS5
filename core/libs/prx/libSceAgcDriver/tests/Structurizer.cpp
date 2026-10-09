#include "ControlFlow/Structurizer.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

ControlFlowGraph makeGraph(const std::vector<std::vector<std::uint32_t>>& successors) {
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

const BasicBlock* innermostLoopHeader(const ControlFlowGraph& graph, std::uint32_t blockId) {
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

void requireStructuredBranches(const ControlFlowGraph& graph, const char* name) {
    for (const auto& block : graph.blocks) {
        const auto& terminator = block.terminator;
        if (terminator.kind != TerminatorKind::ConditionalBranch || terminator.loopHeader || terminator.trueBlock == terminator.falseBlock) continue;
        const auto* loop = innermostLoopHeader(graph, block.id);
        if (terminator.mergeBlock != InvalidControlFlowId) {
            if (loop != nullptr && terminator.mergeBlock == loop->terminator.continueBlock) {
                throw std::runtime_error(std::string(name) + ": block " + std::to_string(block.id) + " in the loop at block " + std::to_string(loop->id) + " merges at the loop's continue block " + std::to_string(terminator.mergeBlock));
            }
            continue;
        }
        const auto exits = [&](std::uint32_t target) { return loop != nullptr && (target == loop->terminator.mergeBlock || target == loop->terminator.continueBlock); };
        if (!exits(terminator.trueBlock) && !exits(terminator.falseBlock)) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(block.id) + " branches to " + std::to_string(terminator.trueBlock) + "/" + std::to_string(terminator.falseBlock) + " without a merge, and neither is its loop's merge or continue");
        }
    }
}

}

int main() {
    try {
        for (const auto& edges : {std::vector<std::vector<std::uint32_t>>{{1}, {2, 3}, {1}, {}},
                std::vector<std::vector<std::uint32_t>>{{1}, {2}, {1, 3}, {}}}) {
            auto graph = makeGraph(edges);
            const auto original = graph;
            Structurizer{}.Structurize(graph);
            for (const auto& source : original.blocks) {
                const auto copies = std::count_if(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& block) {
                    return block.instructionBegin == source.instructionBegin && block.instructionEnd == source.instructionEnd;
                });
                if (copies != 1) throw std::runtime_error("loop normalization changed a guest instruction range");
            }
            requireStructuredBranches(graph, "dedicated loop continuation");
        }
        auto nested = makeGraph({{1}, {2}, {5, 3}, {5, 4}, {7}, {6, 7}, {}, {1}});
        Structurizer{}.Structurize(nested);
        if (nested.FindBlock(2).terminator.mergeBlock == nested.FindBlock(3).terminator.mergeBlock) {
            std::fprintf(stderr, "the nested selections share merge block %u\n", nested.FindBlock(2).terminator.mergeBlock);
            return 1;
        }
        requireStructuredBranches(nested, "nested selections");
        auto exitTail = makeGraph({{1}, {2}, {3, 4}, {6}, {6, 5}, {1}, {}});
        Structurizer{}.Structurize(exitTail);
        requireStructuredBranches(exitTail, "a loop exit through a tail block");
        auto exitTails = makeGraph({{1}, {2}, {3, 4}, {7}, {5, 6}, {7, 1}, {7}, {}});
        Structurizer{}.Structurize(exitTails);
        requireStructuredBranches(exitTails, "a loop with two exit tails");
        auto endingExits = makeGraph({{6, 1}, {2}, {3}, {6, 4}, {2, 5}, {}, {}});
        Structurizer{}.Structurize(endingExits);
        requireStructuredBranches(endingExits, "a loop whose two exits end the program");
        auto threeEndingExits = makeGraph({{1}, {2}, {5, 3}, {6, 4}, {1, 7}, {}, {}, {}});
        Structurizer{}.Structurize(threeEndingExits);
        requireStructuredBranches(threeEndingExits, "a loop whose three exits end the program");
        auto innerEndingExit = makeGraph({{1}, {2}, {3}, {7, 4}, {2, 5}, {1, 6}, {}, {}});
        Structurizer{}.Structurize(innerEndingExit);
        requireStructuredBranches(innerEndingExit, "an inner loop exit that ends the program");
        auto earlyReturn = makeGraph({{2, 1}, {4, 2}, {3, 5}, {5}, {}, {}});
        Structurizer{}.Structurize(earlyReturn);
        requireStructuredBranches(earlyReturn, "an early return inside a selection that joins its parent's merge");
        for (const auto& block : earlyReturn.blocks) {
            const auto merge = block.terminator.mergeBlock;
            if (merge != InvalidControlFlowId && !earlyReturn.Dominates(block.id, merge)) {
                std::fprintf(stderr, "an early return inside a selection: header %u does not dominate its merge %u\n", block.id, merge);
                return 1;
            }
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
