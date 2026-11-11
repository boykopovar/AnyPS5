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


struct LibreSsl3Cipher {
    std::int32_t valid;
    const char* name;
    std::uint64_t id;
    std::uint64_t keyExchange;
    std::uint64_t authentication;
    std::uint64_t encryption;
    std::uint64_t mac;
    std::uint64_t protocol;
    std::uint64_t strengthFlags;
    std::uint64_t extraFlags;
    std::int32_t strengthBits;
    std::int32_t algorithmBits;
};

static_assert(sizeof(LibreSsl3Cipher) == 88);
static_assert(offsetof(LibreSsl3Cipher, name) == 8);
static_assert(offsetof(LibreSsl3Cipher, id) == 16);
static_assert(offsetof(LibreSsl3Cipher, strengthBits) == 80);
static_assert(offsetof(LibreSsl3Cipher, algorithmBits) == 84);
