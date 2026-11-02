// Adapted from the LibreSSL 2.5.4 release tarball (libressl-2.5.4.tar.gz, sha256 107a5b522fbb8318d4c3be668075e5e607296f0a9255d71674caa94571336efa),
// the version identified in docs/dev/debt/libSceLibreSsl.md, which corresponds to the openbsd
// mirror commit 2e2c612066aa993717528a0249c15c51128d61cb. Source files used:
//   ssl/t1_clnt.c - TLS_client_method, SSLv23_client_method
//   ssl/t1_srvr.c - SSLv23_server_method
//   ssl/d1_clnt.c - DTLSv1_client_method
//   ssl/d1_srvr.c - DTLSv1_server_method
//   ssl/t1_lib.c - TLSv1_2_enc_data
//   ssl/d1_lib.c - DTLSv1_enc_data
//   ssl/s3_lib.c - ssl3_new, ssl3_clear, ssl3_free
//   ssl/ssl_err.c - SSL_state_func_code
// The original copyright and licence notices of those files follow unchanged.
/* $OpenBSD: t1_clnt.c,v 1.24 2017/01/26 05:31:25 jsing Exp $ */
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
/* $OpenBSD: t1_srvr.c,v 1.25 2017/01/26 05:31:25 jsing Exp $ */
/* $OpenBSD: d1_clnt.c,v 1.74 2017/02/07 02:08:38 beck Exp $ */
/*
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005.
 */
/* ====================================================================
 * Copyright (c) 1999-2007 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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
/* $OpenBSD: d1_srvr.c,v 1.86 2017/03/10 16:03:27 jsing Exp $ */
/* $OpenBSD: t1_lib.c,v 1.115 2017/02/07 02:08:38 beck Exp $ */
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
/* $OpenBSD: d1_lib.c,v 1.41 2017/02/07 02:08:38 beck Exp $ */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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
/* $OpenBSD: s3_lib.c,v 1.138 2017/03/10 16:03:27 jsing Exp $ */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
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
/* $OpenBSD: ssl_err.c,v 1.33 2017/02/07 02:08:38 beck Exp $ */
/* ====================================================================
 * Copyright (c) 1999-2011 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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
/* NOTE: this file was auto generated by the mkerr.pl script: any changes
 * made to it will be overwritten when the script next updates this file,
 * only reason strings will be preserved.
 */

#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

static_assert(sizeof(LibreSslEncMethod) == 16);
static_assert(offsetof(LibreSslEncMethod, encFlags) == 8);
static_assert(sizeof(LibreSslMethodInternal) == 160);
static_assert(offsetof(LibreSslMethodInternal, minVersion) == 4);
static_assert(offsetof(LibreSslMethodInternal, maxVersion) == 6);
static_assert(offsetof(LibreSslMethodInternal, sslNew) == 8);
static_assert(offsetof(LibreSslMethodInternal, sslAccept) == 32);
static_assert(offsetof(LibreSslMethodInternal, sslConnect) == 40);
static_assert(offsetof(LibreSslMethodInternal, sslShutdown) == 72);
static_assert(offsetof(LibreSslMethodInternal, sslRenegotiate) == 80);
static_assert(offsetof(LibreSslMethodInternal, sslPending) == 120);
static_assert(offsetof(LibreSslMethodInternal, getSslMethod) == 128);
static_assert(offsetof(LibreSslMethodInternal, getTimeout) == 136);
static_assert(offsetof(LibreSslMethodInternal, sslVersion) == 144);
static_assert(offsetof(LibreSslMethodInternal, ssl3Enc) == 152);
static_assert(sizeof(LibreSslMethod) == 48);
static_assert(offsetof(LibreSslMethod, internal) == 40);

using namespace SslLayout;

