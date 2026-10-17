#include "Optimization/ReadLaneEliminator.hpp"
#include <algorithm>
#include <list>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {
namespace {

struct ChainResult {
    IrValue* value = nullptr;
    IrValue* write = nullptr;
    IrValue* scalar = nullptr;
    bool cycle = false;
};

ChainResult searchChain(IrValue* value, std::uint32_t lane, std::uint32_t waveSize);

bool isImmediateU32(const IrValue& value) {
    return value.HasImmediate() && value.Type() == IrType::U32;
}

struct WindowLane {
    IrValue* value = nullptr;
    bool known = false;
};

struct LaneState {
    enum class Kind : std::uint8_t { Unreached, Known, Unknown };
    Kind kind = Kind::Unreached;
    IrValue* value = nullptr;

    bool operator==(const LaneState& other) const {
        return kind == other.kind && (kind != Kind::Known || value->Resolve() == other.value->Resolve());
    }
};

LaneState meetLanes(const LaneState& left, const LaneState& right) {
    if (left.kind == LaneState::Kind::Unreached) return right;
    if (right.kind == LaneState::Kind::Unreached) return left;
    if (left.kind == LaneState::Kind::Unknown || right.kind == LaneState::Kind::Unknown) return {LaneState::Kind::Unknown, nullptr};
    return left.value->Resolve() == right.value->Resolve() ? left : LaneState{LaneState::Kind::Unknown, nullptr};
}

struct LaneFlow {
    std::unordered_map<const IrBlock*, LaneState> exits;
    std::unordered_map<const IrValue*, LaneState> loads;
};

struct WindowFlows {
    const IrProgram* program = nullptr;
    std::unordered_set<std::uint32_t> spillSlots;
    std::unordered_map<std::uint64_t, LaneFlow> flows;
    std::unordered_set<std::uint64_t> computing;
};

thread_local WindowFlows* windowFlows = nullptr;

std::unordered_set<std::uint32_t> collectLaneSpillSlots(const IrProgram& program) {
    std::unordered_set<std::uint32_t> candidates;
    std::unordered_set<std::uint32_t> rejected;
    const auto immediateSlot = [](const IrValue& inst, std::uint32_t& slot) {
        const IrValue* index = inst.Argument(0)->Resolve();
        if (!isImmediateU32(*index)) return false;
        slot = index->ImmediateU32();
        return true;
    };
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            std::uint32_t slot = 0;
            if (inst->Opcode() == IrOpcode::StoreVectorWindow) {
                if (!immediateSlot(*inst, slot)) continue;
                candidates.insert(slot);
                if (inst->Argument(1)->Resolve()->Opcode() != IrOpcode::WriteLane) rejected.insert(slot);
            } else if (inst->Opcode() == IrOpcode::LoadVectorWindow) {
                if (!immediateSlot(*inst, slot)) continue;
                candidates.insert(slot);
                for (const IrUse& use : inst->OperandUses()) {
                    const bool laneRead = use.user->Opcode() == IrOpcode::ReadLane && use.operand == 0u;
                    const bool chained = use.user->Opcode() == IrOpcode::WriteLane && use.operand == 2u;
                    if (!laneRead && !chained) rejected.insert(slot);
                }
            }
        }
    }
    for (const auto slot : rejected) candidates.erase(slot);
    return candidates;
}

LaneState windowLoadLane(const IrValue* load, std::uint32_t lane, std::uint32_t waveSize);

LaneState storedLane(IrValue* value, std::uint32_t slot, std::uint32_t lane, std::uint32_t waveSize, const LaneFlow& flow) {
    for (;;) {
        value = value->Resolve();
        if (value->Opcode() == IrOpcode::WriteLane) {
            IrValue* selector = value->Argument(1)->Resolve();
            if (!isImmediateU32(*selector)) return {LaneState::Kind::Unknown, nullptr};
            if (selector->ImmediateU32() % waveSize == lane) return {LaneState::Kind::Known, value->Argument(0)};
            value = value->Argument(2);
            continue;
        }
        if (value->Opcode() != IrOpcode::LoadVectorWindow) return {LaneState::Kind::Unknown, nullptr};
        const IrValue* index = value->Argument(0)->Resolve();
        if (!isImmediateU32(*index)) return {LaneState::Kind::Unknown, nullptr};
        if (index->ImmediateU32() != slot) return windowLoadLane(value, lane, waveSize);
        const auto found = flow.loads.find(value);
        return found == flow.loads.end() ? LaneState{} : found->second;
    }
}

