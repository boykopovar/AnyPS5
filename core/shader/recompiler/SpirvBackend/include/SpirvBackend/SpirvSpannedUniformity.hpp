#ifndef CORE_SHADER_RECOMPILER_SPIRVSPANNEDUNIFORMITY_HPP
#define CORE_SHADER_RECOMPILER_SPIRVSPANNEDUNIFORMITY_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ShaderRecompiler {

[[nodiscard]] std::optional<std::string> SpannedBarrierInDivergentFlow(std::span<const std::uint32_t> words);

}

#endif
