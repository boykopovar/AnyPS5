#ifndef CORE_SHADER_RECOMPILER_SPIRVBACKEND_SPIRVRUNTIMEBUFFER_HPP
#define CORE_SHADER_RECOMPILER_SPIRVBACKEND_SPIRVRUNTIMEBUFFER_HPP

#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include <functional>

namespace ShaderRecompiler {

std::uint32_t EmitRuntimeBufferLoad(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t components);
void EmitRuntimeBufferStore(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t components);
std::uint32_t EmitRuntimeScalarBufferLoad(SpirvValueEmitContext& context, const IrValue& instruction);
std::uint32_t EmitRuntimeBufferAtomic(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t bits, std::uint32_t active, const std::function<std::uint32_t(std::uint32_t)>& operation);

}

#endif