const LaneFlow* laneFlow(std::uint32_t slot, std::uint32_t lane, std::uint32_t waveSize) {
    auto& flows = *windowFlows;
    const auto key = (static_cast<std::uint64_t>(slot) << 32u) | lane;
    if (const auto found = flows.flows.find(key); found != flows.flows.end()) return &found->second;
    if (!flows.computing.insert(key).second) return nullptr;
    LaneFlow flow;
    const bool spill = flows.spillSlots.contains(slot);
    const auto& blocks = flows.program->Blocks();
    constexpr int MaxPasses = 64;
    bool settled = false;
    for (int pass = 0; pass < MaxPasses && !settled; ++pass) {
        settled = true;
        for (const auto& block : blocks) {
            LaneState running{};
            if (block->Predecessors().empty()) {
                running = {LaneState::Kind::Unknown, nullptr};
            } else {
                for (const IrBlock* predecessor : block->Predecessors()) {
                    const auto exit = flow.exits.find(predecessor);
                    running = meetLanes(running, exit == flow.exits.end() ? LaneState{} : exit->second);
                }
            }
            for (IrValue* inst : block->Instructions()) {
                if (inst->Opcode() == IrOpcode::LoadVectorWindow) {
                    const IrValue* index = inst->Argument(0)->Resolve();
                    if (!isImmediateU32(*index) || index->ImmediateU32() != slot) continue;
                    auto& seen = flow.loads[inst];
                    if (!(seen == running)) {
                        seen = running;
                        settled = false;
                    }
                } else if (inst->Opcode() == IrOpcode::StoreVectorWindow) {
                    const IrValue* index = inst->Argument(0)->Resolve();
                    if (!isImmediateU32(*index)) {
                        if (!spill) running = {LaneState::Kind::Unknown, nullptr};
                        continue;
                    }
                    if (index->ImmediateU32() != slot) continue;
                    running = storedLane(inst->Argument(1), slot, lane, waveSize, flow);
                }
            }
            auto& exit = flow.exits[block.get()];
            if (!(exit == running)) {
                exit = running;
                settled = false;
            }
        }
    }
    flows.computing.erase(key);
    if (!settled) {
        for (auto& [load, state] : flow.loads) state = {LaneState::Kind::Unknown, nullptr};
    }
    return &(flows.flows[key] = std::move(flow));
}

LaneState windowLoadLane(const IrValue* load, std::uint32_t lane, std::uint32_t waveSize) {
    const IrValue* index = load->Argument(0)->Resolve();
    if (windowFlows == nullptr || !isImmediateU32(*index) || load->Parent() == nullptr) return {LaneState::Kind::Unknown, nullptr};
    const LaneFlow* flow = laneFlow(index->ImmediateU32(), lane, waveSize);
    if (flow == nullptr) return {LaneState::Kind::Unknown, nullptr};
    const auto found = flow->loads.find(load);
    return found == flow->loads.end() ? LaneState{LaneState::Kind::Unknown, nullptr} : found->second;
}

WindowLane windowLane(IrValue* load, std::uint32_t lane, std::uint32_t waveSize) {
    const LaneState state = windowLoadLane(load, lane, waveSize);
    if (state.kind == LaneState::Kind::Known) return {state.value, true};
    return {};
}

