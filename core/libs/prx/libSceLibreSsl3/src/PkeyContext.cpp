/* $OpenBSD: rsa_pmeth.c,v 1.41 2024/08/26 22:01:28 op Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: pmeth_lib.c,v 1.41 2024/07/09 17:02:29 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: pmeth_fn.c,v 1.11 2024/04/12 09:41:39 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: m_sigver.c,v 1.27 2024/04/09 13:52:41 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006,2007 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: p_sign.c,v 1.22 2024/04/09 13:52:41 beck Exp $ */
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

/* $OpenBSD: p_verify.c,v 1.21 2024/04/09 13:52:41 beck Exp $ */
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

#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using LibreSsl3::DigestMethod;

constexpr int EvpLibrary = 6;
constexpr int RsaLibrary = 4;
constexpr int AnyFunction = 0xfff;
constexpr int AllocationFailure = 65;

constexpr int ReasonCommandNotSupported = 147;
constexpr int ReasonNoOperationSet = 149;
constexpr int ReasonInvalidOperation = 148;
constexpr int ReasonOperationNotSupported = 150;
constexpr int ReasonOperationNotInitialized = 151;
constexpr int ReasonBufferTooSmall = 155;
constexpr int ReasonUnsupportedAlgorithm = 156;

constexpr int ReasonKeySizeTooSmall = 120;
constexpr int ReasonBadExponentValue = 101;
constexpr int ReasonInvalidPaddingMode = 141;
constexpr int ReasonIllegalPaddingMode = 144;
constexpr int ReasonDigestNotAllowed = 145;
constexpr int ReasonInvalidSaltLength = 146;
constexpr int ReasonKeyTypeOperationNotSupported = 148;
constexpr int ReasonInvalidMaskDigest = 156;
constexpr int ReasonInvalidDigest = 157;
constexpr int ReasonSaltLengthTooSmall = 164;
constexpr int ReasonInvalidDigestLength = 143;
constexpr int ReasonInputNotInitialized = 111;
constexpr int ReasonNoDefaultDigest = 158;

constexpr int Asn1MethodFlags = 4;
constexpr int DefaultDigestNid = 672;
constexpr int DefaultKeyGenerationBits = 2048;
constexpr int NoSaltLengthLimit = -1;

constexpr std::uint64_t DigestCleanedFlag = 2;
constexpr std::uint64_t DigestReuseFlag = 4;
constexpr std::uint64_t DigestKeepKeyContextFlag = 0x400;
constexpr std::uint64_t DigestNoInitFlag = 0x100;

constexpr int Md4Type = 257;
constexpr int Md5Type = 4;
constexpr int Md5Sha1Type = 114;
constexpr int Ripemd160Type = 117;
constexpr int Sha1Type = 64;
constexpr int Sha224Type = 675;
constexpr int Sha256Type = 672;
constexpr int Sha384Type = 673;
constexpr int Sha512Type = 674;

void QueueEvpError(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason);
}

void QueueRsaError(int reason) {
    LibreSslPutError_nid_no_patch(RsaLibrary, AnyFunction, reason);
}

[[noreturn]] void Reject(const std::string& message) {
    throw std::invalid_argument("libSceLibreSsl3 EVP_PKEY_CTX: " + message);
}

const LibreSsl3PkeyMethod* MethodOf(const LibreSsl3PkeyContext* context) {
    return static_cast<const LibreSsl3PkeyMethod*>(context->method);
}

LibreSsl3RsaKeyContext* RsaContextData(const LibreSsl3PkeyContext* context) {
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(context->data);
    if (!keyContext) NotImplemented_nid_no_patch("EVP_PKEY_CTX operation without RSA context data");
    return keyContext;
}

LibreSsl3Rsa* RsaKeyOf(const LibreSsl3PkeyContext* context) {
    const auto* key = static_cast<const LibreSsl3EvpPkey*>(context->key);
    auto* rsa = key ? static_cast<LibreSsl3Rsa*>(key->key) : nullptr;
    if (!rsa) NotImplemented_nid_no_patch("EVP_PKEY_CTX operation without a native RSA key");
    return rsa;
}

bool CheckPaddingDigest(const DigestMethod* digest, int padding) {
    if (!digest) return true;
    if (padding == LibreSsl3RsaPaddingNone) {
        QueueRsaError(ReasonInvalidPaddingMode);
        return false;
    }
    if (padding == LibreSsl3RsaPaddingX931)
        NotImplemented_nid_no_patch("check_padding_md for X9.31 padding");
    switch (digest->type) {
    case Md4Type:
    case Md5Type:
    case Md5Sha1Type:
    case Ripemd160Type:
    case Sha1Type:
    case Sha224Type:
    case Sha256Type:
    case Sha384Type:
    case Sha512Type:
        return true;
    default:
        QueueRsaError(ReasonInvalidDigest);
        return false;
    }
}

bool SetupTemporaryBuffer(LibreSsl3RsaKeyContext* keyContext, const LibreSsl3Rsa* rsa) {
    if (keyContext->temporaryBuffer) return true;
    const int size = LibreSsl3::RsaByteCount_nid_no_patch(rsa);
    if (size <= 0) return false;
    auto* buffer = static_cast<unsigned char*>(calloc_nid_postfix(1, static_cast<std::size_t>(size)));
    if (!buffer) {
        QueueRsaError(AllocationFailure);
        return false;
    }
    keyContext->temporaryBuffer = buffer;
    return true;
}

std::optional<int> CheckAutomaticLength(LibreSsl3PkeyContext* context, const LibreSsl3PkeyMethod* method,
    const unsigned char* output, std::size_t* outputLength, const char* function) {
    if (!outputLength) Reject(std::string(function) + " without an output length");
    if ((method->flags & LibreSsl3PkeyMethodFlagAutomaticArgumentLength) == 0) return std::nullopt;
    const auto keySize = static_cast<std::size_t>(EVP_PKEY_size_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->key)));
    if (!output) {
        *outputLength = keySize;
        return 1;
    }
    if (*outputLength < keySize) {
        QueueEvpError(ReasonBufferTooSmall);
        return 0;
    }
    return std::nullopt;
}

