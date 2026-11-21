/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a) src/lib/libcrypto/pem/pem_lib.c
 * (pem_lib.c,v 1.56), src/lib/libcrypto/pem/pem_pkey.c (pem_pkey.c,v 1.28),
 * src/lib/libcrypto/pem/pem_all.c (pem_all.c,v 1.21), src/lib/libcrypto/pem/pem_oth.c (pem_oth.c,v 1.9),
 * src/lib/libcrypto/pem/pem_pk8.c (pem_pk8.c,v 1.14) and
 * src/lib/libcrypto/evp/evp_encode.c (evp_encode.c,v 1.3) */
/* $OpenBSD: pem_lib.c,v 1.56 2024-02-18 15:44:10 tb Exp $ */
/* $OpenBSD: pem_pkey.c,v 1.28 2023-11-19 15:46:10 tb Exp $ */
/* $OpenBSD: pem_all.c,v 1.21 2023-07-07 13:40:44 beck Exp $ */
/* $OpenBSD: pem_oth.c,v 1.9 2023-07-07 13:40:44 beck Exp $ */
/* $OpenBSD: pem_pk8.c,v 1.14 2023-07-07 13:40:44 beck Exp $ */
/* $OpenBSD: evp_encode.c,v 1.3 2024-04-09 13:52:41 beck Exp $ */
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
 * OR SERVICES; LOSS OF DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int WritePkcs8PrivateKey(LibreSsl::Bio* bio, const LibreSsl3EvpPkey* key) {
    auto* p8inf = EVP_PKEY2PKCS8_nid_postfix(key);
    if (p8inf == nullptr) {
        PutPemError(PemErrorConvertingPrivateKey);
        return 0;
    }
    const int estimated = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(p8inf, nullptr);
    if (estimated < 0 || static_cast<std::size_t>(estimated) + 20
            > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        PutPemError(ErrRAsn1Lib);
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        return 0;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(estimated) + 20));
    if (data == nullptr) {
        PutPemError(ErrRMallocFailure);
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        return 0;
    }
    unsigned char* cursor = data;
    const int encoded = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(p8inf, &cursor);
    if (encoded <= 0) {
        PutPemError(ErrRAsn1Lib);
        ClearBytes(data, static_cast<std::size_t>(estimated) + 20);
        free_nid_postfix(data);
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        return 0;
    }
    const int written = WritePem(bio, Pkcs8InfLabel, "", data, encoded);
    ClearBytes(data, static_cast<std::size_t>(estimated) + 20);
    free_nid_postfix(data);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
    return written > 0 ? 1 : 0;
}

template<typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

template<typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

bool GuestWritablePreservingErrno(void* destination, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    return ok;
}

}


LibreSsl3EvpPkey* APS5_VABI PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(LibreSsl::Bio* bio,
    LibreSsl3EvpPkey** key, LibreSslPemPasswordCallback passwordCallback, void* userData) {
    static_cast<void>(passwordCallback);
    static_cast<void>(userData);
    std::string name;
    std::vector<unsigned char> data;
    const ClearOnExit clearData{data};
    int length = 0;
    if (!ReadKeyBytes(bio, AnyPrivateKeyLabel, name, data, length)) return nullptr;

    const unsigned char* cursor = data.data();
    LibreSsl3EvpPkey* ret = nullptr;
    int suffixLength = 0;
    if (name == Pkcs8InfLabel) {
        auto* p8inf = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(nullptr, &cursor, length);
        if (p8inf != nullptr) {
            ret = EVP_PKCS82PKEY_nid_postfix(p8inf);
            if (key != nullptr) {
                EVP_PKEY_free_nid_disambig1_nid_postfix(*key);
            }
            PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        }
    } else if (name == EncryptedPrivateKeyLabel) {
        EncryptedPrivateKeyUnsupported();
    } else if ((suffixLength = PemCheckSuffix(name.c_str(), PrivateKeySuffix)) > 0) {
        const PkeyAmeth* ameth = FindAmethByString(name.c_str(), suffixLength);
        if (ameth != nullptr && ameth->oldPrivDecode) {
            if (ameth->type != RsaType && ameth->type != EcType) OldPrivateKeyDecodeUnsupported();
            ret = DecodeTraditionalPrivateKey_nid_no_patch(ameth->type, key, &cursor, length);
        }
    }
    if (ret == nullptr) PutPemError(ErrRAsn1Lib);
    return ret;
}