ChainResult searchChain(IrValue* value, std::uint32_t lane, std::uint32_t waveSize) {
    for (;;) {
        value = value->Resolve();
        if (value->Opcode() == IrOpcode::LoadVectorWindow) {
            const WindowLane found = windowLane(value, lane, waveSize);
            if (found.known && found.value != nullptr) {
                return ChainResult {value, value, found.value};
            }
            return ChainResult {value, nullptr, nullptr, found.known};
        }
        if (value->Opcode() != IrOpcode::WriteLane) {
            return ChainResult {value, nullptr};
        }
        IrValue* selector = value->Argument(1)->Resolve();
        if (!isImmediateU32(*selector)) {
            return ChainResult {value, nullptr};
        }
        if (selector->ImmediateU32() % waveSize == lane) {
            return ChainResult {value, value, value->Argument(0)};
        }
        value = value->Argument(2);
    }
}

bool isPossibleToEliminate(IrValue* source, std::uint32_t lane, std::uint32_t waveSize) {
    std::queue<IrValue*> queue;
    std::unordered_set<IrValue*> visited;
    queue.push(source);

    while (!queue.empty()) {
        const ChainResult chain = searchChain(queue.front(), lane, waveSize);
        queue.pop();
        if (chain.write != nullptr) {
            continue;
        }
        IrValue* inst = chain.value;
        if (!inst->IsPhi() || inst->ArgumentCount() == 0u) {
            return false;
        }
        if (!visited.insert(inst).second) {
            continue;
        }
        for (std::size_t index = inst->ArgumentCount(); index-- > 0;) {
            queue.push(inst->Argument(index));
        }
    }
    return true;
}

using PhiMap = std::unordered_map<IrValue*, IrValue*>;

IrValue* getRealValue(IrProgram& program, PhiMap& phiMap, IrValue* source, std::uint32_t lane, std::uint32_t waveSize) {
    const ChainResult chain = searchChain(source, lane, waveSize);
    if (chain.write != nullptr) {
        return chain.scalar;
    }

    IrValue* inst = chain.value;
    if (!inst->IsPhi()) {
        throw std::runtime_error("ReadLaneEliminator::Eliminate reached a non-phi value while rewriting a lane chain");
    }
    const auto [entry, isNew] = phiMap.try_emplace(inst);
    if (!isNew) {
        return entry->second;
    }

    IrBlock* block = inst->Parent();
    IrValue& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
    block->InsertInstructionBefore(inst, &phi);
    entry->second = &phi;

    std::vector<IrValue*> arguments;
    arguments.reserve(inst->ArgumentCount());
    for (std::size_t index = 0; index < inst->ArgumentCount(); index++) {
        arguments.push_back(getRealValue(program, phiMap, inst->Argument(index), lane, waveSize));
    }
    IrValue* first = arguments.front()->Resolve();
    if (std::ranges::all_of(arguments, [first](IrValue* argument) { return argument->Resolve() == first; })) {
        phi.ReplaceUsesWith(first, true);
    } else {
        for (std::size_t index = 0; index < arguments.size(); index++) {
            phi.AddPhiOperand(inst->PhiBlock(index), arguments[index]);
        }
    }
    return &phi;
}

// Spilled SGPRs written with v_writelane inside divergent control flow reach their v_readlane through
// per-lane exec selects and loop phis. Lane `lane` of such a vector can only hold what some WriteLane
// stored to that lane, so the read is collected as the set of those stored values; operands that
// enter through a phi without any lane write (the spill slot's initial contents) are never read back.
struct LaneValues {
    std::unordered_set<IrValue*> values;
    std::unordered_set<IrValue*> visited;
    bool failed = false;
};

void collectLaneValues(IrValue* value, std::uint32_t lane, std::uint32_t waveSize, bool throughPhi, LaneValues& out) {
    if (out.failed) return;
    value = value->Resolve();
    if (!out.visited.insert(value).second) return;
    if (out.visited.size() > 4096u) {
        out.failed = true;
        return;
    }
    switch (value->Opcode()) {
    case IrOpcode::WriteLane: {
        IrValue* selector = value->Argument(1)->Resolve();
        if (!isImmediateU32(*selector)) {
            out.failed = true;
            return;
        }
        if (selector->ImmediateU32() % waveSize == lane) {
            out.values.insert(value->Argument(0)->Resolve());
            return;
        }
        collectLaneValues(value->Argument(2), lane, waveSize, throughPhi, out);
        return;
    }
    case IrOpcode::SelectU32:
        collectLaneValues(value->Argument(1), lane, waveSize, throughPhi, out);
        collectLaneValues(value->Argument(2), lane, waveSize, throughPhi, out);
        return;
    default:
        if (value->IsPhi()) {
            for (std::size_t index = 0; index < value->ArgumentCount(); index++) collectLaneValues(value->Argument(index), lane, waveSize, true, out);
            return;
        }
        if (!throughPhi) out.failed = true;
        return;
    }
}

