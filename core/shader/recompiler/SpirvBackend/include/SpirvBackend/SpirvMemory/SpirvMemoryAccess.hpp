#ifndef CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORYACCESS_HPP
#define CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORYACCESS_HPP

#include "SpirvBackend/SpirvMemory/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t LdsDwordCount(const SpirvEmitterState& state);
std::uint32_t StorageBufferPackedStride(const SpirvEmitterState& state, const MemoryInfo& mem);
IrBufferFormat StorageBufferFormat(const SpirvEmitterState& state, const MemoryInfo& mem);

void EmitMemoryOffsets(SpirvEmitterState& state);

MemoryResourceAccess PrepareMemoryResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem);
MemoryResourceAccess PrepareStorageBufferResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem,
                                                        std::uint32_t variable, std::uint32_t pointerType);

std::uint32_t EmitMemoryElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access,
                                     std::uint32_t rawIndex);
std::uint32_t EmitMemoryElementInBounds(SpirvEmitterState& state, const MemoryResourceAccess& access,
                                        std::uint32_t index);
std::uint32_t EmitMemoryElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access,
                                       std::uint32_t index);
std::uint32_t EmitStorageBufferElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access,
                                              std::uint32_t index, std::uint32_t pointerType);

}

#endif
