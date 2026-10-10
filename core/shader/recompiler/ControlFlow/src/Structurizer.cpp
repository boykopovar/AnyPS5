#include "ControlFlow/Structurizer.hpp"
#include "ControlFlow/ControlFlowHelpers.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

std::vector<std::uint32_t> allBlockIds(std::uint32_t count) {
    std::vector<std::uint32_t> ids;
    ids.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        ids.push_back(i);
    }
    return ids;
}

bool inPlaceIntersections() {
    static const bool enabled = std::getenv("APS5_NO_PERF_DOM_INTERSECTION") == nullptr;
    return enabled;
}

void intersectAnalysisSet(std::vector<std::uint32_t>& values, const std::vector<std::uint32_t>& other) {
    if (!inPlaceIntersections()) {
        values = intersectSorted(values, other);
        return;
    }
    std::size_t left = 0;
    std::size_t right = 0;
    std::size_t count = 0;
    while (left < values.size() && right < other.size()) {
        if (values[left] < other[right]) {
            ++left;
        } else if (other[right] < values[left]) {
            ++right;
        } else {
            values[count++] = values[left++];
            ++right;
        }
    }
    values.resize(count);
}

void replaceAnalysisSet(std::vector<std::uint32_t>& destination, std::vector<std::uint32_t>&& values) {
    if (inPlaceIntersections() && values.capacity() - values.size() > values.size()) {
        std::vector<std::uint32_t>(values).swap(values);
    }
    destination = std::move(values);
}

void addSortedUnique(std::vector<std::uint32_t>& values, std::uint32_t value) {
    const auto position = std::lower_bound(values.begin(), values.end(), value);
    if (position == values.end() || *position != value) {
        values.insert(position, value);
    }
}

bool replaceValue(std::vector<std::uint32_t>& values, std::uint32_t oldValue, std::uint32_t newValue) {
    bool changed = false;
    for (auto& value : values) {
        if (value == oldValue) {
            value = newValue;
            changed = true;
        }
    }
    if (changed) {
        sortUnique(values);
    }
    return changed;
}

bool removeValue(std::vector<std::uint32_t>& values, std::uint32_t value) {
    const auto oldSize = values.size();
    values.erase(std::remove(values.begin(), values.end(), value), values.end());
    return values.size() != oldSize;
}

bool replaceTerminatorTarget(Terminator& terminator, std::uint32_t oldValue, std::uint32_t newValue) {
    bool changed = false;
    if (terminator.trueBlock == oldValue) {
        terminator.trueBlock = newValue;
        changed = true;
    }
    if (terminator.falseBlock == oldValue) {
        terminator.falseBlock = newValue;
        changed = true;
    }
    return changed;
}

std::vector<std::uint32_t> applyBlockOrder(ControlFlowGraph& graph, std::vector<BasicBlock> blocks) {
    std::vector<std::uint32_t> idMap(blocks.size(), InvalidControlFlowId);
    for (std::uint32_t i = 0; i < blocks.size(); ++i) {
        idMap[blocks[i].id] = i;
    }

    graph.blocks = std::move(blocks);
    graph.entryBlock = remapId(graph.entryBlock, idMap);
    for (auto& block : graph.blocks) {
        block.id = remapId(block.id, idMap);
        remapIds(block.predecessors, idMap);
        remapIds(block.successors, idMap);
        remapIds(block.dominators, idMap);
        remapIds(block.postDominators, idMap);
        block.terminator.trueBlock = remapId(block.terminator.trueBlock, idMap);
        block.terminator.falseBlock = remapId(block.terminator.falseBlock, idMap);
        block.terminator.mergeBlock = remapId(block.terminator.mergeBlock, idMap);
        block.terminator.continueBlock = remapId(block.terminator.continueBlock, idMap);
        for (auto& target : block.terminator.indirectTargets) {
            target = remapId(target, idMap);
        }
        for (auto& target : block.terminator.indirectSelectorTargets) {
            target = remapId(target, idMap);
        }
    }

    return idMap;
}

std::uint32_t moveBlockBefore(ControlFlowGraph& graph, std::uint32_t blockId, std::uint32_t beforeId) {
    if (blockId == beforeId || blockId >= graph.blocks.size() || beforeId >= graph.blocks.size()) {
        return blockId;
    }

    const auto blockPos = blockId;
    const auto beforePos = beforeId;
    std::vector<BasicBlock> oldBlocks = std::move(graph.blocks);
    std::vector<BasicBlock> newBlocks;
    newBlocks.reserve(oldBlocks.size());

    for (std::uint32_t i = 0; i < oldBlocks.size(); ++i) {
        if (i == beforePos) {
            newBlocks.push_back(std::move(oldBlocks[blockPos]));
        }
        if (i != blockPos) {
            newBlocks.push_back(std::move(oldBlocks[i]));
        }
    }

    const auto idMap = applyBlockOrder(graph, std::move(newBlocks));
    return remapId(blockId, idMap);
}

std::vector<std::uint32_t> dominatedBlocks(const ControlFlowGraph& graph, std::uint32_t headerBlock, std::uint32_t stopBlock = InvalidControlFlowId) {
    std::vector<std::uint32_t> blocks;
    std::vector<std::uint32_t> stack = {headerBlock};
    std::vector<bool> visited(graph.blocks.size());
    blocks.reserve(graph.blocks.size());
    stack.reserve(graph.blocks.size());

    while (!stack.empty()) {
        const auto blockId = stack.back();
        stack.pop_back();
        if (blockId >= visited.size()) {
            visited.resize(blockId + 1);
        }
        if (blockId == stopBlock || visited[blockId] || !graph.Dominates(headerBlock, blockId)) {
            continue;
        }

        const auto& block = graph.FindBlock(blockId);
        visited[blockId] = true;
        blocks.push_back(blockId);
        for (const auto successor : block.successors) {
            if (successor != stopBlock && graph.Dominates(headerBlock, successor)) {
                stack.push_back(successor);
            }
        }
    }

    sortUnique(blocks);
    return blocks;
}

std::uint32_t appendSyntheticBranchBlock(ControlFlowGraph& graph, std::uint32_t target) {
    const auto& targetBlock = graph.FindBlock(target);

    BasicBlock block;
    block.id = static_cast<std::uint32_t>(graph.blocks.size());
    block.startProgramCounter = targetBlock.startProgramCounter;
    block.endProgramCounter = block.startProgramCounter;
    block.instructionBegin = targetBlock.instructionBegin;
    block.instructionEnd = block.instructionBegin;
    block.successors = {target};
    block.terminator.kind = TerminatorKind::Branch;
    block.terminator.condition = BranchCondition::Always;
    block.terminator.trueBlock = target;
    graph.blocks.push_back(std::move(block));
    return graph.blocks.back().id;
}

bool isolateSemanticLoopHeader(ControlFlowGraph& graph, std::uint32_t oldHeader, const std::function<void(ControlFlowGraph&)>& recompute) {
    const auto& header = graph.FindBlock(oldHeader);
    if (header.instructionBegin == header.instructionEnd) {
        return false;
    }

    const auto predecessors = header.predecessors;
    const auto newHeader = appendSyntheticBranchBlock(graph, oldHeader);
    for (const auto predecessor : predecessors) {
        auto& block = graph.FindBlock(predecessor);
        replaceValue(block.successors, oldHeader, newHeader);
        replaceTerminatorTarget(block.terminator, oldHeader, newHeader);
    }
    if (graph.entryBlock == oldHeader) {
        graph.entryBlock = newHeader;
    }
    moveBlockBefore(graph, newHeader, oldHeader);
    rebuildPredecessors(graph);
    recompute(graph);
    return true;
}

const NaturalLoop* findInnermostContainingLoop(const ControlFlowGraph& graph, std::uint32_t blockId) {
    const NaturalLoop* innermost = nullptr;
    for (const auto& loop : graph.naturalLoops) {
        if (contains(loop.blocks, blockId) && (innermost == nullptr || loop.blocks.size() < innermost->blocks.size())) {
            innermost = &loop;
        }
    }
    return innermost;
}

bool isInsideLoopConstruct(const ControlFlowGraph& graph, const NaturalLoop& loop, std::uint32_t blockId) {
    return blockId != InvalidControlFlowId && blockId != loop.mergeBlock && blockId != loop.continueBlock && graph.Dominates(loop.headerBlock, blockId) && !graph.Dominates(loop.mergeBlock, blockId);
}

