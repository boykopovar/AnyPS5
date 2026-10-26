#include "SpirvBackend/SpirvAluEmitter.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

namespace ShaderRecompiler {

void EmitAluValue(SpirvModule& module, const IrValue& value, std::uint32_t resultId) {
    throw std::runtime_error("EmitAluValue not implemented");
}

void EmitAluValue(SpirvValueEmitContext& context, const IrValue& value, std::uint32_t resultId) {
    throw std::runtime_error("EmitAluValue not implemented");
}

void EmitGetThreadBitScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetThreadBitScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetScalarMaskTag(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetScalarMaskTag(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetVectorRegister(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetVectorRegister(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

namespace {

std::uint32_t VectorWindowElement(SpirvEmitterState& state, std::uint32_t index) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), pointer, state.vectorWindowVariable.at(state.laneHalf), index);
    return pointer;
}

std::uint32_t AccessNarrowedWindow(SpirvEmitterState& state, std::uint32_t index, const std::function<std::uint32_t(std::uint32_t)>& access) {
    const auto& slots = state.vectorWindowSlots.at(state.laneHalf);
    const auto arrayLabel = state.module.AllocateId();
    const auto slotsLabel = state.module.AllocateId();
    const auto slotsMerge = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    const auto inArray = Binary(state, spv::OpULessThan, TypeBool(state), index, ConstantU32(state, state.vectorWindowIndexed));
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, inArray, arrayLabel, slotsLabel);
    EmitLabel(state, arrayLabel);
    const auto fromArray = access(VectorWindowElement(state, index));
    const auto arrayExit = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, slotsLabel);
    std::vector<std::uint32_t> labels(slots.size());
    std::vector<std::uint32_t> words{spv::OpSwitch, index, slotsMerge};
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        labels[slot] = state.module.AllocateId();
        words.push_back(state.vectorWindowIndexed + static_cast<std::uint32_t>(slot));
        words.push_back(labels[slot]);
    }
    state.module.AddFunction(spv::OpSelectionMerge, slotsMerge, spv::SelectionControlMaskNone);
    state.module.AddFunction(std::span<const std::uint32_t>(words));
    std::vector<std::uint32_t> incoming;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        EmitLabel(state, labels[slot]);
        incoming.push_back(access(slots[slot]));
        incoming.push_back(labels[slot]);
        state.module.AddFunction(spv::OpBranch, slotsMerge);
    }
    incoming.push_back(ConstantU32(state, 0u));
    incoming.push_back(slotsLabel);
    EmitLabel(state, slotsMerge);
    const auto fromSlots = state.module.AllocateId();
    std::vector<std::uint32_t> phi{spv::OpPhi, TypeU32(state), fromSlots};
    phi.insert(phi.end(), incoming.begin(), incoming.end());
    state.module.AddFunction(std::span<const std::uint32_t>(phi));
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, TypeU32(state), value, fromArray, arrayExit, fromSlots, slotsMerge);
    return value;
}

std::uint32_t DirectWindowElement(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const IrValue* index = inst.Argument(0)->Resolve();
    if (state.vectorWindowIndexed == 0u) return VectorWindowElement(state, ctx.Arg(inst, 0u));
    if (!index->HasImmediate()) return 0u;
    if (index->ImmediateU32() < state.vectorWindowIndexed) return VectorWindowElement(state, ctx.Arg(inst, 0u));
    return state.vectorWindowSlots.at(state.laneHalf).at(index->ImmediateU32() - state.vectorWindowIndexed);
}

}

void EmitLoadVectorWindow(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto load = [&state](std::uint32_t pointer) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };
    const auto pointer = DirectWindowElement(ctx, inst);
    ctx.Define(inst, pointer != 0u ? load(pointer) : AccessNarrowedWindow(state, ctx.Arg(inst, 0u), load));
}

void EmitStoreVectorWindow(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto value = ctx.Arg(inst, 1u);
    const auto store = [&state, value](std::uint32_t pointer) {
        state.module.AddFunction(spv::OpStore, pointer, value);
        return ConstantU32(state, 0u);
    };
    if (const auto pointer = DirectWindowElement(ctx, inst); pointer != 0u) (void)store(pointer);
    else (void)AccessNarrowedWindow(state, ctx.Arg(inst, 0u), store);
}

void EmitGetGotoVariable(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetGotoVariable(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetScc(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetScc(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetExec(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetExec(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetExecLo(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetExecLo(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetExecHi(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetExecHi(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetVcc(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetVcc(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetVccLo(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetVccLo(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetVccHi(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetVccHi(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitGetM0(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

void EmitSetM0(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitUnreachable(ctx, inst);
}

}
