/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a) src/lib/libcrypto/x509/x509_vfy.c (x509_vfy.c,v 1.145)
 * and src/lib/libcrypto/x509/x509_vpm.c (x509_vpm.c,v 1.45) */
/* $OpenBSD: x509_vfy.c,v 1.145 2024/08/28 07:37:50 tb Exp $ */
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
/* $OpenBSD: x509_vpm.c,v 1.45 2024/03/29 04:50:11 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t VerifyCallback = 0;
constexpr std::size_t VerifyErrorCallback = 1;
constexpr std::size_t IssuerCallback = 2;
constexpr std::size_t IssuedCallback = 3;
constexpr std::size_t CleanupCallback = 11;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int InvalidCall = 65;
constexpr std::uint64_t UseCheckTime = 0x2;
constexpr std::uint64_t TrustedFirst = 0x8000;
constexpr std::uint64_t InheritDefault = 0x1;
constexpr std::uint64_t InheritOverwrite = 0x2;
constexpr std::uint64_t InheritResetFlags = 0x4;
constexpr std::uint64_t InheritLocked = 0x8;
constexpr std::uint64_t InheritOnce = 0x10;

char defaultName[] = "default";
LibreSslX509VerifyParamId defaultId{};
const LibreSslX509VerifyParam defaultParam{defaultName, 0, 0, TrustedFirst, 0, 0, 100, nullptr, &defaultId};

int APS5_VABI InternalVerify(LibreSslX509StoreContext*) {
    NotImplemented_nid_no_patch("X509_STORE_CTX: internal_verify");
    return 0;
}

int APS5_VABI NullCallback(int ok, LibreSslX509StoreContext*) {
    return ok;
}

int APS5_VABI GetIssuer(LibreSsl3X509**, LibreSslX509StoreContext*, LibreSsl3X509*) {
    NotImplemented_nid_no_patch("X509_STORE_CTX_get1_issuer");
    return 0;
}

int APS5_VABI CheckIssued(LibreSslX509StoreContext*, LibreSsl3X509*, LibreSsl3X509*) {
    NotImplemented_nid_no_patch("X509_STORE_CTX: check_issued");
    return 0;
}

LibreSslX509VerifyParam* NewVerifyParam() {
    auto* const param = static_cast<LibreSslX509VerifyParam*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParam)));
    if (!param) return nullptr;
    param->id = static_cast<LibreSslX509VerifyParamId*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParamId)));
    if (!param->id) {
        free_nid_postfix(param);
        return nullptr;
    }
    param->depth = -1;
    return param;
}

void CheckReleasable(const LibreSslX509VerifyParam* param) {
    if (!param->id) throw std::invalid_argument("X509_STORE_CTX_free: verify parameters without identity");
    const auto* const policies = static_cast<const LibreSslStack*>(param->policies);
    if (policies && policies->num > 0) NotImplemented_nid_no_patch("X509_STORE_CTX_free: verify parameter policies");
}

void FreeVerifyParam(LibreSslX509VerifyParam* param) {
    auto* const id = param->id;
    free_nid_postfix(param->name);
    sk_free_nid_postfix(static_cast<LibreSslStack*>(param->policies));
    sk_pop_free_nid_postfix(static_cast<LibreSslStack*>(id->hosts), free_nid_postfix);
    free_nid_postfix(id->peerName);
    free_nid_postfix(id->email);
    free_nid_postfix(id->ip);
    free_nid_postfix(id);
    free_nid_postfix(param);
}

LibreSslStack* CopyHosts(const LibreSslStack* hosts) {
    auto* const copy = sk_new_nid_postfix(nullptr);
    if (!copy) return nullptr;
    for (int index = 0; index < sk_num_nid_postfix(hosts); ++index) {
        char* const host = strdup_nid_postfix(static_cast<const char*>(sk_value_nid_postfix(hosts, index)));
        if (!host || sk_push_nid_postfix(copy, host) <= 0) {
            sk_pop_free_nid_postfix(copy, free_nid_postfix);
            free_nid_postfix(host);
            return nullptr;
        }
    }
    return copy;
}

bool SetBytes(char** destination, std::uint64_t* length, const char* source, std::uint64_t sourceLength, bool noNul) {
    if (!source) return false;
    char* copy = nullptr;
    if (sourceLength == 0) {
        sourceLength = std::strlen(source);
        if (sourceLength == 0) return false;
        if (!(copy = strdup_nid_postfix(source))) return false;
    } else {
        if (noNul && std::memchr(source, '\0', sourceLength)) return false;
        if (!(copy = static_cast<char*>(malloc_nid_postfix(sourceLength)))) return false;
        std::memcpy(copy, source, sourceLength);
    }
    free_nid_postfix(*destination);
    *destination = copy;
    *length = sourceLength;
    return true;
}

bool SetIp(LibreSslX509VerifyParamId* id, const unsigned char* ip, std::uint64_t length) {
    if (length != 4 && length != 16) return false;
    auto* destination = reinterpret_cast<char*>(id->ip);
    if (!SetBytes(&destination, &id->ipLength, reinterpret_cast<const char*>(ip), length, false)) return false;
    id->ip = reinterpret_cast<unsigned char*>(destination);
    return true;
}

bool Inherit(LibreSslX509VerifyParam* destination, const LibreSslX509VerifyParam* source) {
    if (!source) return true;
    if (!source->id) throw std::invalid_argument("X509_STORE_CTX_init: verify parameters without identity");
    auto* const to = destination->id;
    const auto* const from = source->id;
    const auto flags = destination->inheritanceFlags | source->inheritanceFlags;
    if (flags & InheritOnce) destination->inheritanceFlags = 0;
    if (flags & InheritLocked) return true;
    const bool toDefault = flags & InheritDefault;
    const bool overwrite = flags & InheritOverwrite;
    const auto copy = [&](bool sourceSet, bool destinationSet) { return overwrite || (sourceSet && (toDefault || !destinationSet)); };
    if (copy(source->purpose != 0, destination->purpose != 0)) destination->purpose = source->purpose;
    if (copy(source->trust != 0, destination->trust != 0)) destination->trust = source->trust;
    if (copy(source->depth != -1, destination->depth != -1)) destination->depth = source->depth;
    if (overwrite || !(destination->flags & UseCheckTime)) {
        destination->checkTime = source->checkTime;
        destination->flags &= ~UseCheckTime;
    }
    if (flags & InheritResetFlags) destination->flags = 0;
    destination->flags |= source->flags;
    if (copy(source->policies, destination->policies) && (source->policies || destination->policies))
        NotImplemented_nid_no_patch("X509_STORE_CTX_init: verify parameter policies");
    if (copy(from->hostFlags != 0, to->hostFlags != 0)) to->hostFlags = from->hostFlags;
    if (copy(from->hosts, to->hosts)) {
        sk_pop_free_nid_postfix(static_cast<LibreSslStack*>(to->hosts), free_nid_postfix);
        to->hosts = nullptr;
        if (from->hosts && !(to->hosts = CopyHosts(static_cast<const LibreSslStack*>(from->hosts)))) return false;
    }
    if (copy(from->email, to->email) && !SetBytes(&to->email, &to->emailLength, from->email, from->emailLength, true)) return false;
    if (copy(from->ip, to->ip) && !SetIp(to, from->ip, from->ipLength)) return false;
    return true;
}

int Fail() {
    LibreSslPutError_nid_no_patch(X509Library, AnyFunction, MallocFailure);
    return 0;
}

}

extern "C" {

LibreSslX509StoreContext* APS5_VABI X509_STORE_CTX_new_nid_postfix(void) {
    return static_cast<LibreSslX509StoreContext*>(calloc_nid_postfix(1, sizeof(LibreSslX509StoreContext)));
}

int APS5_VABI X509_STORE_CTX_init_nid_postfix(LibreSslX509StoreContext* context, LibreSslX509Store* store, LibreSsl3X509* leaf, LibreSslStack* untrusted) {
    if (!context) throw std::invalid_argument("X509_STORE_CTX_init: null context");
    std::memset(context, 0, sizeof(*context));
    context->error = InvalidCall;
    context->store = store;
    context->cert = leaf;
    context->untrusted = untrusted;
    if (store && store->callbacks[VerifyCallback])
        context->callbacks[VerifyCallback] = store->callbacks[VerifyCallback];
    else
        context->callbacks[VerifyCallback] = reinterpret_cast<void*>(InternalVerify);
    if (store && store->callbacks[VerifyErrorCallback])
        context->callbacks[VerifyErrorCallback] = store->callbacks[VerifyErrorCallback];
    else
        context->callbacks[VerifyErrorCallback] = reinterpret_cast<void*>(NullCallback);
    context->callbacks[IssuerCallback] = reinterpret_cast<void*>(GetIssuer);
    context->callbacks[IssuedCallback] = reinterpret_cast<void*>(CheckIssued);
    auto* const param = NewVerifyParam();
    context->param = param;
    if (!param) return Fail();
    bool inherited = true;
    if (store)
        inherited = Inherit(param, store->param);
    else
        param->inheritanceFlags |= InheritDefault | InheritOnce;
    if (inherited) inherited = Inherit(param, &defaultParam);
    if (!inherited) return Fail();
    return 1;
}

void APS5_VABI X509_STORE_CTX_free_nid_postfix(LibreSslX509StoreContext* context) {
    if (!context) return;
    if (context->callbacks[CleanupCallback]) NotImplemented_nid_no_patch("X509_STORE_CTX_free: cleanup callback");
    if (context->tree) NotImplemented_nid_no_patch("X509_STORE_CTX_free: policy tree");
    if (context->chain) NotImplemented_nid_no_patch("X509_STORE_CTX_free: certificate chain");
    if (context->exData) NotImplemented_nid_no_patch("X509_STORE_CTX_free: extra data");
    auto* const param = context->parent ? nullptr : static_cast<LibreSslX509VerifyParam*>(context->param);
    if (param) {
        CheckReleasable(param);
        FreeVerifyParam(param);
    }
    free_nid_postfix(context);
}

void* APS5_VABI X509_STORE_CTX_get0_param_nid_postfix(LibreSslX509StoreContext* context) {
    if (!context) throw std::invalid_argument("X509_STORE_CTX_get0_param: null context");
    return context->param;
}

int APS5_VABI X509_STORE_up_ref_nid_postfix(LibreSslX509Store* store) {
    if (!store) throw std::invalid_argument("X509_STORE_up_ref: null store");
    std::atomic_ref<std::int32_t> count(store->references);
    auto previous = count.load();
    for (;;) {
        if (previous == std::numeric_limits<std::int32_t>::max()) throw std::overflow_error("X509_STORE_up_ref: reference count overflow");
        if (count.compare_exchange_weak(previous, previous + 1)) return previous > 0;
    }
}

}
