#ifndef CORE_SHADER_RECOMPILER_BUFFERTABLEABI_HPP
#define CORE_SHADER_RECOMPILER_BUFFERTABLEABI_HPP

#include <cstdint>

namespace ShaderRecompiler::BufferTableAbi {

inline constexpr std::uint32_t BufferRecordFlags = 0;
inline constexpr std::uint32_t BufferRecordElement = 1;
inline constexpr std::uint32_t BufferRecordWord0 = 2;
inline constexpr std::uint32_t BufferRecordWord1 = 3;
inline constexpr std::uint32_t BufferRecordWord2 = 4;
inline constexpr std::uint32_t BufferRecordWord3 = 5;
inline constexpr std::uint32_t BufferRecordWords = 6;

inline constexpr std::uint32_t MaxBufferElements = 64;
inline constexpr std::uint32_t BdaFallbackFlag = 0x40000000u;
inline constexpr std::uint32_t ByteOffsetMask = 0x3fffffffu;
inline constexpr std::uint64_t MaxDirectBufferBytes = (1ull << 27u) - 256u;

}

#endif
