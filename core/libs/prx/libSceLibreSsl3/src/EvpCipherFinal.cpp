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

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int BadDecrypt = 100;
constexpr int WrongFinalBlockLength = 109;
constexpr int BadBlockLength = 136;
constexpr int DataNotMultipleOfBlockLength = 138;

const LibreSslEvpCipher& Checked(const LibreSslEvpCipherCtx* context, const int* written, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    if (!context->cipher) throw std::invalid_argument(std::string(function) + ": no cipher set");
    if (!written) throw std::invalid_argument(std::string(function) + ": null output length");
    return *context->cipher;
}

int Fail(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason, __FILE__, __LINE__);
    return 0;
}

int Custom(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const char* function) {
    const auto& cipher = *context->cipher;
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    const int produced = cipher.doCipher(context, out, nullptr, 0);
    if (produced < 0) return 0;
    *written = produced;
    return 1;
}

int Encrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int partial = context->bufLen;
    *written = 0;
    if (cipher.flags & CipherFlagCustomCipher) return Custom(context, out, written, function);
    if (partial < 0 || partial >= blockSize || static_cast<std::size_t>(blockSize) > sizeof(context->buf)) return Fail(BadBlockLength);
    if (blockSize == 1) return 1;
    if (context->flags & CipherFlagNoPadding) return partial != 0 ? Fail(DataNotMultipleOfBlockLength) : 1;
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    if (!out) throw std::invalid_argument(std::string(function) + ": null output");
    const int pad = blockSize - partial;
    std::memset(context->buf + partial, pad, static_cast<std::size_t>(pad));
    if (!cipher.doCipher(context, out, context->buf, static_cast<std::size_t>(blockSize))) return 0;
    *written = blockSize;
    return 1;
}

int Decrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int partial = context->bufLen;
    *written = 0;
    if (cipher.flags & CipherFlagCustomCipher) return Custom(context, out, written, function);
    if (context->flags & CipherFlagNoPadding) return partial != 0 ? Fail(DataNotMultipleOfBlockLength) : 1;
    if (blockSize == 1) return 1;
    if (partial != 0 || !context->finalUsed) return Fail(WrongFinalBlockLength);
    if (static_cast<std::size_t>(blockSize) > sizeof(context->final)) return Fail(BadBlockLength);
    if (blockSize <= 0) throw std::invalid_argument(std::string(function) + ": held block with block size " + std::to_string(blockSize));
    const int pad = context->final[blockSize - 1];
    if (pad <= 0 || pad > blockSize) return Fail(BadDecrypt);
    const int plain = blockSize - pad;
    for (int index = plain; index < blockSize; ++index)
        if (context->final[index] != pad) return Fail(BadDecrypt);
    if (plain > 0) {
        if (!out) throw std::invalid_argument(std::string(function) + ": null output");
        std::memcpy(out, context->final, static_cast<std::size_t>(plain));
    }
    *written = plain;
    return 1;
}

}

extern "C" {

int APS5_VABI EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written) {
    return Encrypt(context, out, written, __func__);
}

int APS5_VABI EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written) {
    return Decrypt(context, out, written, __func__);
}

int APS5_VABI EVP_CipherFinal_ex_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written) {
    if (!context) throw std::invalid_argument(std::string(__func__) + ": null context");
    return context->encrypt ? Encrypt(context, out, written, __func__) : Decrypt(context, out, written, __func__);
}

}
