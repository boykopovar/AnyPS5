#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAVECTOROPDECODER_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAVECTOROPDECODER_HPP

#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <span>

namespace ShaderRecompiler {

[[nodiscard]] RdnaInstruction DecodeRdnaVectorOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex);

RdnaInstruction DecodeRdnaVop1(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaVop2(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaVop3(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaVop3p(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaVopc(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaVintrp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);

}

#endif
