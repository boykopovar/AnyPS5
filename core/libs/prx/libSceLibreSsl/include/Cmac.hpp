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

extern "C" {
LibreSslCmac* APS5_VABI CMAC_CTX_new_nid_postfix();
void APS5_VABI CMAC_CTX_free_nid_postfix(LibreSslCmac* context);
int APS5_VABI CMAC_Init_nid_postfix(LibreSslCmac* context, const void* key, std::size_t keyLength, const LibreSslCipher* cipher, void* engine);
int APS5_VABI CMAC_Update_nid_postfix(LibreSslCmac* context, const void* data, std::size_t length);
int APS5_VABI CMAC_Final_nid_postfix(LibreSslCmac* context, unsigned char* output, std::size_t* length);
const LibreSslCipher* APS5_VABI EVP_aes_128_cbc_nid_postfix();
const LibreSslCipher* APS5_VABI EVP_aes_192_cbc_nid_postfix();
const LibreSslCipher* APS5_VABI EVP_aes_256_cbc_nid_postfix();
}
