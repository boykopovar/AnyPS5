#include "Optimization/GotoPhiRefiner.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::size_t MaxRefinementDepth = 64;
constexpr std::size_t None = static_cast<std::size_t>(-1);

struct Fact {
    const IrValue* condition = nullptr;
    bool value = false;
};

struct Memo {
    IrValue* result = nullptr;
    std::vector<std::size_t> relied;
    bool done = false;
};

IrValue* Reuse(const Memo& memo, std::set<std::size_t>& relied, IrValue* pending) {
    if (!memo.done) return pending;
    relied.insert(memo.relied.begin(), memo.relied.end());
    return memo.result;
}

std::vector<IrBlock*> ReversePostOrder(IrBlock& entry) {
    std::vector<IrBlock*> order;
    std::unordered_map<const IrBlock*, bool> visited;
    std::vector<std::pair<IrBlock*, std::size_t>> stack{{&entry, 0u}};
    visited[&entry] = true;
    while (!stack.empty()) {
        auto& [block, next] = stack.back();
        if (next < block->Successors().size()) {
            IrBlock* successor = block->Successors()[next++];
            if (visited.try_emplace(successor, true).second) stack.emplace_back(successor, 0u);
            continue;
        }
        order.push_back(block);
        stack.pop_back();
    }
    return {order.rbegin(), order.rend()};
}

IrValue* PhiIncoming(const IrValue& phi, const IrBlock* predecessor) {
    for (std::size_t index = 0; index < phi.ArgumentCount(); index++) {
        if (phi.PhiBlock(index) == predecessor) return phi.Argument(index);
    }
    return nullptr;
}

class Refinement {
public:
    explicit Refinement(IrProgram& program) : m_program(program) {}

    std::uint32_t Run() {
        if (m_program.BlockOrder().empty()) return 0;
        m_order = ReversePostOrder(*m_program.BlockOrder().front());
        for (std::size_t position = 0; position < m_order.size(); ++position) m_index[m_order[position]] = position;
        ComputeDominators();
        const auto facts = CollectFacts();
        std::map<std::vector<std::pair<const IrValue*, bool>>, std::size_t> factSets;
        std::uint32_t replaced = 0;
        for (std::size_t position = 0; position < m_order.size(); ++position) {
            if (facts[position].empty()) continue;
            std::vector<std::pair<const IrValue*, bool>> key;
            for (const auto& fact : facts[position]) key.emplace_back(fact.condition, fact.value);
            const auto factSet = factSets.try_emplace(std::move(key), factSets.size()).first->second;
            for (IrValue* inst : m_order[position]->Instructions()) {
                if (inst->IsPhi()) continue;
                for (std::size_t operand = 0; operand < inst->ArgumentCount(); ++operand) {
                    IrValue* argument = inst->Argument(operand);
                    if (argument == nullptr) continue;
                    IrValue* value = argument->Resolve();
                    if (!value->IsPhi() || value->Type() == IrType::Bool) continue;
                    if (IrValue* refined = Refined(value, position, facts[position], factSet); refined != nullptr && refined != value) {
                        inst->ReplaceArgument(operand, refined);
                        ++replaced;
                    }
                }
            }
        }
        return replaced;
    }

private:
    void ComputeDominators() {
        m_dominator.assign(m_order.size(), None);
        m_dominator[0] = 0;
        const auto intersect = [&](std::size_t left, std::size_t right) {
            while (left != right) {
                while (left > right) left = m_dominator[left];
                while (right > left) right = m_dominator[right];
            }
            return left;
        };
        for (bool changed = true; changed;) {
            changed = false;
            for (std::size_t position = 1; position < m_order.size(); ++position) {
                std::size_t candidate = None;
                for (const IrBlock* predecessor : m_order[position]->Predecessors()) {
                    const auto found = m_index.find(predecessor);
                    if (found == m_index.end() || m_dominator[found->second] == None) continue;
                    candidate = candidate == None ? found->second : intersect(candidate, found->second);
                }
                if (candidate != m_dominator[position]) {
                    m_dominator[position] = candidate;
                    changed = true;
                }
            }
        }
        std::vector<std::vector<std::size_t>> children(m_order.size());
        for (std::size_t position = 1; position < m_order.size(); ++position) {
            if (m_dominator[position] != None) children[m_dominator[position]].push_back(position);
        }
        m_enter.assign(m_order.size(), 0);
        m_leave.assign(m_order.size(), 0);
        std::size_t clock = 0;
        std::vector<std::pair<std::size_t, std::size_t>> stack{{0u, 0u}};
        m_enter[0] = clock++;
        while (!stack.empty()) {
            auto& [block, next] = stack.back();
            if (next < children[block].size()) {
                const auto child = children[block][next++];
                m_enter[child] = clock++;
                stack.emplace_back(child, 0u);
                continue;
            }
            m_leave[block] = clock++;
            stack.pop_back();
        }
    }

