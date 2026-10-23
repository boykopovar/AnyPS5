#include "PipelineSpecialization.hpp"
#include "SpirvBackend/SpirvMemory/SpirvBufferAccess.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvDescriptors.hpp"
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <SpirvBackend/SpirvEmitterInstructions.hpp>
#include <SpirvBackend/SpirvMemory/SpirvConstants.hpp>

namespace ShaderRecompiler
{
namespace {

    void EnsureLdsStorage(SpirvEmitterState& state) {
        if (state.ldsVariable != 0) {
            return;
        }
        if (ShaderWorkgroupInput(state) == nullptr) {
            FailEmit("function LDS was not prepared before function emission");
        }
        state.ldsVariable = state.module.DefineGlobalVariable(TypeU32ArrayPointer(state, spv::StorageClassWorkgroup, LdsStorageDwords(state)), spv::StorageClassWorkgroup);
        state.module.AddName(state.ldsVariable, "lds_dwords");
    }

}

std::uint32_t EmitBinaryU32(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t lhs, std::uint32_t rhs) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(opcode, TypeU32(state), result, lhs, rhs);
    return result;
}

std::uint32_t EmitShaderDataDwordLoad(SpirvEmitterState& state, std::uint32_t dwordIndex) {
    const IrBindingLayout& layout = state.program.Metadata().bindings;
    const auto pointer = state.module.AllocateId();
    const auto value = state.module.AllocateId();
    if (layout.UsesPushData()) {
        const auto base = state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::PushDataOffset, state.stencilReplay ? layout.pushDataStartDword % PushData::DwordCount : layout.pushDataStartDword);
        const auto index = Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpISub, TypeU32(state), base, ConstantU32(state, state.stencilReplay ? 0u : layout.PushSlotDword())), ConstantU32(state, dwordIndex));
        state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0), index);
    } else if (state.shaderDataStorageVariable != 0) {
        state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.shaderDataStorageVariable, ConstantU32(state, 0), ConstantU32(state, dwordIndex));
    } else {
        FailEmit("shader data is neither in push constants nor in a storage buffer");
    }
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

void EmitMemoryOffsets(SpirvEmitterState& state) {
    const IrBindingLayout& layout = state.program.Metadata().bindings;
    state.memoryByteOffsets.assign(layout.memoryOffsetCount, 0u);
    for (std::uint32_t i = 0; i < layout.memoryOffsetCount; i++) {
        const auto word = EmitShaderDataDwordLoad(state, layout.memoryOffsetDword + i / 4u);
        const auto shift = ConstantU32(state, (i % 4u) * 8u);
        state.memoryByteOffsets[i] = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, word, shift), ConstantU32(state, 0xffu));
    }
}

std::uint32_t LdsDwordCount(const SpirvEmitterState& state) {
    const auto* workgroup = ShaderWorkgroupInput(state);
    if (workgroup != nullptr) return workgroup->ldsSizeDwords;
    return state.requirements.functionLdsDwords != 0u ? state.requirements.functionLdsDwords : FunctionLdsDwordLimit;
}

bool LdsInDeviceMemory(const SpirvEmitterState& state) {
    return state.program.Info().sharedMemoryBytes != 0u;
}

std::uint32_t LdsMemorySemantics(const SpirvEmitterState& state) {
    return spv::MemorySemanticsWorkgroupMemoryMask | (LdsInDeviceMemory(state) || state.pixelFunctionLdsBuffer ? spv::MemorySemanticsUniformMemoryMask : 0u);
}

void EmitPixelFunctionLdsBase(SpirvEmitterState& state) {
    if (!state.pixelFunctionLdsBuffer) return;
    if (state.ldsBufferVariable == 0u || state.hostSubgroupSize == 0u) FailEmit("pixel function LDS buffer was not declared");
    const auto element = [&](std::uint32_t dword) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.ldsBufferVariable, ConstantU32(state, 0u), ConstantU32(state, dword));
        return pointer;
    };
    const auto slotVar = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), slotVar, spv::StorageClassFunction, ConstantU32(state, 0u));
    const auto elected = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformElect, TypeBool(state), elected, ConstantU32(state, spv::ScopeSubgroup));
    EmitIfCondition(state, elected, [&] {
        const auto acquired = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicIAdd, TypeU32(state), acquired, element(0u), ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsUniformMemoryMask), ConstantU32(state, 1u));
        const auto capacity = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), capacity, element(2u));
        const auto exhausted = state.module.AllocateId();
        state.module.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), exhausted, acquired, capacity);
        EmitIfCondition(state, exhausted, [&] {
            state.module.AddFunction(spv::OpAtomicStore, element(1u), ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsReleaseMask | spv::MemorySemanticsUniformMemoryMask), ConstantU32(state, 1u));
        });
        const auto claimed = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelect, TypeU32(state), claimed, exhausted, ConstantU32(state, 0u), acquired);
        state.module.AddFunction(spv::OpStore, slotVar, claimed);
    });
    const auto local = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), local, slotVar);
    const auto slot = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformBroadcastFirst, TypeU32(state), slot, ConstantU32(state, spv::ScopeSubgroup), local);
    std::uint32_t lane;
    if (state.splitSubgroup) {
        if (state.subgroupLocalInvocationIdVariable == 0) FailEmit("pixel function LDS lane was not declared");
        lane = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), lane, state.subgroupLocalInvocationIdVariable);
    } else lane = EmitSubgroupLocalInvocationId(state);
    const auto within = EmitAddU32(state, EmitBinaryU32(state, spv::OpIMul, slot, ConstantU32(state, state.hostSubgroupSize)), lane);
    state.ldsBufferBase = EmitAddU32(state, EmitBinaryU32(state, spv::OpIMul, within, ConstantU32(state, FunctionLdsDwordLimit)), ConstantU32(state, 3u));
}