int APS5_VABI RsaSign(LibreSsl3PkeyContext* context, unsigned char* signature, std::size_t* signatureLength,
    const unsigned char* content, std::size_t contentLength) {
    auto* keyContext = RsaContextData(context);
    auto* rsa = RsaKeyOf(context);
    if (keyContext->digest) {
        if (contentLength != static_cast<std::size_t>(keyContext->digest->digestSize)) {
            QueueRsaError(ReasonInvalidDigestLength);
            return -1;
        }
        if (keyContext->padding == LibreSsl3RsaPaddingPkcs1) {
            unsigned int sltmp = 0;
            const int ret = LibreSsl3::RsaSign_nid_no_patch(keyContext->digest->type, content,
                static_cast<unsigned int>(contentLength), signature, &sltmp, rsa);
            if (ret <= 0) return ret;
            *signatureLength = sltmp;
            return 1;
        } else if (keyContext->padding == LibreSsl3RsaPaddingPss) {
            if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
            const auto* maskDigest = keyContext->maskDigest ? keyContext->maskDigest : keyContext->digest;
            if (!LibreSsl3::RsaPaddingAddPkcs1PssMgf1_nid_no_patch(rsa, keyContext->temporaryBuffer,
                    content, keyContext->digest, maskDigest, keyContext->saltLength))
                return -1;
            const int ret = LibreSsl3::RsaPrivateEncrypt_nid_no_patch(LibreSsl3::RsaByteCount_nid_no_patch(rsa),
                keyContext->temporaryBuffer, signature, rsa, LibreSsl3RsaPaddingNone);
            if (ret < 0) return ret;
            *signatureLength = ret;
            return 1;
        } else if (keyContext->padding == LibreSsl3RsaPaddingX931) {
            NotImplemented_nid_no_patch("RSA X9.31 signing");
        } else {
            return -1;
        }
    } else {
        const int ret = LibreSsl3::RsaPrivateEncrypt_nid_no_patch(static_cast<int>(contentLength), content,
            signature, rsa, keyContext->padding);
        if (ret < 0) return ret;
        *signatureLength = ret;
        return 1;
    }
}

int APS5_VABI PkeyRsaVerify(LibreSsl3PkeyContext* context, const unsigned char* signature, std::size_t signatureLength,
    const unsigned char* content, std::size_t contentLength) {
    auto* keyContext = RsaContextData(context);
    auto* rsa = RsaKeyOf(context);
    std::size_t rslen = 0;
    if (keyContext->digest) {
        if (keyContext->padding == LibreSsl3RsaPaddingPkcs1) {
            return LibreSsl3::RsaVerify_nid_no_patch(keyContext->digest->type, content,
                static_cast<unsigned int>(contentLength), signature, static_cast<unsigned int>(signatureLength), rsa);
        } else if (contentLength != static_cast<std::size_t>(keyContext->digest->digestSize)) {
            QueueRsaError(ReasonInvalidDigestLength);
            return -1;
        } else if (keyContext->padding == LibreSsl3RsaPaddingPss) {
            if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
            const int ret = LibreSsl3::RsaPublicDecrypt_nid_no_patch(static_cast<int>(signatureLength),
                signature, keyContext->temporaryBuffer, rsa, LibreSsl3RsaPaddingNone);
            if (ret <= 0) return 0;
            const auto* maskDigest = keyContext->maskDigest ? keyContext->maskDigest : keyContext->digest;
            const int verified = LibreSsl3::RsaVerifyPkcs1PssMgf1_nid_no_patch(rsa, content,
                keyContext->digest, maskDigest, keyContext->temporaryBuffer, keyContext->saltLength);
            if (verified <= 0) return 0;
            return 1;
        } else if (keyContext->padding == LibreSsl3RsaPaddingX931) {
            NotImplemented_nid_no_patch("RSA X9.31 verification");
        } else {
            return -1;
        }
    } else {
        if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
        const int ret = LibreSsl3::RsaPublicDecrypt_nid_no_patch(static_cast<int>(signatureLength),
            signature, keyContext->temporaryBuffer, rsa, keyContext->padding);
        if (ret <= 0) return 0;
        rslen = ret;
    }
    if (rslen != contentLength) return 0;
    if (CRYPTO_memcmp_nid_postfix(content, keyContext->temporaryBuffer, rslen)) return 0;
    return 1;
}

int APS5_VABI RsaPssInitialize(LibreSsl3PkeyContext* context) {
    if (MethodOf(context)->keyType != LibreSsl3PkeyRsaPss) return 0;
    if (RsaKeyOf(context)->pss) NotImplemented_nid_no_patch("EVP_PKEY_CTX signature with RSA-PSS key restrictions");
    return 1;
}

int APS5_VABI RsaContextInitialize(LibreSsl3PkeyContext* context) {
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(calloc_nid_postfix(1, sizeof(LibreSsl3RsaKeyContext)));
    if (!keyContext) return 0;
    keyContext->keyBits = DefaultKeyGenerationBits;
    keyContext->padding = MethodOf(context)->keyType == LibreSsl3PkeyRsaPss ? LibreSsl3RsaPaddingPss
        : LibreSsl3RsaPaddingPkcs1;
    keyContext->saltLength = LibreSsl3RsaPssSaltLengthAuto;
    keyContext->minimumSaltLength = NoSaltLengthLimit;
    context->data = keyContext;
    context->generationInformation = keyContext->generationTemporary;
    context->generationInformationCount = 2;
    return 1;
}

