#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVTYPES_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVTYPES_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t TypeVoid(SpirvEmitterState& state);
std::uint32_t TypeBool(SpirvEmitterState& state);
std::uint32_t TypeBoolVector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeU8(SpirvEmitterState& state);
std::uint32_t TypeU32(SpirvEmitterState& state);
std::uint32_t TypeU64(SpirvEmitterState& state);
std::uint32_t TypeScalarU64(SpirvEmitterState& state);
std::uint32_t TypeU32Pair(SpirvEmitterState& state);
std::uint32_t TypeI32(SpirvEmitterState& state);
std::uint32_t TypeI32Pair(SpirvEmitterState& state);
std::uint32_t TypeF32(SpirvEmitterState& state);
std::uint32_t TypeU32Vector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeU32Composite(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeI32Vector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypeF32Vector(SpirvEmitterState& state, std::uint32_t components);
std::uint32_t TypePointer(SpirvEmitterState& state, std::uint32_t storageClass, std::uint32_t pointee);
std::uint32_t TypeFunction(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferPointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferElementPointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU64Pointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU64ElementPointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU8Block(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU8Pointer(SpirvEmitterState& state);
std::uint32_t TypeStorageBufferU8ElementPointer(SpirvEmitterState& state);
std::uint32_t TypePhysicalU32Pointer(SpirvEmitterState& state);
std::uint32_t TypePushConstantElementPointer(SpirvEmitterState& state);
std::uint32_t TypeU32ArrayPointer(SpirvEmitterState& state, std::uint32_t storageClass, std::uint32_t dwords);
std::uint32_t TypeU32ElementPointer(SpirvEmitterState& state, std::uint32_t storageClass);
std::uint32_t TypeId(SpirvEmitterState& state, IrType type);
std::uint32_t GlslStd450(SpirvEmitterState& state);

}

#endif
