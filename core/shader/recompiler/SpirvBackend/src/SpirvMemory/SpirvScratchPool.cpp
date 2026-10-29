#include "SpirvBackend/SpirvMemory/SpirvBufferAccess.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <spirv/unified1/spirv.hpp>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t HeaderWordsPerSlot = 2u;

std::uint32_t PoolWord(SpirvEmitterState& state, std::uint32_t index) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.scratchPoolVariable, ConstantU32(state, 0u), index);
    return pointer;
}

std::uint32_t LoadU32(SpirvEmitterState& state, std::uint32_t pointer) {
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

std::uint32_t InvocationsPerGroup(const SpirvEmitterState& state) {
    const auto size = WorkgroupLocalSize(state, *ShaderWorkgroupInput(state));
    return size[0] * size[1] * size[2];
}

std::uint32_t LinearWorkgroupId(SpirvEmitterState& state) {
    const auto groups = [&](std::uint32_t component) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeU32(state)), pointer, state.numWorkgroupsVariable, ConstantU32(state, component));
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };
    const auto group = [&](std::uint32_t component) { return EmitInputComponentU32(state, StageInputKind::WorkgroupId, component); };
    const auto z = group(2u);
    const auto countY = groups(1u);
    const auto y = group(1u);
    const auto plane = EmitAddU32(state, EmitBinaryU32(state, spv::OpIMul, z, countY), y);
    const auto countX = groups(0u);
    return EmitAddU32(state, EmitBinaryU32(state, spv::OpIMul, plane, countX), group(0u));
}

}

bool ScratchPoolActive(const SpirvEmitterState& state) {
    return state.requirements.functionScratch && state.program.Resources().stage == IrShaderStage::Compute;
}

std::uint32_t ScratchPoolSlotDwords(const SpirvEmitterState& state) {
    const auto* workgroup = ShaderWorkgroupInput(state);
    if (workgroup == nullptr) FailEmit("scratch pool needs a compute workgroup size");
    const auto size = WorkgroupLocalSize(state, *workgroup);
    const auto invocations = size[0] * size[1] * size[2];
    const auto lanes = state.laneCount == 2u ? (invocations - 1u) + ((invocations - 1u) & ~31u) + 33u : invocations;
    return lanes * state.program.Info().scratchDwords;
}

