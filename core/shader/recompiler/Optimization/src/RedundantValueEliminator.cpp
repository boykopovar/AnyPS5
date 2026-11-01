#include "Optimization/RedundantValueEliminator.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

bool Recomputable(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::BitCastU16F16:
        case IrOpcode::BitCastF16U16:
        case IrOpcode::BitCastU32F32:
        case IrOpcode::BitCastF32U32:
        case IrOpcode::ConvertU16U32:
        case IrOpcode::ConvertU32U16:
        case IrOpcode::ConvertU8U32:
        case IrOpcode::ConvertU32U8:
        case IrOpcode::ConvertF32F16:
        case IrOpcode::ConvertF16F32:
        case IrOpcode::ConvertS32F32:
        case IrOpcode::ConvertU32F32:
        case IrOpcode::ConvertF32S32:
        case IrOpcode::ConvertF32U32:
        case IrOpcode::CompositeConstructU64:
        case IrOpcode::CompositeConstructU32x2:
        case IrOpcode::CompositeConstructU32x3:
        case IrOpcode::CompositeConstructF32x2:
        case IrOpcode::CompositeConstructU32x4:
        case IrOpcode::CompositeExtractU64:
        case IrOpcode::CompositeExtractU32x2:
        case IrOpcode::CompositeExtractU32x3:
        case IrOpcode::CompositeExtractU32x4:
        case IrOpcode::FPAbs32:
        case IrOpcode::FPNeg32:
        case IrOpcode::FPSaturate32:
        case IrOpcode::BitFieldInsert:
        case IrOpcode::BitFieldUExtract:
        case IrOpcode::BitFieldSExtract:
        case IrOpcode::SelectU1:
        case IrOpcode::SelectF32:
        case IrOpcode::SelectU32:
        case IrOpcode::IAdd32:
        case IrOpcode::ISub32:
        case IrOpcode::IMul32:
        case IrOpcode::IAdd64:
        case IrOpcode::ISub64:
        case IrOpcode::IMul64:
        case IrOpcode::SMulHi:
        case IrOpcode::UMulHi:
        case IrOpcode::IAbs32:
        case IrOpcode::ShiftLeftLogical32:
        case IrOpcode::ShiftLeftLogical64:
        case IrOpcode::ShiftRightLogical32:
        case IrOpcode::ShiftRightLogical64:
        case IrOpcode::ShiftRightArithmetic32:
        case IrOpcode::ShiftRightArithmetic64:
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseAnd64:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
        case IrOpcode::BitwiseNot32:
        case IrOpcode::BitReverse32:
        case IrOpcode::BitCount32:
        case IrOpcode::BitCount64:
        case IrOpcode::FindUMsb32:
        case IrOpcode::FindUMsb64:
        case IrOpcode::FindILsb32:
        case IrOpcode::SMin32:
        case IrOpcode::UMin32:
        case IrOpcode::SMax32:
        case IrOpcode::UMax32:
        case IrOpcode::SLessThan32:
        case IrOpcode::SLessThan64:
        case IrOpcode::ULessThan32:
        case IrOpcode::ULessThan64:
        case IrOpcode::IEqual32:
        case IrOpcode::IEqual64:
        case IrOpcode::SLessThanEqual32:
        case IrOpcode::ULessThanEqual32:
        case IrOpcode::SGreaterThan32:
        case IrOpcode::UGreaterThan32:
        case IrOpcode::UGreaterThan64:
        case IrOpcode::INotEqual32:
        case IrOpcode::INotEqual64:
        case IrOpcode::SGreaterThanEqual32:
        case IrOpcode::UGreaterThanEqual32:
        case IrOpcode::LogicalOr:
        case IrOpcode::LogicalAnd:
        case IrOpcode::LogicalXor:
        case IrOpcode::LogicalNot:
        case IrOpcode::FPOrdEqual32:
        case IrOpcode::FPUnordEqual32:
        case IrOpcode::FPOrdNotEqual32:
        case IrOpcode::FPUnordNotEqual32:
        case IrOpcode::FPOrdLessThan32:
        case IrOpcode::FPUnordLessThan32:
        case IrOpcode::FPOrdGreaterThan32:
        case IrOpcode::FPUnordGreaterThan32:
        case IrOpcode::FPOrdLessThanEqual32:
        case IrOpcode::FPUnordLessThanEqual32:
        case IrOpcode::FPOrdGreaterThanEqual32:
        case IrOpcode::FPUnordGreaterThanEqual32:
        case IrOpcode::FPIsNan32:
        case IrOpcode::FPAdd32:
        case IrOpcode::FPSub32:
        case IrOpcode::FPFma32:
        case IrOpcode::FPMad32:
        case IrOpcode::FPMul32:
        case IrOpcode::FPMin32:
        case IrOpcode::FPMax32:
        case IrOpcode::FPFloor32:
        case IrOpcode::FPCeil32:
        case IrOpcode::FPTrunc32:
        case IrOpcode::FPFract32:
        case IrOpcode::FPRoundEven32:
        case IrOpcode::GetSrtResource:
        case IrOpcode::ReadConst:
            return true;
        default:
            return false;
    }
}

