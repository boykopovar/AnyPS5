/* $OpenBSD: evp_cipher.c,v 1.23 2024/04/10 15:00:38 beck Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 2015 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
// Derived from LibreSSL 4.0.0 src/lib/libcrypto/evp/evp_cipher.c: https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_cipher.c

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint64_t CipherModeCcm = 0x7;
constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int BadBlockLength = 136;
constexpr int TooLarge = 164;

const LibreSslEvpCipher& Checked(const LibreSslEvpCipherCtx* context, const int* written, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    if (!context->cipher) throw std::invalid_argument(std::string(function) + ": no cipher set");
    if (!written) throw std::invalid_argument(std::string(function) + ": null output length");
    return *context->cipher;
}

int Mask(int blockSize) {
    return static_cast<int>(static_cast<unsigned>(blockSize) - 1u);
}

void Copy(unsigned char* destination, const unsigned char* source, int length, const char* function) {
    if (length <= 0) return;
    if (!destination || !source) throw std::invalid_argument(std::string(function) + ": null buffer");
    std::memcpy(destination, source, static_cast<std::size_t>(length));
}

int Run(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const auto& cipher = *context->cipher;
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    if (cipher.flags & CipherFlagCustomCipher) {
        const int produced = cipher.doCipher(context, out, in, static_cast<std::size_t>(length));
        if (produced < 0) return 0;
        *written = produced;
        return 1;
    }
    if (length > 0 && (!out || !in)) throw std::invalid_argument(std::string(function) + ": null buffer");
    if (!cipher.doCipher(context, out, in, static_cast<std::size_t>(length))) return 0;
    *written = length;
    return 1;
}

int Encrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    int partial = context->bufLen;
    int total = 0;
    *written = 0;
    if ((blockSize & blockMask) != 0) return 0;
    if (length < 0) return 0;
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return 1;
    if (cipher.flags & CipherFlagCustomCipher) return Run(context, out, written, in, length, function);
    if (partial == 0 && (length & blockMask) == 0) return Run(context, out, written, in, length, function);
    if (partial < 0 || partial >= blockSize || static_cast<std::size_t>(blockSize) > sizeof(context->buf)) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, BadBlockLength, __FILE__, __LINE__);
        return 0;
    }
    if (partial > 0) {
        const int needed = blockSize - partial;
        if (needed > length) {
            Copy(context->buf + partial, in, length, function);
            context->bufLen += length;
            return 1;
        }
        if (((length - needed) & ~blockMask) > INT_MAX - blockSize) {
            LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
            return 0;
        }
        Copy(context->buf + partial, in, needed, function);
        int produced = 0;
        if (!Run(context, out, &produced, context->buf, blockSize, function)) return 0;
        total = produced;
        length -= needed;
        in += needed;
        out += produced;
    }
    partial = length & blockMask;
    length -= partial;
    if (length > 0) {
        if (INT_MAX - length < total) return 0;
        int produced = 0;
        if (!Run(context, out, &produced, in, length, function)) return 0;
        if (INT_MAX - produced < total) return 0;
        total += produced;
    }
    Copy(context->buf, in + length, partial, function);
    context->bufLen = partial;
    *written = total;
    return 1;
}

int Decrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    int total = 0;
    *written = 0;
    if ((blockSize & blockMask) != 0) return 0;
    if (length < 0) return 0;
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return 1;
    if (cipher.flags & CipherFlagCustomCipher) return Run(context, out, written, in, length, function);
    if (context->flags & CipherFlagNoPadding) return Encrypt(context, out, written, in, length, function);
    if (static_cast<std::size_t>(blockSize) > sizeof(context->final)) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, BadBlockLength, __FILE__, __LINE__);
        return 0;
    }
    if (context->finalUsed) {
        if ((length & ~blockMask) > INT_MAX - blockSize) {
            LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
            return 0;
        }
        Copy(out, context->final, blockSize, function);
        out += blockSize;
        total = blockSize;
    }
    context->finalUsed = 0;
    int produced = 0;
    if (!Encrypt(context, out, &produced, in, length, function)) return 0;
    if (blockSize > 1 && context->bufLen == 0) {
        if (produced < blockSize) return 0;
        produced -= blockSize;
        Copy(context->final, out + produced, blockSize, function);
        context->finalUsed = 1;
    }
    if (produced > INT_MAX - total) return 0;
    total += produced;
    *written = total;
    return 1;
}

}

extern "C" {

int APS5_VABI EVP_EncryptUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    return Encrypt(context, out, written, in, length, __func__);
}

int APS5_VABI EVP_DecryptUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    return Decrypt(context, out, written, in, length, __func__);
}

int APS5_VABI EVP_CipherUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    if (!context) throw std::invalid_argument(std::string(__func__) + ": null context");
    return context->encrypt ? Encrypt(context, out, written, in, length, __func__) : Decrypt(context, out, written, in, length, __func__);
}

}