void EmitLdsBufferBase(SpirvEmitterState& state) {
    EmitPixelFunctionLdsBase(state);
    if (!LdsInDeviceMemory(state)) {
        return;
    }
    if (state.ldsBufferVariable == 0u || state.numWorkgroupsVariable == 0u) {
        FailEmit("device memory LDS was not declared before function emission");
    }
    if (ShaderWorkgroupInput(state) == nullptr || state.program.Resources().stage != IrShaderStage::Compute || state.program.Info().sharedMemoryBytes != LdsDwordCount(state) * 4u) {
        FailEmit("device memory LDS does not match the compute program's LDS size");
    }
    const auto groups = [&](std::uint32_t component) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeU32(state)), pointer, state.numWorkgroupsVariable, ConstantU32(state, component));
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };
    const auto group = [&](std::uint32_t component) { return EmitInputComponentU32(state, StageInputKind::WorkgroupId, component); };
    const auto plane = EmitAddU32(state, EmitBinaryU32(state, spv::OpIMul, group(2u), groups(1u)), group(1u));
    const auto linear = EmitAddU32(state, EmitBinaryU32(state, spv::OpIMul, plane, groups(0u)), group(0u));
    state.ldsBufferBase = EmitBinaryU32(state, spv::OpIMul, linear, ConstantU32(state, WorkgroupMemoryStrideDwords(state.program.Info())));
}

std::uint32_t EmitLdsLockPointer(SpirvEmitterState& state) {
    if (!state.requirements.ldsLock) {
        FailEmit("LDS lock was not requested by the program analysis");
    }
    if (LdsInDeviceMemory(state)) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.ldsBufferVariable, ConstantU32(state, 0u), EmitAddU32(state, state.ldsBufferBase, ConstantU32(state, LdsDwordCount(state))));
        return pointer;
    }
    EnsureLdsStorage(state);
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeU32ElementPointer(state, spv::StorageClassWorkgroup), pointer, state.ldsVariable, ConstantU32(state, LdsDwordCount(state)));
    return pointer;
}

std::uint32_t EmitLdsBufferU64Pointer(SpirvEmitterState& state, std::uint32_t index) {
    if (!state.requirements.ldsBufferInt64Atomics || state.ldsBufferU64Variable == 0u || state.ldsBufferBase == 0u) {
        FailEmit("64-bit device memory LDS atomics were not prepared at function entry");
    }
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassStorageBuffer, TypeScalarU64(state)), pointer, state.ldsBufferU64Variable, ConstantU32(state, 0u), EmitBinaryU32(state, spv::OpShiftRightLogical, EmitAddU32(state, state.ldsBufferBase, index), ConstantU32(state, 1u)));
    return pointer;
}