void EmitScratchPoolAcquire(SpirvEmitterState& state) {
    if (!ScratchPoolActive(state)) return;
    if (state.scratchPoolVariable == 0u || state.numWorkgroupsVariable == 0u || state.scratchPoolCandidateVariable == 0u || state.scratchPoolWonVariable == 0u) {
        FailEmit("scratch pool was not declared before function emission");
    }
    const auto device = ConstantU32(state, spv::ScopeDevice);
    const auto acquire = ConstantU32(state, spv::MemorySemanticsAcquireMask | spv::MemorySemanticsUniformMemoryMask);
    const auto length = state.module.AllocateId();
    state.module.AddFunction(spv::OpArrayLength, TypeU32(state), length, state.scratchPoolVariable, 0u);
    const auto slotDwords = ScratchPoolSlotDwords(state);
    const auto slots = EmitBinaryU32(state, spv::OpUDiv, length, ConstantU32(state, slotDwords + HeaderWordsPerSlot));
    state.scratchPoolSlotCount = slots;
    state.laneHalf = 0u;
    const auto first = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), first, EmitLocalInvocationIndex(state), ConstantU32(state, 0u));
    const auto linear = LinearWorkgroupId(state);
    EmitIfCondition(state, first, [&] {
        state.module.AddFunction(spv::OpStore, state.scratchPoolCandidateVariable, EmitBinaryU32(state, spv::OpUMod, linear, slots));
        state.module.AddFunction(spv::OpStore, state.scratchPoolWonVariable, ConstantU32(state, 0u));
        const auto header = state.module.AllocateId();
        const auto body = state.module.AllocateId();
        const auto continuation = state.module.AllocateId();
        const auto merge = state.module.AllocateId();
        state.module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, header);
        const auto won = LoadU32(state, state.scratchPoolWonVariable);
        const auto pending = state.module.AllocateId();
        state.module.AddFunction(spv::OpIEqual, TypeBool(state), pending, won, ConstantU32(state, 0u));
        state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, pending, body, merge);
        EmitLabel(state, body);
        const auto candidate = LoadU32(state, state.scratchPoolCandidateVariable);
        const auto previous = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), previous, PoolWord(state, candidate), device, acquire, acquire, ConstantU32(state, 1u), ConstantU32(state, 0u));
        const auto acquired = state.module.AllocateId();
        state.module.AddFunction(spv::OpIEqual, TypeBool(state), acquired, previous, ConstantU32(state, 0u));
        const auto next = EmitAddU32(state, candidate, ConstantU32(state, 1u));
        const auto atEnd = state.module.AllocateId();
        state.module.AddFunction(spv::OpIEqual, TypeBool(state), atEnd, next, slots);
        const auto wrapped = Select(state, TypeU32(state), atEnd, ConstantU32(state, 0u), next);
        state.module.AddFunction(spv::OpStore, state.scratchPoolCandidateVariable, Select(state, TypeU32(state), acquired, candidate, wrapped));
        state.module.AddFunction(spv::OpStore, state.scratchPoolWonVariable, Select(state, TypeU32(state), acquired, ConstantU32(state, 1u), ConstantU32(state, 0u)));
        state.module.AddFunction(spv::OpBranch, continuation);
        EmitLabel(state, continuation);
        state.module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, merge);
        const auto slot = LoadU32(state, state.scratchPoolCandidateVariable);
        state.module.AddFunction(spv::OpAtomicStore, PoolWord(state, EmitAddU32(state, slots, slot)), device, ConstantU32(state, 0u), ConstantU32(state, InvocationsPerGroup(state)));
        state.module.AddFunction(spv::OpStore, state.scratchPoolSlotVariable, slot);
    });
    state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsUniformMemoryMask | spv::MemorySemanticsWorkgroupMemoryMask));
    state.scratchPoolSlot = LoadU32(state, state.scratchPoolSlotVariable);
    const auto header = EmitBinaryU32(state, spv::OpIMul, slots, ConstantU32(state, HeaderWordsPerSlot));
    const auto slotBase = EmitAddU32(state, header, EmitBinaryU32(state, spv::OpIMul, state.scratchPoolSlot, ConstantU32(state, slotDwords)));
    for (std::uint32_t half = 0; half < state.laneCount; half++) {
        state.laneHalf = half;
        state.scratchPoolLaneBase.at(half) = EmitAddU32(state, slotBase, EmitBinaryU32(state, spv::OpIMul, EmitLocalInvocationIndex(state), ConstantU32(state, state.program.Info().scratchDwords)));
    }
    state.laneHalf = 0u;
}

std::uint32_t EmitScratchPoolElementPointer(SpirvEmitterState& state, std::uint32_t index) {
    if (state.scratchPoolLaneBase.at(state.laneHalf) == 0u) FailEmit("scratch pool slot was not acquired before function emission");
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.scratchPoolVariable, ConstantU32(state, 0u), EmitAddU32(state, state.scratchPoolLaneBase.at(state.laneHalf), index));
    return pointer;
}

void EmitReturnReleasingScratch(SpirvEmitterState& state) {
    if (ScratchPoolActive(state)) {
        const auto device = ConstantU32(state, spv::ScopeDevice);
        const auto previous = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicIDecrement, TypeU32(state), previous, PoolWord(state, EmitAddU32(state, state.scratchPoolSlotCount, state.scratchPoolSlot)), device, ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsUniformMemoryMask));
        EmitIfCondition(state, EmitCompareU32Constant(state, spv::OpIEqual, previous, 1u), [&] {
            state.module.AddFunction(spv::OpAtomicStore, PoolWord(state, state.scratchPoolSlot), device, ConstantU32(state, spv::MemorySemanticsReleaseMask | spv::MemorySemanticsUniformMemoryMask), ConstantU32(state, 0u));
        });
        state.scratchPoolReturns++;
    }
    state.module.AddFunction(spv::OpReturn);
}

void VerifyScratchPoolReturns(const SpirvEmitterState& state, const std::vector<std::uint32_t>& words) {
    if (!ScratchPoolActive(state)) return;
    std::uint32_t returns = 0u;
    bool inMain = false;
    for (std::size_t cursor = 5u; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = words[cursor] & 0xffffu;
        if (count == 0u) FailEmit("malformed SPIR-V while checking scratch pool releases");
        if (op == spv::OpFunction) inMain = words[cursor + 2u] == state.mainFunc;
        else if (op == spv::OpFunctionEnd) inMain = false;
        else if (inMain && op == spv::OpReturn) returns++;
        cursor += count;
    }
    if (returns != state.scratchPoolReturns) FailEmit("a compute exit does not release its scratch pool slot");
}

}
