#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAIMAGEOPDECODER_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAIMAGEOPDECODER_HPP

#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <span>

namespace ShaderRecompiler {

struct RdnaImageAddressComponent {
    std::uint32_t bitOffset;
    std::uint32_t bitWidth;
};

    [[nodiscard]] RdnaInstruction DecodeRdnaImageOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex);

RdnaInstruction DecodeRdnaMimg(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex);
const char* GetRdnaImageSampleOpcodeName(std::uint32_t opcode);
RdnaImageAddressComponent GetRdnaImageAddressComponentLayout(std::uint32_t flags, std::uint32_t component);
std::uint32_t GetRdnaImageAddressDwordCount(std::uint32_t flags, std::uint32_t components);

}

#endif