bool isLoopControlGateway(const ControlFlowGraph& graph, const NaturalLoop& loop, std::uint32_t blockId) {
    const auto& block = graph.FindBlock(blockId);
    if (block.terminator.kind != TerminatorKind::ConditionalBranch) {
        return false;
    }
    const auto isControlTarget = [&](std::uint32_t target) {
        return target == loop.mergeBlock || target == loop.continueBlock;
    };
    return isControlTarget(block.terminator.trueBlock) && isControlTarget(block.terminator.falseBlock);
}

bool hasLinearPathToTerminal(const ControlFlowGraph& graph, std::uint32_t start) {
    std::vector<bool> visited(graph.blocks.size(), false);
    for (auto blockId = start;;) {
        if (visited[blockId]) {
            return false;
        }
        const auto& block = graph.FindBlock(blockId);
        if (block.successors.empty()) {
            return true;
        }
        if (block.successors.size() != 1) {
            return false;
        }
        visited[blockId] = true;
        blockId = block.successors.front();
    }
}

bool isEnclosingLinearExit(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t blockId) {
    if (!hasLinearPathToTerminal(graph, blockId)) {
        return false;
    }
    const BasicBlock* block = &graph.FindBlock(blockId);
    while (graph.Dominates(header, block->id) && block->successors.size() == 1u) {
        block = &graph.FindBlock(block->successors.front());
    }
    if (graph.Dominates(header, block->id) || block->predecessors.empty()) {
        return false;
    }
    return std::all_of(block->predecessors.begin(), block->predecessors.end(), [&](std::uint32_t predecessor) {
        return graph.Dominates(header, predecessor) || graph.Dominates(predecessor, header);
    });
}

bool canReachBefore(const ControlFlowGraph& graph, std::uint32_t start, std::uint32_t target, std::uint32_t stop) {
    std::vector<std::uint32_t> pending = {start};
    std::vector<bool> visited(graph.blocks.size(), false);
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (blockId == target) {
            return true;
        }
        if (blockId == stop || visited[blockId]) {
            continue;
        }
        visited[blockId] = true;
        const auto& block = graph.FindBlock(blockId);
        pending.insert(pending.end(), block.successors.begin(), block.successors.end());
    }
    return false;
}

bool reachesWithinIteration(const ControlFlowGraph& graph, const NaturalLoop& loop, std::uint32_t start, std::uint32_t target) {
    std::vector<std::uint32_t> pending = {start};
    std::vector<bool> visited(graph.blocks.size(), false);
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (blockId == target) {
            return true;
        }
        if (blockId == loop.continueBlock || blockId == loop.mergeBlock || visited[blockId]) {
            continue;
        }
        visited[blockId] = true;
        const auto& block = graph.FindBlock(blockId);
        pending.insert(pending.end(), block.successors.begin(), block.successors.end());
    }
    return false;
}

std::vector<std::uint32_t> linearTail(const ControlFlowGraph& graph, std::uint32_t start) {
    std::vector<std::uint32_t> tail;
    for (auto blockId = start; !contains(tail, blockId);) {
        tail.push_back(blockId);
        const auto& block = graph.FindBlock(blockId);
        if (block.successors.size() != 1u) {
            break;
        }
        blockId = block.successors.front();
    }
    return tail;
}

std::uint32_t mergeBesideReturns(const ControlFlowGraph& graph, const BasicBlock& header) {
    const auto reach = [&](std::uint32_t start) {
        std::vector<std::uint32_t> reached;
        std::vector<std::uint32_t> pending = {start};
        while (!pending.empty()) {
            const auto blockId = pending.back();
            pending.pop_back();
            if (blockId == header.id || contains(reached, blockId)) {
                continue;
            }
            reached.push_back(blockId);
            const auto& successors = graph.FindBlock(blockId).successors;
            pending.insert(pending.end(), successors.begin(), successors.end());
        }
        sortUnique(reached);
        return reached;
    };
    const auto trueReach = reach(header.terminator.trueBlock);
    const auto falseReach = reach(header.terminator.falseBlock);
    if (intersectSorted(trueReach, falseReach).empty()) {
        const auto returnsWithin = [&](const std::vector<std::uint32_t>& reached) {
            return std::all_of(reached.begin(), reached.end(), [&](std::uint32_t blockId) { return graph.Dominates(header.id, blockId); });
        };
        if (returnsWithin(trueReach) && graph.Dominates(header.id, header.terminator.falseBlock)) {
            return header.terminator.falseBlock;
        }
        if (returnsWithin(falseReach) && graph.Dominates(header.id, header.terminator.trueBlock)) {
            return header.terminator.trueBlock;
        }
    }
    const auto count = static_cast<std::uint32_t>(graph.blocks.size());
    const auto returns = [&](std::uint32_t blockId) {
        if (!graph.Dominates(header.id, blockId) || !hasLinearPathToTerminal(graph, blockId)) {
            return false;
        }
        const auto tail = linearTail(graph, blockId);
        return std::all_of(tail.begin(), tail.end(), [&](std::uint32_t member) { return graph.FindBlock(member).predecessors.size() == 1u; });
    };
    std::vector<std::vector<std::uint32_t>> kept(count);
    std::vector<std::vector<std::uint32_t>> postDominators(count, allBlockIds(count));
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) {
            if (!returns(successor)) {
                kept[block.id].push_back(successor);
            }
        }
        if (kept[block.id].empty()) {
            postDominators[block.id] = {block.id};
        }
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto& block : graph.blocks) {
            if (kept[block.id].empty()) {
                continue;
            }
            auto next = postDominators[kept[block.id].front()];
            for (std::size_t i = 1; i < kept[block.id].size(); ++i) {
                intersectAnalysisSet(next, postDominators[kept[block.id][i]]);
            }
            addUnique(next, block.id);
            sortUnique(next);
            if (next != postDominators[block.id]) {
                replaceAnalysisSet(postDominators[block.id], std::move(next));
                changed = true;
            }
        }
    }
    const auto common = intersectSorted(postDominators[header.terminator.trueBlock], postDominators[header.terminator.falseBlock]);
    const auto merge = std::find_if(common.begin(), common.end(), [&](std::uint32_t candidate) {
        return std::all_of(common.begin(), common.end(), [&](std::uint32_t other) { return contains(postDominators[candidate], other); });
    });
    if (merge == common.end() || *merge == header.id) {
        return InvalidControlFlowId;
    }
    const auto inConstruct = [&](std::uint32_t blockId) {
        return blockId != *merge && graph.Dominates(header.id, blockId) && !graph.Dominates(*merge, blockId);
    };
    for (const auto& block : graph.blocks) {
        if (block.id == header.id || !inConstruct(block.id)) {
            continue;
        }
        for (const auto successor : block.successors) {
            if (successor != *merge && !inConstruct(successor)) {
                return InvalidControlFlowId;
            }
        }
    }
    if (canReachBefore(graph, *merge, header.id, InvalidControlFlowId)) {
        return InvalidControlFlowId;
    }
    for (const auto& block : graph.blocks) {
        if (block.id != header.id && inConstruct(block.id) && canReachBefore(graph, *merge, block.id, header.id)) {
            return InvalidControlFlowId;
        }
    }
    return *merge;
}

