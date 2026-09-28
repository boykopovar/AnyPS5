#include "Optimization/ReadLaneEliminator.hpp"
#include <algorithm>
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
};

bool isImmediateU32(const IrValue& value) {
    return value.HasImmediate() && value.Type() == IrType::U32;
}

ChainResult searchChain(IrValue* value, std::uint32_t lane, std::uint32_t waveSize) {
    for (;;) {
        value = value->Resolve();
        if (value->Opcode() != IrOpcode::WriteLane) {
            return ChainResult {value, nullptr};
        }
        IrValue* selector = value->Argument(1)->Resolve();
        if (!isImmediateU32(*selector)) {
            return ChainResult {value, nullptr};
        }
        if (selector->ImmediateU32() % waveSize == lane) {
            return ChainResult {value, value};
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
        return chain.write->Argument(0);
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
                inst->ReplaceUsesWith(chain.write->Argument(0), true);
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
