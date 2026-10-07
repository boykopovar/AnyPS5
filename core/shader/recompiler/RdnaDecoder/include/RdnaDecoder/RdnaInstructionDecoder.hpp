#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAINSTRUCTIONDECODER_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAINSTRUCTIONDECODER_HPP

#include "RdnaDecoder/RdnaProgram.hpp"
#include <cstdint>
#include <span>
#include <string>

namespace ShaderRecompiler {

class RdnaInstructionDecoder {
public:
    [[nodiscard]] RdnaProgram Decode(std::span<const std::uint32_t> code) const;

private:
    [[nodiscard]] RdnaInstruction decodeAt(std::span<const std::uint32_t> code, std::uint32_t wordIndex) const;
};

RdnaInstructionFamily GetRdnaInstructionFamily(std::uint32_t word);
RdnaInstruction DecodeRdnaInstruction(std::uint32_t programCounter, std::span<const std::uint32_t> code,
                                      std::uint32_t wordIndex);
RdnaProgram DecodeRdnaFrontProgram(std::span<const std::uint32_t> front);
void DecodeRdnaProgram(std::span<const std::uint32_t> code, RdnaProgram& program);
RdnaOperand DecodeRdnaScalarSource(std::uint32_t code, std::uint32_t programCounter);
RdnaOperand DecodeRdnaScalarDestination(std::uint32_t code, std::uint32_t programCounter);
RdnaOperand DecodeRdnaVectorGpr(std::uint32_t reg);
void ReadRdnaLiteralOperands(std::span<const std::uint32_t> code, std::uint32_t wordIndex,
                             RdnaInstruction& instruction);
void SetRdnaRawWords(RdnaInstruction& instruction, std::span<const std::uint32_t> code, std::uint32_t wordIndex,
                     std::uint32_t wordCount);
void SetRdnaUnsupported(RdnaInstruction& instruction, RdnaInstructionFamily family, std::uint32_t opcodeId,
                        const char* reason);
std::string RdnaOperandToString(const RdnaOperand& operand);
const char* RdnaImageDimensionToString(RdnaImageDimension dimension);
std::string RdnaInstructionToString(const RdnaInstruction& instruction);
std::string RdnaProgramToString(const RdnaProgram& program);

}

#endif