bool ClobbersReadConst(IrOpcode opcode) {
    const auto buffer = BufferAccessOf(opcode);
    if (buffer == BufferAccess::Write || buffer == BufferAccess::Atomic) return true;
    const auto address = AddressOpcodeInfoOf(opcode).access;
    return address == AddressAccess::Write || address == AddressAccess::Atomic || opcode == IrOpcode::Barrier;
}

struct Operand {
    const IrValue* value;
    IrType type;
    std::uint64_t immediate;

    bool operator==(const Operand& other) const = default;
};

Operand OperandOf(const IrValue* resolved) {
    if (resolved->Parent() == nullptr && resolved->HasImmediate()) return {nullptr, resolved->Type(), resolved->ImmediateU64()};
    return {resolved, resolved->Type(), 0u};
}

struct ValueKey {
    IrOpcode opcode;
    IrType type;
    std::uint64_t flags;
    std::uint64_t memoryEpoch = 0;
    std::array<const IrValue*, 4> operands{};
    std::size_t operandCount = 0;

    bool operator==(const ValueKey& other) const {
        if (opcode != other.opcode || type != other.type || flags != other.flags || memoryEpoch != other.memoryEpoch || operandCount != other.operandCount) return false;
        for (std::size_t index = 0; index < operandCount; ++index) {
            if (operands[index] != other.operands[index] && OperandOf(operands[index]) != OperandOf(other.operands[index])) return false;
        }
        return true;
    }
};

struct ValueKeyHash {
    std::size_t operator()(const ValueKey& key) const {
        std::size_t hash = static_cast<std::size_t>(key.opcode) * 0x9e3779b97f4a7c15ull ^ static_cast<std::size_t>(key.type) ^ std::hash<std::uint64_t>{}(key.flags) * 31u ^ std::hash<std::uint64_t>{}(key.memoryEpoch) * 131u;
        for (const auto* argument : std::span(key.operands).first(key.operandCount)) {
            const auto operand = OperandOf(argument);
            hash = hash * 1099511628211ull ^ std::hash<const void*>{}(operand.value) ^ std::hash<std::uint64_t>{}(operand.immediate) * 7u ^ static_cast<std::size_t>(operand.type);
        }
        return hash;
    }
};

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

std::vector<std::vector<IrBlock*>> DominatorChildren(const std::vector<IrBlock*>& order, std::unordered_map<const IrBlock*, std::size_t>& index) {
    for (std::size_t position = 0; position < order.size(); ++position) index[order[position]] = position;
    constexpr std::size_t none = static_cast<std::size_t>(-1);
    std::vector<std::size_t> dominator(order.size(), none);
    dominator[0] = 0;
    const auto intersect = [&](std::size_t left, std::size_t right) {
        while (left != right) {
            while (left > right) left = dominator[left];
            while (right > left) right = dominator[right];
        }
        return left;
    };
    for (bool changed = true; changed;) {
        changed = false;
        for (std::size_t position = 1; position < order.size(); ++position) {
            std::size_t candidate = none;
            for (const IrBlock* predecessor : order[position]->Predecessors()) {
                const auto found = index.find(predecessor);
                if (found == index.end() || dominator[found->second] == none) continue;
                candidate = candidate == none ? found->second : intersect(candidate, found->second);
            }
            if (candidate != dominator[position]) {
                dominator[position] = candidate;
                changed = true;
            }
        }
    }
    std::vector<std::vector<IrBlock*>> children(order.size());
    for (std::size_t position = 1; position < order.size(); ++position) children[dominator[position]].push_back(order[position]);
    return children;
}

}

