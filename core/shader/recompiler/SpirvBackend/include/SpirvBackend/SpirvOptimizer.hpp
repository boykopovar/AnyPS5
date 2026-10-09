#ifndef CORE_SHADER_RECOMPILER_SPIRVBACKEND_SPIRVOPTIMIZER_HPP
#define CORE_SHADER_RECOMPILER_SPIRVBACKEND_SPIRVOPTIMIZER_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace ShaderRecompiler {

[[nodiscard]] std::vector<std::uint32_t> ValidateAndOptimizeSpirv(std::span<const std::uint32_t> spirv, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, bool allowOffsetTextureOperand = false, bool optimize = true, bool specialize = false);

}

#endif
