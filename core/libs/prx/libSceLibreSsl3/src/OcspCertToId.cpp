/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/ocsp/ocsp_lib.c */
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int OcspLibrary = 39;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int DigestError = 102;
constexpr int UnknownNid = 120;
constexpr int IntegerType = 2;
constexpr int OctetStringType = 4;
constexpr int NullType = 5;
constexpr std::int64_t NdefFlag = 0x10;
constexpr std::size_t MaxDigestSize = 64;

struct BufMem {
    std::size_t length;
    const unsigned char* data;
    std::size_t max;
};

static_assert(sizeof(BufMem) == 24);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, modified) == 8);
static_assert(offsetof(LibreSslX509Name, bytes) == 16);
static_assert(offsetof(LibreSslX509Name, canonEnc) == 24);
static_assert(offsetof(LibreSslX509Name, canonEnclen) == 32);
static_assert(offsetof(LibreSsl3CertificateInfo, serialNumber) == 8);
static_assert(offsetof(LibreSsl3CertificateInfo, issuer) == 24);
static_assert(offsetof(LibreSsl3CertificateInfo, subject) == 40);

template <typename T>
T CopyIn(const T* source, const char* what) {
    if (!source) throw std::invalid_argument(std::string("OCSP_cert_to_id: null ") + what);
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string("OCSP_cert_to_id: unreadable ") + what);
    return value;
}

std::vector<unsigned char> CopyBytesIn(const unsigned char* source, std::size_t length, const char* what) {
    std::vector<unsigned char> bytes(length);
    if (length == 0) return bytes;
    if (!source) throw std::invalid_argument(std::string("OCSP_cert_to_id: null ") + what + " data");
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(bytes.data(), source, length);
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string("OCSP_cert_to_id: unreadable ") + what + " data");
    return bytes;
}

LibreSsl3CertificateInfo CertificateInfo(const LibreSsl3Certificate* certificate, const char* what) {
    if (!certificate) throw std::invalid_argument(std::string("OCSP_cert_to_id: null ") + what);
    return CopyIn(CopyIn(&certificate->certInfo, what), "certificate info");
}

Asn1String* NewString(int type) {
    auto* string = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!string) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        return nullptr;
    }
    string->type = type;
    return string;
}

OCSP_CERTID* NewCertId() {
    auto* id = static_cast<OCSP_CERTID*>(calloc_nid_postfix(1, sizeof(OCSP_CERTID)));
    if (!id) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        return nullptr;
    }
    id->hashAlgorithm = static_cast<LibreSsl3X509Algor*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Algor)));
    if (!id->hashAlgorithm) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        OCSP_CERTID_free_nid_postfix(id);
        return nullptr;
    }
    id->hashAlgorithm->algorithm = const_cast<LibreSslAsn1Object*>(LibreSsl3::ObjectByNid(0));
    if (!(id->issuerNameHash = NewString(OctetStringType)) || !(id->issuerKeyHash = NewString(OctetStringType)) ||
        !(id->serialNumber = NewString(IntegerType))) {
        OCSP_CERTID_free_nid_postfix(id);
        return nullptr;
    }
    return id;
}

bool SetHashAlgorithm(LibreSsl3X509Algor* algorithm, int nid) {
    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
    if (!object) return false;
    algorithm->algorithm = const_cast<LibreSslAsn1Object*>(object);
    auto* parameter = static_cast<Asn1Type*>(calloc_nid_postfix(1, sizeof(Asn1Type)));
    if (!parameter) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        return false;
    }
    parameter->type = NullType;
    algorithm->parameter = reinterpret_cast<LibreSsl3Asn1Type*>(parameter);
    return true;
}

