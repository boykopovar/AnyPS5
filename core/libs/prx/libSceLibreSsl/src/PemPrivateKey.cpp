/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/pem/pem_pkey.c
 * (pem_pkey.c,v 1.22), src/lib/libcrypto/asn1/d2i_pr.c (d2i_pr.c,v 1.15) and src/lib/libcrypto/evp/evp_pkey.c
 * (evp_pkey.c,v 1.19) */
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
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
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
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include "PemReadBio.hpp"

namespace {

constexpr int RsaLibrary = 4;
constexpr int EvpLibrary = 6;
constexpr int Asn1Library = 13;
constexpr int FunctionUnknown = 0xfff;
constexpr int ReasonMallocFailure = 65;
constexpr int ReasonUnsupportedPrivateKeyAlgorithm = 118;
constexpr int ReasonPrivateKeyDecodeError = 145;
constexpr int ReasonUnsupportedAlgorithm = 156;
constexpr int KeyRsa = 6;
constexpr int KeyRsaAlias = 19;
constexpr int OctetString = 4;
constexpr int Sequence = 16;
constexpr int OtherKeyMethods[] {28, 66, 67, 70, 113, 116, 408, 811, 815, 855, 894, 946, 947};

void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, FunctionUnknown, reason, __FILE__, __LINE__);
}

bool DecodeOldRsaKey(void* pkey, const unsigned char** input, long length, const char* function) {
    auto* rsa = LibreSsl::DecodeRsaPrivateKey(input, length, function);
    if (!rsa) {
        PutError(RsaLibrary, RsaLibrary);
        return false;
    }
    EVP_PKEY_set1_RSA_nid_postfix(pkey, rsa);
    RSA_free_nid_postfix(rsa);
    return true;
}

void* Pkcs8ToKey(const LibreSsl::Pkcs8Key& info, const char* function) {
    if (info.keyType != OctetString && info.keyType != Sequence) return nullptr;
    const bool rsa = info.algorithm == KeyRsa || info.algorithm == KeyRsaAlias;
    if (!rsa && std::find(std::begin(OtherKeyMethods), std::end(OtherKeyMethods), info.algorithm) != std::end(OtherKeyMethods))
        NotImplemented_nid_no_patch((std::string(function) + ": a PKCS#8 key of type " + std::to_string(info.algorithm)).c_str());
    void* pkey = EVP_PKEY_new_nid_postfix();
    if (!pkey) {
        PutError(EvpLibrary, ReasonMallocFailure);
        return nullptr;
    }
    if (!rsa) {
        PutError(EvpLibrary, ReasonUnsupportedAlgorithm);
        PutError(EvpLibrary, ReasonUnsupportedPrivateKeyAlgorithm);
        EVP_PKEY_free_nid_postfix(pkey);
        return nullptr;
    }
    EVP_PKEY_set1_RSA_nid_postfix(pkey, nullptr);
    const unsigned char* key = info.key;
    if (!DecodeOldRsaKey(pkey, &key, info.keyLength, function)) {
        PutError(EvpLibrary, ReasonPrivateKeyDecodeError);
        EVP_PKEY_free_nid_postfix(pkey);
        return nullptr;
    }
    return pkey;
}

void* DecodeRsaPrivateKey(void** reuse, const unsigned char** input, long length, const char* function) {
    void* pkey = reuse ? *reuse : nullptr;
    if (!pkey) {
        pkey = EVP_PKEY_new_nid_postfix();
        if (!pkey) {
            PutError(Asn1Library, EvpLibrary);
            return nullptr;
        }
    }
    EVP_PKEY_set1_RSA_nid_postfix(pkey, nullptr);
    if (!DecodeOldRsaKey(pkey, input, length, function)) {
        LibreSsl::Pkcs8Key info;
        if (!LibreSsl::DecodePkcs8(input, length, info, function)) {
            if (!reuse || *reuse != pkey) EVP_PKEY_free_nid_postfix(pkey);
            return nullptr;
        }
        EVP_PKEY_free_nid_postfix(pkey);
        pkey = Pkcs8ToKey(info, function);
    }
    if (reuse) *reuse = pkey;
    return pkey;
}

}

extern "C" {

void* APS5_VABI PEM_read_bio_PrivateKey_nid_postfix(LibreSsl::Bio* bio, void** key, LibreSslPemPasswordCallback callback,
                                                     void* userData) {
    static_cast<void>(callback);
    static_cast<void>(userData);
    LibreSsl::PemBuffers record;
    long length = 0;
    if (!LibreSsl::ReadPemBytes(bio, "ANY PRIVATE KEY", record, length, __func__)) return nullptr;
    const char* const name = record.name->data;
    const auto* data = reinterpret_cast<const unsigned char*>(record.data->data);
    void* result = nullptr;
    if (!std::strcmp(name, "PRIVATE KEY")) {
        LibreSsl::Pkcs8Key info;
        if (LibreSsl::DecodePkcs8(&data, length, info, __func__)) {
            result = Pkcs8ToKey(info, __func__);
            if (key) {
                EVP_PKEY_free_nid_postfix(*key);
                *key = result;
            }
        }
    } else if (!std::strcmp(name, "ENCRYPTED PRIVATE KEY")) {
        NotImplemented_nid_no_patch((std::string(__func__) + ": PKCS#8 decryption of ENCRYPTED PRIVATE KEY").c_str());
    } else if (const int prefix = LibreSsl::PemCheckSuffix(name, "PRIVATE KEY"); prefix > 0) {
        const int type = LibreSsl::OldPrivateKeyType(name, prefix);
        if (type == KeyRsa) result = DecodeRsaPrivateKey(key, &data, length, __func__);
        else if (type) NotImplemented_nid_no_patch((std::string(__func__) + ": a traditional " + name + " record").c_str());
    }
    if (!result) LibreSsl::PutPemError(Asn1Library);
    return result;
}

}
