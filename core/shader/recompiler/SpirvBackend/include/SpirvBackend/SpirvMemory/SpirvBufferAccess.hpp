#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVBUFFERACCESS_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVBUFFERACCESS_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <functional>

namespace ShaderRecompiler {

std::uint32_t EmitBinaryU32(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t lhs, std::uint32_t rhs);
std::uint32_t EmitShaderDataDwordLoad(SpirvEmitterState& state, std::uint32_t dwordIndex);
std::uint32_t EmitShaderDataWordLoad(SpirvEmitterState& state, std::uint32_t index);
std::uint32_t ComposeAddress64(SpirvEmitterState& state, const std::function<std::uint32_t()>& loadLow, const std::function<std::uint32_t()>& loadHigh);
std::uint32_t EmitAlignedLoadU32(SpirvEmitterState& state, std::uint32_t pointer);
std::uint32_t EmitRootItemDword(SpirvEmitterState& state, RuntimeAbi::RootItem item, std::uint32_t word);
std::uint32_t EmitBufferRootDword(SpirvEmitterState& state, std::uint32_t buffer, std::uint32_t word);
std::uint32_t EmitRootAddressWordPointer(SpirvEmitterState& state, RuntimeAbi::RootItem item, std::uint32_t index);
std::uint32_t EmitRootAddressReadOnlyWordPointer(SpirvEmitterState& state, RuntimeAbi::RootItem item, std::uint32_t index);
std::uint32_t EmitRootAddress(SpirvEmitterState& state, RuntimeAbi::RootItem item);
std::uint32_t EmitStageRingWordPointer(SpirvEmitterState& state, RuntimeAbi::RootItem item, std::uint32_t index);
std::uint32_t EmitFlattenedSrtLoad(SpirvEmitterState& state, std::uint32_t index);
bool HasFlattenedSrt(const SpirvEmitterState& state);
bool SplitUserData(const SpirvEmitterState& state);
void EmitMemoryOffsets(SpirvEmitterState& state);
std::uint32_t LoadTableMap(SpirvEmitterState& state, std::uint32_t index);
std::uint32_t LoadHeapSlotWord(SpirvEmitterState& state, std::uint32_t dword);
std::uint32_t LdsDwordCount(const SpirvEmitterState& state);
bool LdsInDeviceMemory(const SpirvEmitterState& state);
std::uint32_t LdsMemorySemantics(const SpirvEmitterState& state);
std::uint32_t LdsMemoryBarrierScope(const SpirvEmitterState& state);
void EmitLdsBufferBase(SpirvEmitterState& state);
bool ScratchPoolActive(const SpirvEmitterState& state);
bool GpuScratchPool(const SpirvEmitterState& state);
std::uint32_t ScratchPoolSlotDwords(const SpirvEmitterState& state);
void DefineScratchPool(SpirvEmitterState& state);
void EmitScratchPoolAcquire(SpirvEmitterState& state);
std::uint32_t EmitScratchPoolElementPointer(SpirvEmitterState& state, std::uint32_t index);
void EmitReturnReleasingScratch(SpirvEmitterState& state);
void VerifyScratchPoolReturns(const SpirvEmitterState& state, const std::vector<std::uint32_t>& words);
std::uint32_t EmitLdsLockPointer(SpirvEmitterState& state);
std::uint32_t EmitLdsBufferU64Pointer(SpirvEmitterState& state, std::uint32_t index);
MemoryResourceAccess PrepareMemoryResourceAccess(SpirvEmitterState& state, const MemoryInfo& mem);
std::uint32_t EmitMemoryElementIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t rawIndex);
std::uint32_t EmitMemoryElementInBounds(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index);
bool InFunctionStorage(const SpirvEmitterState& state, ResourceKind kind);
std::uint32_t EmitMemoryElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index);
std::uint32_t EmitStorageBufferElementPointer(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t index, std::uint32_t pointerType);

}

#endif