bool DigestName(const void* guestName, const EvpMd* type, unsigned char* digest, std::uint32_t* size) {
    const auto name = CopyIn(static_cast<const LibreSslX509Name*>(guestName), "issuer name");
    if (name.modified) NotImplementedAt_nid_no_patch("OCSP_cert_to_id: re-encoding a modified issuer name", __FILE__, __LINE__);
    const auto bytes = CopyIn(static_cast<const BufMem*>(name.bytes), "issuer name encoding");
    if (bytes.length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::overflow_error("OCSP_cert_to_id: issuer name encoding exceeds INT_MAX bytes");
    if (bytes.length == 0) return false;
    const auto encoded = CopyBytesIn(bytes.data, bytes.length, "issuer name encoding");
    return EVP_Digest_nid_postfix(encoded.data(), encoded.size(), digest, size, type, nullptr) != 0;
}

bool DigestKey(const LibreSsl3Asn1String& key, const EvpMd* type, unsigned char* digest, std::uint32_t* size) {
    if (key.length < 0) throw std::invalid_argument("OCSP_cert_to_id: negative issuer key length");
    const auto bits = CopyBytesIn(key.data, static_cast<std::size_t>(key.length), "issuer key");
    return EVP_Digest_nid_postfix(bits.data(), bits.size(), digest, size, type, nullptr) != 0;
}

bool SetSerialNumber(OCSP_CERTID* id, const void* guestSerial) {
    ASN1_INTEGER_free_nid_postfix(id->serialNumber);
    id->serialNumber = nullptr;
    const auto serial = CopyIn(static_cast<const Asn1String*>(guestSerial), "serial number");
    if (serial.length < 0) return false;
    Asn1String* copy = NewString(OctetStringType);
    if (!copy) return false;
    if (!ASN1_STRING_set_nid_postfix(copy, serial.data, serial.length)) {
        ASN1_INTEGER_free_nid_postfix(copy);
        return false;
    }
    copy->type = serial.type;
    copy->flags = serial.flags & ~NdefFlag;
    id->serialNumber = copy;
    return true;
}

bool FillCertId(OCSP_CERTID* id, const EvpMd* type, const void* issuerName, const LibreSsl3Asn1String& issuerKey, const void* serial) {
    const int nid = CopyIn(reinterpret_cast<const std::int32_t*>(type), "digest");
    if (nid == 0) {
        LibreSslPutError_nid_no_patch(OcspLibrary, AnyFunction, UnknownNid);
        return false;
    }
    if (!SetHashAlgorithm(id->hashAlgorithm, nid)) return false;
    unsigned char digest[MaxDigestSize];
    std::uint32_t size = 0;
    if (!DigestName(issuerName, type, digest, &size)) {
        LibreSslPutError_nid_no_patch(OcspLibrary, AnyFunction, DigestError);
        return false;
    }
    if (!ASN1_STRING_set_nid_postfix(id->issuerNameHash, digest, static_cast<int>(size))) return false;
    if (!DigestKey(issuerKey, type, digest, &size)) return false;
    if (!ASN1_STRING_set_nid_postfix(id->issuerKeyHash, digest, static_cast<int>(size))) return false;
    return !serial || SetSerialNumber(id, serial);
}

} // namespace

extern "C" OCSP_CERTID* APS5_VABI OCSP_cert_to_id_nid_postfix(const EvpMd* digest, const LibreSsl3Certificate* subject, const LibreSsl3Certificate* issuer) {
    if (!digest) digest = reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix());
    const void* issuerName;
    const void* serial = nullptr;
    if (subject) {
        const auto info = CertificateInfo(subject, "subject certificate");
        issuerName = info.issuer;
        serial = info.serialNumber;
    } else {
        issuerName = CertificateInfo(issuer, "issuer certificate").subject;
    }
    if (!issuer) return nullptr;
    const auto* const key = CertificateInfo(issuer, "issuer certificate").key;
    const auto* const bits = CopyIn(key, "issuer public key").publicKey;
    if (!bits) return nullptr;
    const auto issuerKey = CopyIn(bits, "issuer public key bits");

    OCSP_CERTID* id = NewCertId();
    if (!id) return nullptr;
    bool filled;
    try {
        filled = FillCertId(id, digest, issuerName, issuerKey, serial);
    } catch (...) {
        OCSP_CERTID_free_nid_postfix(id);
        throw;
    }
    if (filled) return id;
    OCSP_CERTID_free_nid_postfix(id);
    return nullptr;
}
