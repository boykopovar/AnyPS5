#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVCONSTANTS_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVCONSTANTS_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t ConstantU32(SpirvEmitterState& state, std::uint32_t value);
std::uint32_t ConstantI32(SpirvEmitterState& state, std::int32_t value);
std::uint32_t ConstantF32(SpirvEmitterState& state, std::uint32_t bits);
std::uint32_t FloatBits(float value);
std::uint32_t ConstantF32Value(SpirvEmitterState& state, float value);
std::uint32_t ConstantBool(SpirvEmitterState& state, bool value);
std::uint32_t ConstantU64(SpirvEmitterState& state, std::uint64_t value);
std::uint32_t ConstantU32CompositeZero(SpirvEmitterState& state, std::uint32_t components);

}

#endif
