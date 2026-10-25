#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_EVP_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_EVP_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl/include/Api.hpp"

inline constexpr std::uint64_t CipherModeMask = 0xF0007;
inline constexpr std::uint64_t CipherModeStream = 0;
inline constexpr std::uint64_t CipherModeEcb = 1;
inline constexpr std::uint64_t CipherModeCbc = 2;
inline constexpr std::uint64_t CipherModeCfb = 3;
inline constexpr std::uint64_t CipherModeOfb = 4;
inline constexpr std::uint64_t CipherModeCtr = 5;
inline constexpr std::uint64_t CipherFlagCustomIv = 0x10;
inline constexpr std::uint64_t CipherFlagAlwaysCallInit = 0x20;
inline constexpr std::uint64_t CipherFlagCtrlInit = 0x40;
inline constexpr std::uint64_t CipherFlagNoPadding = 0x100;
inline constexpr std::uint64_t CipherFlagCustomCipher = 0x100000;
inline constexpr int CipherControlInit = 0;

static_assert(sizeof(LibreSslEvpCipher) == 88);
static_assert(offsetof(LibreSslEvpCipher, flags) == 16);
static_assert(offsetof(LibreSslEvpCipher, init) == 24);
static_assert(offsetof(LibreSslEvpCipher, contextSize) == 48);
static_assert(offsetof(LibreSslEvpCipher, control) == 72);
static_assert(sizeof(LibreSslEvpCipherCtx) == 168);
static_assert(offsetof(LibreSslEvpCipherCtx, num) == 88);
static_assert(offsetof(LibreSslEvpCipherCtx, flags) == 112);
static_assert(offsetof(LibreSslEvpCipherCtx, final) == 136);

#endif
