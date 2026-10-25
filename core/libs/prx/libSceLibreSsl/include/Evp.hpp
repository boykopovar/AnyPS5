#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_EVP_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_EVP_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl/include/Api.hpp"

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