int APS5_VABI PkeyRsaContextCopy(LibreSsl3PkeyContext* destination, LibreSsl3PkeyContext* source) {
    if (RsaContextInitialize(destination) <= 0) return 0;
    const auto* from = RsaContextData(source);
    auto* to = RsaContextData(destination);
    to->keyBits = from->keyBits;
    if (from->publicExponent) {
        to->publicExponent = LibreSsl3::DuplicateRsaBignum_nid_no_patch(from->publicExponent);
        if (!to->publicExponent) return 0;
    }
    to->padding = from->padding;
    to->digest = from->digest;
    to->maskDigest = from->maskDigest;
    if (from->oaepLabel) {
        to->oaepLabel = static_cast<unsigned char*>(calloc_nid_postfix(1, from->oaepLabelLength));
        if (!to->oaepLabel) return 0;
        std::memcpy(to->oaepLabel, from->oaepLabel, from->oaepLabelLength);
        to->oaepLabelLength = from->oaepLabelLength;
    }
    return 1;
}

void APS5_VABI RsaContextClear(LibreSsl3PkeyContext* context) {
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(context->data);
    if (!keyContext) return;
    if (keyContext->publicExponent) BN_clear_free_nid_postfix(keyContext->publicExponent);
    if (keyContext->temporaryBuffer) free_nid_postfix(keyContext->temporaryBuffer);
    if (keyContext->oaepLabel) free_nid_postfix(keyContext->oaepLabel);
    free_nid_postfix(keyContext);
    context->data = nullptr;
}

int APS5_VABI RsaEncryption(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
    const unsigned char* input, std::size_t inputLength) {
    auto* keyContext = RsaContextData(context);
    auto* rsa = RsaKeyOf(context);
    if (keyContext->padding == LibreSsl3RsaPaddingOaep) {
        const int keyLength = LibreSsl3::RsaByteCount_nid_no_patch(rsa);
        if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
        if (!LibreSsl3::RsaPaddingAddPkcs1OaepMgf1_nid_no_patch(keyContext->temporaryBuffer, keyLength, input,
                static_cast<int>(inputLength), keyContext->oaepLabel, static_cast<int>(keyContext->oaepLabelLength),
                keyContext->digest, keyContext->maskDigest))
            return -1;
        const int result = LibreSsl3::RsaPublicEncrypt_nid_no_patch(keyLength, keyContext->temporaryBuffer, output, rsa,
            LibreSsl3RsaPaddingNone);
        if (result < 0) return result;
        *outputLength = static_cast<std::size_t>(result);
        return 1;
    }
    const int result = LibreSsl3::RsaPublicEncrypt_nid_no_patch(static_cast<int>(inputLength), input, output, rsa,
        keyContext->padding);
    if (result < 0) return result;
    *outputLength = static_cast<std::size_t>(result);
    return 1;
}

int APS5_VABI RsaDecryption(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
    const unsigned char* input, std::size_t inputLength) {
    auto* keyContext = RsaContextData(context);
    auto* rsa = RsaKeyOf(context);
    if (keyContext->padding == LibreSsl3RsaPaddingOaep) {
        if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
        const int decrypted = LibreSsl3::RsaPrivateDecrypt_nid_no_patch(static_cast<int>(inputLength), input,
            keyContext->temporaryBuffer, rsa, LibreSsl3RsaPaddingNone);
        if (decrypted <= 0) return decrypted;
        const int recovered = LibreSsl3::RsaPaddingCheckPkcs1OaepMgf1_nid_no_patch(output, decrypted,
            keyContext->temporaryBuffer, decrypted, decrypted, keyContext->oaepLabel,
            static_cast<int>(keyContext->oaepLabelLength), keyContext->digest, keyContext->maskDigest);
        if (recovered < 0) return recovered;
        *outputLength = static_cast<std::size_t>(recovered);
        return 1;
    }
    const int recovered = LibreSsl3::RsaPrivateDecrypt_nid_no_patch(static_cast<int>(inputLength), input, output, rsa,
        keyContext->padding);
    if (recovered < 0) return recovered;
    *outputLength = static_cast<std::size_t>(recovered);
    return 1;
}

