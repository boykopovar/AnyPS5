// Adapted from the LibreSSL 2.5.4 release tarball (libressl-2.5.4.tar.gz, sha256 107a5b522fbb8318d4c3be668075e5e607296f0a9255d71674caa94571336efa),
// the version identified in docs/dev/debt/libSceLibreSsl.md, which corresponds to the openbsd
// mirror commit 2e2c612066aa993717528a0249c15c51128d61cb. Source files used:
//   ssl/ssl_lib.c - SSL_connect, SSL_accept, SSL_read, SSL_peek, SSL_write, SSL_shutdown, SSL_pending, SSL_get_error
//   ssl/ssl_clnt.c - ssl3_connect handshake states and failures
//   ssl/ssl_srvr.c - ssl3_accept handshake states and failures
//   ssl/ssl_pkt.c - ssl3_read_bytes, ssl3_write_bytes, ssl3_peek, ssl3_pending
//   ssl/s3_lib.c - ssl3_read, ssl3_write, ssl3_shutdown
//   ssl/ssl_versions.c - ssl_enabled_version_range, ssl_clamp_version_range
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
/* $OpenBSD: ssl_clnt.c,v 1.11 2017/03/10 16:03:27 jsing Exp $ */
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
/* $OpenBSD: ssl_srvr.c,v 1.11.4.1 2017/04/29 23:41:32 jsing Exp $ */
/* $OpenBSD: ssl_pkt.c,v 1.10 2017/02/07 02:08:38 beck Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: s3_lib.c,v 1.138 2017/03/10 16:03:27 jsing Exp $ */
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
// ssl/ssl_versions.c carries no $OpenBSD revision line in this release (file sha256 e51213158cd98032...); its notice follows.
/*
 * Copyright (c) 2016, 2017 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libSceLibreSsl/include/TlsEngine.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include <mbedtls/x509.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace SslLayout;
using EngineResult = LibreSslTls::Result;
using Tls = LibreSslTls::Connection;

namespace {

struct Session {
    std::unique_ptr<Tls> tls;
    bool failed = false;
    std::uint64_t receivedSeen = 0;
    std::uint64_t sentSeen = 0;
};

std::mutex registryMutex;
std::unordered_map<const LibreSsl*, Session> registry;

Session* Find(const LibreSsl* ssl) {
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto found = registry.find(ssl);
    return found == registry.end() ? nullptr : &found->second;
}

Session& Create(const LibreSsl* ssl, std::unique_ptr<Tls> tls) {
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto [position, inserted] = registry.emplace(ssl, Session{std::move(tls)});
    if (!inserted) throw std::logic_error("SSL: a TLS connection already exists for this SSL");
    return position->second;
}

[[noreturn]] void Unmodelled(const char* function, const std::string& what) {
    NotImplemented_nid_no_patch((std::string(function) + ": " + what).c_str());
}

LibreSsl& SslOf(LibreSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

LibreSslSslInternal& InternalOf(LibreSsl& ssl, const char* function) {
    if (!ssl.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *ssl.internal;
}

const LibreSslMethodInternal& MethodOf(const LibreSsl& ssl, const char* function) {
    const auto* method = static_cast<const LibreSslMethod*>(ssl.method);
    if (!method || !method->internal) throw std::invalid_argument(std::string(function) + ": null method");
    return *method->internal;
}

void* Ssl3InternalOf(const LibreSsl& ssl, const char* function) {
    if (!ssl.s3) throw std::invalid_argument(std::string(function) + ": null SSL3 state");
    void* internal = Read<void*>(ssl.s3, Ssl3InternalPointer);
    if (!internal) throw std::invalid_argument(std::string(function) + ": null SSL3 internal state");
    return internal;
}

void SetGuestErrno(int value) {
    *__error_nid_postfix() = value;
}

void SetHostErrno(int hostError) {
    SetGuestErrno(GuestSockets::GuestErrorFromHost(hostError));
}

LibreSslBio& SharedBio(LibreSsl& ssl, const char* function) {
    auto* read = static_cast<LibreSslBio*>(ssl.rbio);
    auto* write = static_cast<LibreSslBio*>(ssl.wbio);
    if (!read || !write) Unmodelled(function, "the connection has no BIO");
    if (read != write) Unmodelled(function, "separate read and write BIOs");
    return *read;
}

void SetRetry(LibreSslBio& bio, int direction) {
    bio.flags &= ~(BioFlagRead | BioFlagWrite | BioFlagIoSpecial | BioFlagShouldRetry);
    if (direction) bio.flags |= direction | BioFlagShouldRetry;
}

void Account(LibreSslBio& bio, Session& session) {
    const std::uint64_t received = session.tls->BytesReceived();
    const std::uint64_t sent = session.tls->BytesSent();
    bio.numRead += received - session.receivedSeen;
    bio.numWrite += sent - session.sentSeen;
    session.receivedSeen = received;
    session.sentSeen = sent;
}

bool Wait(LibreSslSslInternal& internal, LibreSslBio& bio, EngineResult result, const Tls& tls) {
    if (result != EngineResult::WantRead && result != EngineResult::WantWrite) return false;
    if (tls.SystemError()) SetHostErrno(tls.SystemError());
    internal.rwstate = result == EngineResult::WantRead ? RwReading : RwWriting;
    SetRetry(bio, result == EngineResult::WantRead ? BioFlagRead : BioFlagWrite);
    return true;
}

void FollowDescriptor(LibreSslBio& bio, Tls& tls) {
    const int descriptor = LibreSslBioDescriptor_nid_no_patch(&bio);
    if (descriptor == tls.Socket()) return;
    if (descriptor < 0) {
        tls.DetachSocket();
        return;
    }
    try {
        tls.SetSocket(descriptor);
    } catch (const std::invalid_argument&) {
        tls.DetachSocket();
    }
}

void Succeeded(LibreSslSslInternal& internal, LibreSslBio& bio) {
    internal.rwstate = RwNothing;
    SetRetry(bio, 0);
}

int SystemFailure(LibreSslSslInternal& internal, LibreSslBio& bio, const Tls& tls) {
    SetRetry(bio, 0);
    switch (tls.LastDirection()) {
    case LibreSslTls::Direction::Read: internal.rwstate = RwReading; break;
    case LibreSslTls::Direction::Write: internal.rwstate = RwWriting; break;
    case LibreSslTls::Direction::None: internal.rwstate = RwNothing; break;
    }
    if (!tls.SystemError()) return 0;
    SetHostErrno(tls.SystemError());
    return -1;
}

void ReceivedCloseNotify(LibreSsl& ssl, LibreSslSslInternal& internal, const char* function) {
    internal.shutdown |= ShutdownReceived;
    Write<int>(Ssl3InternalOf(ssl, function), Ssl3InternalWarnAlert, AlertCloseNotify);
}

bool ClampVersionRange(std::uint16_t& low, std::uint16_t& high, std::uint16_t clampLow, std::uint16_t clampHigh) {
    if (clampLow > clampHigh || low > high) return false;
    if (clampHigh < low || clampLow > high) return false;
    low = std::max(low, clampLow);
    high = std::min(high, clampHigh);
    return true;
}

void RequireTls12(const LibreSsl& ssl, const LibreSslSslInternal& internal, const char* function) {
    const std::uint64_t options = internal.options;
    std::uint16_t low = 0;
    std::uint16_t high = Tls1_2Version;
    if (!(options & OptionNoTlsV1)) low = Tls1Version;
    else if (!(options & OptionNoTlsV1_1)) low = Tls1_1Version;
    else if (!(options & OptionNoTlsV1_2)) low = Tls1_2Version;
    if ((options & OptionNoTlsV1_2) && low < Tls1_2Version) high = Tls1_1Version;
    if ((options & OptionNoTlsV1_1) && low < Tls1_1Version) high = Tls1Version;
    if ((options & OptionNoTlsV1) && low < Tls1Version) high = 0;
    const LibreSslMethodInternal& method = MethodOf(ssl, function);
    if (low == 0 || high == 0 || !ClampVersionRange(low, high, Read<std::uint16_t>(&internal, InternalMinVersion), Read<std::uint16_t>(&internal, InternalMaxVersion)) ||
        !ClampVersionRange(low, high, method.minVersion, method.maxVersion))
        Unmodelled(function, "no protocol version is enabled");
    if (high < Tls1_2Version) Unmodelled(function, "TLS 1.2 is disabled and the engine speaks only TLS 1.2");
}

bool Populated(const void* stack) {
    return stack && static_cast<const LibreSslStack*>(stack)->num > 0;
}

void RequireModelled(const LibreSsl& ssl, const LibreSslSslInternal& internal, bool server, const char* function) {
    if (!ssl.ctx || !static_cast<const LibreSslCtx*>(ssl.ctx)->internal) throw std::invalid_argument(std::string(function) + ": null context");
    const LibreSslCtx& ctx = *static_cast<const LibreSslCtx*>(ssl.ctx);
    if (internal.msgCallback) Unmodelled(function, "a message callback is set");
    if (ctx.internal->infoCallback) Unmodelled(function, "an info callback is set");
    if (ssl.session) Unmodelled(function, "an SSL_SESSION is set");
    if (server) {
        if (ssl.verifyMode & LibreSslTls::SslVerifyPeer) Unmodelled(function, "client certificate verification is requested");
        return;
    }
    if (internal.verifyCallback) Unmodelled(function, "a verify callback is set");
    if (ctx.internal->appVerifyCallback) Unmodelled(function, "a certificate verify callback is set");
    if (Read<void*>(&internal, InternalAlpnList)) Unmodelled(function, "an ALPN protocol list is set");
    if (ssl.tlsextStatusType != -1) Unmodelled(function, "a certificate status request is set");
    const auto* param = static_cast<const LibreSslX509VerifyParam*>(ssl.param);
    if (!param) throw std::invalid_argument(std::string(function) + ": null verification parameters");
    if (param->depth != -1) Unmodelled(function, "a verification depth limit is set");
    const auto* store = static_cast<const LibreSslX509Store*>(ctx.certStore);
    if (!store) throw std::invalid_argument(std::string(function) + ": null certificate store");
    if (Populated(store->objects) || Populated(store->lookupMethods)) Unmodelled(function, "the certificate store holds trusted certificates or lookups");
}

std::unique_ptr<Tls> MakeConnection(LibreSsl& ssl, bool server, const char* function) {
    const LibreSslSslInternal& internal = InternalOf(ssl, function);
    RequireTls12(ssl, internal, function);
    RequireModelled(ssl, internal, server, function);
    LibreSslBio& bio = SharedBio(ssl, function);
    const int descriptor = LibreSslBioDescriptor_nid_no_patch(&bio);
    auto tls = std::make_unique<Tls>(server);
    try {
        tls->SetSocket(descriptor);
    } catch (const std::invalid_argument&) {
        Unmodelled(function, "descriptor " + std::to_string(descriptor) + " is not a guest socket");
    }
    tls->SetVerifyMode(!server && ssl.verifyMode != 0 ? LibreSslTls::SslVerifyPeer : LibreSslTls::SslVerifyNone);
    if (!server && ssl.tlsextHostname) tls->SetServerName(ssl.tlsextHostname);
    return tls;
}

std::int64_t VerifyResult(const Tls& tls, const char* function) {
    if (!(tls.VerifyFlags() & MBEDTLS_X509_BADCERT_NOT_TRUSTED)) Unmodelled(function, "the engine verified a chain against an empty trust store");
    switch (tls.PeerChain()) {
    case LibreSslTls::ChainShape::SelfSignedLeaf: return VerifyErrorDepthZeroSelfSigned;
    case LibreSslTls::ChainShape::EndsInSelfSigned: return VerifyErrorSelfSignedInChain;
    case LibreSslTls::ChainShape::Incomplete: return VerifyErrorUnableToGetIssuerLocally;
    case LibreSslTls::ChainShape::None: break;
    }
    Unmodelled(function, "the engine saw no peer certificate chain");
}

int HandshakeFailure(LibreSsl& ssl, LibreSslSslInternal& internal, LibreSslBio& bio, Session& session, bool server, const char* function) {
    const Tls& tls = *session.tls;
    switch (tls.FailureKind()) {
    case LibreSslTls::Failure::VerifyFailed:
        if (server) break;
        ssl.verifyResult = VerifyResult(tls, function);
        internal.state = StateConnectCertB;
        Succeeded(internal, bio);
        LibreSslPutSslError_nid_no_patch(&ssl, ReasonCertificateVerifyFailed);
        session.failed = true;
        return -1;
    case LibreSslTls::Failure::NoSharedCipher:
        internal.state = StateAcceptHelloC;
        Succeeded(internal, bio);
        LibreSslPutSslError_nid_no_patch(&ssl, ReasonNoSharedCipher);
        session.failed = true;
        return -1;
    case LibreSslTls::Failure::BadWriteRetry:
    case LibreSslTls::Failure::Other:
    case LibreSslTls::Failure::None: break;
    }
    Unmodelled(function, "the engine failed the handshake: " + tls.FailureText());
}

int Handshake(LibreSsl& ssl, bool server, const char* function) {
    LibreSslSslInternal& internal = InternalOf(ssl, function);
    ERR_clear_error_nid_postfix();
    SetGuestErrno(0);
    if (!(internal.state & StateInit) || (internal.state & StateBefore)) LibreSslClear_nid_no_patch(&ssl);
    ssl.server = server ? 1 : 0;
    Session* session = Find(&ssl);
    if (!session) {
        if (!(internal.state & StateBefore)) throw std::logic_error(std::string(function) + ": a started handshake has no TLS connection");
        session = &Create(&ssl, MakeConnection(ssl, server, function));
        internal.state = server ? StateAcceptHelloA : StateConnectHelloA;
    } else if (session->failed) {
        Unmodelled(function, "continuing a handshake that already failed");
    }
    LibreSslBio& bio = SharedBio(ssl, function);
    FollowDescriptor(bio, *session->tls);
    const EngineResult result = session->tls->Handshake();
    Account(bio, *session);
    if (Wait(internal, bio, result, *session->tls)) return -1;
    switch (result) {
    case EngineResult::Done:
        if (!server) ssl.verifyResult = VerifyResult(*session->tls, function);
        internal.state = StateOk;
        Succeeded(internal, bio);
        return 1;
    case EngineResult::ZeroReturn:
        ReceivedCloseNotify(ssl, internal, function);
        internal.rwstate = RwReading;
        SetRetry(bio, 0);
        return 0;
    case EngineResult::Syscall: return SystemFailure(internal, bio, *session->tls);
    case EngineResult::Ssl: return HandshakeFailure(ssl, internal, bio, *session, server, function);
    case EngineResult::WantRead:
    case EngineResult::WantWrite: break;
    }
    throw std::logic_error(std::string(function) + ": unhandled engine result");
}

int RunHandshakeFunction(LibreSsl& ssl, LibreSslSslInternal& internal) {
    if (!internal.handshakeFunc) throw std::invalid_argument("SSL: no handshake function");
    const int result = reinterpret_cast<SslStateFunction>(internal.handshakeFunc)(&ssl);
    if (result == 0) {
        LibreSslPutSslError_nid_no_patch(&ssl, ReasonSslHandshakeFailure);
        return -1;
    }
    return result;
}

Session& Established(LibreSsl& ssl, const char* function) {
    Session* session = Find(&ssl);
    if (!session) throw std::logic_error(std::string(function) + ": a finished handshake has no TLS connection");
    if (session->failed) Unmodelled(function, "I/O after a failed handshake");
    return *session;
}

int ReadApplicationData(LibreSsl& ssl, void* buffer, int length, bool peek, const char* function) {
    LibreSslSslInternal& internal = InternalOf(ssl, function);
    SetGuestErrno(0);
    if (length < 0) {
        LibreSslPutSslError_nid_no_patch(&ssl, ErrorInternalError);
        return -1;
    }
    if (internal.state & StateInit) {
        const int result = RunHandshakeFunction(ssl, internal);
        if (result < 0) return result;
    }
    Session& session = Established(ssl, function);
    LibreSslBio& bio = SharedBio(ssl, function);
    FollowDescriptor(bio, *session.tls);
    std::size_t transferred = 0;
    const EngineResult result = session.tls->Read(buffer, static_cast<std::size_t>(length), transferred, peek);
    Account(bio, session);
    if (Wait(internal, bio, result, *session.tls)) return -1;
    switch (result) {
    case EngineResult::Done:
        Succeeded(internal, bio);
        return static_cast<int>(transferred);
    case EngineResult::ZeroReturn:
        ReceivedCloseNotify(ssl, internal, function);
        Succeeded(internal, bio);
        return 0;
    case EngineResult::Syscall: return SystemFailure(internal, bio, *session.tls);
    case EngineResult::Ssl: Unmodelled(function, "the engine failed the read: " + session.tls->FailureText());
    case EngineResult::WantRead:
    case EngineResult::WantWrite: break;
    }
    throw std::logic_error(std::string(function) + ": unhandled engine result");
}

void SetAcceptState(LibreSsl& ssl, LibreSslSslInternal& internal, const char* function) {
    const LibreSslMethodInternal& method = MethodOf(ssl, function);
    if (ssl.encReadCtx || ssl.readHash || internal.encWriteCtx || internal.aeadReadCtx || internal.aeadWriteCtx || internal.writeHash)
        Unmodelled(function, "freeing a live cipher or hash context");
    ssl.server = 1;
    internal.shutdown = 0;
    internal.state = StateAccept | StateBefore;
    internal.handshakeFunc = reinterpret_cast<void*>(method.sslAccept);
}

}

extern "C" {

std::int64_t LibreSslSetTlsExtHostName_nid_no_patch(LibreSsl& connection, std::int64_t type, const void* name) {
    if (type != SslLayout::TlsExtNameTypeHostName) {
        LibreSslPutSslError_nid_no_patch(&connection, SslLayout::ReasonServerNameTypeInvalid);
        return 0;
    }
    free_nid_postfix(connection.tlsextHostname);
    connection.tlsextHostname = nullptr;
    if (!name) return 1;
    const std::size_t length = std::strlen(static_cast<const char*>(name));
    if (length > SslLayout::TlsExtMaxHostNameLength) {
        LibreSslPutSslError_nid_no_patch(&connection, SslLayout::ReasonServerNameInvalid);
        return 0;
    }
    auto* copy = static_cast<char*>(malloc_nid_postfix(length + 1));
    if (!copy) {
        LibreSslPutSslError_nid_no_patch(&connection, SslLayout::ErrorInternalError);
        return 0;
    }
    std::memcpy(copy, name, length + 1);
    connection.tlsextHostname = copy;
    return 1;
}

void LibreSslReleaseConnection_nid_no_patch(const LibreSsl* ssl) {
    std::lock_guard<std::mutex> lock(registryMutex);
    registry.erase(ssl);
}

std::size_t LibreSslConnectionCount_nid_no_patch() {
    std::lock_guard<std::mutex> lock(registryMutex);
    return registry.size();
}

int APS5_VABI LibreSslConnect_nid_no_patch(LibreSsl* ssl) {
    return Handshake(SslOf(ssl, "SSL_connect"), false, "SSL_connect");
}

int APS5_VABI LibreSslAccept_nid_no_patch(LibreSsl* ssl) {
    return Handshake(SslOf(ssl, "SSL_accept"), true, "SSL_accept");
}

int APS5_VABI LibreSslRead_nid_no_patch(LibreSsl* ssl, void* buffer, int length) {
    return ReadApplicationData(SslOf(ssl, "SSL_read"), buffer, length, false, "SSL_read");
}

int APS5_VABI LibreSslPeek_nid_no_patch(LibreSsl* ssl, void* buffer, int length) {
    return ReadApplicationData(SslOf(ssl, "SSL_peek"), buffer, length, true, "SSL_peek");
}

int APS5_VABI LibreSslWrite_nid_no_patch(LibreSsl* ssl, const void* buffer, int length) {
    const char* function = "SSL_write";
    LibreSsl& connection = SslOf(ssl, function);
    LibreSslSslInternal& internal = InternalOf(connection, function);
    SetGuestErrno(0);
    if (length < 0) {
        LibreSslPutSslError_nid_no_patch(&connection, ErrorInternalError);
        return -1;
    }
    internal.rwstate = RwNothing;
    if (internal.state & StateInit) {
        const int result = RunHandshakeFunction(connection, internal);
        if (result < 0) return result;
    }
    Session& session = Established(connection, function);
    LibreSslBio& bio = SharedBio(connection, function);
    FollowDescriptor(bio, *session.tls);
    LibreSslTls::WriteOptions options;
    options.fragment = connection.maxSendFragment;
    options.partial = (internal.mode & ModeEnablePartialWrite) != 0;
    options.movingBuffer = (internal.mode & ModeAcceptMovingWriteBuffer) != 0;
    std::size_t transferred = 0;
    const EngineResult result = session.tls->Write(buffer, static_cast<std::size_t>(length), transferred, options);
    Account(bio, session);
    if (Wait(internal, bio, result, *session.tls)) return -1;
    switch (result) {
    case EngineResult::Done:
        Succeeded(internal, bio);
        return static_cast<int>(transferred);
    case EngineResult::Syscall:
        if (length == 0) {
            Succeeded(internal, bio);
            return 0;
        }
        SystemFailure(internal, bio, *session.tls);
        return -1;
    case EngineResult::Ssl:
        if (session.tls->FailureKind() != LibreSslTls::Failure::BadWriteRetry) Unmodelled(function, "the engine failed the write: " + session.tls->FailureText());
        LibreSslPutSslError_nid_no_patch(&connection, ReasonBadWriteRetry);
        return -1;
    case EngineResult::ZeroReturn:
    case EngineResult::WantRead:
    case EngineResult::WantWrite: break;
    }
    throw std::logic_error(std::string(function) + ": unhandled engine result");
}

int APS5_VABI LibreSslShutdown_nid_no_patch(LibreSsl* ssl) {
    const char* function = "SSL_shutdown";
    LibreSsl& connection = SslOf(ssl, function);
    LibreSslSslInternal& internal = InternalOf(connection, function);
    if (Read<int>(&internal, InternalQuietShutdown) || internal.state == StateBefore) {
        internal.shutdown = ShutdownSent | ShutdownReceived;
        return 1;
    }
    Session& session = Established(connection, function);
    LibreSslBio& bio = SharedBio(connection, function);
    FollowDescriptor(bio, *session.tls);
    bool complete = false;
    const EngineResult result = session.tls->Shutdown(complete);
    Account(bio, session);
    if (session.tls->SentShutdown()) internal.shutdown |= ShutdownSent;
    if (session.tls->ReceivedShutdown()) internal.shutdown |= ShutdownReceived;
    if (Wait(internal, bio, result, *session.tls)) return -1;
    switch (result) {
    case EngineResult::Done:
        if (session.tls->FatalAlertReceived()) Unmodelled(function, "the peer sent a fatal alert");
        Succeeded(internal, bio);
        return complete ? 1 : 0;
    case EngineResult::Syscall:
        SystemFailure(internal, bio, *session.tls);
        return -1;
    case EngineResult::Ssl: Unmodelled(function, "the engine failed the shutdown: " + session.tls->FailureText());
    case EngineResult::ZeroReturn:
    case EngineResult::WantRead:
    case EngineResult::WantWrite: break;
    }
    throw std::logic_error(std::string(function) + ": unhandled engine result");
}

int APS5_VABI LibreSslPending_nid_no_patch(const LibreSsl* ssl) {
    const Session* session = Find(ssl);
    if (!session) return 0;
    return static_cast<int>(std::min<std::size_t>(session->tls->Pending(), 0x7fffffff));
}

int APS5_VABI SSL_connect_nid_postfix(LibreSsl* ssl) {
    LibreSsl& connection = SslOf(ssl, __func__);
    if (!InternalOf(connection, __func__).handshakeFunc) SSL_set_connect_state_nid_postfix(&connection);
    return MethodOf(connection, __func__).sslConnect(&connection);
}

int APS5_VABI SSL_accept_nid_postfix(LibreSsl* ssl) {
    LibreSsl& connection = SslOf(ssl, __func__);
    LibreSslSslInternal& internal = InternalOf(connection, __func__);
    if (!internal.handshakeFunc) SetAcceptState(connection, internal, __func__);
    return MethodOf(connection, __func__).sslAccept(&connection);
}

int APS5_VABI SSL_read_nid_postfix(LibreSsl* ssl, void* buffer, int length) {
    LibreSsl& connection = SslOf(ssl, __func__);
    LibreSslSslInternal& internal = InternalOf(connection, __func__);
    if (!internal.handshakeFunc) {
        LibreSslPutSslError_nid_no_patch(&connection, ReasonUninitialized);
        return -1;
    }
    if (internal.shutdown & ShutdownReceived) {
        internal.rwstate = RwNothing;
        return 0;
    }
    return MethodOf(connection, __func__).sslRead(&connection, buffer, length);
}

int APS5_VABI SSL_peek_nid_postfix(LibreSsl* ssl, void* buffer, int length) {
    LibreSsl& connection = SslOf(ssl, __func__);
    LibreSslSslInternal& internal = InternalOf(connection, __func__);
    if (!internal.handshakeFunc) {
        LibreSslPutSslError_nid_no_patch(&connection, ReasonUninitialized);
        return -1;
    }
    if (internal.shutdown & ShutdownReceived) return 0;
    return MethodOf(connection, __func__).sslPeek(&connection, buffer, length);
}

int APS5_VABI SSL_write_nid_postfix(LibreSsl* ssl, const void* buffer, int length) {
    LibreSsl& connection = SslOf(ssl, __func__);
    LibreSslSslInternal& internal = InternalOf(connection, __func__);
    if (!internal.handshakeFunc) {
        LibreSslPutSslError_nid_no_patch(&connection, ReasonUninitialized);
        return -1;
    }
    if (internal.shutdown & ShutdownSent) {
        internal.rwstate = RwNothing;
        LibreSslPutSslError_nid_no_patch(&connection, ReasonProtocolIsShutdown);
        return -1;
    }
    return MethodOf(connection, __func__).sslWrite(&connection, buffer, length);
}

int APS5_VABI SSL_shutdown_nid_postfix(LibreSsl* ssl) {
    LibreSsl& connection = SslOf(ssl, __func__);
    LibreSslSslInternal& internal = InternalOf(connection, __func__);
    if (!internal.handshakeFunc) {
        LibreSslPutSslError_nid_no_patch(&connection, ReasonUninitialized);
        return -1;
    }
    if (internal.state & StateInit) return 1;
    return MethodOf(connection, __func__).sslShutdown(&connection);
}

int APS5_VABI SSL_pending_nid_postfix(const LibreSsl* ssl) {
    if (!ssl) throw std::invalid_argument(std::string(__func__) + ": null connection");
    return MethodOf(*ssl, __func__).sslPending(ssl);
}

int APS5_VABI SSL_get_error_nid_postfix(const LibreSsl* ssl, int result) {
    if (result > 0) return SslErrorNone;
    const std::uint64_t queued = ERR_peek_error_nid_postfix();
    if (queued) return ((queued >> 24) & 0xff) == static_cast<std::uint64_t>(ErrorLibrarySys) ? SslErrorSyscall : SslErrorSsl;
    if (!ssl) throw std::invalid_argument(std::string(__func__) + ": null connection");
    if (!ssl->internal) throw std::invalid_argument(std::string(__func__) + ": null internal state");
    const LibreSslSslInternal& internal = *ssl->internal;
    const auto special = [](const LibreSslBio& bio) {
        if (bio.retryReason == BioRetryConnect) return SslErrorWantConnect;
        if (bio.retryReason == BioRetryAccept) return SslErrorWantAccept;
        return SslErrorSyscall;
    };
    const auto classify = [&](const void* pointer, int preferred, int other) {
        if (!pointer) throw std::invalid_argument("SSL_get_error: null BIO");
        const auto& bio = *static_cast<const LibreSslBio*>(pointer);
        if (bio.flags & preferred) return preferred == BioFlagRead ? SslErrorWantRead : SslErrorWantWrite;
        if (bio.flags & other) return other == BioFlagRead ? SslErrorWantRead : SslErrorWantWrite;
        if (bio.flags & BioFlagIoSpecial) return special(bio);
        return 0;
    };
    if (result < 0 && internal.rwstate == RwReading) {
        if (const int error = classify(ssl->rbio, BioFlagRead, BioFlagWrite)) return error;
    }
    if (result < 0 && internal.rwstate == RwWriting) {
        if (const int error = classify(ssl->wbio, BioFlagWrite, BioFlagRead)) return error;
    }
    if (result < 0 && internal.rwstate == RwX509Lookup) return SslErrorWantX509Lookup;
    if (result == 0 && (internal.shutdown & ShutdownReceived) && Read<int>(Ssl3InternalOf(*ssl, __func__), Ssl3InternalWarnAlert) == AlertCloseNotify)
        return SslErrorZeroReturn;
    return SslErrorSyscall;
}

}