std::uint32_t findSelectionMerge(const ControlFlowGraph& graph, const BasicBlock& block) {
    const auto globalMerge = graph.FindNearestCommonPostDominator(block.terminator.trueBlock, block.terminator.falseBlock);
    const auto* loop = findInnermostContainingLoop(graph, block.id);
    const auto trueTarget = block.terminator.trueBlock;
    const auto falseTarget = block.terminator.falseBlock;
    if (loop == nullptr) {
        const bool globalMergeIsExit = globalMerge == InvalidControlFlowId || graph.FindBlock(globalMerge).successors.empty();
        if (globalMergeIsExit) {
            const bool falseReachesTrue = canReachBefore(graph, falseTarget, trueTarget, globalMerge);
            const bool trueReachesFalse = canReachBefore(graph, trueTarget, falseTarget, globalMerge);
            if (falseReachesTrue != trueReachesFalse) {
                return falseReachesTrue ? trueTarget : falseTarget;
            }
            if (globalMerge == InvalidControlFlowId) {
                if (isEnclosingLinearExit(graph, block.id, trueTarget)) {
                    return trueTarget;
                }
                if (isEnclosingLinearExit(graph, block.id, falseTarget)) {
                    return falseTarget;
                }
                if (graph.Dominates(block.id, falseTarget) && hasLinearPathToTerminal(graph, trueTarget)) {
                    return falseTarget;
                }
                if (graph.Dominates(block.id, trueTarget) && hasLinearPathToTerminal(graph, falseTarget)) {
                    return trueTarget;
                }
                return mergeBesideReturns(graph, block);
            }
        }
        return globalMerge;
    }

    if (isLoopControlGateway(graph, *loop, trueTarget) && graph.Dominates(block.id, trueTarget) && isInsideLoopConstruct(graph, *loop, falseTarget)) {
        return trueTarget;
    }
    if (isLoopControlGateway(graph, *loop, falseTarget) && graph.Dominates(block.id, falseTarget) && isInsideLoopConstruct(graph, *loop, trueTarget)) {
        return falseTarget;
    }
    const auto joinsAt = [&](std::uint32_t join, std::uint32_t other) {
        const auto& arm = graph.FindBlock(other);
        const bool armLeavesOrJoins = arm.terminator.kind == TerminatorKind::ConditionalBranch && arm.predecessors.size() == 1u && std::all_of(arm.successors.begin(), arm.successors.end(), [&](std::uint32_t successor) {
            return successor == join || successor == loop->continueBlock || successor == loop->mergeBlock;
        });
        return armLeavesOrJoins && isInsideLoopConstruct(graph, *loop, other) && isInsideLoopConstruct(graph, *loop, join) && graph.Dominates(block.id, join);
    };
    if (joinsAt(trueTarget, falseTarget)) {
        return trueTarget;
    }
    if (joinsAt(falseTarget, trueTarget)) {
        return falseTarget;
    }
    if (globalMerge != InvalidControlFlowId) {
        const bool trueJoins = reachesWithinIteration(graph, *loop, trueTarget, globalMerge);
        const bool falseJoins = reachesWithinIteration(graph, *loop, falseTarget, globalMerge);
        if (trueJoins != falseJoins) {
            return trueJoins ? trueTarget : falseTarget;
        }
    }
    return globalMerge;
}

bool isInnermostLoopControlConditional(const ControlFlowGraph& graph, const BasicBlock& block) {
    if (block.terminator.kind != TerminatorKind::ConditionalBranch) {
        return false;
    }
    const auto* loop = findInnermostContainingLoop(graph, block.id);
    if (loop == nullptr || loop->mergeBlock == InvalidControlFlowId || loop->continueBlock == InvalidControlFlowId) {
        return false;
    }
    const auto trueTarget = block.terminator.trueBlock;
    const auto falseTarget = block.terminator.falseBlock;
    if (block.id == loop->continueBlock) {
        const auto isRepeatTarget = [&](std::uint32_t target) {
            return target == loop->headerBlock || target == loop->mergeBlock;
        };
        return isRepeatTarget(trueTarget) && isRepeatTarget(falseTarget);
    }
    const bool trueInBody = contains(loop->blocks, trueTarget);
    const bool falseInBody = contains(loop->blocks, falseTarget);
    if (trueInBody != falseInBody) {
        return true;
    }
    const auto isControlTarget = [&](std::uint32_t target) {
        return target == loop->mergeBlock || target == loop->continueBlock;
    };
    return (isControlTarget(trueTarget) && (isControlTarget(falseTarget) || isInsideLoopConstruct(graph, *loop, falseTarget))) || (isControlTarget(falseTarget) && isInsideLoopConstruct(graph, *loop, trueTarget));
}

std::uint32_t exitTailSelectionMerge(const ControlFlowGraph& graph, const BasicBlock& block) {
    if (block.terminator.kind != TerminatorKind::ConditionalBranch) {
        return InvalidControlFlowId;
    }
    const auto* loop = findInnermostContainingLoop(graph, block.id);
    if (loop == nullptr || loop->mergeBlock == InvalidControlFlowId || loop->continueBlock == InvalidControlFlowId || block.id == loop->continueBlock) {
        return InvalidControlFlowId;
    }
    const auto trueTarget = block.terminator.trueBlock;
    const auto falseTarget = block.terminator.falseBlock;
    const bool trueInBody = contains(loop->blocks, trueTarget);
    if (trueInBody == contains(loop->blocks, falseTarget)) {
        return InvalidControlFlowId;
    }
    const auto body = trueInBody ? trueTarget : falseTarget;
    const auto tail = trueInBody ? falseTarget : trueTarget;
    if (tail == loop->mergeBlock || tail == loop->continueBlock || body == loop->continueBlock || !isInsideLoopConstruct(graph, *loop, tail) || !graph.Dominates(block.id, body) || !graph.Dominates(block.id, tail)) {
        return InvalidControlFlowId;
    }
    std::vector<std::uint32_t> pending{tail};
    std::vector<std::uint32_t> seen;
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (contains(seen, blockId)) {
            continue;
        }
        seen.push_back(blockId);
        for (const auto successor : graph.FindBlock(blockId).successors) {
            if (successor == loop->mergeBlock) {
                continue;
            }
            if (contains(loop->blocks, successor) || !graph.Dominates(tail, successor) || !isInsideLoopConstruct(graph, *loop, successor)) {
                return InvalidControlFlowId;
            }
            pending.push_back(successor);
        }
    }
    return body;
}

bool mergeLeavesContainingLoop(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t merge) {
    for (const auto& loop : graph.naturalLoops) {
        if (loop.headerBlock != header && isInsideLoopConstruct(graph, loop, header) && !isInsideLoopConstruct(graph, loop, merge)) {
            return true;
        }
    }
    return false;
}

bool splitSharedMergeBlock(ControlFlowGraph& graph, std::uint32_t merge, const std::vector<std::uint32_t>& constructBlocks, bool forceSplit = false) {
    if (merge == InvalidControlFlowId || merge >= graph.blocks.size() || contains(constructBlocks, merge)) {
        return false;
    }

    const auto& mergeBlock = graph.FindBlock(merge);
    std::vector<std::uint32_t> constructPredecessors;
    bool hasExternalPredecessor = false;
    for (const auto predecessor : mergeBlock.predecessors) {
        if (contains(constructBlocks, predecessor)) {
            addUnique(constructPredecessors, predecessor);
        } else {
            hasExternalPredecessor = true;
        }
    }

    if (constructPredecessors.empty() || (!forceSplit && !hasExternalPredecessor)) {
        return false;
    }

    const auto syntheticMerge = appendSyntheticBranchBlock(graph, merge);
    auto& syntheticBlock = graph.FindBlock(syntheticMerge);
    syntheticBlock.predecessors = constructPredecessors;
    sortUnique(syntheticBlock.predecessors);

    for (const auto predecessor : constructPredecessors) {
        auto& block = graph.FindBlock(predecessor);
        replaceValue(block.successors, merge, syntheticMerge);
        replaceTerminatorTarget(block.terminator, merge, syntheticMerge);
    }

    auto& oldMerge = graph.FindBlock(merge);
    for (const auto predecessor : constructPredecessors) {
        removeValue(oldMerge.predecessors, predecessor);
    }
    addUnique(oldMerge.predecessors, syntheticMerge);
    sortUnique(oldMerge.predecessors);

    moveBlockBefore(graph, syntheticMerge, merge);
    return true;
}

bool splitOneReturnJoin(ControlFlowGraph& graph) {
    for (const auto& block : graph.blocks) {
        if (block.terminator.kind != TerminatorKind::ConditionalBranch || findInnermostContainingLoop(graph, block.id) != nullptr || findSelectionMerge(graph, block) != InvalidControlFlowId) {
            continue;
        }
        const auto returns = [&](std::uint32_t target) { return graph.Dominates(block.id, target) && hasLinearPathToTerminal(graph, target); };
        const auto joinsEnclosing = [&](std::uint32_t target) {
            const auto& predecessors = graph.FindBlock(target).predecessors;
            return !graph.Dominates(block.id, target) && std::all_of(predecessors.begin(), predecessors.end(), [&](std::uint32_t predecessor) {
                return graph.Dominates(block.id, predecessor) || graph.Dominates(predecessor, block.id);
            });
        };
        const auto trueTarget = block.terminator.trueBlock;
        const auto falseTarget = block.terminator.falseBlock;
        const auto join = returns(trueTarget) && joinsEnclosing(falseTarget) ? falseTarget : returns(falseTarget) && joinsEnclosing(trueTarget) ? trueTarget : InvalidControlFlowId;
        if (join != InvalidControlFlowId && splitSharedMergeBlock(graph, join, dominatedBlocks(graph, block.id, join))) {
            return true;
        }
    }
    return false;
}

