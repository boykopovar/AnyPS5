#include "Optimization/VectorWindowStoreEliminator.hpp"
#include <unordered_map>
#include <vector>

namespace ShaderRecompiler {

namespace {

using LiveSlots = std::vector<bool>;

const IrValue* constantSlot(const IrValue* inst, std::size_t slots) {
    const IrValue* index = inst->Argument(0)->Resolve();
    return index->HasImmediate() && index->ImmediateU32() < slots ? index : nullptr;
}

void transfer(const IrValue* inst, LiveSlots& live) {
    if (inst->Opcode() == IrOpcode::LoadVectorWindow) {
        if (const IrValue* slot = constantSlot(inst, live.size())) live[slot->ImmediateU32()] = true;
        else live.assign(live.size(), true);
    } else if (inst->Opcode() == IrOpcode::StoreVectorWindow) {
        if (const IrValue* slot = constantSlot(inst, live.size())) live[slot->ImmediateU32()] = false;
    }
}

}

VectorWindowStoreEliminationStats VectorWindowStoreEliminator::Eliminate(IrProgram& program) const {
    VectorWindowStoreEliminationStats stats;
    const std::size_t slots = program.Metadata().vectorWindowSize;
    if (slots == 0u) return stats;
    const auto& order = program.BlockOrder();
    std::unordered_map<const IrBlock*, LiveSlots> liveIn;
    for (const IrBlock* block : order) liveIn.emplace(block, LiveSlots(slots, false));
    const auto liveOut = [&](const IrBlock* block) {
        LiveSlots live(slots, false);
        for (const IrBlock* successor : block->Successors()) {
            const auto found = liveIn.find(successor);
            if (found == liveIn.end()) return LiveSlots(slots, true);
            for (std::size_t slot = 0; slot < slots; ++slot) live[slot] = live[slot] || found->second[slot];
        }
        return live;
    };
    for (bool changed = true; changed;) {
        changed = false;
        for (auto block = order.rbegin(); block != order.rend(); ++block) {
            auto live = liveOut(*block);
            const auto& instructions = (*block)->Instructions();
            for (auto inst = instructions.rbegin(); inst != instructions.rend(); ++inst) transfer(*inst, live);
            auto& entry = liveIn[*block];
            if (live != entry) {
                entry = std::move(live);
                changed = true;
            }
        }
    }
    for (IrBlock* block : order) {
        auto live = liveOut(block);
        auto& instructions = block->Instructions();
        for (auto inst = instructions.end(); inst != instructions.begin();) {
            --inst;
            IrValue* value = *inst;
            if (value->Opcode() == IrOpcode::StoreVectorWindow) {
                const IrValue* slot = constantSlot(value, slots);
                if (slot != nullptr && !live[slot->ImmediateU32()]) {
                    value->Invalidate();
                    inst = instructions.erase(inst);
                    value->SetParent(nullptr);
                    ++stats.removedStores;
                    continue;
                }
            }
            transfer(value, live);
        }
    }
    return stats;
}

}