int APS5_VABI PkeyRsaControl(LibreSsl3PkeyContext* context, int type, int first, void* second) {
    auto* keyContext = RsaContextData(context);
    const auto* method = MethodOf(context);
    switch (type) {
    case LibreSsl3PkeyControlRsaPadding:
        if (first >= LibreSsl3RsaPaddingPkcs1 && first <= LibreSsl3RsaPaddingPss) {
            if (!CheckPaddingDigest(keyContext->digest, first)) return 0;
            if (first == LibreSsl3RsaPaddingPss) {
                if ((context->operation & (LibreSsl3PkeyOperationSign | LibreSsl3PkeyOperationVerify)) == 0)
                    goto badPadding;
                if (!keyContext->digest) keyContext->digest = EVP_sha1_nid_postfix();
            } else if (method->keyType == LibreSsl3PkeyRsaPss) {
                goto badPadding;
            }
            if (first == LibreSsl3RsaPaddingOaep) {
                if ((context->operation & LibreSsl3PkeyOperationTypeCrypt) == 0) goto badPadding;
                if (!keyContext->digest) keyContext->digest = EVP_sha1_nid_postfix();
            }
            keyContext->padding = first;
            return 1;
        }
badPadding:
        QueueRsaError(ReasonIllegalPaddingMode);
        return -2;

    case LibreSsl3PkeyControlGetRsaPadding:
        if (!second) Reject("EVP_PKEY_CTX_ctrl get RSA padding without an output");
        *static_cast<int*>(second) = keyContext->padding;
        return 1;

    case LibreSsl3PkeyControlRsaPssSaltLength:
    case LibreSsl3PkeyControlGetRsaPssSaltLength:
        if (keyContext->padding != LibreSsl3RsaPaddingPss) {
            QueueRsaError(ReasonInvalidSaltLength);
            return -2;
        }
        if (type == LibreSsl3PkeyControlGetRsaPssSaltLength) {
            if (!second) Reject("EVP_PKEY_CTX_ctrl get RSA PSS salt length without an output");
            *static_cast<int*>(second) = keyContext->saltLength;
        } else {
            if (first < LibreSsl3RsaPssSaltLengthMaximum) return -2;
            if (keyContext->minimumSaltLength != NoSaltLengthLimit) {
                if (first == LibreSsl3RsaPssSaltLengthAuto &&
                    context->operation == LibreSsl3PkeyOperationVerify) {
                    QueueRsaError(ReasonInvalidSaltLength);
                    return -2;
                }
                const int digestSize = keyContext->digest ? keyContext->digest->digestSize : 0;
                if ((first == LibreSsl3RsaPssSaltLengthDigest && keyContext->minimumSaltLength > digestSize) ||
                    (first >= 0 && first < keyContext->minimumSaltLength)) {
                    QueueRsaError(ReasonSaltLengthTooSmall);
                    return 0;
                }
            }
            keyContext->saltLength = first;
        }
        return 1;

    case LibreSsl3PkeyControlRsaKeyGenerationBits:
        if (first < LibreSsl3RsaMinimumModulusBits) {
            QueueRsaError(ReasonKeySizeTooSmall);
            return -2;
        }
        keyContext->keyBits = first;
        return 1;

    case LibreSsl3PkeyControlRsaKeyGenerationPublicExponent: {
        auto* exponent = static_cast<LibreSsl3Bignum*>(second);
        const bool odd = exponent != nullptr && exponent->top > 0 && exponent->data != nullptr &&
            (exponent->data[0] & 1U) != 0;
        const bool one = exponent != nullptr && exponent->top == 1 && exponent->data != nullptr &&
            exponent->data[0] == 1;
        if (!odd || one) {
            QueueRsaError(ReasonBadExponentValue);
            return -2;
        }
        if (keyContext->publicExponent) BN_clear_free_nid_postfix(keyContext->publicExponent);
        keyContext->publicExponent = exponent;
        return 1;
    }

    case LibreSsl3PkeyControlRsaOaepDigest:
    case LibreSsl3PkeyControlGetRsaOaepDigest:
        if (keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidPaddingMode);
            return -2;
        }
        if (type == LibreSsl3PkeyControlGetRsaOaepDigest) {
            if (!second) Reject("EVP_PKEY_CTX_ctrl get OAEP digest without an output");
            *static_cast<const DigestMethod**>(second) = keyContext->digest;
        } else {
            keyContext->digest = static_cast<const DigestMethod*>(second);
        }
        return 1;

    case LibreSsl3PkeyControlMessageDigest:
        if (!CheckPaddingDigest(static_cast<const DigestMethod*>(second), keyContext->padding)) return 0;
        if (keyContext->minimumSaltLength != NoSaltLengthLimit) {
            const int restricted = keyContext->digest ? keyContext->digest->type : -1;
            const int requested = second ? static_cast<const DigestMethod*>(second)->type : -1;
            if (restricted == requested) return 1;
            QueueRsaError(ReasonDigestNotAllowed);
            return 0;
        }
        keyContext->digest = static_cast<const DigestMethod*>(second);
        return 1;

    case LibreSsl3PkeyControlGetMessageDigest:
        if (!second) Reject("EVP_PKEY_CTX_ctrl get digest without an output");
        *static_cast<const DigestMethod**>(second) = keyContext->digest;
        return 1;

    case LibreSsl3PkeyControlRsaMaskDigest:
    case LibreSsl3PkeyControlGetRsaMaskDigest:
        if (keyContext->padding != LibreSsl3RsaPaddingPss && keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidMaskDigest);
            return -2;
        }
        if (type == LibreSsl3PkeyControlGetRsaMaskDigest) {
            if (!second) Reject("EVP_PKEY_CTX_ctrl get MGF1 digest without an output");
            *static_cast<const DigestMethod**>(second) = keyContext->maskDigest ? keyContext->maskDigest
                : keyContext->digest;
        } else {
            if (keyContext->minimumSaltLength != NoSaltLengthLimit) {
                const int restricted = keyContext->maskDigest ? keyContext->maskDigest->type : -1;
                const int requested = second ? static_cast<const DigestMethod*>(second)->type : -1;
                if (restricted == requested) return 1;
                QueueRsaError(ReasonInvalidMaskDigest);
                return 0;
            }
            keyContext->maskDigest = static_cast<const DigestMethod*>(second);
        }
        return 1;

    case LibreSsl3PkeyControlRsaOaepLabel:
        if (keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidPaddingMode);
            return -2;
        }
        if (keyContext->oaepLabel) free_nid_postfix(keyContext->oaepLabel);
        if (second && first > 0) {
            keyContext->oaepLabel = static_cast<unsigned char*>(second);
            keyContext->oaepLabelLength = static_cast<std::size_t>(first);
        } else {
            keyContext->oaepLabel = nullptr;
            keyContext->oaepLabelLength = 0;
        }
        return 1;

    case LibreSsl3PkeyControlGetRsaOaepLabel:
        if (keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidPaddingMode);
            return -2;
        }
        if (!second) Reject("EVP_PKEY_CTX_ctrl get OAEP label without an output");
        *static_cast<unsigned char**>(second) = keyContext->oaepLabel;
        return static_cast<int>(keyContext->oaepLabelLength);

    case LibreSsl3PkeyControlDigestInit:
    case LibreSsl3PkeyControlPkcs7Sign:
        return 1;

    case LibreSsl3PkeyControlPkcs7Encrypt:
    case LibreSsl3PkeyControlPkcs7Decrypt:
        if (method->keyType != LibreSsl3PkeyRsaPss) return 1;
        QueueRsaError(ReasonKeyTypeOperationNotSupported);
        return -2;

    case LibreSsl3PkeyControlPeerKey:
        QueueRsaError(ReasonKeyTypeOperationNotSupported);
        return -2;

    default:
        return -2;
    }
}

