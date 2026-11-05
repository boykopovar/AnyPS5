#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNASCALAROPDECODER_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNASCALAROPDECODER_HPP

#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <span>

namespace ShaderRecompiler {

[[nodiscard]] RdnaInstruction DecodeRdnaScalarOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex);

RdnaInstruction DecodeRdnaSop1(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaSop2(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaSopk(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaSopc(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
RdnaInstruction DecodeRdnaSopp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);

}

#endif
