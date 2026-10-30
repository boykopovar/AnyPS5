// Adapted from the LibreSSL 2.5.4 release tarball (libressl-2.5.4.tar.gz, sha256 107a5b522fbb8318d4c3be668075e5e607296f0a9255d71674caa94571336efa),
// the version identified in docs/dev/debt/libSceLibreSsl.md, which corresponds to the openbsd
// mirror commit 2e2c612066aa993717528a0249c15c51128d61cb. Source files used:
//   crypto/bio/bss_sock.c - BIO_s_socket
//   crypto/bio/bio_lib.c - BIO_free, BIO_free_all
// The original copyright and licence notices of those files follow unchanged.
/* $OpenBSD: bss_sock.c,v 1.23 2014/07/11 08:44:47 jsing Exp $ */
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
/* $OpenBSD: bio_lib.c,v 1.23 2017/01/29 17:49:22 beck Exp $ */
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

#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>

static_assert(sizeof(LibreSslBioMethod) == 80);
static_assert(offsetof(LibreSslBioMethod, name) == 8);
static_assert(sizeof(LibreSslBio) == 104);
static_assert(offsetof(LibreSslBio, callback) == 8);
static_assert(offsetof(LibreSslBio, init) == 24);
static_assert(offsetof(LibreSslBio, shutdown) == 28);
static_assert(offsetof(LibreSslBio, flags) == 32);
static_assert(offsetof(LibreSslBio, retryReason) == 36);
static_assert(offsetof(LibreSslBio, num) == 40);
static_assert(offsetof(LibreSslBio, pointer) == 48);
static_assert(offsetof(LibreSslBio, nextBio) == 56);
static_assert(offsetof(LibreSslBio, previousBio) == 64);
static_assert(offsetof(LibreSslBio, references) == 72);
static_assert(offsetof(LibreSslBio, numRead) == 80);
static_assert(offsetof(LibreSslBio, numWrite) == 88);
static_assert(offsetof(LibreSslBio, exData) == 96);

namespace {

int APS5_VABI SocketWrite_nid_no_patch(LibreSslBio*, const char*, int) {
    NotImplemented_nid_no_patch("BIO_s_socket write callback");
}

int APS5_VABI SocketRead_nid_no_patch(LibreSslBio*, char*, int) {
    NotImplemented_nid_no_patch("BIO_s_socket read callback");
}

int APS5_VABI SocketPuts_nid_no_patch(LibreSslBio*, const char*) {
    NotImplemented_nid_no_patch("BIO_s_socket puts callback");
}

std::int64_t APS5_VABI SocketControl_nid_no_patch(LibreSslBio*, int, std::int64_t, void*) {
    NotImplemented_nid_no_patch("BIO_s_socket control callback");
}

int APS5_VABI SocketCreate_nid_no_patch(LibreSslBio*) {
    NotImplemented_nid_no_patch("BIO_s_socket create callback");
}

int APS5_VABI SocketDestroy_nid_no_patch(LibreSslBio*) {
    NotImplemented_nid_no_patch("BIO_s_socket destroy callback");
}

const LibreSslBioMethod socketMethod{
    SslLayout::BioTypeSocket,
    "socket",
    reinterpret_cast<void*>(SocketWrite_nid_no_patch),
    reinterpret_cast<void*>(SocketRead_nid_no_patch),
    reinterpret_cast<void*>(SocketPuts_nid_no_patch),
    nullptr,
    reinterpret_cast<void*>(SocketControl_nid_no_patch),
    reinterpret_cast<void*>(SocketCreate_nid_no_patch),
    reinterpret_cast<void*>(SocketDestroy_nid_no_patch),
    nullptr};

void FreeBio(LibreSslBio* bio) {
    if (std::atomic_ref<int>(bio->references).fetch_sub(1) - 1 > 0) return;
    if (bio->callback) NotImplemented_nid_no_patch("BIO_free: BIO callback");
    if (bio->method != &socketMethod) NotImplemented_nid_no_patch("BIO_free: BIO type other than a socket");
    if (bio->shutdown && bio->init) NotImplemented_nid_no_patch("BIO_free: closing the descriptor of a socket BIO");
    if (bio->exData.stack) NotImplemented_nid_no_patch("BIO_free: BIO ex_data");
    free_nid_postfix(bio);
}

}

extern "C" {

LibreSslBio* LibreSslNewSocketBio_nid_no_patch(int descriptor) {
    auto* bio = static_cast<LibreSslBio*>(calloc_nid_postfix(1, sizeof(LibreSslBio)));
    if (!bio) return nullptr;
    bio->method = &socketMethod;
    bio->references = 1;
    bio->init = 1;
    bio->shutdown = 0;
    bio->num = descriptor;
    return bio;
}

void LibreSslFreeBioChain_nid_no_patch(LibreSslBio* bio) {
    while (bio) {
        LibreSslBio* current = bio;
        const int references = current->references;
        bio = bio->nextBio;
        FreeBio(current);
        if (references > 1) break;
    }
}

int LibreSslBioDescriptor_nid_no_patch(const LibreSslBio* bio) {
    for (const LibreSslBio* current = bio; current; current = current->nextBio) {
        if (!current->method || !(current->method->type & SslLayout::BioTypeDescriptor)) continue;
        if (current->method != &socketMethod) NotImplemented_nid_no_patch("BIO_get_fd: BIO type other than a socket");
        return current->init ? current->num : -1;
    }
    return -1;
}

bool LibreSslIsSocketBio_nid_no_patch(const LibreSslBio* bio) {
    return bio && bio->method == &socketMethod;
}

}