int APS5_VABI RsaKeySize(const LibreSsl3EvpPkey* key) {
    return LibreSsl3::RsaByteCount_nid_no_patch(static_cast<const LibreSsl3Rsa*>(key->key));
}

int APS5_VABI RsaKeyBits(const LibreSsl3EvpPkey* key) {
    return LibreSsl3::RsaBitCount_nid_no_patch(static_cast<const LibreSsl3Rsa*>(key->key));
}

void APS5_VABI RsaKeyFree(LibreSsl3EvpPkey* key) {
    LibreSsl3::FreeRsa_nid_no_patch(static_cast<LibreSsl3Rsa*>(key->key));
}

int APS5_VABI RsaKeyControl(LibreSsl3EvpPkey* key, int operation, long first, void* second) {
    (void) first;
    if (operation != LibreSsl3Asn1PkeyControlDefaultDigestNid) return -2;
    const auto* rsa = static_cast<const LibreSsl3Rsa*>(key->key);
    if (!rsa) NotImplemented_nid_no_patch("EVP_PKEY_asn1 method control without a native RSA key");
    if (rsa->pss) NotImplemented_nid_no_patch("EVP_PKEY_asn1 method control of a key with PSS restrictions");
    if (!second) Reject("EVP_PKEY_asn1 method control without a digest identifier");
    *static_cast<int*>(second) = DefaultDigestNid;
    return 1;
}

const LibreSsl3PkeyMethod RsaMethod {
    LibreSsl3PkeyRsa,
    LibreSsl3PkeyMethodFlagAutomaticArgumentLength,
    &RsaContextInitialize,
    &PkeyRsaContextCopy,
    &RsaContextClear,
    nullptr,
    nullptr,
    nullptr,
    &RsaSign,
    nullptr,
    &PkeyRsaVerify,
    nullptr,
    nullptr,
    nullptr,
    &RsaEncryption,
    &RsaDecryption,
    nullptr,
    nullptr,
    &PkeyRsaControl,
    nullptr,
    nullptr,
    nullptr,
};

const LibreSsl3PkeyMethod RsaPssMethod {
    LibreSsl3PkeyRsaPss,
    LibreSsl3PkeyMethodFlagAutomaticArgumentLength,
    &RsaContextInitialize,
    &PkeyRsaContextCopy,
    &RsaContextClear,
    nullptr,
    nullptr,
    &RsaPssInitialize,
    &RsaSign,
    &RsaPssInitialize,
    &PkeyRsaVerify,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &PkeyRsaControl,
    nullptr,
    nullptr,
    nullptr,
};

