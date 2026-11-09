#ifndef CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVATOMICS_HPP
#define CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVATOMICS_HPP

#include "SpirvBackend/SpirvMemory/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

void EmitDeviceAtomicMemoryBarrier(SpirvEmitterState& state);
std::uint32_t EmitShaderDataDwordLoad(SpirvEmitterState& state, std::uint32_t dwordIndex);

}

#endif
