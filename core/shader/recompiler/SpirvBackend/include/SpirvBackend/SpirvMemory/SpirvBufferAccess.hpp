#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVBUFFERACCESS_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVBUFFERACCESS_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t EmitBinaryU32(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t lhs, std::uint32_t rhs);
std::uint32_t EmitShaderDataDwordLoad(SpirvEmitterState& state, std::uint32_t dwordIndex);
void EmitMemoryOffsets(SpirvEmitterState& state);
std::uint32_t LdsDwordCount(const SpirvEmitterState& state);
bool LdsInDeviceMemory(const SpirvEmitterState& state);
std::uint32_t LdsMemorySemantics(const SpirvEmitterState& state);
void EmitLdsBufferBase(SpirvEmitterState& state);
std::uint32_t EmitLdsLockPointer(SpirvEmitterState& state);
MemoryResourceAccess PrepareMemoryResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem);
std::uint32_t EmitMemoryElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t rawIndex);
std::uint32_t EmitMemoryElementInBounds(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index);
std::uint32_t EmitMemoryElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index);
std::uint32_t EmitStorageBufferElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index, std::uint32_t pointerType);

}

#endif