const LibreSsl3PkeyAsn1Method RsaAsn1Method {
    &RsaAsn1Method,
    LibreSsl3PkeyRsa,
    Asn1MethodFlags,
    "RSA",
    "OpenSSL RSA method",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeySize,
    &RsaKeyBits,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeyFree,
    &RsaKeyControl,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

const LibreSsl3PkeyAsn1Method RsaPssAsn1Method {
    &RsaPssAsn1Method,
    LibreSsl3PkeyRsaPss,
    Asn1MethodFlags,
    "RSA-PSS",
    "OpenSSL RSA-PSS method",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeySize,
    &RsaKeyBits,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeyFree,
    &RsaKeyControl,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

int DigestInitialize(LibreSsl3::DigestContext* context, const DigestMethod* type) {
    context->flags &= ~DigestCleanedFlag;

    if (context->digest != type) {
        if (context->digest && context->digest->contextSize && context->data &&
            !(context->flags & DigestReuseFlag)) {
            if (context->digest->contextSize > 0) {
                for (int i = 0; i < context->digest->contextSize; ++i)
                    static_cast<volatile unsigned char*>(context->data)[i] = 0;
            }
            free_nid_postfix(context->data);
            context->data = nullptr;
        }
        context->digest = type;
        if (!(context->flags & DigestNoInitFlag) && type->contextSize) {
            context->update = type->update;
            context->data = calloc_nid_postfix(1, static_cast<std::size_t>(type->contextSize));
            if (!context->data) {
                EVP_PKEY_CTX_free_nid_postfix(static_cast<LibreSsl3PkeyContext*>(context->keyContext));
                context->keyContext = nullptr;
                QueueEvpError(AllocationFailure);
                return 0;
            }
        }
    }
    if (context->keyContext) {
        const int r = EVP_PKEY_CTX_ctrl_nid_postfix(static_cast<LibreSsl3PkeyContext*>(context->keyContext), -1,
            LibreSsl3PkeyOperationTypeSignature, LibreSsl3PkeyControlDigestInit, 0, context);
        if (r <= 0 && r != -2)
            return 0;
    }
    if (context->flags & DigestNoInitFlag) return 1;
    return type->init(context);
}

int DigestSignInitialize(LibreSsl3::DigestContext* ctx,
    LibreSsl3PkeyContext** pctx, const DigestMethod* type, LibreSsl3EvpPkey* pkey) {
    if (!ctx->keyContext)
        ctx->keyContext = EVP_PKEY_CTX_new_nid_postfix(pkey, nullptr);
    if (!ctx->keyContext) return 0;

    const auto* method = MethodOf(static_cast<LibreSsl3PkeyContext*>(ctx->keyContext));
    const bool hasSignCtxCustom = (method->flags & LibreSsl3PkeyMethodFlagSignatureContextCustom) != 0;
    if (!hasSignCtxCustom) {
        if (type == nullptr) {
            int nid = 0;
            const auto* keyAsn1 = static_cast<const LibreSsl3PkeyAsn1Method*>(pkey->method);
            if (!keyAsn1 || keyAsn1->keyControl(pkey, LibreSsl3Asn1PkeyControlDefaultDigestNid, 0,
                    &nid) <= 0) {
                QueueEvpError(ReasonNoDefaultDigest);
                return 0;
            }
            if (nid == DefaultDigestNid) {
                type = reinterpret_cast<const DigestMethod*>(EVP_sha256_nid_postfix());
            } else {
                NotImplemented_nid_no_patch("EVP_DigestSignInit default digest lookup for non-SHA256");
            }
        }
    }

    if (method->signatureContextInitialize) NotImplemented_nid_no_patch("EVP_DigestSignInit with a signctx_init method");
    if (method->digestSignature) NotImplemented_nid_no_patch("EVP_DigestSignInit with a digestsign method");
    if (EVP_PKEY_sign_init_nid_postfix(static_cast<LibreSsl3PkeyContext*>(ctx->keyContext)) <= 0)
        return 0;

    if (EVP_PKEY_CTX_ctrl_nid_postfix(static_cast<LibreSsl3PkeyContext*>(ctx->keyContext), -1,
            LibreSsl3PkeyOperationTypeSignature, LibreSsl3PkeyControlMessageDigest, 0,
            static_cast<void*>(const_cast<DigestMethod*>(type))) <= 0)
        return 0;

    if (pctx) *pctx = static_cast<LibreSsl3PkeyContext*>(ctx->keyContext);

    if (!hasSignCtxCustom) {
        if (!DigestInitialize(ctx, type)) return 0;
    }
    return 1;
}

int DigestCopy(LibreSsl3::DigestContext* out, const LibreSsl3::DigestContext* in) {
    if (!in || !in->digest) {
        QueueEvpError(ReasonInputNotInitialized);
        return 0;
    }

    unsigned char* tmp_buf;
    if (out->digest == in->digest) {
        tmp_buf = static_cast<unsigned char*>(out->data);
        out->flags |= DigestReuseFlag;
    } else {
        tmp_buf = nullptr;
    }

    if (out->digest && out->digest->cleanup && !(out->flags & DigestCleanedFlag))
        out->digest->cleanup(out);
    if (out->digest && out->digest->contextSize && out->data && !(out->flags & DigestReuseFlag)) {
        if (out->digest->contextSize > 0) {
            for (int i = 0; i < out->digest->contextSize; ++i)
                static_cast<volatile unsigned char*>(out->data)[i] = 0;
        }
        free_nid_postfix(out->data);
    }

    std::memcpy(out, in, sizeof(*in));
    out->data = nullptr;
    out->keyContext = nullptr;
    out->flags &= ~DigestKeepKeyContextFlag;

    if (in->data && out->digest->contextSize) {
        if (tmp_buf) {
            out->data = tmp_buf;
        } else {
            out->data = calloc_nid_postfix(1, static_cast<std::size_t>(out->digest->contextSize));
            if (!out->data) {
                QueueEvpError(AllocationFailure);
                return 0;
            }
        }
        std::memcpy(out->data, in->data, static_cast<std::size_t>(out->digest->contextSize));
    }

    out->update = in->update;

    if (out->digest->copy) {
        return out->digest->copy(out, in);
    }

    return 1;
}

int DigestFinal(LibreSsl3::DigestContext* ctx, unsigned char* md, unsigned int* mdlen) {
    if (!ctx->digest) return 0;
    if (!ctx->digest->finalize) return 0;
    const int ret = ctx->digest->finalize(ctx, md);
    if (mdlen) *mdlen = ctx->digest->digestSize;
    if (ctx->digest->cleanup) {
        ctx->digest->cleanup(ctx);
        ctx->flags |= DigestCleanedFlag;
    }
    if (ctx->data && ctx->digest->contextSize) {
        for (int i = 0; i < ctx->digest->contextSize; ++i)
            static_cast<volatile unsigned char*>(ctx->data)[i] = 0;
    }
    return ret;
}

} // namespace

const LibreSsl3PkeyMethod* FindNativePkeyMethod_nid_no_patch(std::int32_t keyType) {
    if (keyType == LibreSsl3PkeyRsa) return &RsaMethod;
    if (keyType == LibreSsl3PkeyRsaPss) return &RsaPssMethod;
    return nullptr;
}

const LibreSsl3PkeyAsn1Method* FindNativePkeyAsn1Method_nid_no_patch(std::int32_t keyType) {
    if (keyType == LibreSsl3PkeyRsa) return &RsaAsn1Method;
    if (keyType == LibreSsl3PkeyRsaPss) return &RsaPssAsn1Method;
    return nullptr;
}

bool IsNativePkeyMethod_nid_no_patch(const void* method) {
    return method == &RsaAsn1Method || method == &RsaPssAsn1Method;
}

namespace {

constexpr std::int32_t PkeyCmac = 894;
constexpr std::int32_t PkeyDh = 28;
constexpr std::int32_t PkeyDsa = 116;
constexpr std::int32_t PkeyEc = 408;
constexpr std::int32_t PkeyEd25519 = 952;
constexpr std::int32_t PkeyHkdf = 1022;
constexpr std::int32_t PkeyTls1Prf = 1055;
constexpr std::int32_t PkeyX25519 = 950;

bool IsLibreSslOnlyPkeyMethod(int keyType) {
    for (const auto supported : {PkeyCmac, PkeyDh, PkeyDsa, PkeyEc, PkeyEd25519, PkeyHkdf, LibreSsl3PkeyHmac,
             PkeyTls1Prf, PkeyX25519}) {
        if (keyType == supported) return true;
    }
    return false;
}

LibreSsl3PkeyContext* NewPkeyContext(LibreSsl3EvpPkey* key, const LibreSsl3PkeyMethod* method) {
    auto* context = static_cast<LibreSsl3PkeyContext*>(calloc_nid_postfix(1, sizeof(LibreSsl3PkeyContext)));
    if (!context) {
        QueueEvpError(AllocationFailure);
        return nullptr;
    }
    context->method = method;
    context->operation = LibreSsl3PkeyOperationUndefined;
    if ((context->key = key) != nullptr) EVP_PKEY_up_ref_nid_postfix(key);
    if (method->initialize != nullptr && method->initialize(context) <= 0) {
        EVP_PKEY_CTX_free_nid_postfix(context);
        return nullptr;
    }
    return context;
}

} // namespace

extern "C" {

LibreSsl3PkeyContext* APS5_VABI EVP_PKEY_CTX_new_nid_postfix(LibreSsl3EvpPkey* key, void* engine) {
    (void) engine;
    if (!key || !key->method) return nullptr;
    const auto* asn1Method = static_cast<const LibreSsl3PkeyAsn1Method*>(key->method);
    const auto* method = FindNativePkeyMethod_nid_no_patch(asn1Method->keyType);
    if (!method) {
        QueueEvpError(ReasonUnsupportedAlgorithm);
        return nullptr;
    }
    return NewPkeyContext(key, method);
}

LibreSsl3PkeyContext* APS5_VABI EVP_PKEY_CTX_new_id_nid_postfix(int keyType, void* engine) {
    (void) engine;
    if (keyType == -1) return nullptr;
    const auto* method = FindNativePkeyMethod_nid_no_patch(keyType);
    if (!method) {
        if (IsLibreSslOnlyPkeyMethod(keyType)) NotImplemented_nid_no_patch(__func__);
        QueueEvpError(ReasonUnsupportedAlgorithm);
        return nullptr;
    }
    return NewPkeyContext(nullptr, method);
}

void APS5_VABI EVP_PKEY_CTX_free_nid_postfix(LibreSsl3PkeyContext* context) {
    if (!context) return;
    const auto* method = MethodOf(context);
    if (method != nullptr && method->clear != nullptr) method->clear(context);
    EVP_PKEY_free_nid_disambig1_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->key));
    EVP_PKEY_free_nid_disambig1_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->peerKey));
    free_nid_postfix(context);
}

