#include "ControlFlow/Structurizer.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
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

void requireExactPostDominators(const ControlFlowGraph& graph, const char* name) {
    const auto count = static_cast<std::uint32_t>(graph.blocks.size());
    const auto reachesExitAvoiding = [&](std::uint32_t from, std::uint32_t avoided) {
        std::vector<bool> seen(count, false);
        std::vector<std::uint32_t> stack{from};
        seen[from] = true;
        while (!stack.empty()) {
            const auto id = stack.back();
            stack.pop_back();
            if (graph.blocks[id].successors.empty()) return true;
            for (const auto successor : graph.blocks[id].successors) {
                if (successor == avoided || seen[successor]) continue;
                seen[successor] = true;
                stack.push_back(successor);
            }
        }
        return false;
    };
    for (const auto& block : graph.blocks) {
        std::vector<std::uint32_t> expected;
        for (std::uint32_t candidate = 0; candidate < count; ++candidate) {
            if (candidate == block.id || !reachesExitAvoiding(block.id, candidate)) expected.push_back(candidate);
        }
        if (block.postDominators != expected) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(block.id) + " has " + std::to_string(block.postDominators.size()) + " post-dominators, expected " + std::to_string(expected.size()));
        }
    }
}

void requireDominatedMerges(const ControlFlowGraph& graph, const char* name) {
    for (const auto& block : graph.blocks) {
        const auto merge = block.terminator.mergeBlock;
        if (merge != InvalidControlFlowId && !graph.Dominates(block.id, merge)) {
            throw std::runtime_error(std::string(name) + ": header " + std::to_string(block.id) + " does not dominate its merge " + std::to_string(merge));
        }
    }
}

using Routes = std::map<std::uint32_t, bool>;

std::uint32_t originalBlock(const ControlFlowGraph& graph, std::uint32_t blockId, Routes& routes) {
    for (std::size_t step = 0; step <= graph.blocks.size(); ++step) {
        const auto& block = graph.FindBlock(blockId);
        const auto& terminator = block.terminator;
        if (block.instructionBegin != block.instructionEnd) return blockId;
        if (terminator.gotoValue >= 0) routes[terminator.gotoVariable] = terminator.gotoValue != 0;
        if (terminator.kind == TerminatorKind::Branch) {
            blockId = terminator.trueBlock;
        } else if (terminator.kind == TerminatorKind::ConditionalBranch && terminator.condition == BranchCondition::GotoVariable && routes.contains(terminator.gotoVariable)) {
            blockId = routes.at(terminator.gotoVariable) ? terminator.trueBlock : terminator.falseBlock;
        } else {
            break;
        }
    }
    throw std::runtime_error("block " + std::to_string(blockId) + " does not lead to an original block");
}

void requireOriginalBranches(const ControlFlowGraph& graph, const std::vector<std::vector<std::uint32_t>>& successors, const char* name) {
    std::vector<bool> seen(successors.size(), false);
    Routes entryRoutes;
    std::vector<std::pair<std::uint32_t, Routes>> pending{{originalBlock(graph, graph.entryBlock, entryRoutes), entryRoutes}};
    std::vector<std::pair<std::uint32_t, Routes>> visited;
    while (!pending.empty()) {
        auto state = std::move(pending.back());
        pending.pop_back();
        if (std::find(visited.begin(), visited.end(), state) != visited.end()) continue;
        visited.push_back(state);
        const auto& block = graph.FindBlock(state.first);
        const auto original = block.instructionBegin / 2;
        std::vector<std::uint32_t> branchTargets;
        if (block.terminator.kind == TerminatorKind::Branch) branchTargets = {block.terminator.trueBlock};
        if (block.terminator.kind == TerminatorKind::ConditionalBranch) branchTargets = {block.terminator.trueBlock, block.terminator.falseBlock};
        std::vector<std::uint32_t> targets;
        for (const auto target : branchTargets) {
            auto routes = state.second;
            const auto reached = originalBlock(graph, target, routes);
            targets.push_back(graph.FindBlock(reached).instructionBegin / 2);
            pending.emplace_back(reached, std::move(routes));
        }
        if (targets != successors[original]) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(original) + " no longer branches to its original targets");
        }
        seen[original] = true;
    }
    if (std::find(seen.begin(), seen.end(), false) != seen.end()) {
        throw std::runtime_error(std::string(name) + ": an original block is missing");
    }
}

