#ifndef CORE_SHADER_RECOMPILER_WGSLBACKEND_INCLUDE_WGSLBACKEND_WGSLTARGET_HPP
#define CORE_SHADER_RECOMPILER_WGSLBACKEND_INCLUDE_WGSLBACKEND_WGSLTARGET_HPP

#include "Recompiler.hpp"

namespace ShaderRecompiler {

[[nodiscard]] const SpirvTarget& WebGpuTarget();

}  // namespace ShaderRecompiler

#endif