int APS5_VABI EVP_PKEY_CTX_ctrl_nid_postfix(LibreSsl3PkeyContext* context, int keyType, int operationType, int command,
    int first, void* second) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->control) {
        QueueEvpError(ReasonCommandNotSupported);
        return -2;
    }
    if (keyType != -1 && method->keyType != keyType) return -1;
    if (context->operation == LibreSsl3PkeyOperationUndefined) {
        QueueEvpError(ReasonNoOperationSet);
        return -1;
    }
    if (operationType != -1 && (context->operation & operationType) == 0) {
        QueueEvpError(ReasonInvalidOperation);
        return -1;
    }
    const int result = method->control(context, command, first, second);
    if (result == -2) QueueEvpError(ReasonCommandNotSupported);
    return result;
}

int APS5_VABI EVP_PKEY_encrypt_init_nid_postfix(LibreSsl3PkeyContext* context) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->encryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    context->operation = LibreSsl3PkeyOperationEncrypt;
    return 1;
}

int APS5_VABI EVP_PKEY_encrypt_nid_postfix(LibreSsl3PkeyContext* context, unsigned char* output,
    std::size_t* outputLength, const unsigned char* input, std::size_t inputLength) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->encryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    if (context->operation != LibreSsl3PkeyOperationEncrypt) {
        QueueEvpError(ReasonOperationNotInitialized);
        return -1;
    }
    if (const auto result = CheckAutomaticLength(context, method, output, outputLength, "EVP_PKEY_encrypt")) {
        return *result;
    }
    return method->encryption(context, output, outputLength, input, inputLength);
}

int APS5_VABI EVP_PKEY_decrypt_init_nid_postfix(LibreSsl3PkeyContext* context) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->decryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    context->operation = LibreSsl3PkeyOperationDecrypt;
    return 1;
}

int APS5_VABI EVP_PKEY_decrypt_nid_postfix(LibreSsl3PkeyContext* context, unsigned char* output,
    std::size_t* outputLength, const unsigned char* input, std::size_t inputLength) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->decryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    if (context->operation != LibreSsl3PkeyOperationDecrypt) {
        QueueEvpError(ReasonOperationNotInitialized);
        return -1;
    }
    if (const auto result = CheckAutomaticLength(context, method, output, outputLength, "EVP_PKEY_decrypt")) {
        return *result;
    }
    return method->decryption(context, output, outputLength, input, inputLength);
}

int APS5_VABI EVP_PKEY_sign_init_nid_postfix(LibreSsl3PkeyContext* context) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->signature) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    context->operation = LibreSsl3PkeyOperationSign;
    if (!method->signatureInitialize) return 1;
    const int ret = method->signatureInitialize(context);
    if (ret <= 0) context->operation = LibreSsl3PkeyOperationUndefined;
    return ret;
}

int APS5_VABI EVP_PKEY_sign_nid_postfix(LibreSsl3PkeyContext* context, unsigned char* signature,
    std::size_t* signatureLength, const unsigned char* content, std::size_t contentLength) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->signature) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    if (context->operation != LibreSsl3PkeyOperationSign) {
        QueueEvpError(ReasonOperationNotInitialized);
        return -1;
    }
    if (const auto result = CheckAutomaticLength(context, method, signature, signatureLength, "EVP_PKEY_sign")) {
        return *result;
    }
    return method->signature(context, signature, signatureLength, content, contentLength);
}

int APS5_VABI EVP_PKEY_verify_init_nid_postfix(LibreSsl3PkeyContext* context) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->verification) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    context->operation = LibreSsl3PkeyOperationVerify;
    if (!method->verificationInitialize) return 1;
    const int ret = method->verificationInitialize(context);
    if (ret <= 0) context->operation = LibreSsl3PkeyOperationUndefined;
    return ret;
}

