#ifndef CORE_SHADER_RECOMPILER_SPIRVSPECIALIZATION_HPP
#define CORE_SHADER_RECOMPILER_SPIRVSPECIALIZATION_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace ShaderRecompiler {

[[nodiscard]] std::vector<std::uint32_t> SpecializeSpirv(std::span<const std::uint32_t> words);

}

#endif
