/* $OpenBSD: ocsp_cl.c,v 1.25 2024/03/24 11:30:12 beck Exp $ */
/* Written by Tom Titchener <Tom_Titchener@groove.net> for the OpenSSL
 * project. */

/* History:
   This file was transfered to Richard Levitte from CertCo by Kathy
   Weinhold in mid-spring 2000 to be included in OpenSSL or released
   as a patch kit. */

/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: ocsp_lib.c,v 1.28 2024/08/28 06:27:19 tb Exp $ */
/* Written by Tom Titchener <Tom_Titchener@groove.net> for the OpenSSL
 * project. */

/* History:
   This file was transfered to Richard Levitte from CertCo by Kathy
   Weinhold in mid-spring 2000 to be included in OpenSSL or released
   as a patch kit. */

/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: a_int.c,v 1.48 2024/07/08 14:52:31 beck Exp $ */
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

#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <limits>
#include <stdexcept>

namespace {

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

int IntegerCompare(const Asn1String* first, const Asn1String* second, const char* function) {
    if (!first || !second) throw std::invalid_argument("OCSP_resp_find_status: null serial number");
    Asn1String guestFirst{};
    Asn1String guestSecond{};
    if (!CopyGuestInPreservingErrno(guestFirst, first) || !CopyGuestInPreservingErrno(guestSecond, second))
        NotImplemented_nid_no_patch(function);
    int result = 1;
    if ((guestFirst.type & 0x100) == (guestSecond.type & 0x100))
        result = ASN1_OCTET_STRING_cmp_nid_postfix(first, second);
    if (guestFirst.type & 0x100) {
        if (result == std::numeric_limits<int>::min())
            throw std::overflow_error("OCSP_resp_find_status: serial comparison overflow");
        return -result;
    }
    return result;
}

int CertIdCompare(const OCSP_CERTID* first, const OCSP_CERTID* second, const char* function) {
    if (!first || !second)
        throw std::invalid_argument("OCSP_resp_find_status: null certificate ID or algorithm");
    OCSP_CERTID guestFirst{};
    OCSP_CERTID guestSecond{};
    if (!CopyGuestInPreservingErrno(guestFirst, first) || !CopyGuestInPreservingErrno(guestSecond, second))
        NotImplemented_nid_no_patch(function);
    if (!guestFirst.hashAlgorithm || !guestSecond.hashAlgorithm)
        throw std::invalid_argument("OCSP_resp_find_status: null certificate ID or algorithm");
    LibreSsl3X509Algor firstAlgorithm{};
    LibreSsl3X509Algor secondAlgorithm{};
    if (!CopyGuestInPreservingErrno(firstAlgorithm, guestFirst.hashAlgorithm) ||
        !CopyGuestInPreservingErrno(secondAlgorithm, guestSecond.hashAlgorithm))
        NotImplemented_nid_no_patch(function);
    int result = OBJ_cmp_nid_postfix(firstAlgorithm.algorithm, secondAlgorithm.algorithm);
    if (result) return result;
    result = ASN1_OCTET_STRING_cmp_nid_postfix(guestFirst.issuerNameHash, guestSecond.issuerNameHash);
    if (result) return result;
    result = ASN1_OCTET_STRING_cmp_nid_postfix(guestFirst.issuerKeyHash, guestSecond.issuerKeyHash);
    if (result) return result;
    return IntegerCompare(guestFirst.serialNumber, guestSecond.serialNumber, function);
}

}

extern "C" int APS5_VABI OCSP_resp_find_status_nid_postfix(LibreSslOcspBasicResponse* bs, OCSP_CERTID* id, int* status,
    int* reason, Asn1String** revtime, Asn1String** thisupd, Asn1String** nextupd) {
    if (!bs) return 0;
    LibreSslOcspBasicResponse guestBs{};
    if (!CopyGuestInPreservingErrno(guestBs, bs))
        NotImplemented_nid_no_patch(__func__);
    if (!guestBs.tbsResponseData) throw std::invalid_argument("OCSP_resp_find_status: null response data");
    LibreSslOcspResponseData guestTbs{};
    if (!CopyGuestInPreservingErrno(guestTbs, guestBs.tbsResponseData))
        NotImplemented_nid_no_patch(__func__);
    const auto* responses = guestTbs.responses;
    for (int index = 0; index < sk_num_nid_postfix(responses); ++index) {
        const auto* single = static_cast<const LibreSslOcspSingleResponse*>(sk_value_nid_postfix(responses, index));
        if (!single) throw std::invalid_argument("OCSP_resp_find_status: null single response");
        LibreSslOcspSingleResponse guestSingle{};
        if (!CopyGuestInPreservingErrno(guestSingle, single))
            NotImplemented_nid_no_patch(__func__);
        if (CertIdCompare(id, static_cast<const OCSP_CERTID*>(guestSingle.certId), __func__)) continue;
        int localReason = 0;
        Asn1String* localRevtime = nullptr;
        Asn1String* localThisupd = nullptr;
        Asn1String* localNextupd = nullptr;
        const int result = OCSP_single_get0_status_nid_postfix(single, reason ? &localReason : nullptr,
            revtime ? &localRevtime : nullptr, thisupd ? &localThisupd : nullptr, nextupd ? &localNextupd : nullptr);
        const bool writeRevoked = (result == 1);
        Asn1String* previousRevtime = nullptr;
        const bool hadRevtime = writeRevoked && revtime && CopyGuestInPreservingErrno(previousRevtime, revtime);
        if (writeRevoked && revtime && !CopyGuestOutPreservingErrno(revtime, localRevtime))
            NotImplemented_nid_no_patch(__func__);
        int previousReason = 0;
        const bool hadReason = writeRevoked && reason && CopyGuestInPreservingErrno(previousReason, reason);
        if (writeRevoked && reason && !CopyGuestOutPreservingErrno(reason, localReason)) {
            if (hadRevtime) CopyGuestOutPreservingErrno(revtime, previousRevtime);
            NotImplemented_nid_no_patch(__func__);
        }
        Asn1String* previousThisupd = nullptr;
        const bool hadThisupd = thisupd && CopyGuestInPreservingErrno(previousThisupd, thisupd);
        if (thisupd && !CopyGuestOutPreservingErrno(thisupd, localThisupd)) {
            if (hadReason) CopyGuestOutPreservingErrno(reason, previousReason);
            if (hadRevtime) CopyGuestOutPreservingErrno(revtime, previousRevtime);
            NotImplemented_nid_no_patch(__func__);
        }
        Asn1String* previousNextupd = nullptr;
        const bool hadNextupd = nextupd && CopyGuestInPreservingErrno(previousNextupd, nextupd);
        if (nextupd && !CopyGuestOutPreservingErrno(nextupd, localNextupd)) {
            if (hadThisupd) CopyGuestOutPreservingErrno(thisupd, previousThisupd);
            if (hadReason) CopyGuestOutPreservingErrno(reason, previousReason);
            if (hadRevtime) CopyGuestOutPreservingErrno(revtime, previousRevtime);
            NotImplemented_nid_no_patch(__func__);
        }
        if (status && !CopyGuestOutPreservingErrno(status, result)) {
            if (hadNextupd) CopyGuestOutPreservingErrno(nextupd, previousNextupd);
            if (hadThisupd) CopyGuestOutPreservingErrno(thisupd, previousThisupd);
            if (hadReason) CopyGuestOutPreservingErrno(reason, previousReason);
            if (hadRevtime) CopyGuestOutPreservingErrno(revtime, previousRevtime);
            NotImplemented_nid_no_patch(__func__);
        }
        return 1;
    }
    return 0;
}
