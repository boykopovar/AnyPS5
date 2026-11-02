/* $OpenBSD: digest.c,v 1.27 2017/01/29 17:49:23 beck Exp $ */
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
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* $OpenBSD: hmac.c,v 1.24 2017/03/03 10:39:07 inoguchi Exp $ */
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

#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

namespace {
static_assert(sizeof(EvpMd) == 120);
static_assert(offsetof(EvpMd, flags) == 16);
static_assert(offsetof(EvpMd, sign) == 64);
static_assert(offsetof(EvpMd, requiredPkeyType) == 80);
static_assert(offsetof(EvpMd, blockSize) == 100);
static_assert(offsetof(EvpMd, control) == 112);
static_assert(sizeof(EvpMdContext) == 48);
static_assert(offsetof(EvpMdContext, update) == 40);
static_assert(sizeof(HmacContext) == 288);
static_assert(offsetof(HmacContext, keyLength) == 152);
static_assert(offsetof(HmacContext, key) == 156);

constexpr std::uint64_t FlagOneShot = 0x1;
constexpr std::uint64_t FlagCleaned = 0x2;
constexpr std::uint64_t FlagReuse = 0x4;
constexpr std::uint64_t FlagNoInit = 0x100;
constexpr std::size_t MaxDigestSize = 64;
constexpr std::uint64_t PkeyMethodSignature = 0x4;
constexpr std::uint64_t DigestAlgorithmIdAbsent = 0x8;
constexpr std::int32_t PkeyRsa = 6;
constexpr std::int32_t PkeyRsa2 = 19;
constexpr std::int32_t PointerSize = 8;

int APS5_VABI Md5Init(EvpMdContext* context) {
    return MD5_Init_nid_postfix(static_cast<Md5Context*>(context->mdData));
}

int APS5_VABI Md5Update(EvpMdContext* context, const void* data, std::size_t length) {
    return MD5_Update_nid_postfix(static_cast<Md5Context*>(context->mdData), data, length);
}

int APS5_VABI Md5Final(EvpMdContext* context, unsigned char* digest) {
    return MD5_Final_nid_postfix(digest, static_cast<Md5Context*>(context->mdData));
}

int APS5_VABI Sha256Init(EvpMdContext* context) {
    return SHA256_Init_nid_postfix(static_cast<Sha256Context*>(context->mdData));
}

int APS5_VABI Sha256Update(EvpMdContext* context, const void* data, std::size_t length) {
    return SHA256_Update_nid_postfix(static_cast<Sha256Context*>(context->mdData), data, length);
}

int APS5_VABI Sha256Final(EvpMdContext* context, unsigned char* digest) {
    return SHA256_Final_nid_postfix(digest, static_cast<Sha256Context*>(context->mdData));
}

int APS5_VABI Sha384Init(EvpMdContext* context) {
    return SHA384_Init_nid_postfix(static_cast<Sha512Context*>(context->mdData));
}

int APS5_VABI Sha512Init(EvpMdContext* context) {
    return SHA512_Init_nid_postfix(static_cast<Sha512Context*>(context->mdData));
}

int APS5_VABI Sha512Update(EvpMdContext* context, const void* data, std::size_t length) {
    return SHA512_Update_nid_postfix(static_cast<Sha512Context*>(context->mdData), data, length);
}

int APS5_VABI Sha512Final(EvpMdContext* context, unsigned char* digest) {
    return SHA512_Final_nid_postfix(digest, static_cast<Sha512Context*>(context->mdData));
}

constexpr EvpMd Md5 {
    4, 8, 16, 0, Md5Init, Md5Update, Md5Final, nullptr, nullptr, nullptr, nullptr,
    {PkeyRsa, PkeyRsa2, 0, 0, 0}, 64, PointerSize + 92, nullptr,
};

constexpr EvpMd Sha256 {
    672, 668, 32, PkeyMethodSignature | DigestAlgorithmIdAbsent, Sha256Init, Sha256Update, Sha256Final, nullptr, nullptr, nullptr, nullptr,
    {PkeyRsa, PkeyRsa2, 0, 0, 0}, 64, PointerSize + 112, nullptr,
};

constexpr EvpMd Sha384 {
    673, 669, 48, PkeyMethodSignature | DigestAlgorithmIdAbsent, Sha384Init, Sha512Update, Sha512Final, nullptr, nullptr, nullptr, nullptr,
    {PkeyRsa, PkeyRsa2, 0, 0, 0}, 128, PointerSize + 216, nullptr,
};

constexpr EvpMd Sha512 {
    674, 670, 64, PkeyMethodSignature | DigestAlgorithmIdAbsent, Sha512Init, Sha512Update, Sha512Final, nullptr, nullptr, nullptr, nullptr,
    {PkeyRsa, PkeyRsa2, 0, 0, 0}, 128, PointerSize + 216, nullptr,
};

template<class TContext> TContext& RequireContext(TContext* context, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    return *context;
}

void RequireNoEngineOrKey(const EvpMdContext& context, const char* function) {
    if (context.engine) NotImplemented_nid_no_patch((std::string(function) + ": ENGINE").c_str());
    if (context.pkeyContext) NotImplemented_nid_no_patch((std::string(function) + ": EVP_PKEY_CTX").c_str());
}

void ReleaseData(EvpMdContext& context) {
    if (!context.digest || context.digest->contextSize <= 0 || !context.mdData || (context.flags & FlagReuse)) return;
    std::memset(context.mdData, 0, static_cast<std::size_t>(context.digest->contextSize));
    free_nid_postfix(context.mdData);
    context.mdData = nullptr;
}

int CleanupContext(EvpMdContext& context, const char* function) {
    RequireNoEngineOrKey(context, function);
    if (context.digest && context.digest->cleanup && !(context.flags & FlagCleaned)) context.digest->cleanup(&context);
    ReleaseData(context);
    std::memset(&context, 0, sizeof(context));
    return 1;
}

int DigestInit(EvpMdContext& context, const EvpMd* type, void* engine, const char* function) {
    RequireNoEngineOrKey(context, function);
    if (engine) NotImplemented_nid_no_patch((std::string(function) + ": ENGINE").c_str());
    context.flags &= ~FlagCleaned;
    if (!type) {
        if (!context.digest) return 0;
        throw std::invalid_argument(std::string(function) + ": null digest type for a context that has one");
    }
    if (context.digest != type) {
        ReleaseData(context);
        if (!(context.flags & FlagNoInit) && type->contextSize) {
            if (type->contextSize < 0) throw std::invalid_argument(std::string(function) + ": negative digest context size");
            context.mdData = malloc_nid_postfix(static_cast<std::size_t>(type->contextSize));
            if (!context.mdData) {
                context.digest = nullptr;
                context.update = nullptr;
                return 0;
            }
            context.update = type->update;
        }
        context.digest = type;
    }
    if (context.flags & FlagNoInit) return 1;
    if (!context.digest->init) throw std::invalid_argument(std::string(function) + ": digest without an init function");
    return context.digest->init(&context);
}

int DigestUpdate(EvpMdContext& context, const void* data, std::size_t length, const char* function) {
    if (!context.update) throw std::invalid_argument(std::string(function) + ": context without an update function");
    return context.update(&context, data, length);
}

int DigestFinal(EvpMdContext& context, unsigned char* digest, std::uint32_t* size, const char* function) {
    if (!context.digest) throw std::invalid_argument(std::string(function) + ": context without a digest");
    const auto* type = context.digest;
    if (static_cast<std::size_t>(type->mdSize) > MaxDigestSize) return 0;
    if (!type->finalize) throw std::invalid_argument(std::string(function) + ": digest without a final function");
    const int result = type->finalize(&context, digest);
    if (size) *size = static_cast<std::uint32_t>(type->mdSize);
    if (type->cleanup) {
        type->cleanup(&context);
        context.flags |= FlagCleaned;
    }
    if (type->contextSize) {
        if (!context.mdData || type->contextSize < 0) throw std::invalid_argument(std::string(function) + ": context without digest data");
        std::memset(context.mdData, 0, static_cast<std::size_t>(type->contextSize));
    }
    return result;
}

int CopyContext(EvpMdContext& to, const EvpMdContext& from, const char* function) {
    if (!from.digest) return 0;
    RequireNoEngineOrKey(from, function);
    void* reused = nullptr;
    if (to.digest == from.digest) {
        reused = to.mdData;
        to.flags |= FlagReuse;
    }
    CleanupContext(to, function);
    std::memcpy(&to, &from, sizeof(to));
    if (from.mdData && to.digest->contextSize) {
        if (to.digest->contextSize < 0) throw std::invalid_argument(std::string(function) + ": negative digest context size");
        const auto size = static_cast<std::size_t>(to.digest->contextSize);
        to.mdData = reused ? reused : malloc_nid_postfix(size);
        if (!to.mdData) {
            to.digest = nullptr;
            to.update = nullptr;
            return 0;
        }
        std::memmove(to.mdData, from.mdData, size);
    }
    to.update = from.update;
    if (to.digest->copy) return to.digest->copy(&to, &from);
    return 1;
}

void HmacContextInit(HmacContext& context) {
    std::memset(&context.innerContext, 0, sizeof(context.innerContext));
    std::memset(&context.outerContext, 0, sizeof(context.outerContext));
    std::memset(&context.mdContext, 0, sizeof(context.mdContext));
    context.md = nullptr;
}

void HmacContextCleanup(HmacContext& context, const char* function) {
    CleanupContext(context.innerContext, function);
    CleanupContext(context.outerContext, function);
    CleanupContext(context.mdContext, function);
    std::memset(&context, 0, sizeof(context));
}

int HmacInit(HmacContext& context, const void* key, int length, const EvpMd* md, void* engine, const char* function) {
    if (md && md != context.md && (!key || length < 0)) return 0;
    bool reset = false;
    if (md) {
        reset = true;
        context.md = md;
    } else if (context.md) {
        md = context.md;
    } else {
        return 0;
    }
    if (key) {
        reset = true;
        const int block = md->blockSize;
        if (static_cast<std::size_t>(block) > sizeof(context.key)) return 0;
        if (block < length) {
            if (!DigestInit(context.mdContext, md, engine, function)) return 0;
            if (!DigestUpdate(context.mdContext, key, static_cast<std::size_t>(length), function)) return 0;
            if (!DigestFinal(context.mdContext, context.key, &context.keyLength, function)) return 0;
        } else {
            if (length < 0 || static_cast<std::size_t>(length) > sizeof(context.key)) return 0;
            std::memcpy(context.key, key, static_cast<std::size_t>(length));
            context.keyLength = static_cast<std::uint32_t>(length);
        }
        if (context.keyLength != HmacMaxBlock) std::memset(context.key + context.keyLength, 0, HmacMaxBlock - context.keyLength);
    }
    if (reset) {
        unsigned char pad[HmacMaxBlock];
        const auto block = static_cast<std::size_t>(md->blockSize);
        for (std::size_t i = 0; i < HmacMaxBlock; ++i) pad[i] = 0x36 ^ context.key[i];
        if (!DigestInit(context.innerContext, md, engine, function)) return 0;
        if (!DigestUpdate(context.innerContext, pad, block, function)) return 0;
        for (std::size_t i = 0; i < HmacMaxBlock; ++i) pad[i] = 0x5c ^ context.key[i];
        if (!DigestInit(context.outerContext, md, engine, function)) return 0;
        if (!DigestUpdate(context.outerContext, pad, block, function)) return 0;
    }
    return CopyContext(context.mdContext, context.innerContext, function) ? 1 : 0;
}

int HmacUpdate(HmacContext& context, const unsigned char* data, std::size_t length, const char* function) {
    if (!context.md) return 0;
    return DigestUpdate(context.mdContext, data, length, function);
}

int HmacFinal(HmacContext& context, unsigned char* digest, std::uint32_t* length, const char* function) {
    if (!context.md) return 0;
    unsigned char inner[MaxDigestSize];
    std::uint32_t innerLength = 0;
    if (!DigestFinal(context.mdContext, inner, &innerLength, function)) return 0;
    if (!CopyContext(context.mdContext, context.outerContext, function)) return 0;
    if (!DigestUpdate(context.mdContext, inner, innerLength, function)) return 0;
    if (!DigestFinal(context.mdContext, digest, length, function)) return 0;
    return 1;
}
}

