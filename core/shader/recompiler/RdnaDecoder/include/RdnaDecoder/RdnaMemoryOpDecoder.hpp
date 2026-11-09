#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAMEMORYOPDECODER_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAMEMORYOPDECODER_HPP

#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <span>

namespace ShaderRecompiler {

[[nodiscard]] RdnaInstruction DecodeRdnaMemoryOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex);

RdnaInstruction DecodeRdnaSmem(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaMubuf(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaMtbuf(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaFlat(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaDs(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);

}

#endif