bool splitOneLoopMerge(ControlFlowGraph& graph) {
    for (const auto& loop : graph.naturalLoops) {
        const auto constructBlocks = dominatedBlocks(graph, loop.headerBlock, loop.mergeBlock);
        const auto forceSplit = mergeLeavesContainingLoop(graph, loop.headerBlock, loop.mergeBlock);
        if (splitSharedMergeBlock(graph, loop.mergeBlock, constructBlocks, forceSplit)) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint32_t> selectionRegion(const ControlFlowGraph& graph, const BasicBlock& header, std::uint32_t merge) {
    std::vector<std::uint32_t> region;
    std::vector<std::uint32_t> pending = {header.terminator.trueBlock, header.terminator.falseBlock};
    const auto* loop = findInnermostContainingLoop(graph, header.id);
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (blockId == merge || contains(region, blockId) || (loop != nullptr && (blockId == loop->mergeBlock || blockId == loop->continueBlock))) {
            continue;
        }
        const auto& block = graph.FindBlock(blockId);
        addUnique(region, blockId);
        pending.insert(pending.end(), block.successors.begin(), block.successors.end());
    }
    sortUnique(region);
    return region;
}

constexpr std::uint32_t CloneWordLimit = 1024u;
constexpr std::uint32_t CloneWordFloor = 256u;
constexpr std::uint32_t CloneShareDivisor = 8u;
constexpr std::uint32_t CloneBudgetDivisor = 4u;

struct SplitBudget {
    std::uint32_t programWords = 0;
    std::uint32_t remainingCloneWords = 0;
    std::uint32_t remainingFallbackWords = 0;
    std::uint32_t nextGotoVariable = 0;
};

void redirectEdge(BasicBlock& block, std::uint32_t oldTarget, std::uint32_t newTarget) {
    replaceValue(block.successors, oldTarget, newTarget);
    replaceTerminatorTarget(block.terminator, oldTarget, newTarget);
    for (auto& target : block.terminator.indirectTargets) {
        if (target == oldTarget) {
            target = newTarget;
        }
    }
    for (auto& target : block.terminator.indirectSelectorTargets) {
        if (target == oldTarget) {
            target = newTarget;
        }
    }
}

std::vector<std::uint32_t> reversePostOrder(const ControlFlowGraph& graph) {
    std::vector<std::uint32_t> postOrder;
    std::vector<bool> visited(graph.blocks.size(), false);
    std::vector<std::pair<std::uint32_t, std::size_t>> stack = {{graph.entryBlock, 0u}};
    visited[graph.entryBlock] = true;
    while (!stack.empty()) {
        auto& [blockId, next] = stack.back();
        const auto& successors = graph.FindBlock(blockId).successors;
        if (next < successors.size()) {
            const auto successor = successors[next++];
            if (!visited[successor]) {
                visited[successor] = true;
                stack.emplace_back(successor, 0u);
            }
            continue;
        }
        postOrder.push_back(blockId);
        stack.pop_back();
    }
    return {postOrder.rbegin(), postOrder.rend()};
}

using Edge = std::pair<std::uint32_t, std::uint32_t>;

std::vector<Edge> externalEntryEdges(const ControlFlowGraph& graph, std::uint32_t header, const std::vector<std::uint32_t>& region) {
    std::vector<Edge> edges;
    for (const auto member : region) {
        for (const auto predecessor : graph.FindBlock(member).predecessors) {
            if (predecessor == header || contains(region, predecessor)) {
                continue;
            }
            if (graph.Dominates(member, predecessor)) {
                throw std::runtime_error("selection header block " + std::to_string(header) + " region block " + std::to_string(member) + " is entered by a back edge from block " + std::to_string(predecessor));
            }
            edges.emplace_back(predecessor, member);
        }
    }
    return edges;
}

std::vector<std::uint32_t> reachableWithin(const ControlFlowGraph& graph, const std::vector<Edge>& entryEdges, const std::vector<std::uint32_t>& region) {
    std::vector<std::uint32_t> reached;
    std::vector<std::uint32_t> pending;
    for (const auto& edge : entryEdges) {
        pending.push_back(edge.second);
    }
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (contains(reached, blockId)) {
            continue;
        }
        reached.push_back(blockId);
        for (const auto successor : graph.FindBlock(blockId).successors) {
            if (contains(region, successor)) {
                pending.push_back(successor);
            }
        }
    }
    sortUnique(reached);
    return reached;
}

std::uint32_t estimatedSpirvWords(const ControlFlowGraph& graph, const std::vector<std::uint32_t>& blocks) {
    std::uint32_t words = 0;
    for (const auto blockId : blocks) {
        words += graph.FindBlock(blockId).estimatedSpirvWords;
    }
    return words;
}

std::string cloneCostRefusal(const SplitBudget& budget, std::uint32_t words) {
    const auto shareLimit = std::max(CloneWordFloor, budget.programWords / CloneShareDivisor);
    const auto prefix = "cloning an estimated " + std::to_string(words) + " SPIR-V words exceeds ";
    if (words > CloneWordLimit) {
        return prefix + "the clone limit of " + std::to_string(CloneWordLimit);
    }
    if (words > shareLimit) {
        return prefix + std::to_string(shareLimit) + ", the share of the program's " + std::to_string(budget.programWords) + " a clone may take";
    }
    if (words > budget.remainingCloneWords) {
        return prefix + "the remaining clone budget of " + std::to_string(budget.remainingCloneWords);
    }
    return {};
}

std::string cloneRefusal(const ControlFlowGraph& graph, std::uint32_t header, const std::vector<std::uint32_t>& blocks) {
    for (const auto blockId : blocks) {
        const auto& block = graph.FindBlock(blockId);
        if (graph.Dominates(blockId, header)) {
            return "the region wraps around to dominating block " + std::to_string(blockId);
        }
        if (block.terminator.kind == TerminatorKind::IndirectBranch || block.terminator.kind == TerminatorKind::Unsupported) {
            return "block " + std::to_string(blockId) + " ends in an indirect or unsupported branch";
        }
    }
    for (const auto& loop : graph.naturalLoops) {
        const bool headerCloned = contains(blocks, loop.headerBlock);
        for (const auto member : loop.blocks) {
            if (headerCloned && !contains(blocks, member)) {
                return "loop " + std::to_string(loop.headerBlock) + " would be cloned without its body block " + std::to_string(member);
            }
        }
        if (!headerCloned && contains(blocks, loop.continueBlock)) {
            return "the continue block " + std::to_string(loop.continueBlock) + " of loop " + std::to_string(loop.headerBlock) + " would be cloned without its header";
        }
    }
    return {};
}

void cloneBlocks(ControlFlowGraph& graph, const std::vector<std::uint32_t>& blocks, const std::vector<Edge>& entryEdges) {
    std::map<std::uint32_t, std::uint32_t> cloneOf;
    for (const auto blockId : blocks) {
        cloneOf.emplace(blockId, static_cast<std::uint32_t>(graph.blocks.size() + cloneOf.size()));
    }
    std::vector<BasicBlock> clones;
    clones.reserve(blocks.size());
    for (const auto blockId : blocks) {
        BasicBlock copy = graph.FindBlock(blockId);
        copy.id = cloneOf.at(blockId);
        copy.predecessors.clear();
        copy.dominators.clear();
        copy.postDominators.clear();
        for (const auto& [original, clone] : cloneOf) {
            redirectEdge(copy, original, clone);
        }
        clones.push_back(std::move(copy));
    }
    for (auto& clone : clones) {
        graph.blocks.push_back(std::move(clone));
    }
    for (const auto& [predecessor, entry] : entryEdges) {
        redirectEdge(graph.FindBlock(predecessor), entry, cloneOf.at(entry));
    }
    rebuildPredecessors(graph);

    const auto firstClone = static_cast<std::uint32_t>(graph.blocks.size() - blocks.size());
    std::vector<BasicBlock> ordered;
    ordered.reserve(graph.blocks.size());
    for (std::uint32_t blockId = 0; blockId < firstClone; ++blockId) {
        ordered.push_back(graph.blocks[blockId]);
    }
    for (const auto blockId : reversePostOrder(graph)) {
        if (blockId >= firstClone) {
            ordered.push_back(graph.blocks[blockId]);
        }
    }
    if (ordered.size() != graph.blocks.size()) {
        throw std::runtime_error("a cloned control flow block is unreachable");
    }
    applyBlockOrder(graph, std::move(ordered));
}