namespace {

constexpr unsigned int EncFlagExplicitIv = 1u << 0;
constexpr unsigned int EncFlagSigalgs = 1u << 1;
constexpr unsigned int EncFlagSha256Prf = 1u << 2;
constexpr unsigned int EncFlagTls1_2Ciphers = 1u << 4;

const LibreSslMethod& MethodOf(const LibreSsl* ssl) {
    const auto* method = static_cast<const LibreSslMethod*>(ssl->method);
    if (!method || !method->internal) throw std::invalid_argument("SSL_METHOD: null method");
    return *method;
}

int APS5_VABI UndefinedFunction_nid_no_patch(LibreSsl* ssl) {
    LibreSslPutSslError_nid_no_patch(ssl, ErrorShouldNotHaveBeenCalled);
    return 0;
}

int APS5_VABI UndefinedVoidFunction_nid_no_patch(void) {
    LibreSslPutSslErrorX_nid_no_patch(ErrorShouldNotHaveBeenCalled);
    return 0;
}

int APS5_VABI AlwaysOk_nid_no_patch(LibreSsl*) {
    return 1;
}

std::int64_t APS5_VABI TlsDefaultTimeout_nid_no_patch(void) {
    return DefaultTimeout;
}

int APS5_VABI TlsNew_nid_no_patch(LibreSsl* ssl) {
    void* state = calloc_nid_postfix(1, Ssl3Size);
    void* internal = calloc_nid_postfix(1, Ssl3InternalSize);
    if (!state || !internal) {
        free_nid_postfix(internal);
        free_nid_postfix(state);
        return 0;
    }
    Write<void*>(state, Ssl3InternalPointer, internal);
    ssl->s3 = state;
    MethodOf(ssl).internal->sslClear(ssl);
    MethodOf(ssl).internal->sslClear(ssl);
    return 1;
}

void APS5_VABI TlsClear_nid_no_patch(LibreSsl* ssl) {
    void* state = ssl->s3;
    void* internal = Read<void*>(state, Ssl3InternalPointer);
    std::memset(internal, 0, Ssl3InternalSize);
    std::memset(state, 0, Ssl3Size);
    Write<void*>(state, Ssl3InternalPointer, internal);
    Write<unsigned int>(ssl->internal, InternalPacketLength, 0);
    ssl->version = MethodOf(ssl).internal->version;
}

void APS5_VABI TlsFree_nid_no_patch(LibreSsl* ssl) {
    void* state = ssl->s3;
    if (!state) return;
    free_nid_postfix(Read<void*>(state, Ssl3InternalPointer));
    free_nid_postfix(state);
    ssl->s3 = nullptr;
}

int APS5_VABI DtlsNew_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_new: DTLS connection state");
}

void APS5_VABI DtlsClear_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_clear: DTLS connection state");
}

void APS5_VABI DtlsFree_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_free: DTLS connection state");
}

int APS5_VABI ConnectUnavailable_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_connect");
}

int APS5_VABI AcceptUnavailable_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_accept");
}

int APS5_VABI ReadUnavailable_nid_no_patch(LibreSsl*, void*, int) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_read");
}

int APS5_VABI PeekUnavailable_nid_no_patch(LibreSsl*, void*, int) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_peek");
}

int APS5_VABI WriteUnavailable_nid_no_patch(LibreSsl*, const void*, int) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_write");
}

int APS5_VABI ShutdownUnavailable_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_shutdown");
}

int APS5_VABI PendingUnavailable_nid_no_patch(const LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_pending");
}

int APS5_VABI RenegotiateUnavailable_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_renegotiate");
}

int APS5_VABI RenegotiateCheckUnavailable_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_renegotiate_check");
}

std::int64_t APS5_VABI GetMessageUnavailable_nid_no_patch(LibreSsl*, int, int, int, std::int64_t, int*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_get_message");
}

int APS5_VABI ReadBytesUnavailable_nid_no_patch(LibreSsl*, int, unsigned char*, int, int) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_read_bytes");
}

int APS5_VABI WriteBytesUnavailable_nid_no_patch(LibreSsl*, int, const void*, int) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_write_bytes");
}

const LibreSslMethod* APS5_VABI GetMethodUnavailable_nid_no_patch(int) {
    NotImplemented_nid_no_patch("SSL_METHOD get_ssl_method");
}

int APS5_VABI EncUnavailable_nid_no_patch(LibreSsl*, int) {
    NotImplemented_nid_no_patch("SSL3_ENC_METHOD enc");
}

int APS5_VABI DispatchAlertUnavailable_nid_no_patch(LibreSsl*) {
    NotImplemented_nid_no_patch("SSL_METHOD ssl_dispatch_alert");
}