LibreSsl3Rsa* APS5_VABI PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(
    FileStream* stream, LibreSsl3Rsa** rsa, LibreSslPemPasswordCallback passwordCallback, void* userData) {
    if (stream == nullptr) throw std::invalid_argument("PEM_read_RSAPrivateKey: null stream");
    std::unique_ptr<LibreSsl::Bio, decltype(&BIO_free_nid_postfix)> bio(
        BIO_new_fp_nid_postfix(stream, 0), BIO_free_nid_postfix);
    if (bio.get() == nullptr) {
        PutPemError(ErrRBufLib);
        return nullptr;
    }
    std::unique_ptr<LibreSsl3EvpPkey, decltype(&EVP_PKEY_free_nid_disambig1_nid_postfix)> key(
        PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bio.get(), nullptr, passwordCallback, userData),
        EVP_PKEY_free_nid_disambig1_nid_postfix);
    bio.reset();
    if (key.get() == nullptr) return nullptr;
    std::unique_ptr<LibreSsl3Rsa, decltype(&RSA_free_nid_disambig1_nid_postfix)> decoded(
        EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(key.get()), RSA_free_nid_disambig1_nid_postfix);
    key.reset();
    if (decoded.get() == nullptr) return nullptr;
    if (rsa != nullptr) {
        LibreSsl3Rsa* previous = nullptr;
        if (!CopyGuestInPreservingErrno(previous, rsa) || !GuestWritablePreservingErrno(rsa, sizeof(previous))) {
            NotImplemented_nid_no_patch(__func__);
        }
        if (previous != nullptr) RSA_free_nid_disambig1_nid_postfix(previous);
        LibreSsl3Rsa* replacement = decoded.get();
        if (!CopyGuestOutPreservingErrno(rsa, replacement)) NotImplemented_nid_no_patch(__func__);
    }
    return decoded.release();
}

LibreSsl3EvpPkey* APS5_VABI PEM_read_bio_PUBKEY_nid_postfix(LibreSsl::Bio* bio, LibreSsl3EvpPkey** key,
    LibreSslPemPasswordCallback passwordCallback, void* userData) {
    static_cast<void>(passwordCallback);
    static_cast<void>(userData);
    std::string name;
    std::vector<unsigned char> data;
    const ClearOnExit clearData{data};
    int length = 0;
    if (!ReadKeyBytes(bio, PublicKeyLabel, name, data, length)) return nullptr;

    const unsigned char* cursor = data.data();
    LibreSsl3EvpPkey* ret = d2i_PUBKEY_nid_postfix(key, &cursor, length);
    if (ret == nullptr) PutPemError(ErrRAsn1Lib);
    return ret;
}

int APS5_VABI PEM_write_bio_PrivateKey_nid_postfix(LibreSsl::Bio* bio, LibreSsl3EvpPkey* key,
    const LibreSsl3EvpCipher* cipher, unsigned char* keyString, int keyLength,
    LibreSslPemPasswordCallback passwordCallback, void* userData) {
    static_cast<void>(keyString);
    static_cast<void>(keyLength);
    static_cast<void>(passwordCallback);
    static_cast<void>(userData);
    if (cipher != nullptr) EncryptedPrivateKeyUnsupported();
    if (key == nullptr) throw std::invalid_argument("PEM_write_bio_PrivateKey: null key");
    return WritePkcs8PrivateKey(bio, key);
}

