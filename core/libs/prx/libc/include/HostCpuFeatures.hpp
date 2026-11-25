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
inline constexpr std::uint64_t Rdrand = 1ull << 7u;
inline constexpr std::uint64_t Rdseed = 1ull << 8u;
inline constexpr std::uint64_t Adx = 1ull << 9u;
inline constexpr std::uint64_t Sha = 1ull << 10u;
inline constexpr std::uint64_t Clflushopt = 1ull << 11u;
inline constexpr std::uint64_t Clwb = 1ull << 12u;
inline constexpr std::uint64_t Rdpid = 1ull << 13u;
inline constexpr std::uint64_t Xsaveopt = 1ull << 14u;
inline constexpr std::uint64_t Xsavec = 1ull << 15u;
inline constexpr std::uint64_t Xsaves = 1ull << 16u;
inline constexpr std::uint64_t Aes = 1ull << 17u;
inline constexpr std::uint64_t Pclmul = 1ull << 18u;
inline constexpr std::uint64_t Sse4a = 1ull << 19u;
inline constexpr std::uint64_t Clzero = 1ull << 20u;
inline constexpr std::uint64_t Rdtscp = 1ull << 21u;
inline constexpr std::uint64_t Fsgsbase = 1ull << 22u;
inline constexpr std::uint64_t AvxVnni = 1ull << 23u;
inline constexpr std::uint64_t AvxIfma = 1ull << 24u;
inline constexpr std::uint64_t AvxVnniInt8 = 1ull << 25u;
inline constexpr std::uint64_t AvxNeConvert = 1ull << 26u;
inline constexpr std::uint64_t All = Lzcnt | Bmi1 | Bmi2 | Movbe | Avx2 | Fma | F16c | Rdrand | Rdseed | Adx | Sha |
                                   Clflushopt | Clwb | Rdpid | Xsaveopt | Xsavec | Xsaves | Aes | Pclmul | Sse4a |
                                   Clzero | Rdtscp | Fsgsbase | AvxVnni | AvxIfma | AvxVnniInt8 | AvxNeConvert;

inline constexpr std::uint64_t RecordMagic = 0x3150434835535041ull;
inline constexpr std::uint64_t RecordSize = 16;

}

#endif
