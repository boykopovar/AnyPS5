#ifndef CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVFORMATCONVERSION_HPP
#define CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVFORMATCONVERSION_HPP

#include "SpirvBackend/SpirvMemory/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

bool IsSignedFormatComponent(SpirvFormatComponentType type);
std::uint32_t EmitUFloatToF32Bits(SpirvEmitterState& state, std::uint32_t raw, std::uint32_t bits);
std::uint32_t NormalizeFormatComponent(SpirvEmitterState& state, const SpirvBufferFormatInfo& info, std::uint32_t component, std::uint32_t raw);
std::uint32_t EmitTBufferBitcastU32ToI32(SpirvEmitterState& state, std::uint32_t value);

}

#endif
