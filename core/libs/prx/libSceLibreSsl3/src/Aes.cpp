/* $OpenBSD: aes.c,v 1.4 2024/08/11 13:02:39 jsing Exp $ */
/* ====================================================================
 * Copyright (c) 2002-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 */

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstring>
#include <stdexcept>

namespace {

constexpr unsigned char DefaultIv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

}

extern "C" {

int APS5_VABI AES_unwrap_key_nid_postfix(AesKey* key, const unsigned char* iv, unsigned char* out, const unsigned char* in, unsigned int length) {
    if ((length & 0x7) || length < 24)
        return -1;
    if (!key || !out || !in)
        throw std::invalid_argument("AES_unwrap_key requires a key, input and output");
    length -= 8;
    unsigned char block[16];
    unsigned int step = 6 * (length >> 3);
    std::memcpy(block, in, 8);
    std::memmove(out, in + 8, length);
    for (unsigned int round = 0; round < 6; ++round) {
        unsigned char* chunk = out + length - 8;
        for (unsigned int offset = 0; offset < length; offset += 8, --step, chunk -= 8) {
            block[7] ^= static_cast<unsigned char>(step & 0xff);
            if (step > 0xff) {
                block[6] ^= static_cast<unsigned char>((step >> 8) & 0xff);
                block[5] ^= static_cast<unsigned char>((step >> 16) & 0xff);
                block[4] ^= static_cast<unsigned char>((step >> 24) & 0xff);
            }
            std::memcpy(block + 8, chunk, 8);
            AES_decrypt_nid_postfix(block, block, key);
            std::memcpy(chunk, block + 8, 8);
        }
    }
    if (!iv)
        iv = DefaultIv;
    if (std::memcmp(block, iv, 8)) {
        volatile unsigned char* cleared = out;
        for (unsigned int index = 0; index < length; ++index) cleared[index] = 0;
        return 0;
    }
    return static_cast<int>(length);
}

}