bool privatizeOneSharedReturn(ControlFlowGraph& graph) {
    for (const auto& block : graph.blocks) {
        if (!block.successors.empty() || block.terminator.kind != TerminatorKind::Return || block.predecessors.size() < 2u || block.estimatedSpirvWords > CloneWordFloor) {
            continue;
        }
        for (const auto predecessor : block.predecessors) {
            const auto& from = graph.FindBlock(predecessor);
            if (from.terminator.kind != TerminatorKind::ConditionalBranch || findInnermostContainingLoop(graph, predecessor) != nullptr) {
                continue;
            }
            const auto shared = block.id;
            cloneBlocks(graph, {shared}, {{predecessor, shared}});
            return true;
        }
    }
    return false;
}

std::optional<ControlFlowGraph> privatizeMergeTail(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t merge, std::uint32_t& cost, const std::function<void(ControlFlowGraph&)>& recompute) {
    const auto& headerBlock = graph.FindBlock(header);
    if ((headerBlock.terminator.trueBlock != merge && headerBlock.terminator.falseBlock != merge) || graph.FindBlock(merge).predecessors.size() < 2u || !hasLinearPathToTerminal(graph, merge)) {
        return std::nullopt;
    }
    const auto tail = linearTail(graph, merge);
    if (!cloneRefusal(graph, header, tail).empty()) {
        return std::nullopt;
    }
    auto candidate = graph;
    cloneBlocks(candidate, tail, {{header, merge}});
    recompute(candidate);
    if (candidate.irreducible || findSelectionMerge(candidate, candidate.FindBlock(header)) == InvalidControlFlowId) {
        return std::nullopt;
    }
    cost = estimatedSpirvWords(graph, tail);
    return candidate;
}

std::vector<Edge> loopEdges(const ControlFlowGraph& graph) {
    std::vector<Edge> edges;
    for (const auto& loop : graph.naturalLoops) {
        edges.emplace_back(loop.headerBlock, loop.latchBlock);
    }
    std::sort(edges.begin(), edges.end());
    return edges;
}

std::string junctionRefusal(const ControlFlowGraph& graph, std::uint32_t header, const std::vector<std::uint32_t>& entered, const std::vector<Edge>& routed) {
    for (const auto blockId : entered) {
        if (graph.Dominates(blockId, header)) {
            return "the region wraps around to dominating block " + std::to_string(blockId);
        }
    }
    for (const auto& [source, target] : routed) {
        const auto kind = graph.FindBlock(source).terminator.kind;
        if (kind != TerminatorKind::Branch && kind != TerminatorKind::ConditionalBranch) {
            return "block " + std::to_string(source) + " ends in an indirect or unsupported branch";
        }
        if (graph.Dominates(target, source)) {
            return "the edge from block " + std::to_string(source) + " to block " + std::to_string(target) + " is a back edge";
        }
    }
    return {};
}

std::uint32_t appendRouteBlock(ControlFlowGraph& graph, std::uint32_t positionBlock) {
    const auto& position = graph.FindBlock(positionBlock);
    BasicBlock block;
    block.id = static_cast<std::uint32_t>(graph.blocks.size());
    block.startProgramCounter = position.startProgramCounter;
    block.endProgramCounter = position.startProgramCounter;
    block.instructionBegin = position.instructionBegin;
    block.instructionEnd = position.instructionBegin;
    graph.blocks.push_back(std::move(block));
    return graph.blocks.back().id;
}

void setBranch(BasicBlock& block, std::uint32_t target) {
    block.successors = {target};
    block.terminator.kind = TerminatorKind::Branch;
    block.terminator.condition = BranchCondition::Always;
    block.terminator.trueBlock = target;
}

std::uint32_t appendAssignmentBlock(ControlFlowGraph& graph, std::uint32_t positionBlock, std::uint32_t target, std::uint32_t variable, std::int32_t value) {
    const auto assignment = appendRouteBlock(graph, positionBlock);
    auto& block = graph.FindBlock(assignment);
    setBranch(block, target);
    block.terminator.gotoVariable = variable;
    block.terminator.gotoValue = value;
    return assignment;
}

std::uint32_t appendJunction(ControlFlowGraph& graph, const std::vector<std::uint32_t>& targets, std::uint32_t firstVariable) {
    auto next = targets.back();
    for (auto index = static_cast<std::uint32_t>(targets.size() - 1u); index-- > 0u;) {
        const auto dispatch = appendRouteBlock(graph, targets[index]);
        auto& block = graph.FindBlock(dispatch);
        block.successors = {targets[index], next};
        block.terminator.kind = TerminatorKind::ConditionalBranch;
        block.terminator.condition = BranchCondition::GotoVariable;
        block.terminator.trueBlock = targets[index];
        block.terminator.falseBlock = next;
        block.terminator.gotoVariable = firstVariable + index;
        next = dispatch;
    }
    const auto junction = appendRouteBlock(graph, targets.front());
    setBranch(graph.FindBlock(junction), next);
    return junction;
}

std::optional<ControlFlowGraph> routeThroughJunction(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t merge, const std::vector<std::uint32_t>& region, const std::vector<std::uint32_t>& entered, const std::vector<Edge>& entryEdges, SplitBudget& budget, const std::function<void(ControlFlowGraph&)>& recompute, std::string& refusal) {
    std::vector<Edge> routed = entryEdges;
    std::vector<std::uint32_t> sources = {header};
    for (const auto member : region) {
        if (!contains(entered, member)) {
            sources.push_back(member);
        }
    }
    for (const auto source : sources) {
        for (const auto successor : graph.FindBlock(source).successors) {
            if (successor == merge || contains(entered, successor)) {
                routed.emplace_back(source, successor);
            }
        }
    }
    refusal = junctionRefusal(graph, header, entered, routed);
    if (!refusal.empty()) {
        return std::nullopt;
    }

    const auto order = reversePostOrder(graph);
    const auto orderOf = [&](std::uint32_t blockId) {
        return std::distance(order.begin(), std::find(order.begin(), order.end(), blockId));
    };
    std::vector<std::uint32_t> targets;
    bool reachesMerge = false;
    for (const auto& edge : routed) {
        if (edge.second == merge) {
            reachesMerge = true;
        } else {
            addUnique(targets, edge.second);
        }
    }
    std::sort(targets.begin(), targets.end(), [&](std::uint32_t lhs, std::uint32_t rhs) { return orderOf(lhs) < orderOf(rhs); });
    if (reachesMerge) {
        targets.push_back(merge);
    }

    auto candidate = graph;
    const auto variableCount = static_cast<std::uint32_t>(targets.size() - 1u);
    const auto firstVariable = budget.nextGotoVariable;
    const auto junction = appendJunction(candidate, targets, firstVariable);
    for (const auto& [source, target] : routed) {
        const auto targetIndex = static_cast<std::uint32_t>(std::distance(targets.begin(), std::find(targets.begin(), targets.end(), target)));
        auto entry = junction;
        for (auto index = std::min(targetIndex + 1u, variableCount); index-- > 0u;) {
            entry = appendAssignmentBlock(candidate, target, entry, firstVariable + index, index == targetIndex ? 1 : 0);
        }
        redirectEdge(candidate.FindBlock(source), target, entry);
    }
    rebuildPredecessors(candidate);
    recompute(candidate);
    if (candidate.irreducible || candidate.unsupported) {
        refusal = "routing through a junction would make the graph irreducible";
        return std::nullopt;
    }
    if (loopEdges(candidate) != loopEdges(graph)) {
        refusal = "routing through a junction would change the natural loops";
        return std::nullopt;
    }
    budget.nextGotoVariable += variableCount;
    return candidate;
}

std::uint32_t nextGotoVariable(const ControlFlowGraph& graph) {
    std::uint32_t next = 0;
    for (const auto& block : graph.blocks) {
        if (block.terminator.gotoVariable != InvalidControlFlowId) {
            next = std::max(next, block.terminator.gotoVariable + 1u);
        }
    }
    return next;
}

