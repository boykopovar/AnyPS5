#pragma once

#include <cstddef>
#include <cstdint>

struct LibreSsl3EvpCipher {
    std::int32_t nid;
    std::int32_t blockSize;
    std::int32_t keyLength;
    std::int32_t ivLength;
    std::uint64_t flags;
    const void* init;
    const void* doCipher;
    const void* cleanup;
    std::int32_t contextSize;
    const void* setAsn1Parameters;
    const void* getAsn1Parameters;
    const void* control;
};

static_assert(sizeof(LibreSsl3EvpCipher) == 80);
static_assert(offsetof(LibreSsl3EvpCipher, keyLength) == 8);
static_assert(offsetof(LibreSsl3EvpCipher, ivLength) == 12);
static_assert(offsetof(LibreSsl3EvpCipher, contextSize) == 48);