int APS5_VABI EVP_PKEY_verify_nid_postfix(LibreSsl3PkeyContext* context, const unsigned char* signature,
    std::size_t signatureLength, const unsigned char* content, std::size_t contentLength) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->verification) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    if (context->operation != LibreSsl3PkeyOperationVerify) {
        QueueEvpError(ReasonOperationNotInitialized);
        return -1;
    }
    return method->verification(context, signature, signatureLength, content, contentLength);
}

int APS5_VABI RSA_pkey_ctx_ctrl_nid_postfix(LibreSsl3PkeyContext* context, int operationType, int command,
    int first, void* second) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (context != nullptr && method != nullptr &&
        method->keyType != LibreSsl3PkeyRsa && method->keyType != LibreSsl3PkeyRsaPss)
        return -1;
    return EVP_PKEY_CTX_ctrl_nid_postfix(context, -1, operationType, command, first, second);
}

int APS5_VABI EVP_DigestSignInit_nid_postfix(LibreSsl3::DigestContext* ctx,
    LibreSsl3PkeyContext** pctx, const LibreSsl3::DigestMethod* type, void* engine,
    LibreSsl3EvpPkey* pkey) {
    if (engine) NotImplemented_nid_no_patch("EVP_DigestSignInit with engine");
    return DigestSignInitialize(ctx, pctx, type, pkey);
}

int APS5_VABI EVP_DigestSignFinal_nid_postfix(LibreSsl3::DigestContext* ctx,
    unsigned char* sigret, std::size_t* siglen) {
    auto* pctx = static_cast<LibreSsl3PkeyContext*>(ctx->keyContext);
    if (!pctx) NotImplemented_nid_no_patch("EVP_DigestSignFinal without an EVP_PKEY_CTX");
    const auto* method = MethodOf(pctx);
    if (method->flags & LibreSsl3PkeyMethodFlagSignatureContextCustom)
        NotImplemented_nid_no_patch("EVP_DigestSignFinal with a custom signature context");
    if (method->signatureContext) NotImplemented_nid_no_patch("EVP_DigestSignFinal with a signctx method");
    if (sigret == nullptr) {
        const auto size = ctx->digest->digestSize;
        if (size < 0) return 0;
        return EVP_PKEY_sign_nid_postfix(pctx, nullptr, siglen, nullptr, static_cast<std::size_t>(size)) > 0;
    }
    LibreSsl3::DigestContext copy = {};
    unsigned char digest[LibreSsl3MaximumDigestSize];
    unsigned int digestLength = 0;
    auto result = 0;
    if (DigestCopy(&copy, ctx) && DigestFinal(&copy, digest, &digestLength))
        result = EVP_PKEY_sign_nid_postfix(pctx, sigret, siglen, digest, digestLength) > 0;
    if (copy.data) free_nid_postfix(copy.data);
    for (unsigned int i = 0; i < digestLength; ++i) digest[i] = 0;
    return result;
}

int APS5_VABI EVP_SignFinal_nid_postfix(LibreSsl3::DigestContext* ctx, unsigned char* sigret,
    unsigned int* siglen, LibreSsl3EvpPkey* pkey) {
    unsigned char digest[LibreSsl3MaximumDigestSize];
    unsigned int digestLength = 0;
    LibreSsl3::DigestContext copy = {};
    LibreSsl3PkeyContext* pkctx = nullptr;
    std::size_t sltmp = 0;
    int ret = 0;

    *siglen = 0;

    if (!DigestCopy(&copy, ctx)) goto err;
    if (!DigestFinal(&copy, digest, &digestLength)) goto err;

    sltmp = EVP_PKEY_size_nid_postfix(pkey);

    pkctx = EVP_PKEY_CTX_new_nid_postfix(pkey, nullptr);
    if (!pkctx) goto err;
    if (EVP_PKEY_sign_init_nid_postfix(pkctx) <= 0) goto err;
    if (EVP_PKEY_CTX_ctrl_nid_postfix(pkctx, -1, LibreSsl3PkeyOperationTypeSignature,
            LibreSsl3PkeyControlMessageDigest, 0,
            static_cast<void*>(const_cast<DigestMethod*>(ctx->digest))) <= 0)
        goto err;
    if (EVP_PKEY_sign_nid_postfix(pkctx, sigret, &sltmp, digest, digestLength) <= 0) goto err;
    *siglen = static_cast<unsigned int>(sltmp);
    ret = 1;

err:
    if (copy.data) free_nid_postfix(copy.data);
    EVP_PKEY_CTX_free_nid_postfix(pkctx);
    return ret;
}

int APS5_VABI EVP_VerifyFinal_nid_postfix(LibreSsl3::DigestContext* ctx,
    const unsigned char* sigbuf, unsigned int siglen, LibreSsl3EvpPkey* pkey) {
    unsigned char digest[LibreSsl3MaximumDigestSize];
    unsigned int digestLength = 0;
    LibreSsl3::DigestContext copy = {};
    LibreSsl3PkeyContext* pkctx = nullptr;
    int ret = 0;

    if (!DigestCopy(&copy, ctx)) goto err;
    if (!DigestFinal(&copy, digest, &digestLength)) goto err;

    ret = -1;
    pkctx = EVP_PKEY_CTX_new_nid_postfix(pkey, nullptr);
    if (!pkctx) goto err;
    if (EVP_PKEY_verify_init_nid_postfix(pkctx) <= 0) goto err;
    if (EVP_PKEY_CTX_ctrl_nid_postfix(pkctx, -1, LibreSsl3PkeyOperationTypeSignature,
            LibreSsl3PkeyControlMessageDigest, 0,
            static_cast<void*>(const_cast<DigestMethod*>(ctx->digest))) <= 0)
        goto err;
    ret = EVP_PKEY_verify_nid_postfix(pkctx, sigbuf, siglen, digest, digestLength);

err:
    if (copy.data) free_nid_postfix(copy.data);
    EVP_PKEY_CTX_free_nid_postfix(pkctx);
    return ret;
}

}