void requireStructuredLoop(const std::vector<std::vector<std::uint32_t>>& successors, const char* name) {
    auto graph = makeGraph(successors);
    Structurizer{}.Structurize(graph);
    requireExactPostDominators(graph, name);
    requireStructuredBranches(graph, name);
    requireDominatedMerges(graph, name);
    requireOriginalBranches(graph, successors, name);
}

const BasicBlock& originalCopy(const ControlFlowGraph& graph, std::uint32_t original) {
    const auto copy = std::find_if(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& block) { return block.instructionBegin == original * 2 && block.instructionEnd == original * 2 + 2; });
    if (copy == graph.blocks.end()) throw std::runtime_error("original block " + std::to_string(original) + " is missing");
    return *copy;
}

std::uint32_t guestBlock(const ControlFlowGraph& graph, std::uint32_t blockId, Routes& routes, const char* name) {
    for (std::size_t step = 0; step <= graph.blocks.size(); ++step) {
        const auto& block = graph.FindBlock(blockId);
        const auto& terminator = block.terminator;
        if (block.instructionBegin != block.instructionEnd) {
            if (terminator.gotoValue >= 0) throw std::runtime_error(std::string(name) + ": block " + std::to_string(blockId) + " sets a route variable after guest instructions");
            return blockId;
        }
        if (terminator.gotoValue >= 0) routes[terminator.gotoVariable] = terminator.gotoValue != 0;
        if (terminator.kind == TerminatorKind::Branch) {
            blockId = terminator.trueBlock;
        } else if (terminator.kind == TerminatorKind::ConditionalBranch && terminator.condition == BranchCondition::GotoVariable) {
            const auto route = routes.find(terminator.gotoVariable);
            if (route == routes.end()) throw std::runtime_error(std::string(name) + ": block " + std::to_string(blockId) + " reads route variable " + std::to_string(terminator.gotoVariable) + " before any path sets it");
            blockId = route->second ? terminator.trueBlock : terminator.falseBlock;
        } else {
            return blockId;
        }
    }
    throw std::runtime_error(std::string(name) + ": a cycle of empty blocks");
}

void requireSameExecutions(const ControlFlowGraph& original, const ControlFlowGraph& structured, const char* name) {
    if (structured.blocks.empty() || structured.entryBlock != 0 || structured.blocks.front().id != 0) {
        throw std::runtime_error(std::string(name) + ": the structured graph does not start at block 0");
    }
    using State = std::tuple<std::uint32_t, std::uint32_t, Routes>;
    std::vector<State> pending{{0u, original.entryBlock, {}}};
    std::set<State> visited;
    while (!pending.empty()) {
        auto [structuredId, originalId, routes] = std::move(pending.back());
        pending.pop_back();
        const auto copyId = guestBlock(structured, structuredId, routes, name);
        if (!visited.emplace(copyId, originalId, routes).second) continue;
        const auto& copy = structured.FindBlock(copyId);
        const auto& source = original.FindBlock(originalId);
        if (copy.instructionBegin != source.instructionBegin || copy.instructionEnd != source.instructionEnd || copy.terminator.kind != source.terminator.kind || copy.terminator.condition != source.terminator.condition) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(copyId) + " does not run like original block " + std::to_string(originalId));
        }
        if (source.terminator.kind == TerminatorKind::Branch || source.terminator.kind == TerminatorKind::ConditionalBranch) pending.emplace_back(copy.terminator.trueBlock, source.terminator.trueBlock, routes);
        if (source.terminator.kind == TerminatorKind::ConditionalBranch) pending.emplace_back(copy.terminator.falseBlock, source.terminator.falseBlock, routes);
    }
}