int APS5_VABI NumCiphersUnavailable_nid_no_patch(void) {
    NotImplemented_nid_no_patch("SSL_METHOD num_ciphers");
}

const void* APS5_VABI GetCipherUnavailable_nid_no_patch(unsigned int) {
    NotImplemented_nid_no_patch("SSL_METHOD get_cipher");
}

const void* APS5_VABI GetCipherByCharUnavailable_nid_no_patch(const unsigned char*) {
    NotImplemented_nid_no_patch("SSL_METHOD get_cipher_by_char");
}

int APS5_VABI PutCipherByCharUnavailable_nid_no_patch(const void*, unsigned char*) {
    NotImplemented_nid_no_patch("SSL_METHOD put_cipher_by_char");
}

const LibreSslEncMethod tlsEnc{reinterpret_cast<void*>(EncUnavailable_nid_no_patch), EncFlagExplicitIv | EncFlagSigalgs | EncFlagSha256Prf | EncFlagTls1_2Ciphers};
const LibreSslEncMethod dtlsEnc{reinterpret_cast<void*>(EncUnavailable_nid_no_patch), EncFlagExplicitIv};

const LibreSslMethodInternal tlsClientInternal{
    Tls1_2Version, Tls1Version, Tls1_2Version,
    TlsNew_nid_no_patch, TlsClear_nid_no_patch, TlsFree_nid_no_patch,
    UndefinedFunction_nid_no_patch, ConnectUnavailable_nid_no_patch,
    ReadUnavailable_nid_no_patch, PeekUnavailable_nid_no_patch, WriteUnavailable_nid_no_patch, ShutdownUnavailable_nid_no_patch,
    UndefinedFunction_nid_no_patch, AlwaysOk_nid_no_patch,
    GetMessageUnavailable_nid_no_patch, ReadBytesUnavailable_nid_no_patch, WriteBytesUnavailable_nid_no_patch,
    PendingUnavailable_nid_no_patch, GetMethodUnavailable_nid_no_patch, TlsDefaultTimeout_nid_no_patch, UndefinedVoidFunction_nid_no_patch, &tlsEnc};

const LibreSslMethodInternal tlsServerInternal{
    Tls1_2Version, Tls1Version, Tls1_2Version,
    TlsNew_nid_no_patch, TlsClear_nid_no_patch, TlsFree_nid_no_patch,
    AcceptUnavailable_nid_no_patch, UndefinedFunction_nid_no_patch,
    ReadUnavailable_nid_no_patch, PeekUnavailable_nid_no_patch, WriteUnavailable_nid_no_patch, ShutdownUnavailable_nid_no_patch,
    UndefinedFunction_nid_no_patch, AlwaysOk_nid_no_patch,
    GetMessageUnavailable_nid_no_patch, ReadBytesUnavailable_nid_no_patch, WriteBytesUnavailable_nid_no_patch,
    PendingUnavailable_nid_no_patch, GetMethodUnavailable_nid_no_patch, TlsDefaultTimeout_nid_no_patch, UndefinedVoidFunction_nid_no_patch, &tlsEnc};

const LibreSslMethodInternal dtlsClientInternal{
    Dtls1Version, Dtls1Version, Dtls1Version,
    DtlsNew_nid_no_patch, DtlsClear_nid_no_patch, DtlsFree_nid_no_patch,
    UndefinedFunction_nid_no_patch, ConnectUnavailable_nid_no_patch,
    ReadUnavailable_nid_no_patch, PeekUnavailable_nid_no_patch, WriteUnavailable_nid_no_patch, ShutdownUnavailable_nid_no_patch,
    RenegotiateUnavailable_nid_no_patch, RenegotiateCheckUnavailable_nid_no_patch,
    GetMessageUnavailable_nid_no_patch, ReadBytesUnavailable_nid_no_patch, WriteBytesUnavailable_nid_no_patch,
    PendingUnavailable_nid_no_patch, GetMethodUnavailable_nid_no_patch, TlsDefaultTimeout_nid_no_patch, UndefinedVoidFunction_nid_no_patch, &dtlsEnc};