// A stored value computed from the read itself (a spilled loop counter) is not a fixed value.
bool dependsOn(IrValue* value, IrValue* target) {
    std::vector<IrValue*> pending{value};
    std::unordered_set<IrValue*> visited;
    while (!pending.empty()) {
        IrValue* current = pending.back()->Resolve();
        pending.pop_back();
        if (current == target) return true;
        if (!visited.insert(current).second || visited.size() > 65536u) continue;
        for (std::size_t index = 0; index < current->ArgumentCount(); index++) pending.push_back(current->Argument(index));
    }
    return false;
}

// A scalar phi whose operands are all `read` itself or members of `candidates` adds nothing new.
IrValue* uniqueLaneValue(IrValue* read, std::unordered_set<IrValue*> candidates) {
    std::unordered_set<IrValue*> expanded;
    for (bool changed = true; changed;) {
        changed = false;
        for (IrValue* candidate : std::vector<IrValue*>(candidates.begin(), candidates.end())) {
            if (!candidate->IsPhi() || !expanded.insert(candidate).second) continue;
            std::vector<IrValue*> operands;
            bool closed = true;
            for (std::size_t index = 0; index < candidate->ArgumentCount(); index++) {
                IrValue* operand = candidate->Argument(index)->Resolve();
                if (operand == read || operand == candidate || expanded.contains(operand)) continue;
                operands.push_back(operand);
            }
            if (operands.empty()) closed = false;
            if (!closed) continue;
            candidates.erase(candidate);
            for (IrValue* operand : operands) candidates.insert(operand);
            changed = true;
        }
    }
    candidates.erase(read);
    return candidates.size() == 1u ? *candidates.begin() : nullptr;
}

}

ReadLaneEliminationStats ReadLaneEliminator::Eliminate(IrProgram& program, std::uint32_t waveSize) const {
    ReadLaneEliminationStats stats;
    if (waveSize != 32u && waveSize != 64u) {
        return stats;
    }
    WindowFlows flows;
    flows.program = &program;
    flows.spillSlots = collectLaneSpillSlots(program);
    struct FlowScope {
        ~FlowScope() { windowFlows = nullptr; }
    } flowScope;
    windowFlows = &flows;

    for (const auto& block : program.Blocks()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::ReadLane) {
                continue;
            }
            IrValue* selector = inst->Argument(1)->Resolve();
            if (!isImmediateU32(*selector)) {
                continue;
            }

            const std::uint32_t lane = selector->ImmediateU32() % waveSize;
            const ChainResult chain = searchChain(inst->Argument(0), lane, waveSize);
            if (chain.write != nullptr) {
                inst->ReplaceUsesWith(chain.scalar, true);
                stats.rewrittenReads++;
                continue;
            }
            if (!chain.value->IsPhi() || !isPossibleToEliminate(chain.value, lane, waveSize)) {
                LaneValues values;
                collectLaneValues(inst->Argument(0), lane, waveSize, false, values);
                if (values.failed || values.values.empty()) continue;
                if (IrValue* unique = uniqueLaneValue(inst, values.values); unique != nullptr && !dependsOn(unique, inst)) {
                    inst->ReplaceUsesWith(unique, true);
                    stats.rewrittenReads++;
                }
                continue;
            }

            PhiMap phiMap;
            inst->ReplaceUsesWith(getRealValue(program, phiMap, chain.value, lane, waveSize), true);
            stats.rewrittenReads++;
        }
    }
    return stats;
}

}