std::optional<ControlFlowGraph> routeLoopExits(const ControlFlowGraph& graph, const NaturalLoop& loop, const std::function<void(ControlFlowGraph&)>& recompute, std::string& refusal) {
    std::vector<Edge> exits;
    for (const auto member : loop.blocks) {
        const auto& block = graph.FindBlock(member);
        for (const auto successor : block.successors) {
            if (contains(loop.blocks, successor)) {
                continue;
            }
            if (block.terminator.kind != TerminatorKind::Branch && block.terminator.kind != TerminatorKind::ConditionalBranch) {
                refusal = "block " + std::to_string(member) + " ends in an indirect or unsupported branch";
                return std::nullopt;
            }
            exits.emplace_back(member, successor);
        }
    }

    const auto order = reversePostOrder(graph);
    const auto orderOf = [&](std::uint32_t blockId) {
        return std::distance(order.begin(), std::find(order.begin(), order.end(), blockId));
    };
    std::vector<std::uint32_t> targets;
    for (const auto& edge : exits) {
        addUnique(targets, edge.second);
    }
    std::sort(targets.begin(), targets.end(), [&](std::uint32_t lhs, std::uint32_t rhs) { return orderOf(lhs) < orderOf(rhs); });
    const auto latchExit = std::find_if(exits.begin(), exits.end(), [&](const Edge& edge) { return edge.first == loop.latchBlock; });
    const auto fallback = latchExit != exits.end() ? latchExit->second : targets.back();
    targets.erase(std::find(targets.begin(), targets.end(), fallback));
    targets.push_back(fallback);

    auto candidate = graph;
    const auto firstVariable = nextGotoVariable(graph);
    const auto variableCount = static_cast<std::uint32_t>(targets.size() - 1u);
    const auto junction = appendJunction(candidate, targets, firstVariable);

    auto entry = loop.headerBlock;
    for (auto index = variableCount; index-- > 0u;) {
        entry = appendAssignmentBlock(candidate, loop.headerBlock, entry, firstVariable + index, 0);
    }
    for (const auto predecessor : graph.FindBlock(loop.headerBlock).predecessors) {
        if (!contains(loop.blocks, predecessor)) {
            redirectEdge(candidate.FindBlock(predecessor), loop.headerBlock, entry);
        }
    }
    if (candidate.entryBlock == loop.headerBlock) {
        candidate.entryBlock = entry;
    }

    for (const auto& [source, target] : exits) {
        const auto index = static_cast<std::uint32_t>(std::distance(targets.begin(), std::find(targets.begin(), targets.end(), target)));
        const auto exit = index == variableCount ? junction : appendAssignmentBlock(candidate, target, junction, firstVariable + index, 1);
        redirectEdge(candidate.FindBlock(source), target, exit);
    }
    rebuildPredecessors(candidate);
    recompute(candidate);
    if (candidate.irreducible || candidate.unsupported) {
        refusal = "routing its exits through one block would make the graph irreducible";
        return std::nullopt;
    }
    if (loopEdges(candidate) != loopEdges(graph)) {
        refusal = "routing its exits through one block would change the natural loops";
        return std::nullopt;
    }
    const auto routed = std::find_if(candidate.naturalLoops.begin(), candidate.naturalLoops.end(), [&](const NaturalLoop& value) { return value.headerBlock == loop.headerBlock; });
    if (routed == candidate.naturalLoops.end() || routed->mergeBlock != junction) {
        refusal = "routing its exits through one block does not make that block its merge";
        return std::nullopt;
    }
    return candidate;
}

bool structurizeEnteredRegion(ControlFlowGraph& graph, std::uint32_t header, std::uint32_t merge, const std::vector<std::uint32_t>& region, SplitBudget& budget, const std::function<void(ControlFlowGraph&)>& recompute) {
    const auto entryEdges = externalEntryEdges(graph, header, region);
    if (entryEdges.empty()) {
        return false;
    }
    const auto entered = reachableWithin(graph, entryEdges, region);
    const auto enteredWords = estimatedSpirvWords(graph, entered);
    const auto structuralReason = cloneRefusal(graph, header, entered);
    auto cloneReason = structuralReason.empty() ? cloneCostRefusal(budget, enteredWords) : structuralReason;

    std::uint32_t tailWords = 0;
    if (auto privatized = privatizeMergeTail(graph, header, merge, tailWords, recompute); privatized && cloneCostRefusal(budget, tailWords).empty() && (!cloneReason.empty() || tailWords <= enteredWords)) {
        budget.remainingCloneWords -= tailWords;
        budget.remainingFallbackWords -= std::min(tailWords, budget.remainingFallbackWords);
        graph = std::move(*privatized);
        return true;
    }
    if (cloneReason.empty()) {
        budget.remainingCloneWords -= enteredWords;
        budget.remainingFallbackWords -= std::min(enteredWords, budget.remainingFallbackWords);
        cloneBlocks(graph, entered, entryEdges);
        return true;
    }
    std::string junctionReason;
    if (auto routed = routeThroughJunction(graph, header, merge, region, entered, entryEdges, budget, recompute, junctionReason)) {
        graph = std::move(*routed);
        return true;
    }
    if (structuralReason.empty() && enteredWords <= budget.remainingFallbackWords) {
        budget.remainingFallbackWords -= enteredWords;
        cloneBlocks(graph, entered, entryEdges);
        return true;
    }
    if (structuralReason.empty()) {
        cloneReason += " and the fallback budget of " + std::to_string(budget.remainingFallbackWords);
    }
    throw std::runtime_error("selection header block " + std::to_string(header) + " has externally entered region block " + std::to_string(entryEdges.front().second) + " (from block " + std::to_string(entryEdges.front().first) + "): " + cloneReason + "; " + junctionReason);
}