void requireUnconditionalEntry(const ControlFlowGraph& original, const ControlFlowGraph& structured, const char* name) {
    const auto& entry = original.FindBlock(original.entryBlock);
    auto blockId = structured.blocks.front().id;
    for (std::size_t step = 0; step <= structured.blocks.size(); ++step) {
        const auto& block = structured.FindBlock(blockId);
        if (block.instructionBegin == entry.instructionBegin && block.instructionEnd == entry.instructionEnd) return;
        if (block.instructionBegin != block.instructionEnd || block.terminator.kind != TerminatorKind::Branch) break;
        blockId = block.terminator.trueBlock;
    }
    throw std::runtime_error(std::string(name) + ": the entry block does not run unconditionally first");
}

void requireSameStructuredExecutions(const std::vector<std::vector<std::uint32_t>>& successors, const char* name, const std::map<std::uint32_t, std::uint32_t>& words = {}) {
    auto original = makeGraph(successors);
    for (const auto& [blockId, count] : words) original.blocks[blockId].estimatedSpirvWords = count;
    auto graph = original;
    Structurizer{}.Structurize(graph);
    requireExactPostDominators(graph, name);
    requireStructuredBranches(graph, name);
    requireDominatedMerges(graph, name);
    requireOriginalBranches(graph, successors, name);
    requireSameExecutions(original, graph, name);
    requireUnconditionalEntry(original, graph, name);
}

void requireNoRouteVariables(const std::vector<std::vector<std::uint32_t>>& successors, const char* name) {
    if (std::getenv("APS5_FORCE_STRUCTURIZER_FALLBACK") != nullptr) return;
    auto graph = makeGraph(successors);
    Structurizer{}.Structurize(graph);
    for (const auto& block : graph.blocks) {
        if (block.terminator.condition == BranchCondition::GotoVariable || block.terminator.gotoValue >= 0) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(block.id) + " routes through a goto variable instead of a structured selection");
        }
    }
}

void requireGrowthBudgetRefusal(ControlFlowGraph graph, const char* name) {
    const std::string growthBudget = "CFG shared merge splitting exceeded growth budget";
    try {
        Structurizer{}.Structurize(graph);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).starts_with(growthBudget)) return;
        throw std::runtime_error(std::string(name) + ": refused with \"" + error.what() + "\" instead of the shared merge splitting growth budget");
    }
    throw std::runtime_error(std::string(name) + ": was structurized instead of refused at the shared merge splitting growth budget");
}

std::vector<std::vector<std::uint32_t>> originalPostDominators(const ControlFlowGraph& graph) {
    std::vector<std::uint32_t> all;
    for (std::uint32_t id = 0; id < graph.blocks.size(); ++id) all.push_back(id);
    std::vector<std::vector<std::uint32_t>> result;
    for (const auto& block : graph.blocks) result.push_back(block.successors.empty() ? std::vector<std::uint32_t>{block.id} : all);
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& block : graph.blocks) {
            std::vector<std::uint32_t> next;
            if (block.successors.empty()) {
                next = {block.id};
            } else {
                next = result[block.successors.front()];
                for (std::size_t index = 1; index < block.successors.size(); ++index) {
                    std::vector<std::uint32_t> intersection;
                    const auto& successor = result[block.successors[index]];
                    std::set_intersection(next.begin(), next.end(), successor.begin(), successor.end(), std::back_inserter(intersection));
                    next = std::move(intersection);
                }
                if (std::find(next.begin(), next.end(), block.id) == next.end()) next.push_back(block.id);
                std::sort(next.begin(), next.end());
                next.erase(std::unique(next.begin(), next.end()), next.end());
            }
            if (next != result[block.id]) {
                result[block.id] = std::move(next);
                changed = true;
            }
        }
    }
    return result;
}