const LibreSslMethodInternal dtlsServerInternal{
    Dtls1Version, Dtls1Version, Dtls1Version,
    DtlsNew_nid_no_patch, DtlsClear_nid_no_patch, DtlsFree_nid_no_patch,
    AcceptUnavailable_nid_no_patch, UndefinedFunction_nid_no_patch,
    ReadUnavailable_nid_no_patch, PeekUnavailable_nid_no_patch, WriteUnavailable_nid_no_patch, ShutdownUnavailable_nid_no_patch,
    RenegotiateUnavailable_nid_no_patch, RenegotiateCheckUnavailable_nid_no_patch,
    GetMessageUnavailable_nid_no_patch, ReadBytesUnavailable_nid_no_patch, WriteBytesUnavailable_nid_no_patch,
    PendingUnavailable_nid_no_patch, GetMethodUnavailable_nid_no_patch, TlsDefaultTimeout_nid_no_patch, UndefinedVoidFunction_nid_no_patch, &dtlsEnc};

const LibreSslMethod tlsClientMethod{
    DispatchAlertUnavailable_nid_no_patch, NumCiphersUnavailable_nid_no_patch, GetCipherUnavailable_nid_no_patch,
    GetCipherByCharUnavailable_nid_no_patch, PutCipherByCharUnavailable_nid_no_patch, &tlsClientInternal};

const LibreSslMethod tlsServerMethod{
    DispatchAlertUnavailable_nid_no_patch, NumCiphersUnavailable_nid_no_patch, GetCipherUnavailable_nid_no_patch,
    GetCipherByCharUnavailable_nid_no_patch, PutCipherByCharUnavailable_nid_no_patch, &tlsServerInternal};

const LibreSslMethod dtlsClientMethod{
    DispatchAlertUnavailable_nid_no_patch, NumCiphersUnavailable_nid_no_patch, GetCipherUnavailable_nid_no_patch,
    GetCipherByCharUnavailable_nid_no_patch, PutCipherByCharUnavailable_nid_no_patch, &dtlsClientInternal};

const LibreSslMethod dtlsServerMethod{
    DispatchAlertUnavailable_nid_no_patch, NumCiphersUnavailable_nid_no_patch, GetCipherUnavailable_nid_no_patch,
    GetCipherByCharUnavailable_nid_no_patch, PutCipherByCharUnavailable_nid_no_patch, &dtlsServerInternal};

}

extern "C" {

bool LibreSslIsUndefinedFunction_nid_no_patch(SslStateFunction function) {
    return function == UndefinedFunction_nid_no_patch;
}

int LibreSslStateFunction_nid_no_patch(int state) {
    switch (state) {
    case 0: return ErrorFunctionAny;
    case StateBefore: return 73;
    case StateAccept: return 74;
    case StateConnect: return 75;
    case StateOk: return 76;
    case StateConnect | StateAccept | 0x04: return 77;
    case StateBefore | StateConnect: return 78;
    case StateOk | StateConnect: return 79;
    case StateBefore | StateAccept: return 80;
    case StateOk | StateAccept: return 81;
    default: NotImplemented_nid_no_patch("SSLerror: handshake state not modelled");
    }
}

void LibreSslPutSslError_nid_no_patch(const LibreSsl* ssl, int reason) {
    if (!ssl || !ssl->internal) throw std::invalid_argument("SSLerror: null connection state");
    LibreSslPutError_nid_no_patch(ErrorLibrarySsl, LibreSslStateFunction_nid_no_patch(ssl->internal->state), reason);
}

void LibreSslPutSslErrorX_nid_no_patch(int reason) {
    LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionAny, reason);
}

const LibreSslMethod* APS5_VABI SSLv23_client_method_nid_postfix(void) {
    return TLS_client_method_nid_postfix();
}

const LibreSslMethod* APS5_VABI SSLv23_server_method_nid_postfix(void) {
    return &tlsServerMethod;
}

const LibreSslMethod* APS5_VABI TLS_client_method_nid_postfix(void) {
    return &tlsClientMethod;
}

const LibreSslMethod* APS5_VABI DTLSv1_client_method_nid_postfix(void) {
    return &dtlsClientMethod;
}

const LibreSslMethod* APS5_VABI DTLSv1_server_method_nid_postfix(void) {
    return &dtlsServerMethod;
}

}