bool splitOneSelectionMerge(ControlFlowGraph& graph, SplitBudget& budget, const std::function<void(ControlFlowGraph&)>& recompute) {
    std::vector<std::uint32_t> loopHeaders;
    loopHeaders.reserve(graph.naturalLoops.size());
    for (const auto& loop : graph.naturalLoops) {
        addUnique(loopHeaders, loop.headerBlock);
    }

    std::vector<std::uint32_t> selectionHeaders;
    for (const auto& block : graph.blocks) {
        if (block.terminator.kind == TerminatorKind::ConditionalBranch && !contains(loopHeaders, block.id)) {
            selectionHeaders.push_back(block.id);
        }
    }
    std::sort(selectionHeaders.begin(), selectionHeaders.end(), [&](std::uint32_t lhs, std::uint32_t rhs) {
        const auto lhsDepth = graph.FindBlock(lhs).dominators.size();
        const auto rhsDepth = graph.FindBlock(rhs).dominators.size();
        return lhsDepth != rhsDepth ? lhsDepth > rhsDepth : lhs < rhs;
    });

    for (const auto blockId : selectionHeaders) {
        const auto& block = graph.FindBlock(blockId);
        if (isInnermostLoopControlConditional(graph, block)) {
            continue;
        }

        const auto merge = findSelectionMerge(graph, block);
        if (merge == InvalidControlFlowId) {
            continue;
        }

        const auto region = selectionRegion(graph, block, merge);
        if (structurizeEnteredRegion(graph, blockId, merge, region, budget, recompute)) {
            return true;
        }

        const auto constructBlocks = dominatedBlocks(graph, blockId, merge);
        const auto forceSplit = mergeLeavesContainingLoop(graph, blockId, merge);
        if (splitSharedMergeBlock(graph, merge, constructBlocks, forceSplit)) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint32_t> naturalLoopBody(const ControlFlowGraph& graph, std::uint32_t headerBlock, std::uint32_t latchBlock, bool& natural) {
    std::vector<std::uint32_t> body;
    std::vector<std::uint32_t> stack;
    natural = true;
    addUnique(body, headerBlock);
    addUnique(body, latchBlock);
    if (latchBlock != headerBlock) {
        stack.push_back(latchBlock);
    }

    while (!stack.empty()) {
        const auto blockId = stack.back();
        stack.pop_back();
        const auto& block = graph.FindBlock(blockId);
        for (const auto predecessor : block.predecessors) {
            if (!graph.Dominates(headerBlock, predecessor)) {
                natural = false;
            }
            if (!contains(body, predecessor)) {
                body.push_back(predecessor);
                if (predecessor != headerBlock) {
                    stack.push_back(predecessor);
                }
            }
        }
    }

    sortUnique(body);
    return body;
}

struct TarjanState {
    const ControlFlowGraph* graph = nullptr;
    std::uint32_t nextIndex = 0;
    std::vector<std::uint32_t> index;
    std::vector<std::uint32_t> lowlink;
    std::vector<bool> onStack;
    std::vector<std::uint32_t> stack;
    std::vector<StronglyConnectedComponent> components;
};

void tarjanVisit(TarjanState& state, std::uint32_t blockId) {
    state.index[blockId] = state.nextIndex;
    state.lowlink[blockId] = state.nextIndex;
    ++state.nextIndex;
    state.stack.push_back(blockId);
    state.onStack[blockId] = true;

    const auto& block = state.graph->FindBlock(blockId);
    for (const auto successor : block.successors) {
        if (state.index[successor] == InvalidControlFlowId) {
            tarjanVisit(state, successor);
            state.lowlink[blockId] = std::min(state.lowlink[blockId], state.lowlink[successor]);
        } else if (state.onStack[successor]) {
            state.lowlink[blockId] = std::min(state.lowlink[blockId], state.index[successor]);
        }
    }

    if (state.lowlink[blockId] != state.index[blockId]) {
        return;
    }

    StronglyConnectedComponent component;
    for (;;) {
        const auto member = state.stack.back();
        state.stack.pop_back();
        state.onStack[member] = false;
        component.blocks.push_back(member);
        if (member == blockId) {
            break;
        }
    }
    sortUnique(component.blocks);

    bool cyclic = component.blocks.size() > 1u;
    for (const auto member : component.blocks) {
        const auto& memberBlock = state.graph->FindBlock(member);
        if (contains(memberBlock.successors, member)) {
            cyclic = true;
        }
        for (const auto predecessor : memberBlock.predecessors) {
            if (!contains(component.blocks, predecessor)) {
                addUnique(component.entryBlocks, member);
            }
        }
    }
    sortUnique(component.entryBlocks);
    component.irreducible = cyclic && component.entryBlocks.size() > 1u;
    state.components.push_back(std::move(component));
}

}

void Structurizer::Structurize(ControlFlowGraph& graph) const {
    const auto original = graph;
    try {
        structurize(graph, false);
    } catch (const std::runtime_error&) {
        graph = original;
        structurize(graph, true);
    }
}

void Structurizer::privatizeSharedReturns(ControlFlowGraph& graph) const {
    const auto budget = std::max<std::size_t>(16u, graph.blocks.size() * 4u);
    for (std::size_t clones = 0; privatizeOneSharedReturn(graph); ++clones) {
        if (clones == budget) {
            throw std::runtime_error("CFG shared return privatization exceeded budget");
        }
        rebuildPredecessors(graph);
        recomputeAnalyses(graph);
    }
}

void Structurizer::structurize(ControlFlowGraph& graph, bool privatizeReturns) const {
    recomputeAnalyses(graph);
    verifyReducibility(graph);
    if (privatizeReturns) {
        privatizeSharedReturns(graph);
    }
    canonicalizeNaturalLoops(graph);
    splitSharedMergeBlocks(graph);
    isolateSemanticLoopHeaders(graph);
    orderByDominance(graph);
    clearStructuredTerminators(graph);

    std::map<std::uint32_t, std::uint32_t> mergeHeaders;
    const auto reserveMergeBlock = [&](std::uint32_t header, std::uint32_t merge) {
        const auto [it, inserted] = mergeHeaders.emplace(merge, header);
        if (!inserted && it->second != header) {
            throw std::runtime_error("duplicate structured merge block " + std::to_string(merge) + " for header " + std::to_string(header) + " (already used by header " + std::to_string(it->second) + ")");
        }
    };

    for (const auto& loop : graph.naturalLoops) {
        auto& header = graph.FindBlock(loop.headerBlock);
        if (loop.mergeBlock == InvalidControlFlowId || loop.continueBlock == InvalidControlFlowId) {
            throw std::runtime_error("loop at block " + std::to_string(loop.headerBlock) + " has no structured merge/continue");
        }
        if (header.instructionBegin != header.instructionEnd || header.terminator.kind != TerminatorKind::Branch) {
            throw std::runtime_error("loop header " + std::to_string(loop.headerBlock) + " is not a dedicated empty control block");
        }
        reserveMergeBlock(loop.headerBlock, loop.mergeBlock);
        header.terminator.loopHeader = true;
        header.terminator.mergeBlock = loop.mergeBlock;
        header.terminator.continueBlock = loop.continueBlock;
    }

    std::vector<std::pair<std::uint32_t, std::uint32_t>> exitTails;
    for (auto& block : graph.blocks) {
        if (block.terminator.kind != TerminatorKind::ConditionalBranch || block.terminator.loopHeader) {
            continue;
        }
        if (isInnermostLoopControlConditional(graph, block)) {
            if (const auto merge = exitTailSelectionMerge(graph, block); merge != InvalidControlFlowId) {
                exitTails.emplace_back(block.id, merge);
            }
            continue;
        }

        const auto merge = findSelectionMerge(graph, block);
        if (merge == InvalidControlFlowId) {
            throw std::runtime_error("conditional block " + std::to_string(block.id) + " has no structured merge");
        }

        reserveMergeBlock(block.id, merge);
        block.terminator.mergeBlock = merge;
    }
    for (const auto& [header, merge] : exitTails) {
        if (mergeHeaders.contains(merge)) {
            continue;
        }
        reserveMergeBlock(header, merge);
        graph.FindBlock(header).terminator.mergeBlock = merge;
    }
}

void Structurizer::computeDominatorTree(ControlFlowGraph& graph) const {
    const auto count = static_cast<std::uint32_t>(graph.blocks.size());
    const auto all = allBlockIds(count);

    for (auto& block : graph.blocks) {
        block.dominators = block.id == graph.entryBlock ? std::vector<std::uint32_t>{block.id} : all;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& block : graph.blocks) {
            if (block.id == graph.entryBlock) {
                continue;
            }

            std::vector<std::uint32_t> next;
            if (block.predecessors.empty()) {
                next = {block.id};
            } else {
                next = graph.blocks[block.predecessors.front()].dominators;
                for (std::size_t i = 1; i < block.predecessors.size(); ++i) {
                    intersectAnalysisSet(next, graph.blocks[block.predecessors[i]].dominators);
                }
                addSortedUnique(next, block.id);
            }

            if (next != block.dominators) {
                replaceAnalysisSet(block.dominators, std::move(next));
                changed = true;
            }
        }
    }
}

void Structurizer::detectNaturalLoops(ControlFlowGraph& graph) const {
    graph.naturalLoops.clear();
    for (auto& edge : graph.backEdges) {
        bool natural = true;
        auto body = naturalLoopBody(graph, edge.targetBlock, edge.sourceBlock, natural);
        edge.natural = natural;

        NaturalLoop loop;
        loop.headerBlock = edge.targetBlock;
        loop.latchBlock = edge.sourceBlock;
        loop.continueBlock = edge.sourceBlock;
        loop.blocks = std::move(body);

        for (const auto blockId : loop.blocks) {
            const auto& block = graph.FindBlock(blockId);
            for (const auto successor : block.successors) {
                if (!contains(loop.blocks, successor)) {
                    addUnique(loop.exitBlocks, successor);
                }
            }
        }
        sortUnique(loop.exitBlocks);

        if (!loop.exitBlocks.empty()) {
            std::uint32_t merge = loop.exitBlocks.front();
            for (std::size_t i = 1; i < loop.exitBlocks.size() && merge != InvalidControlFlowId; ++i) {
                merge = graph.FindNearestCommonPostDominator(merge, loop.exitBlocks[i]);
            }
            loop.mergeBlock = merge;
        }

        graph.naturalLoops.push_back(std::move(loop));
    }
}

void Structurizer::computePostDominators(ControlFlowGraph& graph) const {
    const auto count = static_cast<std::uint32_t>(graph.blocks.size());
    const auto all = allBlockIds(count);

    for (auto& block : graph.blocks) {
        block.postDominators = block.successors.empty() ? std::vector<std::uint32_t>{block.id} : all;
    }

    std::vector<std::uint32_t> order;
    order.reserve(count);
    std::vector<bool> ordered(count, false);
    if (graph.entryBlock < count) {
        const auto forward = reversePostOrder(graph);
        for (auto it = forward.rbegin(); it != forward.rend(); ++it) {
            order.push_back(*it);
            ordered[*it] = true;
        }
    }
    for (std::uint32_t id = 0; id < count; ++id) {
        if (!ordered[id]) order.push_back(id);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto id : order) {
            auto& block = graph.blocks[id];
            std::vector<std::uint32_t> next;
            if (block.successors.empty()) {
                next = {block.id};
            } else {
                next = graph.blocks[block.successors.front()].postDominators;
                for (std::size_t i = 1; i < block.successors.size(); ++i) {
                    intersectAnalysisSet(next, graph.blocks[block.successors[i]].postDominators);
                }
                addSortedUnique(next, block.id);
            }

            if (next != block.postDominators) {
                replaceAnalysisSet(block.postDominators, std::move(next));
                changed = true;
            }
        }
    }
}

