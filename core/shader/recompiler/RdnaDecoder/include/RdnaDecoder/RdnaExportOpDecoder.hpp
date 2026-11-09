#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAEXPORTOPDECODER_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAEXPORTOPDECODER_HPP

#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <span>

namespace ShaderRecompiler {

[[nodiscard]] RdnaInstruction DecodeRdnaExportOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex);

[[nodiscard]] RdnaInstruction DecodeRdnaExportOp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);

}

#endif