std::vector<std::vector<std::uint32_t>> originalDominators(const ControlFlowGraph& graph) {
    std::vector<std::uint32_t> all;
    for (std::uint32_t id = 0; id < graph.blocks.size(); ++id) all.push_back(id);
    std::vector<std::vector<std::uint32_t>> result;
    for (const auto& block : graph.blocks) result.push_back(block.id == graph.entryBlock ? std::vector<std::uint32_t>{block.id} : all);
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& block : graph.blocks) {
            if (block.id == graph.entryBlock) continue;
            std::vector<std::uint32_t> next;
            if (block.predecessors.empty()) {
                next = {block.id};
            } else {
                next = result[block.predecessors.front()];
                for (std::size_t index = 1; index < block.predecessors.size(); ++index) {
                    std::vector<std::uint32_t> intersection;
                    const auto& predecessor = result[block.predecessors[index]];
                    std::set_intersection(next.begin(), next.end(), predecessor.begin(), predecessor.end(), std::back_inserter(intersection));
                    next = std::move(intersection);
                }
                if (std::find(next.begin(), next.end(), block.id) == next.end()) next.push_back(block.id);
                std::sort(next.begin(), next.end());
                next.erase(std::unique(next.begin(), next.end()), next.end());
            }
            if (next != result[block.id]) {
                result[block.id] = std::move(next);
                changed = true;
            }
        }
    }
    return result;
}

void requireOriginalDominanceSets(ControlFlowGraph graph, const char* name) {
    for (auto& block : graph.blocks) {
        block.dominators.clear();
        block.postDominators.clear();
    }
    const auto expected = originalPostDominators(graph);
    const auto expectedDominators = originalDominators(graph);
    const auto original = graph;
    const std::string stopAfterAnalyses = "dominance differential fixture: stop before CFG rewrites";
    graph.unsupported = true;
    graph.unsupportedReason = stopAfterAnalyses;
    bool stopped = false;
    try {
        Structurizer{}.Structurize(graph);
    } catch (const std::runtime_error& error) {
        if (error.what() != stopAfterAnalyses) throw;
        stopped = true;
    }
    if (!stopped || graph.blocks.size() != original.blocks.size() || graph.entryBlock != original.entryBlock) {
        throw std::runtime_error(std::string(name) + ": differential fixture did not stop before CFG rewrites");
    }
    for (std::size_t id = 0; id < graph.blocks.size(); ++id) {
        const auto& block = graph.blocks[id];
        if (block.id != original.blocks[id].id || block.successors != original.blocks[id].successors || block.predecessors != original.blocks[id].predecessors) {
            throw std::runtime_error(std::string(name) + ": differential fixture changed the CFG");
        }
        if (block.postDominators != expected[id]) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(id) + " differs from the original post-dominator fixed point");
        }
        if (block.dominators != expectedDominators[id]) {
            throw std::runtime_error(std::string(name) + ": block " + std::to_string(id) + " differs from the original dominator fixed point");
        }
    }
}

}