MemoryResourceAccess PrepareMemoryResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem) {
    MemoryResourceAccess access;
    access.kind = mem.kind;
    switch (mem.kind) {
    case ResourceKind::Lds:
        if (state.pixelFunctionLdsBuffer) {
            if (state.ldsBufferVariable == 0u || state.ldsBufferBase == 0u) FailEmit("pixel function LDS buffer was not prepared at function entry");
            access.objectPointer = state.ldsBufferVariable;
            access.length = ConstantU32(state, LdsDwordCount(state));
            return access;
        }
        if (LdsInDeviceMemory(state)) {
            if (state.ldsBufferVariable == 0u || state.ldsBufferBase == 0u) {
                FailEmit("device memory LDS was not prepared at function entry");
            }
            access.objectPointer = state.ldsBufferVariable;
            access.length = ConstantU32(state, LdsDwordCount(state));
            return access;
        }
        EnsureLdsStorage(state);
        access.objectPointer = state.ldsVariable;
        access.length = ConstantU32(state, LdsDwordCount(state));
        return access;
    case ResourceKind::Gds:
        if (state.gdsVariable == 0) {
            ExitDescriptorBindingFailure(state, DescriptorBindingKind::Gds, mem.resource, "GDS binding was not emitted");
        }
        if (state.gdsLength == 0) {
            FailEmit("GDS length was not prepared at function entry");
        }
        access.objectPointer = state.gdsVariable;
        access.length = state.gdsLength;
        return access;
    case ResourceKind::Scratch:
        if (state.scratchVariable.at(state.laneHalf) == 0) {
            FailEmit("scratch storage was not prepared before function emission");
        }
        access.objectPointer = state.scratchVariable.at(state.laneHalf);
        access.length = ConstantU32(state, state.program.Info().scratchDwords);
        return access;
    case ResourceKind::ScalarAddress:
    case ResourceKind::Flat:
    case ResourceKind::Global:
        FailEmit("physical address memory must use the BDA emitter");
    case ResourceKind::ScalarBuffer:
    case ResourceKind::Buffer:
        FailEmit("buffer memory must use the runtime V# emitter");
    default:
        FailEmit("unsupported memory resource kind " + std::to_string(static_cast<std::uint32_t>(mem.kind)));
    }
}

std::uint32_t EmitMemoryElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t rawIndex) {
    return access.addIndexOffset ? EmitAddU32(state, rawIndex, access.indexOffset) : rawIndex;
}

std::uint32_t EmitMemoryElementInBounds(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index) {
    const auto last = access.misalignment != 0u ? EmitAddU32(state, index, ConstantU32(state, 1u)) : index;
    const auto inBounds = state.module.AllocateId();
    state.module.AddFunction(spv::OpULessThan, TypeBool(state), inBounds, last, access.length);
    return inBounds;
}

bool InFunctionStorage(const SpirvEmitterState& state, ResourceKind kind) {
    if (kind == ResourceKind::Scratch) return true;
    return kind == ResourceKind::Lds && !LdsInDeviceMemory(state) && ShaderWorkgroupInput(state) == nullptr;
}

std::uint32_t EmitValueOrZero(SpirvEmitterState& state, std::uint32_t condition, std::uint32_t value) {
    return Select(state, TypeU32(state), condition, value, ConstantU32(state, 0u));
}

std::uint32_t LdsStorageDwords(const SpirvEmitterState& state) {
    return LdsDwordCount(state) + (state.requirements.ldsLock ? 1u : 0u);
}

bool LoadsSelectable(const SpirvEmitterState& state, const MemoryResourceAccess& access) {
    if (access.kind == ResourceKind::Scratch) return true;
    if (access.kind == ResourceKind::Lds) return false;
    return state.robustBufferAccess2;
}

std::uint32_t GuardedElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t condition, std::uint32_t index) {
    if (access.kind == ResourceKind::Lds) FailEmit("LDS loads keep their branch and have no guarded element");
    const auto fallback = access.kind == ResourceKind::Scratch ? 0u : 0xfffffffeu;
    return Select(state, TypeU32(state), condition, index, ConstantU32(state, fallback));
}

std::uint32_t EmitMemoryElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index) {
    if (access.kind == ResourceKind::Lds && state.pixelFunctionLdsBuffer) {
        return EmitStorageBufferElementPointer(state, access, EmitAddU32(state, state.ldsBufferBase, index), TypeStorageBufferElementPointer(state));
    }
    if (access.kind == ResourceKind::Lds && LdsInDeviceMemory(state)) {
        return EmitStorageBufferElementPointer(state, access, EmitAddU32(state, state.ldsBufferBase, index), TypeStorageBufferElementPointer(state));
    }
    if (access.kind == ResourceKind::Lds || access.kind == ResourceKind::Scratch) {
        const auto pointer = state.module.AllocateId();
        const std::uint32_t storageClass = InFunctionStorage(state, access.kind) ? spv::StorageClassFunction : spv::StorageClassWorkgroup;
        state.module.AddFunction(spv::OpAccessChain, TypeU32ElementPointer(state, storageClass), pointer, access.objectPointer, index);
        return pointer;
    }
    return EmitStorageBufferElementPointer(state, access, index, TypeStorageBufferElementPointer(state));
}

std::uint32_t EmitStorageBufferElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index, std::uint32_t pointerType) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, pointerType, pointer, access.objectPointer, ConstantU32(state, 0), index);
    return pointer;
}
}
