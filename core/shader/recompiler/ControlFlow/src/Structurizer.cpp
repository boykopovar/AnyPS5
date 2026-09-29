#include "ControlFlow/Structurizer.hpp"
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
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

std::vector<std::uint32_t> intersectSorted(const std::vector<std::uint32_t>& first, const std::vector<std::uint32_t>& second) {
    std::vector<std::uint32_t> result;
    std::set_intersection(first.begin(), first.end(), second.begin(), second.end(), std::back_inserter(result));
    return result;
}

void sortUnique(std::vector<std::uint32_t>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void addUnique(std::vector<std::uint32_t>& values, std::uint32_t value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

bool contains(const std::vector<std::uint32_t>& values, std::uint32_t value) {
    return std::find(values.begin(), values.end(), value) != values.end();
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
    blocks.reserve(graph.blocks.size());
    stack.reserve(graph.blocks.size());

    while (!stack.empty()) {
        const auto blockId = stack.back();
        stack.pop_back();
        if (blockId == stopBlock || contains(blocks, blockId) || !graph.Dominates(headerBlock, blockId)) {
            continue;
        }

        const auto& block = graph.FindBlock(blockId);
        addUnique(blocks, blockId);
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

// Whether `start` reaches `target` without passing the loop's continue or merge block.
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
    // An arm that reaches the post-dominator only in a later iteration (around the back edge)
    // leaves the selection by continuing; the merge is where the other arm goes. Splitting the
    // post-dominator off would leave it where it is, and the split repeats without end.
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

bool splitOneSelectionMerge(ControlFlowGraph& graph) {
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
        const auto external = std::find_if(region.begin(), region.end(), [&](std::uint32_t member) {
            const auto& memberBlock = graph.FindBlock(member);
            return std::any_of(memberBlock.predecessors.begin(), memberBlock.predecessors.end(), [&](std::uint32_t predecessor) {
                return predecessor != blockId && !contains(region, predecessor);
            });
        });
        if (external != region.end()) {
            throw std::runtime_error("selection header block " + std::to_string(blockId) + " has externally entered region block " + std::to_string(*external) + "; semantic block cloning is disabled");
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
    recomputeAnalyses(graph);
    verifyReducibility(graph);
    canonicalizeNaturalLoops(graph);
    splitSharedMergeBlocks(graph);
    isolateSemanticLoopHeaders(graph);
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

    for (auto& block : graph.blocks) {
        if (block.terminator.kind != TerminatorKind::ConditionalBranch || block.terminator.loopHeader) {
            continue;
        }
        if (isInnermostLoopControlConditional(graph, block)) {
            continue;
        }

        const auto merge = findSelectionMerge(graph, block);
        if (merge == InvalidControlFlowId) {
            throw std::runtime_error("conditional block " + std::to_string(block.id) + " has no structured merge");
        }

        reserveMergeBlock(block.id, merge);
        block.terminator.mergeBlock = merge;
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
                    next = intersectSorted(next, graph.blocks[block.predecessors[i]].dominators);
                }
                addUnique(next, block.id);
                sortUnique(next);
            }

            if (next != block.dominators) {
                block.dominators = std::move(next);
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
            for (std::size_t i = 1; i < loop.exitBlocks.size(); ++i) {
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

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& block : graph.blocks) {
            std::vector<std::uint32_t> next;
            if (block.successors.empty()) {
                next = {block.id};
            } else {
                next = graph.blocks[block.successors.front()].postDominators;
                for (std::size_t i = 1; i < block.successors.size(); ++i) {
                    next = intersectSorted(next, graph.blocks[block.successors[i]].postDominators);
                }
                addUnique(next, block.id);
                sortUnique(next);
            }

            if (next != block.postDominators) {
                block.postDominators = std::move(next);
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
        if (!changed) {
            return;
        }
    }

    throw std::runtime_error("CFG loop canonicalization exceeded rewrite budget");
}

void Structurizer::splitSharedMergeBlocks(ControlFlowGraph& graph) const {
    const auto originalBlockCount = static_cast<std::uint32_t>(graph.blocks.size());
    const auto splitBudget = std::max<std::uint32_t>(16u, originalBlockCount * 4u);
    for (std::uint32_t splits = 0; splits < splitBudget; ++splits) {
        if (!splitOneLoopMerge(graph) && !splitOneSelectionMerge(graph)) {
            return;
        }
        rebuildPredecessors(graph);
        recomputeAnalyses(graph);
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
