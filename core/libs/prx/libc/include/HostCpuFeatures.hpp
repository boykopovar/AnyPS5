#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTCPUFEATURES_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTCPUFEATURES_HPP

#include <cstdint>

namespace HostCpuFeatureBits {

inline constexpr std::uint64_t Lzcnt = 1ull << 0u;
inline constexpr std::uint64_t Bmi1 = 1ull << 1u;
inline constexpr std::uint64_t Bmi2 = 1ull << 2u;
inline constexpr std::uint64_t Movbe = 1ull << 3u;
inline constexpr std::uint64_t Avx2 = 1ull << 4u;
inline constexpr std::uint64_t Fma = 1ull << 5u;
inline constexpr std::uint64_t F16c = 1ull << 6u;
inline constexpr std::uint64_t All = Lzcnt | Bmi1 | Bmi2 | Movbe | Avx2 | Fma | F16c;

inline constexpr std::uint64_t RecordMagic = 0x3150434835535041ull;
inline constexpr std::uint64_t RecordSize = 16;

}

#endif
