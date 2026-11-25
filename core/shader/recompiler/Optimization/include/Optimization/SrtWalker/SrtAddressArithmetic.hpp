#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTADDRESSARITHMETIC_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTADDRESSARITHMETIC_HPP

#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <cstdint>

namespace ShaderRecompiler::Detail {

inline constexpr std::uint64_t AddressMask = 0x0000ffffffffffffull;

bool AddSignedAddress(std::uint64_t base, std::int64_t offset, std::uint64_t& result);

inline std::uint64_t ConstBufferBase(std::uint64_t word0, std::uint64_t word1) {
    const auto high = static_cast<std::uint32_t>(word1) & (GcnGuestShaders() ? GcnBufferWord1Mask : 0xffffffffu);
    return ((static_cast<std::uint64_t>(high) << 32u) | static_cast<std::uint32_t>(word0)) & AddressMask;
}

}

#endif