    bool Dominates(std::size_t dominator, std::size_t block) const {
        return m_enter[dominator] <= m_enter[block] && m_leave[block] <= m_leave[dominator];
    }

    const std::vector<bool>& ReachableAvoiding(std::size_t from, std::size_t barrier) {
        const auto key = std::make_pair(from, barrier);
        if (const auto found = m_reachable.find(key); found != m_reachable.end()) return found->second;
        std::vector<bool> reached(m_order.size(), false);
        std::vector<std::size_t> pending{from};
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            for (const IrBlock* successor : m_order[current]->Successors()) {
                const auto found = m_index.find(successor);
                if (found == m_index.end() || found->second == barrier || found->second == from || reached[found->second]) continue;
                reached[found->second] = true;
                pending.push_back(found->second);
            }
        }
        return m_reachable.emplace(key, std::move(reached)).first->second;
    }

    std::vector<std::vector<Fact>> CollectFacts() {
        std::vector<std::vector<Fact>> facts(m_order.size());
        const auto& programOrder = m_program.BlockOrder();
        const auto& info = m_program.Metadata().blockInfo;
        if (programOrder.size() != info.size()) return facts;
        std::unordered_map<std::uint32_t, IrBlock*> byId;
        for (std::size_t index = 0; index < programOrder.size(); ++index) byId.emplace(info[index].id, programOrder[index]);
        for (std::size_t index = 0; index < programOrder.size(); ++index) {
            const auto& terminator = info[index].terminator;
            if (terminator.kind != TerminatorKind::ConditionalBranch || terminator.loopHeader || info[index].condition == nullptr || terminator.trueBlock == terminator.falseBlock) continue;
            const IrValue* condition = info[index].condition->Resolve();
            if (!condition->IsPhi() || condition->Type() != IrType::Bool) continue;
            for (const auto& [target, value] : {std::pair{terminator.trueBlock, true}, std::pair{terminator.falseBlock, false}}) {
                const auto block = byId.find(target);
                if (block == byId.end() || block->second->Predecessors().size() != 1u || block->second->Predecessors().front() != programOrder[index]) continue;
                const auto edge = m_index.find(block->second);
                if (edge == m_index.end()) continue;
                for (std::size_t position = 0; position < m_order.size(); ++position) {
                    if (Dominates(edge->second, position)) facts[position].push_back(Fact{condition, value});
                }
            }
        }
        return facts;
    }

    IrValue* Refined(IrValue* value, std::size_t use, const std::vector<Fact>& facts, std::size_t factSet) {
        std::set<std::size_t> relied;
        IrValue* refined = RefineValue(value, facts, factSet, relied, 0u);
        if (refined == nullptr || refined == value) return nullptr;
        refined = refined->Resolve();
        if (refined->Type() != value->Type()) return nullptr;
        if (!refined->HasImmediate() && refined->Parent() != nullptr) {
            const auto defined = m_index.find(refined->Parent());
            if (defined == m_index.end() || defined->second == use || !Dominates(defined->second, use)) return nullptr;
            relied.insert(defined->second);
        }
        const auto junction = m_index.find(value->Parent());
        if (junction == m_index.end()) return nullptr;
        for (const auto block : relied) {
            if (block != junction->second && ReachableAvoiding(block, junction->second)[use]) return nullptr;
        }
        return refined;
    }

    IrValue* RefineValue(IrValue* value, const std::vector<Fact>& facts, std::size_t factSet, std::set<std::size_t>& relied, std::size_t depth) {
        value = value->Resolve();
        if (!value->IsPhi() || depth > MaxRefinementDepth) return value;
        const auto [entry, inserted] = m_refined.try_emplace({factSet, value});
        if (!inserted) return Reuse(entry->second, relied, value);
        IrValue* result = value;
        std::set<std::size_t> own;
        if (const auto block = m_index.find(value->Parent()); block != m_index.end()) {
            for (const auto& fact : facts) {
                if (fact.condition == value || fact.condition->Parent() != value->Parent()) continue;
                std::set<std::size_t> attempt;
                if (IrValue* restricted = Restrict(*value, *fact.condition, fact.value, facts, factSet, attempt, depth + 1u)) {
                    attempt.insert(block->second);
                    result = RefineValue(restricted, facts, factSet, attempt, depth + 1u);
                    own = std::move(attempt);
                    break;
                }
            }
        }
        entry->second = Memo{result, {own.begin(), own.end()}, true};
        relied.insert(own.begin(), own.end());
        return result;
    }

    IrValue* Restrict(const IrValue& phi, const IrValue& guardPhi, bool expected, const std::vector<Fact>& facts, std::size_t factSet, std::set<std::size_t>& relied, std::size_t depth) {
        if (depth > MaxRefinementDepth) return nullptr;
        const auto [entry, inserted] = m_restricted.try_emplace({factSet, &phi, &guardPhi, expected});
        if (!inserted) return Reuse(entry->second, relied, nullptr);
        std::set<std::size_t> own;
        IrValue* single = RestrictArms(phi, guardPhi, expected, facts, factSet, own, depth);
        entry->second = Memo{single, single != nullptr ? std::vector<std::size_t>(own.begin(), own.end()) : std::vector<std::size_t>{}, true};
        if (single != nullptr) relied.insert(own.begin(), own.end());
        return single;
    }

    IrValue* RestrictArms(const IrValue& phi, const IrValue& guardPhi, bool expected, const std::vector<Fact>& facts, std::size_t factSet, std::set<std::size_t>& relied, std::size_t depth) {
        IrValue* single = nullptr;
        for (std::size_t index = 0; index < phi.ArgumentCount(); ++index) {
            const IrBlock* predecessor = phi.PhiBlock(index);
            IrValue* guard = PhiIncoming(guardPhi, predecessor);
            if (guard == nullptr) return nullptr;
            guard = guard->Resolve();
            IrValue* arm = phi.Argument(index)->Resolve();
            IrValue* kept = nullptr;
            if (guard->HasImmediate()) {
                if (guard->ImmediateBool() != expected) continue;
                kept = RefineValue(arm, facts, factSet, relied, depth + 1u);
            } else if (guard->IsPhi() && arm->IsPhi() && guard->Parent() == arm->Parent()) {
                kept = Restrict(*arm, *guard, expected, facts, factSet, relied, depth + 1u);
                const auto block = m_index.find(arm->Parent());
                if (kept == nullptr || block == m_index.end()) return nullptr;
                relied.insert(block->second);
            } else {
                kept = RefineValue(arm, facts, factSet, relied, depth + 1u);
            }
            if (kept == nullptr) return nullptr;
            kept = kept->Resolve();
            if (single == nullptr) {
                single = kept;
            } else if (single != kept) {
                return nullptr;
            }
        }
        return single;
    }

    IrProgram& m_program;
    std::vector<IrBlock*> m_order;
    std::unordered_map<const IrBlock*, std::size_t> m_index;
    std::vector<std::size_t> m_dominator;
    std::vector<std::size_t> m_enter;
    std::vector<std::size_t> m_leave;
    std::map<std::pair<std::size_t, std::size_t>, std::vector<bool>> m_reachable;
    std::map<std::pair<std::size_t, const IrValue*>, Memo> m_refined;
    std::map<std::tuple<std::size_t, const IrValue*, const IrValue*, bool>, Memo> m_restricted;
};

}

GotoPhiRefinementStats GotoPhiRefiner::Refine(IrProgram& program) const {
    GotoPhiRefinementStats stats;
    stats.replacedOperands = Refinement(program).Run();
    return stats;
}

}
