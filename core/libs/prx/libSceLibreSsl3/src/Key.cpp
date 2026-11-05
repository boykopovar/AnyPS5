/* $OpenBSD: rsa_lib.c,v 1.50 2024/03/27 01:22:30 tb Exp $ */
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <atomic>
#include <limits>
#include <stdexcept>

namespace {

static_assert(std::atomic_ref<std::int32_t>::required_alignment <= alignof(std::int32_t));

int Retain_nid_no_patch(std::int32_t& references) {
    std::atomic_ref<std::int32_t> count(references);
    auto previous = count.load();
    for (;;) {
        if (previous == std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("LibreSSL3 key reference count overflow");
        if (count.compare_exchange_weak(previous, previous + 1))
            return previous > 0;
    }
}

} // namespace

extern "C" {

int APS5_VABI EVP_PKEY_up_ref_nid_postfix(LibreSsl3EvpPkey* key) {
    if (!key)
        throw std::invalid_argument("EVP_PKEY_up_ref requires a key");
    return Retain_nid_no_patch(key->references);
}

int APS5_VABI EC_KEY_up_ref_nid_postfix(LibreSsl3EcKey* key) {
    if (!key)
        throw std::invalid_argument("EC_KEY_up_ref requires a key");
    return Retain_nid_no_patch(key->references);
}

int APS5_VABI RSA_up_ref_nid_postfix(LibreSsl3Rsa* rsa) {
    if (!rsa)
        throw std::invalid_argument("RSA_up_ref requires a key");
    return Retain_nid_no_patch(rsa->references);
}

void APS5_VABI RSA_get0_factors_nid_postfix(const LibreSsl3Rsa* rsa, const void** p, const void** q) {
    if (!rsa && (p || q))
        throw std::invalid_argument("RSA_get0_factors requires a key when outputs are requested");
    if (p)
        *p = rsa->p;
    if (q)
        *q = rsa->q;
}

void APS5_VABI RSA_get0_crt_params_nid_postfix(const LibreSsl3Rsa* rsa, const void** dmp1, const void** dmq1, const void** iqmp) {
    if (!rsa && (dmp1 || dmq1 || iqmp))
        throw std::invalid_argument("RSA_get0_crt_params requires a key when outputs are requested");
    if (dmp1)
        *dmp1 = rsa->dmp1;
    if (dmq1)
        *dmq1 = rsa->dmq1;
    if (iqmp)
        *iqmp = rsa->iqmp;
}

}
