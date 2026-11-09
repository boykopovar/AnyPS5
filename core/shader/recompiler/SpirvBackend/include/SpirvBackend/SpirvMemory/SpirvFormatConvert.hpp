#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVFORMATCONVERT_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVFORMATCONVERT_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t EmitTBufferBitcastU32ToI32(SpirvEmitterState& state, std::uint32_t value);
bool IsSignedFormatComponent(SpirvFormatComponentType type);
std::uint32_t EmitUFloatToF32Bits(SpirvEmitterState& state, std::uint32_t raw, std::uint32_t bits);
std::uint32_t NormalizeFormatComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t raw);
void EmitDeviceAtomicMemoryBarrier(SpirvEmitterState& state);
std::uint32_t EmitDsSwizzleTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t control);

}

#endif
