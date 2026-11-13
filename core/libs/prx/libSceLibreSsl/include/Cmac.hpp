#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

struct LibreSslCipherContext;

struct LibreSslCipher {
    std::int32_t nid;
    std::int32_t blockSize;
    std::int32_t keyLength;
    std::int32_t ivLength;
    std::uint64_t flags;
    int (APS5_VABI* init)(LibreSslCipherContext*, const unsigned char*, const unsigned char*, int);
    int (APS5_VABI* cipher)(LibreSslCipherContext*, unsigned char*, const unsigned char*, std::size_t);
    int (APS5_VABI* cleanup)(LibreSslCipherContext*);
    std::int32_t contextSize;
    int (APS5_VABI* setAsn1Parameters)(LibreSslCipherContext*, void*);
    int (APS5_VABI* getAsn1Parameters)(LibreSslCipherContext*, void*);
    int (APS5_VABI* control)(LibreSslCipherContext*, int, int, void*);
    void* applicationData;
};

struct LibreSslCipherContext {
    const LibreSslCipher* cipher;
    void* engine;
    std::int32_t encrypt;
    std::int32_t bufferLength;
    unsigned char originalIv[16];
    unsigned char iv[16];
    unsigned char buffer[32];
    std::int32_t position;
    void* applicationData;
    std::int32_t keyLength;
    std::uint64_t flags;
    void* cipherData;
    std::int32_t finalUsed;
    std::int32_t blockMask;
    unsigned char final[32];
};

static_assert(sizeof(LibreSslCipher) == 88 && offsetof(LibreSslCipher, init) == 24 && offsetof(LibreSslCipher, contextSize) == 48);
static_assert(sizeof(LibreSslCipherContext) == 168 && offsetof(LibreSslCipherContext, iv) == 40 && offsetof(LibreSslCipherContext, flags) == 112 && offsetof(LibreSslCipherContext, cipherData) == 120);

struct LibreSslCmac;