extern "C" {

const EvpMd* APS5_VABI EVP_md5_nid_postfix(void) {
    return &Md5;
}

const EvpMd* APS5_VABI EVP_sha256_nid_postfix(void) {
    return &Sha256;
}

const EvpMd* APS5_VABI EVP_sha384_nid_postfix(void) {
    return &Sha384;
}

const EvpMd* APS5_VABI EVP_sha512_nid_postfix(void) {
    return &Sha512;
}

int APS5_VABI EVP_MD_size_nid_postfix(const EvpMd* md) {
    return md ? md->mdSize : -1;
}

void APS5_VABI EVP_MD_CTX_init_nid_postfix(EvpMdContext* context) {
    std::memset(&RequireContext(context, __func__), 0, sizeof(EvpMdContext));
}

EvpMdContext* APS5_VABI EVP_MD_CTX_create_nid_postfix(void) {
    return static_cast<EvpMdContext*>(calloc_nid_postfix(1, sizeof(EvpMdContext)));
}

int APS5_VABI EVP_MD_CTX_cleanup_nid_postfix(EvpMdContext* context) {
    return CleanupContext(RequireContext(context, __func__), __func__);
}

void APS5_VABI EVP_MD_CTX_destroy_nid_postfix(EvpMdContext* context) {
    if (!context) return;
    CleanupContext(*context, __func__);
    free_nid_postfix(context);
}

int APS5_VABI EVP_DigestInit_ex_nid_postfix(EvpMdContext* context, const EvpMd* type, void* engine) {
    return DigestInit(RequireContext(context, __func__), type, engine, __func__);
}

int APS5_VABI EVP_DigestUpdate_nid_postfix(EvpMdContext* context, const void* data, std::size_t length) {
    return DigestUpdate(RequireContext(context, __func__), data, length, __func__);
}

int APS5_VABI EVP_DigestFinal_ex_nid_postfix(EvpMdContext* context, unsigned char* digest, std::uint32_t* size) {
    return DigestFinal(RequireContext(context, __func__), digest, size, __func__);
}

int APS5_VABI EVP_Digest_nid_postfix(const void* data, std::size_t length, unsigned char* digest, std::uint32_t* size, const EvpMd* type, void* engine) {
    EvpMdContext context {};
    context.flags = FlagOneShot;
    const int result = DigestInit(context, type, engine, __func__) && DigestUpdate(context, data, length, __func__) && DigestFinal(context, digest, size, __func__);
    CleanupContext(context, __func__);
    return result;
}

void APS5_VABI HMAC_CTX_init_nid_postfix(HmacContext* context) {
    HmacContextInit(RequireContext(context, __func__));
}

void APS5_VABI HMAC_CTX_cleanup_nid_postfix(HmacContext* context) {
    HmacContextCleanup(RequireContext(context, __func__), __func__);
}

int APS5_VABI HMAC_Init_ex_nid_postfix(HmacContext* context, const void* key, int length, const EvpMd* md, void* engine) {
    return HmacInit(RequireContext(context, __func__), key, length, md, engine, __func__);
}

int APS5_VABI HMAC_Init_nid_postfix(HmacContext* context, const void* key, int length, const EvpMd* md) {
    auto& c = RequireContext(context, __func__);
    if (key && md) HmacContextInit(c);
    return HmacInit(c, key, length, md, nullptr, __func__);
}

int APS5_VABI HMAC_Update_nid_postfix(HmacContext* context, const unsigned char* data, std::size_t length) {
    return HmacUpdate(RequireContext(context, __func__), data, length, __func__);
}

int APS5_VABI HMAC_Final_nid_postfix(HmacContext* context, unsigned char* digest, std::uint32_t* length) {
    return HmacFinal(RequireContext(context, __func__), digest, length, __func__);
}

unsigned char* APS5_VABI HMAC_nid_postfix(const EvpMd* md, const void* key, int keyLength, const unsigned char* data, std::size_t length, unsigned char* digest, std::uint32_t* digestLength) {
    static unsigned char buffer[MaxDigestSize];
    if (!digest) digest = buffer;
    HmacContext context;
    HmacContextInit(context);
    const bool ok = HMAC_Init_nid_postfix(&context, key, keyLength, md) && HmacUpdate(context, data, length, __func__) && HmacFinal(context, digest, digestLength, __func__);
    HmacContextCleanup(context, __func__);
    return ok ? digest : nullptr;
}

}
