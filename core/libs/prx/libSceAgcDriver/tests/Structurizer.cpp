// A selection in a loop with an arm that continues and another target it shares with an enclosing
// selection: its merge is where the other arm goes, so splitting the shared merge terminates.
#include "ControlFlow/Structurizer.hpp"
#include <cstdio>
#include <exception>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

int main() {
    // 0 -> 1 (loop header) -> 2; 2: 5 | 3; 3: 5 | 4; 4 -> 7 (continue); 5: 6 (exit) | 7; 6 returns;
    // 7 -> 1. Around the back edge every path from 4 passes 5, so 5 post-dominates both selections.
    const std::vector<std::vector<std::uint32_t>> successors{{1}, {2}, {5, 3}, {5, 4}, {7}, {6, 7}, {}, {1}};
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
    try {
        Structurizer{}.Structurize(graph);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (graph.FindBlock(2).terminator.mergeBlock == graph.FindBlock(3).terminator.mergeBlock) {
        std::fprintf(stderr, "the nested selections share merge block %u\n", graph.FindBlock(2).terminator.mergeBlock);
        return 1;
    }
    return 0;
}