RedundantValueEliminationStats RedundantValueEliminator::Eliminate(IrProgram& program) const {
    RedundantValueEliminationStats stats;
    if (program.BlockOrder().empty()) return stats;
    const auto order = ReversePostOrder(*program.BlockOrder().front());
    std::unordered_map<const IrBlock*, std::size_t> index;
    const auto children = DominatorChildren(order, index);
    for (const auto* block : program.BlockOrder()) {
        if (!index.contains(block)) throw std::runtime_error("redundant value elimination: block " + std::to_string(block->Id()) + " is unreachable from the entry block");
    }
    std::uint64_t nextClobberEpoch = order.size() + 2u;
    std::vector<std::uint64_t> firstClobber(order.size(), 0u);
    std::vector<std::uint64_t> lastClobber(order.size(), 0u);
    bool hasClobber = false;
    for (std::size_t position = 0; position < order.size(); ++position) {
        firstClobber[position] = nextClobberEpoch;
        for (const auto* inst : order[position]->Instructions()) {
            if (ClobbersReadConst(inst->Opcode())) {
                lastClobber[position] = nextClobberEpoch++;
                hasClobber = true;
            }
        }
    }
    std::vector<std::uint64_t> entryEpoch(order.size(), hasClobber ? 0u : 1u);
    const auto exitEpoch = [&](std::size_t position) -> std::uint64_t {
        if (entryEpoch[position] == 0u) return 0u;
        return lastClobber[position] != 0u ? lastClobber[position] : entryEpoch[position];
    };
    for (bool changed = hasClobber; changed;) {
        changed = false;
        for (std::size_t position = 0; position < order.size(); ++position) {
            std::uint64_t candidate = position == 0u ? 1u : 0u;
            for (const IrBlock* predecessor : order[position]->Predecessors()) {
                const auto found = index.find(predecessor);
                if (found == index.end()) continue;
                const auto incoming = exitEpoch(found->second);
                if (incoming == 0u) continue;
                if (candidate == 0u) {
                    candidate = incoming;
                } else if (candidate != incoming) {
                    candidate = position + 2u;
                }
            }
            if (candidate != entryEpoch[position]) {
                entryEpoch[position] = candidate;
                changed = true;
            }
        }
    }
    std::unordered_map<ValueKey, IrValue*, ValueKeyHash> available;
    std::vector<const ValueKey*> added;
    std::unordered_map<const IrValue*, IrValue*> replacements;
    const auto visit = [&](std::size_t position) {
        std::uint64_t epoch = entryEpoch[position];
        std::uint64_t nextClobber = firstClobber[position];
        for (auto* inst : order[position]->Instructions()) {
            if (ClobbersReadConst(inst->Opcode())) {
                epoch = nextClobber++;
                continue;
            }
            if (!Recomputable(inst->Opcode()) || inst->HasImmediate() || !inst->HasUses()) continue;
            ValueKey key{inst->Opcode(), inst->Type(), inst->Flags<std::uint64_t>(), inst->Opcode() == IrOpcode::ReadConst ? epoch : 0u};
            key.operandCount = inst->ArgumentCount();
            if (key.operandCount > key.operands.size()) continue;
            for (std::size_t index = 0; index < key.operandCount; ++index) key.operands[index] = inst->Argument(index)->Resolve();
            const auto [found, inserted] = available.try_emplace(key, inst);
            if (inserted) {
                added.push_back(&found->first);
                continue;
            }
            inst->ReplaceAllUsesWith(found->second);
            replacements.emplace(inst, found->second);
            ++stats.replacedValues;
        }
    };
    std::vector<std::pair<std::size_t, std::size_t>> stack{{0u, 0u}};
    std::vector<std::size_t> scopes{added.size()};
    visit(0u);
    while (!stack.empty()) {
        auto& [position, next] = stack.back();
        if (next < children[position].size()) {
            const auto child = index.at(children[position][next++]);
            scopes.push_back(added.size());
            visit(child);
            stack.emplace_back(child, 0u);
            continue;
        }
        while (added.size() > scopes.back()) {
            available.erase(available.find(*added.back()));
            added.pop_back();
        }
        scopes.pop_back();
        stack.pop_back();
    }
    const auto replaced = [&](IrValue*& value) {
        if (value == nullptr) return;
        if (const auto found = replacements.find(value->Resolve()); found != replacements.end()) value = found->second;
    };
    for (auto& info : program.Metadata().blockInfo) {
        replaced(info.condition);
        replaced(info.indirectTarget);
    }
    for (auto*& read : program.Metadata().dynamicReads) replaced(read);
    for (auto& descriptor : program.Resources().descriptorSources) {
        for (auto*& dword : descriptor.dwords) replaced(dword);
    }
    return stats;
}

}
