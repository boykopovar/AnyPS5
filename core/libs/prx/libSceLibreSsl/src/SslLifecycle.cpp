// Adapted from the LibreSSL 2.5.4 release tarball (libressl-2.5.4.tar.gz, sha256 107a5b522fbb8318d4c3be668075e5e607296f0a9255d71674caa94571336efa),
// the version identified in docs/dev/debt/libSceLibreSsl.md, which corresponds to the openbsd
// mirror commit 2e2c612066aa993717528a0249c15c51128d61cb. Source files used:
//   ssl/ssl_lib.c - SSL_new, SSL_free, SSL_CTX_new, SSL_CTX_free, SSL_clear, SSL_set_bio, SSL_set_fd, SSL_get_fd
//   ssl/ssl_cert.c - ssl_cert_new, ssl_cert_dup, ssl_cert_free
// The original copyright and licence notices of those files follow unchanged.
/* $OpenBSD: ssl_lib.c,v 1.158 2017/02/28 14:08:49 jsing Exp $ */
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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */
/* $OpenBSD: ssl_cert.c,v 1.64 2017/02/07 02:08:38 beck Exp $ */

#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace SslLayout;

namespace {

struct CertificateKey {
    void* certificate;
    void* privateKey;
    const void* digest;
};

struct Certificate {
    CertificateKey* key;
    int valid;
    std::uint64_t maskKeyExchange;
    std::uint64_t maskAuthentication;
    void* dhTemporary;
    void* dhTemporaryCallback;
    int dhTemporaryAuto;
    void* ecdhTemporary;
    void* ecdhTemporaryCallback;
    int ecdhTemporaryAuto;
    CertificateKey keys[7];
    int references;
};

static_assert(sizeof(CertificateKey) == 24);
static_assert(sizeof(Certificate) == 256);
static_assert(offsetof(Certificate, valid) == 8);
static_assert(offsetof(Certificate, keys) == 80);
static_assert(offsetof(Certificate, references) == 248);
static_assert(offsetof(LibreSslCtx, internal) == 112);
static_assert(offsetof(LibreSsl, internal) == 224);
static_assert(offsetof(LibreSslSslInternal, rwstate) == 156);
static_assert(offsetof(LibreSslSslInternal, shutdown) == 168);
static_assert(offsetof(LibreSslSslInternal, state) == 412);
static_assert(offsetof(LibreSslSslInternal, options) == 8);
static_assert(offsetof(LibreSslSslInternal, mode) == 16);
static_assert(offsetof(LibreSslSslInternal, handshakeFunc) == 56);
static_assert(offsetof(LibreSslSslInternal, msgCallback) == 64);
static_assert(offsetof(LibreSslSslInternal, msgCallbackArg) == 72);
static_assert(offsetof(LibreSslSslInternal, verifyCallback) == 88);
static_assert(offsetof(LibreSslSslInternal, readAhead) == 212);
static_assert(offsetof(LibreSslSslInternal, maxCertList) == 296);
static_assert(offsetof(LibreSslSslInternal, rest) == InternalReadState);
static_assert(sizeof(LibreSslSslInternal) == InternalSize);
static_assert(offsetof(LibreSslCtxInternal, generateSessionId) == 120);
static_assert(offsetof(LibreSslCtxInternal, exData) == 248);
static_assert(offsetof(LibreSslCtxInternal, cert) == 264);
static_assert(offsetof(LibreSslCtxInternal, clientCa) == 272);
static_assert(offsetof(LibreSslCtxInternal, maxCertList) == 280);
static_assert(offsetof(LibreSslCtxInternal, alpnClientProtoList) == 416);
static_assert(offsetof(LibreSslCtxInternal, tlsextEcpointformatlist) == 440);

constexpr std::uint64_t VerifyParamDefault = 0x1;
constexpr std::uint64_t VerifyParamOverwrite = 0x2;
constexpr std::uint64_t VerifyParamResetFlags = 0x4;
constexpr std::uint64_t VerifyParamLocked = 0x8;
constexpr std::uint64_t VerifyParamOnce = 0x10;
constexpr std::uint64_t VerifyFlagUseCheckTime = 0x2;

const LibreSslMethod& MethodOf(const LibreSslCtx& ctx) {
    const auto* method = static_cast<const LibreSslMethod*>(ctx.method);
    if (!method || !method->internal) throw std::invalid_argument("SSL_CTX: null method");
    return *method;
}

const LibreSslMethod& MethodOf(const LibreSsl& ssl) {
    const auto* method = static_cast<const LibreSslMethod*>(ssl.method);
    if (!method || !method->internal) throw std::invalid_argument("SSL: null method");
    return *method;
}

LibreSslCtxInternal& InternalOf(LibreSslCtx& ctx, const char* function) {
    if (!ctx.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *ctx.internal;
}

LibreSslSslInternal& InternalOf(LibreSsl& ssl, const char* function) {
    if (!ssl.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *ssl.internal;
}

Certificate* NewCertificate() {
    auto* certificate = static_cast<Certificate*>(calloc_nid_postfix(1, sizeof(Certificate)));
    if (!certificate) return nullptr;
    certificate->key = &certificate->keys[0];
    certificate->references = 1;
    return certificate;
}

Certificate* DuplicateCertificate(const Certificate& source) {
    if (source.dhTemporary || source.ecdhTemporary) NotImplemented_nid_no_patch("SSL_new: duplicating temporary DH or ECDH parameters");
    for (const CertificateKey& key : source.keys)
        if (key.certificate || key.privateKey) NotImplemented_nid_no_patch("SSL_new: duplicating a certificate or private key");
    if (!source.key) throw std::invalid_argument("SSL_new: certificate without an active key");
    auto* copy = static_cast<Certificate*>(calloc_nid_postfix(1, sizeof(Certificate)));
    if (!copy) return nullptr;
    copy->key = &copy->keys[source.key - &source.keys[0]];
    copy->valid = source.valid;
    copy->maskKeyExchange = source.maskKeyExchange;
    copy->maskAuthentication = source.maskAuthentication;
    copy->dhTemporaryCallback = source.dhTemporaryCallback;
    copy->dhTemporaryAuto = source.dhTemporaryAuto;
    copy->ecdhTemporaryCallback = source.ecdhTemporaryCallback;
    copy->ecdhTemporaryAuto = source.ecdhTemporaryAuto;
    copy->references = 1;
    return copy;
}

void FreeCertificate(Certificate* certificate) {
    if (!certificate) return;
    if (std::atomic_ref<int>(certificate->references).fetch_sub(1) - 1 > 0) return;
    if (certificate->dhTemporary || certificate->ecdhTemporary) NotImplemented_nid_no_patch("freeing temporary DH or ECDH parameters");
    for (const CertificateKey& key : certificate->keys)
        if (key.certificate || key.privateKey) NotImplemented_nid_no_patch("freeing a certificate or private key");
    free_nid_postfix(certificate);
}

LibreSslStack* NewEmptyStack() {
    constexpr int initialNodes = 4;
    auto* stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(calloc_nid_postfix(initialNodes, sizeof(char*)));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    stack->num = 0;
    stack->sorted = 0;
    stack->numAlloc = initialNodes;
    stack->comp = nullptr;
    return stack;
}

void FreeStackShell(LibreSslStack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

void FreeStackWithElements(LibreSslStack* stack, const char* what) {
    if (!stack) return;
    if (stack->num > 0) {
        if (!stack->data) throw std::invalid_argument(std::string(what) + ": null element array");
        for (int index = 0; index < stack->num; ++index)
            if (stack->data[index]) NotImplemented_nid_no_patch((std::string(what) + ": releasing stored objects").c_str());
    }
    FreeStackShell(stack);
}

bool InheritVerifyParam(LibreSslX509VerifyParam& destination, const LibreSslX509VerifyParam* source) {
    if (!source) return true;
    const std::uint64_t inherited = destination.inheritanceFlags | source->inheritanceFlags;
    if (inherited & VerifyParamOnce) destination.inheritanceFlags = 0;
    if (inherited & VerifyParamLocked) return true;
    const bool toDefault = (inherited & VerifyParamDefault) != 0;
    const bool toOverwrite = (inherited & VerifyParamOverwrite) != 0;
    const auto copies = [&](auto destinationValue, auto sourceValue, auto defaultValue) {
        return toOverwrite || (sourceValue != defaultValue && (toDefault || destinationValue == defaultValue));
    };
    if (copies(destination.purpose, source->purpose, 0)) destination.purpose = source->purpose;
    if (copies(destination.trust, source->trust, 0)) destination.trust = source->trust;
    if (copies(destination.depth, source->depth, -1)) destination.depth = source->depth;
    if (toOverwrite || !(destination.flags & VerifyFlagUseCheckTime)) {
        destination.checkTime = source->checkTime;
        destination.flags &= ~VerifyFlagUseCheckTime;
    }
    if (inherited & VerifyParamResetFlags) destination.flags = 0;
    destination.flags |= source->flags;
    if (copies(destination.policies, source->policies, static_cast<void*>(nullptr)) && source->policies)
        NotImplemented_nid_no_patch("SSL_new: inheriting verification policies");
    const LibreSslX509VerifyParamId* id = source->id;
    if (!id || !destination.id) throw std::invalid_argument("SSL_new: verification parameter without identity checks");
    if (id->hosts || id->peerName || id->email || id->ip)
        NotImplemented_nid_no_patch("SSL_new: inheriting verification host, e-mail or address checks");
    return true;
}

void ClearSsl(LibreSsl& ssl) {
    LibreSslSslInternal& internal = InternalOf(ssl, "SSL_clear");
    if (!ssl.method) {
        LibreSslPutSslError_nid_no_patch(&ssl, ReasonNoMethodSpecified);
        return;
    }
    ssl.error = 0;
    Write<int>(&internal, InternalHit, 0);
    internal.shutdown = 0;
    if (Read<int>(&internal, InternalRenegotiate)) {
        LibreSslPutSslError_nid_no_patch(&ssl, ErrorInternalError);
        return;
    }
    Write<int>(&internal, InternalType, 0);
    internal.state = StateBefore | (ssl.server ? StateAccept : StateConnect);
    ssl.version = MethodOf(ssl).internal->version;
    ssl.clientVersion = ssl.version;
    internal.rwstate = RwNothing;
    Write<int>(&internal, InternalReadState, StateReadHeader);
    if (ssl.encReadCtx || ssl.readHash || internal.encWriteCtx || internal.writeHash || internal.aeadReadCtx || internal.aeadWriteCtx)
        NotImplemented_nid_no_patch("SSL_new: a live cipher or hash context");
    MethodOf(ssl).internal->sslClear(&ssl);
}

void SetBio(LibreSsl& ssl, LibreSslBio* readBio, LibreSslBio* writeBio) {
    auto* buffer = static_cast<LibreSslBio*>(ssl.bbio);
    if (buffer && ssl.wbio == buffer) {
        ssl.wbio = buffer->nextBio;
        buffer->nextBio = nullptr;
    }
    auto* currentRead = static_cast<LibreSslBio*>(ssl.rbio);
    auto* currentWrite = static_cast<LibreSslBio*>(ssl.wbio);
    if (currentRead != readBio && currentRead != currentWrite) LibreSslFreeBioChain_nid_no_patch(currentRead);
    if (currentWrite != writeBio) LibreSslFreeBioChain_nid_no_patch(currentWrite);
    ssl.rbio = readBio;
    ssl.wbio = writeBio;
}

void FreeCopiedLists(LibreSslSslInternal& internal) {
    free_nid_postfix(Read<void*>(&internal, InternalEcPointFormats));
    free_nid_postfix(Read<void*>(&internal, InternalSupportedGroups));
    free_nid_postfix(Read<void*>(&internal, InternalAlpnList));
}

void DestroySsl(LibreSsl& ssl, bool methodReady, bool releaseExData) {
    LibreSslSslInternal& internal = InternalOf(ssl, "SSL_free");
    LibreSslFreeVerifyParam_nid_no_patch(static_cast<LibreSslX509VerifyParam*>(ssl.param), "SSL_free");
    if (releaseExData) LibreSslFreeExData_nid_no_patch(ExClassSsl, &ssl, reinterpret_cast<LibreSslExData*>(reinterpret_cast<unsigned char*>(&internal) + InternalExData));
    if (ssl.bbio) NotImplemented_nid_no_patch("SSL_free: write buffering BIO");
    if (ssl.rbio != ssl.wbio) LibreSslFreeBioChain_nid_no_patch(static_cast<LibreSslBio*>(ssl.rbio));
    LibreSslFreeBioChain_nid_no_patch(static_cast<LibreSslBio*>(ssl.wbio));
    FreeStackShell(static_cast<LibreSslStack*>(ssl.cipherList));
    if (ssl.session) NotImplemented_nid_no_patch("SSL_free: SSL_SESSION");
    if (ssl.encReadCtx || ssl.readHash || internal.encWriteCtx || internal.writeHash || internal.aeadReadCtx || internal.aeadWriteCtx)
        NotImplemented_nid_no_patch("SSL_free: a live cipher or hash context");
    FreeCertificate(static_cast<Certificate*>(ssl.cert));
    free_nid_postfix(ssl.tlsextHostname);
    SSL_CTX_free_nid_postfix(static_cast<LibreSslCtx*>(ssl.initialCtx));
    FreeStackWithElements(Read<LibreSslStack*>(&internal, InternalClientCa), "SSL_free: client CA list");
    if (ssl.method && methodReady) MethodOf(ssl).internal->sslFree(&ssl);
    SSL_CTX_free_nid_postfix(static_cast<LibreSslCtx*>(ssl.ctx));
    FreeCopiedLists(internal);
    free_nid_postfix(ssl.internal);
    free_nid_postfix(&ssl);
}

bool CopyList(LibreSslSslInternal& internal, std::size_t pointerOffset, std::size_t lengthOffset, const void* source, std::uint64_t length, std::size_t elementSize, std::size_t allocationElementSize) {
    void* copy = calloc_nid_postfix(static_cast<std::size_t>(length), allocationElementSize);
    if (!copy) return false;
    std::memcpy(copy, source, static_cast<std::size_t>(length) * elementSize);
    Write<void*>(&internal, pointerOffset, copy);
    Write<std::uint64_t>(&internal, lengthOffset, length);
    return true;
}

bool PopulateSsl(LibreSsl& ssl, LibreSslCtx& ctx, LibreSslCtxInternal& ctxInternal) {
    LibreSslSslInternal& internal = *ssl.internal;
    Write<std::uint16_t>(&internal, InternalMinVersion, ctxInternal.minVersion);
    Write<std::uint16_t>(&internal, InternalMaxVersion, ctxInternal.maxVersion);
    internal.options = ctxInternal.options;
    internal.mode = ctxInternal.mode;
    internal.maxCertList = ctxInternal.maxCertList;
    if (ctxInternal.cert) {
        ssl.cert = DuplicateCertificate(*static_cast<const Certificate*>(ctxInternal.cert));
        if (!ssl.cert) return false;
    } else {
        ssl.cert = nullptr;
    }
    internal.readAhead = ctxInternal.readAhead;
    internal.msgCallback = ctxInternal.msgCallback;
    internal.msgCallbackArg = ctxInternal.msgCallbackArg;
    ssl.verifyMode = ctx.verifyMode;
    ssl.sidCtxLength = ctx.sidCtxLength;
    if (ssl.sidCtxLength > sizeof(ssl.sidCtx)) throw std::invalid_argument("SSL_new: session id context length exceeds its buffer");
    std::memcpy(ssl.sidCtx, ctx.sidCtx, sizeof(ssl.sidCtx));
    internal.verifyCallback = ctxInternal.defaultVerifyCallback;
    Write<void*>(&internal, InternalGenerateSessionId, ctxInternal.generateSessionId);
    ssl.param = LibreSslNewVerifyParam_nid_no_patch();
    if (!ssl.param) return false;
    InheritVerifyParam(*static_cast<LibreSslX509VerifyParam*>(ssl.param), static_cast<const LibreSslX509VerifyParam*>(ctx.param));
    Write<int>(&internal, InternalQuietShutdown, ctxInternal.quietShutdown);
    ssl.maxSendFragment = ctxInternal.maxSendFragment;
    std::atomic_ref<int>(ctx.references).fetch_add(1);
    ssl.ctx = &ctx;
    ssl.tlsextStatusType = -1;
    Write<int>(&internal, InternalOcspResponseLength, -1);
    std::atomic_ref<int>(ctx.references).fetch_add(1);
    ssl.initialCtx = &ctx;
    if (ctxInternal.tlsextEcpointformatlist) {
        if (!CopyList(internal, InternalEcPointFormats, InternalEcPointFormatLength, ctxInternal.tlsextEcpointformatlist, ctxInternal.tlsextEcpointformatlistLength, sizeof(std::uint8_t), sizeof(std::uint8_t))) return false;
    }
    if (ctxInternal.tlsextSupportedgroups) {
        if (!CopyList(internal, InternalSupportedGroups, InternalSupportedGroupsLength, ctxInternal.tlsextSupportedgroups, ctxInternal.tlsextSupportedgroupsLength, sizeof(std::uint16_t), sizeof(std::uint16_t*))) return false;
    }
    if (ctxInternal.alpnClientProtoList) {
        void* list = malloc_nid_postfix(ctxInternal.alpnClientProtoListLen);
        if (!list) return false;
        std::memcpy(list, ctxInternal.alpnClientProtoList, ctxInternal.alpnClientProtoListLen);
        Write<void*>(&internal, InternalAlpnList, list);
        Write<unsigned int>(&internal, InternalAlpnListLength, ctxInternal.alpnClientProtoListLen);
    }
    ssl.verifyResult = 0;
    ssl.method = ctx.method;
    return true;
}

bool PopulateContext(LibreSslCtx& ctx, const LibreSslMethod& method) {
    LibreSslCtxInternal& internal = *ctx.internal;
    ctx.method = &method;
    internal.minVersion = method.internal->minVersion;
    internal.maxVersion = method.internal->maxVersion;
    internal.sessionCacheMode = SessionCacheServer;
    internal.sessionCacheSize = SessionCacheSizeDefault;
    ctx.sessionTimeout = method.internal->getTimeout();
    ctx.references = 1;
    internal.maxCertList = MaxCertListDefault;
    ctx.verifyMode = 0;
    ctx.sidCtxLength = 0;
    internal.cert = NewCertificate();
    if (!internal.cert) return false;
    ctx.certStore = X509_STORE_new_nid_postfix();
    if (!ctx.certStore) return false;
    ctx.param = LibreSslNewVerifyParam_nid_no_patch();
    if (!ctx.param) return false;
    internal.clientCa = NewEmptyStack();
    if (!internal.clientCa) return false;
    if (!LibreSslNewExData_nid_no_patch(ExClassSslCtx, &ctx, reinterpret_cast<LibreSslExData*>(&internal.exData))) return false;
    internal.maxSendFragment = MaxPlainLength;
    HostEntropy_nid_no_patch(internal.tlsextTickKeyName, sizeof(internal.tlsextTickKeyName));
    HostEntropy_nid_no_patch(internal.tlsextTickHmacKey, sizeof(internal.tlsextTickHmacKey));
    HostEntropy_nid_no_patch(internal.tlsextTickAesKey, sizeof(internal.tlsextTickAesKey));
    internal.options |= OptionLegacyServerConnect;
    return true;
}

}

extern "C" {

LibreSslCtx* APS5_VABI SSL_CTX_new_nid_postfix(const LibreSslMethod* method) {
    if (!method) {
        LibreSslPutSslErrorX_nid_no_patch(ReasonNullSslMethodPassed);
        return nullptr;
    }
    if (!method->internal) throw std::invalid_argument(std::string(__func__) + ": null method internal state");
    auto* ctx = static_cast<LibreSslCtx*>(calloc_nid_postfix(1, sizeof(LibreSslCtx)));
    if (!ctx) {
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        return nullptr;
    }
    ctx->internal = static_cast<LibreSslCtxInternal*>(calloc_nid_postfix(1, sizeof(LibreSslCtxInternal)));
    if (!ctx->internal) {
        free_nid_postfix(ctx);
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        return nullptr;
    }
    if (SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix() < 0) {
        LibreSslPutSslErrorX_nid_no_patch(ReasonVerificationSetupProblems);
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        SSL_CTX_free_nid_postfix(ctx);
        return nullptr;
    }
    if (!PopulateContext(*ctx, *method)) {
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        SSL_CTX_free_nid_postfix(ctx);
        return nullptr;
    }
    return ctx;
}

void APS5_VABI SSL_CTX_free_nid_postfix(LibreSslCtx* ctx) {
    if (!ctx) return;
    if (std::atomic_ref<int>(ctx->references).fetch_sub(1) - 1 > 0) return;
    LibreSslCtxInternal& internal = InternalOf(*ctx, __func__);
    LibreSslFreeVerifyParam_nid_no_patch(static_cast<LibreSslX509VerifyParam*>(ctx->param), __func__);
    if (internal.sessions) NotImplemented_nid_no_patch("SSL_CTX_free: session cache");
    LibreSslFreeExData_nid_no_patch(ExClassSslCtx, ctx, reinterpret_cast<LibreSslExData*>(&internal.exData));
    X509_STORE_free_nid_postfix(static_cast<LibreSslX509Store*>(ctx->certStore));
    FreeStackShell(static_cast<LibreSslStack*>(ctx->cipherList));
    FreeStackShell(static_cast<LibreSslStack*>(internal.cipherListById));
    FreeCertificate(static_cast<Certificate*>(internal.cert));
    FreeStackWithElements(static_cast<LibreSslStack*>(internal.clientCa), "SSL_CTX_free: client CA list");
    FreeStackWithElements(static_cast<LibreSslStack*>(ctx->extraCerts), "SSL_CTX_free: extra certificates");
    if (internal.srtpProfiles) NotImplemented_nid_no_patch("SSL_CTX_free: SRTP profiles");
    if (internal.clientCertEngine) NotImplemented_nid_no_patch("SSL_CTX_free: client certificate engine");
    free_nid_postfix(internal.tlsextEcpointformatlist);
    free_nid_postfix(internal.tlsextSupportedgroups);
    free_nid_postfix(internal.alpnClientProtoList);
    free_nid_postfix(ctx->internal);
    free_nid_postfix(ctx);
}

LibreSsl* APS5_VABI SSL_new_nid_postfix(LibreSslCtx* ctx) {
    if (!ctx) {
        LibreSslPutSslErrorX_nid_no_patch(ReasonNullSslCtx);
        return nullptr;
    }
    if (!ctx->method) {
        LibreSslPutSslErrorX_nid_no_patch(ReasonSslCtxHasNoDefaultSslVersion);
        return nullptr;
    }
    LibreSslCtxInternal& ctxInternal = InternalOf(*ctx, __func__);
    const LibreSslMethod& method = MethodOf(*ctx);
    auto* ssl = static_cast<LibreSsl*>(calloc_nid_postfix(1, sizeof(LibreSsl)));
    if (!ssl) {
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        return nullptr;
    }
    ssl->internal = static_cast<LibreSslSslInternal*>(calloc_nid_postfix(1, sizeof(LibreSslSslInternal)));
    if (!ssl->internal) {
        free_nid_postfix(ssl);
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        return nullptr;
    }
    bool created = false;
    try {
        created = PopulateSsl(*ssl, *ctx, ctxInternal) && method.internal->sslNew(ssl) != 0;
    } catch (...) {
        DestroySsl(*ssl, false, false);
        throw;
    }
    if (!created) {
        DestroySsl(*ssl, false, true);
        LibreSslPutSslErrorX_nid_no_patch(ErrorMallocFailure);
        return nullptr;
    }
    ssl->references = 1;
    ssl->server = LibreSslIsUndefinedFunction_nid_no_patch(method.internal->sslAccept) ? 0 : 1;
    ClearSsl(*ssl);
    LibreSslNewExData_nid_no_patch(ExClassSsl, ssl, reinterpret_cast<LibreSslExData*>(reinterpret_cast<unsigned char*>(ssl->internal) + InternalExData));
    return ssl;
}

void APS5_VABI SSL_free_nid_postfix(LibreSsl* ssl) {
    if (!ssl) return;
    if (std::atomic_ref<int>(ssl->references).fetch_sub(1) - 1 > 0) return;
    DestroySsl(*ssl, true, true);
}

int APS5_VABI SSL_set_fd_nid_postfix(LibreSsl* ssl, int fd) {
    if (!ssl) throw std::invalid_argument(std::string(__func__) + ": null connection");
    LibreSslBio* bio = LibreSslNewSocketBio_nid_no_patch(fd);
    if (!bio) {
        LibreSslPutSslError_nid_no_patch(ssl, ErrorBufferLibrary);
        return 0;
    }
    SetBio(*ssl, bio, bio);
    return 1;
}

int APS5_VABI SSL_get_fd_nid_postfix(const LibreSsl* ssl) {
    if (!ssl) throw std::invalid_argument(std::string(__func__) + ": null connection");
    return LibreSslBioDescriptor_nid_no_patch(static_cast<const LibreSslBio*>(ssl->rbio));
}

}
