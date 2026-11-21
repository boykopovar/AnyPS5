/* $OpenBSD: hkdf.c,v 1.11 2024/03/25 13:09:13 jsing Exp $ */
/*
 * Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr int CryptoLibrary = 15;
constexpr int AnyFunction = 0xfff;
constexpr int CryptoLibraryReason = 15;
constexpr int TooLargeReason = 164;
constexpr std::size_t MaxDigestSize = 64;

void Cleanse(unsigned char* data, std::size_t size) {
    volatile unsigned char* bytes = data;
    for (std::size_t i = 0; i < size; ++i) bytes[i] = 0;
}

int Extract(std::uint8_t* prk, std::size_t* prkLength, const EvpMd* digest, const std::uint8_t* secret, std::size_t secretLength, const std::uint8_t* salt, std::size_t saltLength) {
    const unsigned char emptyKey[1] {};
    const void* key = salt;
    int keyLength = static_cast<int>(saltLength);
    if (!key) {
        key = emptyKey;
        keyLength = 0;
    }
    std::uint32_t length = 0;
    if (!HMAC_nid_postfix(digest, key, keyLength, secret, secretLength, prk, &length)) {
        LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, CryptoLibraryReason);
        return 0;
    }
    *prkLength = length;
    return 1;
}

int Expand(std::uint8_t* out, std::size_t outLength, const EvpMd* digest, const std::uint8_t* prk, std::size_t prkLength, const std::uint8_t* info, std::size_t infoLength) {
    if (!out && outLength) APS5_INVALID_ARG_EX;
    const auto digestLength = static_cast<std::size_t>(EVP_MD_size_nid_postfix(digest));
    unsigned char block[MaxDigestSize];
    int result = 0;
    HmacContext hmac;
    HMAC_CTX_init_nid_postfix(&hmac);
    if (HMAC_Init_ex_nid_postfix(&hmac, prk, static_cast<int>(prkLength), digest, nullptr)) {
        std::size_t remaining = outLength;
        std::uint8_t counter = 0;
        result = 1;
        while (remaining > 0) {
            if (++counter == 0) {
                LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, TooLargeReason);
                result = 0;
                break;
            }
            if (!HMAC_Update_nid_postfix(&hmac, info, infoLength) || !HMAC_Update_nid_postfix(&hmac, &counter, 1) || !HMAC_Final_nid_postfix(&hmac, block, nullptr)) {
                result = 0;
                break;
            }
            const std::size_t count = remaining > digestLength ? digestLength : remaining;
            std::memcpy(out + (outLength - remaining), block, count);
            remaining -= count;
            if (remaining > 0 && (!HMAC_Init_ex_nid_postfix(&hmac, nullptr, 0, nullptr, nullptr) || !HMAC_Update_nid_postfix(&hmac, block, digestLength))) {
                result = 0;
                break;
            }
        }
    }
    HMAC_CTX_cleanup_nid_postfix(&hmac);
    Cleanse(block, sizeof(block));
    return result;
}

}

extern "C" {

int APS5_VABI HKDF_nid_postfix(std::uint8_t* outKey, std::size_t outLength, const EvpMd* digest, const std::uint8_t* secret, std::size_t secretLength, const std::uint8_t* salt, std::size_t saltLength, const std::uint8_t* info, std::size_t infoLength) {
    std::uint8_t prk[MaxDigestSize];
    std::size_t prkLength = 0;
    if (!Extract(prk, &prkLength, digest, secret, secretLength, salt, saltLength)) return 0;
    return Expand(outKey, outLength, digest, prk, prkLength, info, infoLength);
}

}