int main() {
    try {
        auto largeBlocks = makeGraph({{1}, {2, 3}, {1, 4}, {5, 6}, {1, 7}, {8, 7}, {9, 4}, {10}, {6, 10}, {10}, {}});
        for (auto& block : largeBlocks.blocks) block.estimatedSpirvWords = 1000u;
        Structurizer{}.Structurize(largeBlocks);
        requireStructuredBranches(largeBlocks, "large loop exit blocks retain the baseline result");
        auto exitJunction = makeGraph({{1}, {2, 3}, {9}, {4, 6}, {5, 9}, {1}, {7, 8}, {9}, {1}, {}});
        Structurizer{}.Structurize(exitJunction);
        requireStructuredBranches(exitJunction, "shared loop exit junctions retain structured branches");
        auto nested = makeGraph({{1}, {2}, {5, 3}, {5, 4}, {7}, {6, 7}, {}, {1}});
        Structurizer{}.Structurize(nested);
        requireExactPostDominators(nested, "nested selections");
        if (originalCopy(nested, 2).terminator.mergeBlock == originalCopy(nested, 3).terminator.mergeBlock) {
            std::fprintf(stderr, "the nested selections share merge block %u\n", originalCopy(nested, 2).terminator.mergeBlock);
            return 1;
        }
        requireStructuredBranches(nested, "nested selections");
        auto exitTail = makeGraph({{1}, {2}, {3, 4}, {6}, {6, 5}, {1}, {}});
        Structurizer{}.Structurize(exitTail);
        requireExactPostDominators(exitTail, "a loop exit through a tail block");
        requireStructuredBranches(exitTail, "a loop exit through a tail block");
        auto exitTails = makeGraph({{1}, {2}, {3, 4}, {7}, {5, 6}, {7, 1}, {7}, {}});
        Structurizer{}.Structurize(exitTails);
        requireExactPostDominators(exitTails, "a loop with two exit tails");
        requireStructuredBranches(exitTails, "a loop with two exit tails");
        auto endingExits = makeGraph({{6, 1}, {2}, {3}, {6, 4}, {2, 5}, {}, {}});
        Structurizer{}.Structurize(endingExits);
        requireExactPostDominators(endingExits, "a loop whose two exits end the program");
        requireStructuredBranches(endingExits, "a loop whose two exits end the program");
        auto threeEndingExits = makeGraph({{1}, {2}, {5, 3}, {6, 4}, {1, 7}, {}, {}, {}});
        Structurizer{}.Structurize(threeEndingExits);
        requireExactPostDominators(threeEndingExits, "a loop whose three exits end the program");
        requireStructuredBranches(threeEndingExits, "a loop whose three exits end the program");
        auto innerEndingExit = makeGraph({{1}, {2}, {3}, {7, 4}, {2, 5}, {1, 6}, {}, {}});
        Structurizer{}.Structurize(innerEndingExit);
        requireExactPostDominators(innerEndingExit, "an inner loop exit that ends the program");
        requireStructuredBranches(innerEndingExit, "an inner loop exit that ends the program");
        requireStructuredLoop({{1}, {2, 4}, {3, 4}, {4, 5}, {1, 6}, {1}, {}}, "nested selections in a loop with two latches");
        requireStructuredLoop({{1}, {5, 2}, {5, 3}, {6, 4}, {7, 5}, {8, 1}, {1}, {1}, {}}, "nested selections on the false side in a loop with three latches");
        requireStructuredLoop({{1}, {2, 5}, {3, 4}, {5, 6}, {5}, {1, 7}, {1}, {}}, "an if-else in a loop whose one arm continues");
        requireStructuredLoop({{1}, {2}, {3, 4}, {2, 5}, {}, {1}}, "an inner loop that returns or continues its outer loop");
        requireStructuredLoop({{1}, {2, 3}, {4, 5}, {}, {3, 6}, {1}, {2}}, "an inner loop that leaves both loops or continues its outer loop");
        requireStructuredLoop({{1, 3}, {2, 4}, {3, 1}, {4}, {}}, "a loop whose latch exits into a block the branch before the loop also reaches");
        requireStructuredLoop({{1, 2}, {3, 2}, {4}, {4, 1}, {}}, "a loop whose header exits into a block the branch before the loop also reaches");
        requireStructuredLoop({{1, 2}, {4}, {3, 1}, {4, 2}, {}}, "a loop whose header exits into a block the branch before the loop reaches first");
        auto earlyReturn = makeGraph({{2, 1}, {4, 2}, {3, 5}, {5}, {}, {}});
        Structurizer{}.Structurize(earlyReturn);
        requireExactPostDominators(earlyReturn, "an early return inside a selection that joins its parent's merge");
        requireStructuredBranches(earlyReturn, "an early return inside a selection that joins its parent's merge");
        requireDominatedMerges(earlyReturn, "an early return inside a selection that joins its parent's merge");
        const std::vector<std::vector<std::uint32_t>> elseReturnSuccessors{{1, 2}, {4}, {3, 4}, {}, {5, 6}, {7}, {7}, {}};
        auto elseReturn = makeGraph(elseReturnSuccessors);
        Structurizer{}.Structurize(elseReturn);
        requireExactPostDominators(elseReturn, "an early return in one arm of a selection whose other arm reaches the same merge");
        requireStructuredBranches(elseReturn, "an early return in one arm of a selection whose other arm reaches the same merge");
        requireDominatedMerges(elseReturn, "an early return in one arm of a selection whose other arm reaches the same merge");
        requireOriginalBranches(elseReturn, elseReturnSuccessors, "an early return in one arm of a selection whose other arm reaches the same merge");
        const std::vector<std::vector<std::uint32_t>> branchingReturn{{5, 1}, {3, 2}, {13}, {9, 4}, {13}, {10, 6}, {8, 7}, {13}, {13, 9}, {13}, {12, 11}, {12}, {13}, {}};
        requireSameStructuredExecutions(branchingReturn, "an arm whose returns branch once a tail shared across arms is cloned");
        requireNoRouteVariables(branchingReturn, "an arm whose returns branch once a tail shared across arms is cloned");
        std::vector<std::vector<std::uint32_t>> diamonds;
        for (std::uint32_t diamond = 0; diamond < 50u; ++diamond) {
            const auto head = diamond * 3u;
            diamonds.push_back({head + 1u, head + 2u});
            diamonds.push_back({head + 3u});
            diamonds.push_back({head + 3u});
        }
        diamonds.push_back({});
        auto chain = makeGraph(diamonds);
        Structurizer{}.Structurize(chain);
        requireExactPostDominators(chain, "a chain of fifty selections");
        requireStructuredLoop({{1}, {2, 8}, {3, 5}, {4, 8}, {7}, {6, 7}, {7, 8}, {1}, {}}, "nested selections sharing a loop continuation");
        auto sharedDiscard = makeGraph({{5, 1}, {4, 2}, {7, 3}, {7, 4}, {5}, {7, 6}, {}, {}});
        const std::vector<std::uint32_t> sharedDiscardWords{444, 1944, 68, 1128, 8, 2084, 148, 28};
        for (auto& block : sharedDiscard.blocks) {
            std::sort(block.successors.begin(), block.successors.end());
            block.estimatedSpirvWords = sharedDiscardWords[block.id];
        }
        Structurizer{}.Structurize(sharedDiscard);
        requireStructuredBranches(sharedDiscard, "early discards that share one ending block");
        requireSameStructuredExecutions({{1, 2}, {3, 0}, {}, {2}}, "an entry loop with two exits whose merges collide");
        requireSameStructuredExecutions({{2}, {}, {4, 1}, {1}, {2, 3}}, "a selection region that wraps around to its dominator");
        requireSameStructuredExecutions({{1}, {2, 3}, {0, 4}, {}, {3}}, "an entry loop whose merge splitting runs out of budget");
        requireSameStructuredExecutions({{2, 5}, {4, 1}, {1, 3}, {}, {}, {1}}, "a self-loop entered from both arms of the entry selection");
        requireSameStructuredExecutions({{1, 2}, {2, 3}, {1, 3}, {}}, "an irreducible cycle entered at both blocks");
        requireSameStructuredExecutions({{1}, {2, 4}, {3, 4}, {4, 5}, {1, 6}, {1}, {}}, "nested selections in a loop with two latches compared run by run");
        requireSameStructuredExecutions({{1, 2}, {2, 0}, {1}}, "a cycle entered at both blocks inside a loop that never ends");
        requireSameStructuredExecutions({{1, 3}, {2}, {1}, {}}, "a branch that returns or enters a loop that never ends");
        requireSameStructuredExecutions({{1}, {1}}, "a block that branches to itself forever");
        requireSameStructuredExecutions({{1, 13}, {2, 8}, {3, 6}, {4, 0}, {5, 4}, {6}, {}, {}, {5, 9}, {10, 0}, {11}, {11, 12}, {}, {14}, {15}, {16, 17}, {4}, {18, 19}, {7, 0}, {}}, "merge splitting that adds one forwarder per round until its budget runs out");
        const std::vector<std::vector<std::uint32_t>> junctionLoop{{1, 2}, {}, {3, 10}, {4, 8}, {6, 0}, {}, {7, 8}, {0}, {5, 9}, {7}, {9, 11}, {11}};
        requireSameStructuredExecutions(junctionLoop, "a loop whose latch is too large to clone, so merge splitting keeps adding junctions", {{7u, 2000u}});
        auto unsupportedExit = makeGraph(junctionLoop);
        unsupportedExit.blocks[7].estimatedSpirvWords = 2000u;
        unsupportedExit.blocks[5].terminator.kind = TerminatorKind::Unsupported;
        requireGrowthBudgetRefusal(std::move(unsupportedExit), "a junction loop whose unsupported exit keeps the reach-flag fallback out");
        requireOriginalDominanceSets(makeGraph(diamonds), "raw diamond chain");
        requireOriginalDominanceSets(makeGraph({{1}, {2, 3}, {1}, {}}), "a loop with an exit");
        requireOriginalDominanceSets(makeGraph({{1}, {2}, {1}}), "a non-exiting SCC");
        requireOriginalDominanceSets(makeGraph({{1, 3}, {2}, {1}, {}}), "an exit-or-spin branch");
        requireOriginalDominanceSets(makeGraph({{1}, {}, {3}, {}, {5}, {4}}), "unreachable exit and non-exiting islands");
        auto reordered = makeGraph({{1}, {}, {2}, {0, 4}, {1}});
        reordered.entryBlock = 3;
        requireOriginalDominanceSets(std::move(reordered), "a nonzero entry with permuted block IDs");
        requireOriginalDominanceSets(makeGraph({{1, 2}, {}, {}}), "disjoint exits with an empty post-dominator intersection");
        requireOriginalDominanceSets(makeGraph({{2}, {2}, {}}), "separate roots with an empty dominator intersection");
        requireOriginalDominanceSets(makeGraph({{1}, {1, 2}, {}}), "a self-loop with an exit");
        std::uint32_t randomState = 0x7b19a5c3u;
        const auto randomWord = [&] { randomState = randomState * 1664525u + 1013904223u; return randomState; };
        for (std::uint32_t trial = 0; trial < 24u; ++trial) {
            const auto count = 8u + trial % 9u;
            std::vector<std::uint32_t> ids(count);
            for (std::uint32_t id = 0; id < count; ++id) ids[id] = id;
            for (auto size = count; size > 1u; --size) std::swap(ids[size - 1u], ids[randomWord() % size]);
            if (ids.front() == 0u) std::swap(ids.front(), ids.back());
            std::vector<std::vector<std::uint32_t>> successors(count);
            for (std::uint32_t position = 0; position + 1u < count; ++position) {
                const auto remaining = count - position - 1u;
                const auto degree = position == 0u ? 2u : position == 1u ? 1u : std::min(randomWord() % 3u, remaining);
                if (degree == 0u) continue;
                auto& edges = successors[ids[position]];
                const auto first = position + 1u + randomWord() % remaining;
                edges.push_back(ids[first]);
                if (degree == 2u) {
                    auto second = position + 1u + randomWord() % (remaining - 1u);
                    if (second >= first) ++second;
                    edges.push_back(ids[second]);
                }
                std::sort(edges.begin(), edges.end());
            }
            auto dag = makeGraph(successors);
            dag.entryBlock = ids.front();
            const auto name = "deterministic DAG " + std::to_string(trial);
            requireOriginalDominanceSets(std::move(dag), name.c_str());
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
