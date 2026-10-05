#include "SpirvBackend/SpirvMemory/SpirvBufferAccess.hpp"
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

    [[noreturn]] void FailEmit(const std::string& reason) {
        throw std::runtime_error("SPIR-V module emission failed: " + reason);
    }

    const ShaderWorkgroupInputInfo* ShaderWorkgroupInput(const SpirvEmitterState& state) {
        switch (state.program.Resources().stage) {
        case IrShaderStage::Compute:
            if (state.inputInfo.compute == nullptr) {
                FailEmit("compute input info is missing");
            }
            return state.inputInfo.compute;
        case IrShaderStage::Mesh:
            if (state.inputInfo.vertex == nullptr) {
                FailEmit("vertex input info is missing");
            }
            return &state.inputInfo.vertex->mesh;
        default:
            return nullptr;
        }
    }

    void EnsureLdsStorage(SpirvEmitterState& state) {
        if (state.ldsVariable != 0) {
            return;
        }
        if (ShaderWorkgroupInput(state) == nullptr) {
            FailEmit("function LDS was not prepared before function emission");
        }
        state.ldsVariable = state.module.DefineGlobalVariable(TypeU32ArrayPointer(state, spv::StorageClassWorkgroup, LdsDwordCount(state) + (state.requirements.ldsLock ? 1u : 0u)), spv::StorageClassWorkgroup);
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
        state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0), ConstantU32(state, dwordIndex + layout.pushDataStartDword));
    } else if (state.shaderDataStorageVariable != 0) {
        state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.shaderDataStorageVariable, ConstantU32(state, 0), ConstantU32(state, dwordIndex));
    } else {
        FailEmit("shader data is neither in push constants nor in a storage buffer");
    }
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

std::uint32_t StorageBufferPackedStride(const SpirvEmitterState& state, const MemoryInfo& mem) {
    if (mem.resource >= state.program.Info().buffers.size()) {
        ExitDescriptorBindingFailure(state, DescriptorBindingKind::Buffers, mem.resource, "buffer specialization is missing");
    }
    return state.program.Info().buffers[mem.resource].packedStride;
}

IrBufferFormat StorageBufferFormat(const SpirvEmitterState& state, const MemoryInfo& mem) {
    if (mem.resource >= state.program.Info().buffers.size()) {
        ExitDescriptorBindingFailure(state, DescriptorBindingKind::Buffers, mem.resource, "buffer specialization is missing");
    }
    return state.program.Info().buffers[mem.resource].descriptorFormat;
}

void EmitMemoryOffsets(SpirvEmitterState& state) {
    const IrBindingLayout& layout = state.program.Metadata().bindings;
    if (layout.memoryOffsetCount > state.memoryByteOffsets.size()) {
        FailEmit("memory offset count exceeds the buffer limit");
    }
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

std::uint32_t EmitLdsLockPointer(SpirvEmitterState& state) {
    if (!state.requirements.ldsLock) {
        FailEmit("LDS lock was not requested by the program analysis");
    }
    EnsureLdsStorage(state);
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeU32ElementPointer(state, spv::StorageClassWorkgroup), pointer, state.ldsVariable, ConstantU32(state, LdsDwordCount(state)));
    return pointer;
}

MemoryResourceAccess PrepareStorageBufferResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem, std::uint32_t variable, std::uint32_t pointerType) {
    if (variable == 0) {
        ExitDescriptorBindingFailure(state, DescriptorBindingKind::Buffers, mem.resource, "storage buffer descriptor array was not emitted");
    }
    const auto arrayIndex = ResourceForDescriptor(state, DescriptorBindingKind::Buffers, mem.resource);
    MemoryResourceAccess access;
    access.kind = mem.kind;
    access.objectPointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, pointerType, access.objectPointer, variable, ConstantU32(state, arrayIndex));
    access.byteOffset = state.memoryByteOffsets.at(arrayIndex);
    access.memoryAccess = mem.coherent ? spv::MemoryAccessVolatileMask : 0u;
    access.length = state.module.AllocateId();
    state.module.AddFunction(spv::OpArrayLength, TypeU32(state), access.length, access.objectPointer, 0u);
    if (mem.resource < state.program.Info().buffers.size() && state.program.Info().buffers[mem.resource].empty) {
        access.length = ConstantU32(state, 0u);
    }
    return access;
}

MemoryResourceAccess PrepareMemoryResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem) {
    MemoryResourceAccess access;
    access.kind = mem.kind;
    switch (mem.kind) {
    case ResourceKind::Lds:
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
        access = PrepareStorageBufferResourceAccess(state, mem, state.storageBufferVariable, TypeStorageBufferPointer(state));
        access.indexOffset = EmitBinaryU32(state, spv::OpShiftRightLogical, access.byteOffset, ConstantU32(state, 2u));
        access.addIndexOffset = true;
        return access;
    default:
        FailEmit("unsupported memory resource kind " + std::to_string(static_cast<std::uint32_t>(mem.kind)));
    }
}

std::uint32_t EmitMemoryElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t rawIndex) {
    return access.addIndexOffset ? EmitAddU32(state, rawIndex, access.indexOffset) : rawIndex;
}

std::uint32_t EmitMemoryElementInBounds(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index) {
    const auto inBounds = state.module.AllocateId();
    state.module.AddFunction(spv::OpULessThan, TypeBool(state), inBounds, index, access.length);
    return inBounds;
}

std::uint32_t EmitMemoryElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index) {
    if (access.kind == ResourceKind::Lds || access.kind == ResourceKind::Scratch) {
        const auto pointer = state.module.AllocateId();
        const std::uint32_t storageClass = access.kind == ResourceKind::Scratch ? spv::StorageClassFunction : ShaderWorkgroupInput(state) != nullptr ? spv::StorageClassWorkgroup : spv::StorageClassFunction;
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
