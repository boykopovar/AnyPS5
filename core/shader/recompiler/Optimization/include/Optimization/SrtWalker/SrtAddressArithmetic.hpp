#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTADDRESSARITHMETIC_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTADDRESSARITHMETIC_HPP

#include <cstdint>

namespace ShaderRecompiler::Detail {

inline constexpr std::uint64_t AddressMask = 0x0000ffffffffffffull;

bool AddSignedAddress(std::uint64_t base, std::int64_t offset, std::uint64_t& result);

}

#endif