void Structurizer::computeBackEdges(ControlFlowGraph& graph) const {
    graph.backEdges.clear();
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) {
            if (graph.Dominates(successor, block.id)) {
                graph.backEdges.push_back({block.id, successor, true});
            }
        }
    }
}

void Structurizer::computeStronglyConnectedComponents(ControlFlowGraph& graph) const {
    TarjanState state;
    state.graph = &graph;
    state.index.assign(graph.blocks.size(), InvalidControlFlowId);
    state.lowlink.assign(graph.blocks.size(), InvalidControlFlowId);
    state.onStack.assign(graph.blocks.size(), false);
    state.stack.reserve(graph.blocks.size());
    state.components.reserve(graph.blocks.size());

    for (const auto& block : graph.blocks) {
        if (state.index[block.id] == InvalidControlFlowId) {
            tarjanVisit(state, block.id);
        }
    }

    graph.components = std::move(state.components);
    graph.irreducible = false;
    for (const auto& component : graph.components) {
        if (component.irreducible) {
            graph.irreducible = true;
            graph.failureKind = FailureKind::IrreducibleControlFlow;
            graph.failureBlock = component.entryBlocks.empty() ? component.blocks.front() : component.entryBlocks.front();
            graph.unsupportedReason = "irreducible CFG: cyclic component has multiple entries";
            break;
        }
    }
}

void Structurizer::recomputeAnalyses(ControlFlowGraph& graph) const {
    computeDominatorTree(graph);
    computePostDominators(graph);
    computeBackEdges(graph);
    detectNaturalLoops(graph);
    computeStronglyConnectedComponents(graph);
}

void Structurizer::canonicalizeNaturalLoops(ControlFlowGraph& graph) const {
    const auto rewriteBudget = graph.blocks.size() * 2u + 16u;
    for (std::size_t rewrite = 0; rewrite < rewriteBudget; ++rewrite) {
        bool changed = false;
        for (const auto& loop : graph.naturalLoops) {
            std::vector<std::uint32_t> latches;
            for (const auto& edge : graph.backEdges) {
                if (edge.targetBlock == loop.headerBlock) {
                    addUnique(latches, edge.sourceBlock);
                }
            }
            if (latches.size() <= 1u) {
                continue;
            }

            const auto continueBlock = appendSyntheticBranchBlock(graph, loop.headerBlock);
            for (const auto latch : latches) {
                auto& block = graph.FindBlock(latch);
                replaceValue(block.successors, loop.headerBlock, continueBlock);
                replaceTerminatorTarget(block.terminator, loop.headerBlock, continueBlock);
            }
            rebuildPredecessors(graph);
            recomputeAnalyses(graph);
            changed = true;
            break;
        }
        if (changed) {
            continue;
        }

        for (const auto& loop : graph.naturalLoops) {
            const auto& header = graph.FindBlock(loop.headerBlock);
            const auto isLoopControlTarget = [&](std::uint32_t target) {
                return target == loop.mergeBlock || target == loop.continueBlock;
            };
            if (header.terminator.kind != TerminatorKind::ConditionalBranch || isLoopControlTarget(header.terminator.trueBlock) || isLoopControlTarget(header.terminator.falseBlock) || !contains(loop.blocks, header.terminator.trueBlock) || !contains(loop.blocks, header.terminator.falseBlock)) {
                continue;
            }

            if (!isolateSemanticLoopHeader(graph, loop.headerBlock, [this](ControlFlowGraph& innerGraph) { recomputeAnalyses(innerGraph); })) {
                throw std::runtime_error("failed to isolate semantic loop header " + std::to_string(loop.headerBlock));
            }
            changed = true;
            break;
        }
        if (changed) {
            continue;
        }

        const NaturalLoop* unmerged = nullptr;
        for (const auto& loop : graph.naturalLoops) {
            if (loop.exitBlocks.size() > 1u && loop.mergeBlock == InvalidControlFlowId && (unmerged == nullptr || loop.blocks.size() < unmerged->blocks.size())) {
                unmerged = &loop;
            }
        }
        if (unmerged == nullptr) {
            return;
        }
        std::string refusal;
        auto routed = routeLoopExits(graph, *unmerged, [this](ControlFlowGraph& candidate) { recomputeAnalyses(candidate); }, refusal);
        if (!routed) {
            throw std::runtime_error("loop at block " + std::to_string(unmerged->headerBlock) + " has exits with no common post-dominator: " + refusal);
        }
        graph = std::move(*routed);
    }

    throw std::runtime_error("CFG loop canonicalization exceeded rewrite budget");
}

void Structurizer::splitSharedMergeBlocks(ControlFlowGraph& graph) const {
    const auto originalBlockCount = static_cast<std::uint32_t>(graph.blocks.size());
    const auto splitBudget = std::max<std::uint32_t>(16u, originalBlockCount * 4u);
    const auto programWords = estimatedSpirvWords(graph, allBlockIds(originalBlockCount));
    SplitBudget budget{programWords, std::max(CloneWordLimit, programWords / CloneBudgetDivisor), std::max(CloneWordLimit, programWords), nextGotoVariable(graph)};
    for (std::uint32_t splits = 0; splits < splitBudget; ++splits) {
        if (!splitOneLoopMerge(graph) && !splitOneSelectionMerge(graph, budget, [this](ControlFlowGraph& candidate) { recomputeAnalyses(candidate); }) && !splitOneReturnJoin(graph)) {
            return;
        }
        rebuildPredecessors(graph);
        recomputeAnalyses(graph);
        verifyReducibility(graph);
    }

    throw std::runtime_error("CFG shared merge splitting exceeded budget: originalBlocks=" + std::to_string(originalBlockCount) + " currentBlocks=" + std::to_string(graph.blocks.size()) + " splitBudget=" + std::to_string(splitBudget));
}

void Structurizer::isolateSemanticLoopHeaders(ControlFlowGraph& graph) const {
    const auto isolationBudget = graph.naturalLoops.size() + 1u;
    for (std::size_t isolation = 0; isolation < isolationBudget; ++isolation) {
        const auto loop = std::find_if(graph.naturalLoops.begin(), graph.naturalLoops.end(), [&](const NaturalLoop& value) {
            const auto& header = graph.FindBlock(value.headerBlock);
            return header.instructionBegin != header.instructionEnd;
        });
        if (loop == graph.naturalLoops.end()) {
            return;
        }

        if (!isolateSemanticLoopHeader(graph, loop->headerBlock, [this](ControlFlowGraph& innerGraph) { recomputeAnalyses(innerGraph); })) {
            throw std::runtime_error("failed to isolate semantic loop header " + std::to_string(loop->headerBlock));
        }
    }

    throw std::runtime_error("CFG semantic loop-header isolation exceeded rewrite budget");
}

void Structurizer::orderByDominance(ControlFlowGraph& graph) const {
    const bool ordered = std::all_of(graph.blocks.begin(), graph.blocks.end(), [](const BasicBlock& block) {
        return std::all_of(block.dominators.begin(), block.dominators.end(), [&](std::uint32_t dominator) { return dominator <= block.id; });
    });
    if (ordered) {
        return;
    }
    const auto order = reversePostOrder(graph);
    if (order.size() != graph.blocks.size()) {
        throw std::runtime_error("control flow graph has unreachable blocks");
    }
    std::vector<BasicBlock> blocks;
    blocks.reserve(order.size());
    for (const auto blockId : order) {
        blocks.push_back(graph.blocks[blockId]);
    }
    applyBlockOrder(graph, std::move(blocks));
    recomputeAnalyses(graph);
}

void Structurizer::clearStructuredTerminators(ControlFlowGraph& graph) const {
    for (auto& block : graph.blocks) {
        block.terminator.mergeBlock = InvalidControlFlowId;
        block.terminator.continueBlock = InvalidControlFlowId;
        block.terminator.loopHeader = false;
    }
}

void Structurizer::verifyReducibility(const ControlFlowGraph& graph) const {
    if (graph.unsupported || graph.irreducible) {
        throw std::runtime_error(graph.unsupportedReason.empty() ? std::string("unsupported CFG") : graph.unsupportedReason);
    }
}

}
