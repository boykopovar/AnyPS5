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
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <vector>
#include <mbedtls/aes.h>
#include <mbedtls/des.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>
#include "PemReadBio.hpp"

namespace {

constexpr int RsaLibrary = 4;
constexpr int EvpLibrary = 6;
constexpr int PemLibrary = 9;
constexpr int Asn1Library = 13;
constexpr int Pkcs12Library = 35;
constexpr int FunctionUnknown = 0xfff;
constexpr int ReasonMallocFailure = 65;
constexpr int ReasonBadPasswordRead = 104;
constexpr int ReasonAsn1NestedError = 58;
constexpr int ReasonAsn1WrongTag = 168;
constexpr int ReasonUnsupportedPrivateKeyAlgorithm = 118;
constexpr int ReasonPrivateKeyDecodeError = 145;
constexpr int ReasonUnsupportedAlgorithm = 156;
constexpr int ReasonBadDecrypt = 100;
constexpr int ReasonDecodeError = 114;
constexpr int ReasonUnknownPbeAlgorithm = 121;
constexpr int ReasonKeygenFailure = 120;
constexpr int ReasonUnsupportedKeyDerivationFunction = 124;
constexpr int ReasonUnsupportedCipher = 107;
constexpr int ReasonUnsupportedPrf = 125;
constexpr int ReasonCipherParameterError = 122;
constexpr int ReasonPkcs12AlgorithmCipherInitError = 115;
constexpr int ReasonPkcs12DecodeError = 101;
constexpr int ReasonPkcs12CipherFinalError = 116;
constexpr int ReasonPkcs12DecryptError = 117;
constexpr int KeyRsa = 6;
constexpr int KeyRsaAlias = 19;
constexpr int OctetString = 4;
constexpr int Sequence = 16;
constexpr int Integer = 2;
constexpr int ObjectIdentifier = 6;
constexpr int OtherKeyMethods[] {28, 66, 67, 70, 113, 116, 408, 811, 815, 855, 894, 946, 947};
constexpr int PbeMd5DesNid = 10;
constexpr int DesCbcNid = 31;
constexpr int DesEde3CbcNid = 44;
constexpr int Pbkdf2Nid = 69;
constexpr int PbeSha1ThreeDesNid = 146;
constexpr int Pbes2Nid = 161;
constexpr int HmacWithSha1Nid = 163;
constexpr int PbeSha1DesNid = 170;
constexpr int Aes128CbcNid = 419;
constexpr int Aes192CbcNid = 423;
constexpr int Aes256CbcNid = 427;
constexpr int HmacWithSha224Nid = 798;
constexpr int HmacWithSha256Nid = 799;
constexpr int HmacWithSha384Nid = 800;
constexpr int HmacWithSha512Nid = 801;

struct WipeGuard {
    void* data;
    std::size_t length;
    ~WipeGuard() { mbedtls_platform_zeroize(data, length); }
};

struct VectorWipeGuard {
    std::vector<unsigned char>& bytes;
    ~VectorWipeGuard() { mbedtls_platform_zeroize(bytes.data(), bytes.size()); }
};

struct MdContextGuard {
    mbedtls_md_context_t context;
    ~MdContextGuard() { mbedtls_md_free(&context); }
};

struct Element {
    int tag;
    int tagClass;
    bool constructed;
    bool indefinite;
    const unsigned char* content;
    long length;
    const unsigned char* end;
};

struct Cursor {
    const unsigned char* data;
    long length;
};

enum class Field { Present, Absent, Failed };
enum class CipherKind { Aes, Des3, Des };
enum class DecryptResult { Success, BadDecrypt, UnknownPbe, DecodeError, UnsupportedKdf, UnsupportedCipher,
                           UnsupportedPrf, CipherParameterError, KdfParameterWrongTag };

struct Algorithm {
    const unsigned char* object;
    long objectLength;
    bool parameter;
    bool nullParameter;
    Element parameterElement;
};

[[noreturn]] void UnsupportedEncoding(const char* function, const char* what) {
    NotImplemented_nid_no_patch((std::string(function) + ": BER " + what).c_str());
}

bool ReadHeader(const unsigned char* start, std::int64_t available, Element& element) {
    if (available <= 0) return false;
    const unsigned char* data = start;
    std::int64_t remaining = available;
    const bool constructed = *data & 0x20;
    const int tagClass = *data & 0xC0;
    int tag = *data & 0x1F;
    if (tag == 0x1F) {
        ++data;
        if (--remaining == 0) return false;
        long value = 0;
        while (*data & 0x80) {
            value = value << 7 | (*data++ & 0x7F);
            if (--remaining == 0) return false;
            if (value > (INT_MAX >> 7)) return false;
        }
        value = value << 7 | (*data++ & 0x7F);
        tag = static_cast<int>(value);
        if (--remaining == 0) return false;
    } else {
        ++data;
        if (--remaining == 0) return false;
    }
    int lengthBytes = static_cast<int>(remaining);
    if (lengthBytes-- < 1) return false;
    bool indefinite = false;
    unsigned long length = 0;
    if (*data == 0x80) {
        indefinite = true;
        ++data;
    } else {
        unsigned count = *data & 0x7F;
        if (*data++ & 0x80) {
            if (lengthBytes < static_cast<int>(count)) return false;
            while (count && !*data) {
                ++data;
                --count;
            }
            if (count > sizeof(long)) return false;
            while (count-- > 0) length = length << 8 | *data++;
        } else {
            length = count;
        }
    }
    if (length > static_cast<unsigned long>(LONG_MAX)) return false;
    if (indefinite && !constructed) return false;
    const long header = data - start;
    if (static_cast<long>(length) > available - header) return false;
    element = {tag, tagClass, constructed, indefinite, data, static_cast<long>(length), data + length};
    return true;
}

bool ReadTop(const unsigned char* data, std::int64_t length, int tag, Element& element) {
    return ReadHeader(data, length, element) && element.tag == tag && element.tagClass == 0;
}

Field ReadField(Cursor& cursor, int tag, Element& element) {
    if (!cursor.length) return Field::Absent;
    if (!ReadHeader(cursor.data, cursor.length, element)) return Field::Failed;
    if (tag >= 0 && (element.tag != tag || element.tagClass != 0)) return Field::Failed;
    return Field::Present;
}

void Advance(Cursor& cursor, const Element& element) {
    cursor.length -= element.end - cursor.data;
    cursor.data = element.end;
}

bool ValidObject(const Element& element) {
    if (element.length <= 0 || element.length > INT_MAX || (element.content[element.length - 1] & 0x80)) return false;
    for (long index = 0; index < element.length; ++index)
        if (element.content[index] == 0x80 && (!index || !(element.content[index - 1] & 0x80))) return false;
    return true;
}

bool ValidAny(const Element& element, const char* function) {
    if (element.tagClass != 0) {
        if (element.indefinite) UnsupportedEncoding(function, "indefinite-length parameter");
        return true;
    }
    if (element.tag == 16 || element.tag == 17) {
        if (!element.constructed) return false;
        if (element.indefinite) UnsupportedEncoding(function, "indefinite-length parameter");
        return true;
    }
    if (element.constructed) UnsupportedEncoding(function, "constructed parameter");
    switch (element.tag) {
    case 1: return element.length == 1;
    case 3: return element.length >= 1;
    case 5: return element.length == 0;
    case ObjectIdentifier: return ValidObject(element);
    case 28: return element.length % 4 == 0;
    case 30: return element.length % 2 == 0;
    default: return true;
    }
}

bool ReadSequence(Cursor& cursor, Element& element, const char* function) {
    if (ReadField(cursor, Sequence, element) != Field::Present || !element.constructed) return false;
    if (element.indefinite) UnsupportedEncoding(function, "indefinite-length SEQUENCE");
    return true;
}

bool ReadPrimitive(Cursor& cursor, int tag, Element& element, const char* function) {
    if (ReadField(cursor, tag, element) != Field::Present) return false;
    if (element.constructed) UnsupportedEncoding(function, "constructed string");
    Advance(cursor, element);
    return true;
}

bool ReadAlgorithm(Cursor& cursor, Algorithm& algorithm, const char* function) {
    Element sequence;
    if (!ReadSequence(cursor, sequence, function)) return false;
    Advance(cursor, sequence);
    Cursor fields {sequence.content, sequence.length};
    Element object;
    if (!ReadPrimitive(fields, ObjectIdentifier, object, function) || !ValidObject(object)) return false;
    algorithm = {object.content, object.length, false, false, Element{}};
    if (fields.length) {
        Element parameter;
        if (ReadField(fields, -1, parameter) != Field::Present || !ValidAny(parameter, function)) return false;
        Advance(fields, parameter);
        algorithm.parameter = true;
        algorithm.parameterElement = parameter;
        algorithm.nullParameter = parameter.tagClass == 0 && parameter.tag == 5;
    }
    return !fields.length;
}

Field ReadInteger(Cursor& cursor, std::uint64_t& value) {
    if (!cursor.length) return Field::Absent;
    Element integer;
    if (ReadField(cursor, Integer, integer) != Field::Present || integer.constructed || integer.length < 1 ||
        (integer.content[0] & 0x80)) return Field::Failed;
    std::uint64_t result = 0;
    for (long index = 0; index < integer.length; ++index) {
        const std::uint64_t byte = integer.content[index];
        if (result > (std::numeric_limits<std::uint64_t>::max() - byte) / 256) return Field::Failed;
        result = result * 256 + byte;
    }
    if (!result) return Field::Failed;
    Advance(cursor, integer);
    value = result;
    return Field::Present;
}

int AlgorithmNid(const Algorithm& algorithm) {
    const LibreSslAsn1Object object {nullptr, nullptr, 0, static_cast<int>(algorithm.objectLength), algorithm.object, 0};
    return LibreSslObjectNid_nid_no_patch(&object);
}

bool SequenceParameter(const Algorithm& algorithm, Cursor& cursor) {
    if (!algorithm.parameter || algorithm.parameterElement.tag != Sequence || algorithm.parameterElement.tagClass != 0 ||
        !algorithm.parameterElement.constructed || algorithm.parameterElement.indefinite) return false;
    cursor = {algorithm.parameterElement.content, algorithm.parameterElement.length};
    return true;
}

bool PrfDigest(int nid, mbedtls_md_type_t& type) {
    switch (nid) {
    case HmacWithSha1Nid: type = MBEDTLS_MD_SHA1; return true;
    case HmacWithSha224Nid: type = MBEDTLS_MD_SHA224; return true;
    case HmacWithSha256Nid: type = MBEDTLS_MD_SHA256; return true;
    case HmacWithSha384Nid: type = MBEDTLS_MD_SHA384; return true;
    case HmacWithSha512Nid: type = MBEDTLS_MD_SHA512; return true;
    default: return false;
    }
}

int MdBlockSize(mbedtls_md_type_t type) {
    switch (type) {
    case MBEDTLS_MD_MD5:
    case MBEDTLS_MD_SHA1:
    case MBEDTLS_MD_SHA224:
    case MBEDTLS_MD_SHA256: return 64;
    case MBEDTLS_MD_SHA384:
    case MBEDTLS_MD_SHA512: return 128;
    default: return 0;
    }
}

bool CipherInfoForNid(int nid, CipherKind& kind, std::size_t& keyLength, std::size_t& ivLength) {
    switch (nid) {
    case Aes128CbcNid: kind = CipherKind::Aes; keyLength = 16; ivLength = 16; return true;
    case Aes192CbcNid: kind = CipherKind::Aes; keyLength = 24; ivLength = 16; return true;
    case Aes256CbcNid: kind = CipherKind::Aes; keyLength = 32; ivLength = 16; return true;
    case DesEde3CbcNid: kind = CipherKind::Des3; keyLength = 24; ivLength = 8; return true;
    case DesCbcNid: kind = CipherKind::Des; keyLength = 8; ivLength = 8; return true;
    default: return false;
    }
}

struct DerivedKey {
    unsigned char key[32];
    unsigned char iv[16];
    std::size_t keyLength;
    std::size_t ivLength;
    int blockSize;
    CipherKind kind;
};

bool Pkcs12KeyGenUni(const char* pass, int passlen, const Element& salt, int id, std::uint64_t iterations,
                     std::size_t length, unsigned char* out, mbedtls_md_type_t type) {
    const auto* info = mbedtls_md_info_from_type(type);
    if (!info) return false;
    const int blockSize = MdBlockSize(type);
    const std::size_t digestLength = mbedtls_md_get_size(info);
    if (blockSize <= 0 || !digestLength || salt.length <= 0 || !iterations || !length) return false;
    std::vector<unsigned char> unicode;
    unicode.reserve((static_cast<std::size_t>(passlen) + 1) * 2);
    for (int index = 0; index < passlen; ++index) {
        unicode.push_back(0);
        unicode.push_back(static_cast<unsigned char>(pass[index]));
    }
    unicode.push_back(0);
    unicode.push_back(0);
    VectorWipeGuard unicodeGuard {unicode};
    const std::size_t saltLength = static_cast<std::size_t>(salt.length);
    const std::size_t saltBlocks = static_cast<std::size_t>(blockSize) * ((saltLength + blockSize - 1) / blockSize);
    const std::size_t passLength = unicode.size();
    const std::size_t passBlocks = static_cast<std::size_t>(blockSize) * ((passLength + blockSize - 1) / blockSize);
    std::vector<unsigned char> input(saltBlocks + passBlocks);
    VectorWipeGuard inputGuard {input};
    for (std::size_t index = 0; index < saltBlocks; ++index) input[index] = salt.content[index % saltLength];
    for (std::size_t index = 0; index < passBlocks; ++index) input[saltBlocks + index] = unicode[index % passLength];
    std::vector<unsigned char> d(static_cast<std::size_t>(blockSize), static_cast<unsigned char>(id));
    std::vector<unsigned char> digest(digestLength);
    std::vector<unsigned char> block(static_cast<std::size_t>(blockSize));
    std::vector<unsigned char> next(static_cast<std::size_t>(blockSize));
    VectorWipeGuard dGuard {d};
    VectorWipeGuard digestGuard {digest};
    VectorWipeGuard blockGuard {block};
    VectorWipeGuard nextGuard {next};
    MdContextGuard contextGuard;
    mbedtls_md_init(&contextGuard.context);
    if (mbedtls_md_setup(&contextGuard.context, info, 0) != 0) return false;
    std::size_t produced = 0;
    while (produced < length) {
        if (mbedtls_md_starts(&contextGuard.context) != 0 ||
            mbedtls_md_update(&contextGuard.context, d.data(), d.size()) != 0 ||
            mbedtls_md_update(&contextGuard.context, input.data(), input.size()) != 0 ||
            mbedtls_md_finish(&contextGuard.context, digest.data()) != 0) return false;
        for (std::uint64_t round = 1; round < iterations; ++round) {
            if (mbedtls_md_starts(&contextGuard.context) != 0 ||
                mbedtls_md_update(&contextGuard.context, digest.data(), digest.size()) != 0 ||
                mbedtls_md_finish(&contextGuard.context, digest.data()) != 0) return false;
        }
        const std::size_t copied = std::min(digestLength, length - produced);
        std::memcpy(out + produced, digest.data(), copied);
        produced += copied;
        if (produced == length) break;
        for (std::size_t index = 0; index < block.size(); ++index) block[index] = digest[index % digestLength];
        std::memcpy(next.data(), block.data(), block.size());
        for (std::size_t index = block.size(); index-- > 0;) {
            if (++next[index] != 0) break;
        }
        for (std::size_t offset = 0; offset < input.size(); offset += block.size()) {
            unsigned carry = 0;
            for (std::size_t index = block.size(); index-- > 0;) {
                const unsigned sum = static_cast<unsigned>(input[offset + index]) + next[index] + carry;
                input[offset + index] = static_cast<unsigned char>(sum);
                carry = sum >> 8;
            }
        }
    }
    return true;
}

bool Pbes1KeyIv(const char* pass, int passlen, const Element& salt, std::uint64_t iterations, mbedtls_md_type_t type,
                unsigned char* key, unsigned char* iv) {
    const auto* info = mbedtls_md_info_from_type(type);
    if (!info) return false;
    const std::size_t digestLength = mbedtls_md_get_size(info);
    if (digestLength < 16 || salt.length <= 0 || !iterations) return false;
    std::array<unsigned char, MBEDTLS_MD_MAX_SIZE> digest {};
    WipeGuard digestGuard {digest.data(), digest.size()};
    MdContextGuard contextGuard;
    mbedtls_md_init(&contextGuard.context);
    if (mbedtls_md_setup(&contextGuard.context, info, 0) != 0) return false;
    if (mbedtls_md_starts(&contextGuard.context) != 0 ||
        mbedtls_md_update(&contextGuard.context, reinterpret_cast<const unsigned char*>(pass), static_cast<std::size_t>(passlen)) != 0 ||
        mbedtls_md_update(&contextGuard.context, salt.content, static_cast<std::size_t>(salt.length)) != 0 ||
        mbedtls_md_finish(&contextGuard.context, digest.data()) != 0) return false;
    for (std::uint64_t round = 1; round < iterations; ++round) {
        if (mbedtls_md_starts(&contextGuard.context) != 0 ||
            mbedtls_md_update(&contextGuard.context, digest.data(), digestLength) != 0 ||
            mbedtls_md_finish(&contextGuard.context, digest.data()) != 0) return false;
    }
    std::memcpy(key, digest.data(), 8);
    std::memcpy(iv, digest.data() + 8, 8);
    return true;
}

DecryptResult DerivePbes2(const Algorithm& algorithm, const char* pass, int passlen, DerivedKey& derived) {
    Cursor parameters;
    if (!SequenceParameter(algorithm, parameters)) return DecryptResult::DecodeError;
    Algorithm kdf;
    Algorithm scheme;
    if (!ReadAlgorithm(parameters, kdf, __func__) || !ReadAlgorithm(parameters, scheme, __func__) || parameters.length)
        return DecryptResult::DecodeError;
    if (AlgorithmNid(kdf) != Pbkdf2Nid) return DecryptResult::UnsupportedKdf;
    CipherKind kind;
    std::size_t keyLength = 0;
    std::size_t ivLength = 0;
    if (!CipherInfoForNid(AlgorithmNid(scheme), kind, keyLength, ivLength)) return DecryptResult::UnsupportedCipher;
    if (!scheme.parameter || scheme.parameterElement.tag != OctetString || scheme.parameterElement.tagClass != 0 ||
        scheme.parameterElement.constructed || scheme.parameterElement.length != static_cast<long>(ivLength))
        return DecryptResult::CipherParameterError;
    Cursor kdfParameters;
    if (!SequenceParameter(kdf, kdfParameters)) return DecryptResult::DecodeError;
    Element salt;
    std::uint64_t iterations = 0;
    if (!ReadPrimitive(kdfParameters, OctetString, salt, __func__) || ReadInteger(kdfParameters, iterations) != Field::Present)
        return DecryptResult::DecodeError;
    Element next {};
    const Field nextField = ReadField(kdfParameters, -1, next);
    if (nextField == Field::Failed) return DecryptResult::DecodeError;
    if (nextField == Field::Present && next.tag == Integer && next.tagClass == 0 && !next.constructed) {
        std::uint64_t declaredKeyLength = 0;
        if (ReadInteger(kdfParameters, declaredKeyLength) != Field::Present || declaredKeyLength != keyLength)
            return DecryptResult::DecodeError;
    }
    mbedtls_md_type_t type = MBEDTLS_MD_SHA1;
    if (kdfParameters.length) {
        Element trailing {};
        if (ReadField(kdfParameters, -1, trailing) == Field::Present &&
            (trailing.tag != Sequence || trailing.tagClass != 0 || !trailing.constructed)) {
            return DecryptResult::KdfParameterWrongTag;
        }
        Algorithm prf;
        if (!ReadAlgorithm(kdfParameters, prf, __func__)) return DecryptResult::DecodeError;
        if (!PrfDigest(AlgorithmNid(prf), type)) return DecryptResult::UnsupportedPrf;
    }
    if (kdfParameters.length || !iterations || iterations > std::numeric_limits<unsigned int>::max())
        return DecryptResult::DecodeError;
    derived.kind = kind;
    derived.keyLength = keyLength;
    derived.ivLength = ivLength;
    derived.blockSize = kind == CipherKind::Aes ? 16 : 8;
    std::memcpy(derived.iv, scheme.parameterElement.content, ivLength);
    return mbedtls_pkcs5_pbkdf2_hmac_ext(type, reinterpret_cast<const unsigned char*>(pass), static_cast<std::size_t>(passlen),
                                         salt.content, static_cast<std::size_t>(salt.length), static_cast<unsigned int>(iterations),
                                         static_cast<std::uint32_t>(keyLength), derived.key) == 0 ? DecryptResult::Success : DecryptResult::BadDecrypt;
}

bool DerivePbe(const Algorithm& algorithm, const char* pass, int passlen, DerivedKey& derived) {
    const int nid = AlgorithmNid(algorithm);
    mbedtls_md_type_t type;
    CipherKind kind;
    std::size_t keyLength = 0;
    std::size_t ivLength = 8;
    if (nid == PbeSha1ThreeDesNid) {
        type = MBEDTLS_MD_SHA1;
        kind = CipherKind::Des3;
        keyLength = 24;
    } else if (nid == PbeMd5DesNid || nid == PbeSha1DesNid) {
        type = nid == PbeMd5DesNid ? MBEDTLS_MD_MD5 : MBEDTLS_MD_SHA1;
        kind = CipherKind::Des;
        keyLength = 8;
    } else {
        return false;
    }
    Cursor parameters;
    if (!SequenceParameter(algorithm, parameters)) return false;
    Element salt;
    std::uint64_t iterations = 1;
    if (!ReadPrimitive(parameters, OctetString, salt, __func__) || ReadInteger(parameters, iterations) == Field::Failed ||
        parameters.length || !iterations || iterations > std::numeric_limits<unsigned int>::max()) return false;
    derived.kind = kind;
    derived.keyLength = keyLength;
    derived.ivLength = ivLength;
    derived.blockSize = 8;
    if (kind == CipherKind::Des3)
        return Pkcs12KeyGenUni(pass, passlen, salt, 1, iterations, keyLength, derived.key, type) &&
               Pkcs12KeyGenUni(pass, passlen, salt, 2, iterations, ivLength, derived.iv, type);
    return Pbes1KeyIv(pass, passlen, salt, iterations, type, derived.key, derived.iv);
}

bool DecryptCipher(const DerivedKey& derived, const Element& encrypted, std::vector<unsigned char>& output) {
    if (encrypted.length <= 0 || encrypted.length % derived.blockSize != 0) return false;
    output.assign(static_cast<std::size_t>(encrypted.length), 0);
    bool decrypted = false;
    if (derived.kind == CipherKind::Aes) {
        unsigned char iv[16];
        std::memcpy(iv, derived.iv, sizeof(iv));
        WipeGuard ivGuard {iv, sizeof(iv)};
        mbedtls_aes_context context;
        mbedtls_aes_init(&context);
        WipeGuard contextGuard {&context, sizeof(context)};
        decrypted = mbedtls_aes_setkey_dec(&context, derived.key, static_cast<unsigned>(derived.keyLength * 8)) == 0 &&
                    mbedtls_aes_crypt_cbc(&context, MBEDTLS_AES_DECRYPT, output.size(), iv, encrypted.content, output.data()) == 0;
        mbedtls_aes_free(&context);
    } else if (derived.kind == CipherKind::Des3) {
        unsigned char iv[8];
        std::memcpy(iv, derived.iv, sizeof(iv));
        WipeGuard ivGuard {iv, sizeof(iv)};
        mbedtls_des3_context context;
        mbedtls_des3_init(&context);
        WipeGuard contextGuard {&context, sizeof(context)};
        decrypted = mbedtls_des3_set3key_dec(&context, derived.key) == 0 &&
                    mbedtls_des3_crypt_cbc(&context, MBEDTLS_DES_DECRYPT, output.size(), iv, encrypted.content, output.data()) == 0;
        mbedtls_des3_free(&context);
    } else {
        unsigned char iv[8];
        std::memcpy(iv, derived.iv, sizeof(iv));
        WipeGuard ivGuard {iv, sizeof(iv)};
        mbedtls_des_context context;
        mbedtls_des_init(&context);
        WipeGuard contextGuard {&context, sizeof(context)};
        decrypted = mbedtls_des_setkey_dec(&context, derived.key) == 0 &&
                    mbedtls_des_crypt_cbc(&context, MBEDTLS_DES_DECRYPT, output.size(), iv, encrypted.content, output.data()) == 0;
        mbedtls_des_free(&context);
    }
    if (!decrypted || output.empty()) {
        mbedtls_platform_zeroize(output.data(), output.size());
        return false;
    }
    const unsigned padding = output.back();
    if (padding < 1 || padding > static_cast<unsigned>(derived.blockSize) || padding > output.size()) {
        mbedtls_platform_zeroize(output.data(), output.size());
        return false;
    }
    for (std::size_t index = 0; index < padding; ++index) {
        if (output[output.size() - 1 - index] != padding) {
            mbedtls_platform_zeroize(output.data(), output.size());
            return false;
        }
    }
    output.resize(output.size() - padding);
    return true;
}

DecryptResult DecryptPrivateKey(const Algorithm& algorithm, const Element& encrypted, const char* pass, int passlen,
                                std::vector<unsigned char>& output) {
    DerivedKey derived {};
    WipeGuard keyGuard {derived.key, sizeof(derived.key)};
    WipeGuard ivGuard {derived.iv, sizeof(derived.iv)};
    const int nid = AlgorithmNid(algorithm);
    if (nid != Pbes2Nid && nid != PbeSha1ThreeDesNid && nid != PbeMd5DesNid && nid != PbeSha1DesNid)
        return DecryptResult::UnknownPbe;
    const DecryptResult result = nid == Pbes2Nid ? DerivePbes2(algorithm, pass, passlen, derived) :
        (DerivePbe(algorithm, pass, passlen, derived) ? DecryptResult::Success : DecryptResult::DecodeError);
    if (result != DecryptResult::Success) return result;
    return DecryptCipher(derived, encrypted, output) ? DecryptResult::Success : DecryptResult::BadDecrypt;
}

void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, FunctionUnknown, reason, __FILE__, __LINE__);
}

void PushDecryptFailure() {
    PutError(EvpLibrary, ReasonBadDecrypt);
    PutError(Pkcs12Library, ReasonPkcs12CipherFinalError);
    PutError(Pkcs12Library, ReasonPkcs12DecryptError);
    LibreSsl::PutPemError(Asn1Library);
}

void PushAlgorithmFailure(DecryptResult result) {
    switch (result) {
    case DecryptResult::UnknownPbe: PutError(EvpLibrary, ReasonUnknownPbeAlgorithm); break;
    case DecryptResult::DecodeError: PutError(EvpLibrary, ReasonDecodeError); break;
    case DecryptResult::KdfParameterWrongTag:
        PutError(Asn1Library, ReasonAsn1WrongTag);
        PutError(Asn1Library, ReasonAsn1NestedError);
        PutError(Asn1Library, ReasonAsn1NestedError);
        PutError(EvpLibrary, ReasonDecodeError);
        break;
    case DecryptResult::UnsupportedKdf: PutError(EvpLibrary, ReasonUnsupportedKeyDerivationFunction); break;
    case DecryptResult::UnsupportedCipher: PutError(EvpLibrary, ReasonUnsupportedCipher); break;
    case DecryptResult::UnsupportedPrf: PutError(EvpLibrary, ReasonUnsupportedPrf); break;
    case DecryptResult::CipherParameterError:
        PutError(Asn1Library, 109);
        PutError(EvpLibrary, ReasonCipherParameterError);
        break;
    default: break;
    }
    if (result != DecryptResult::UnknownPbe) PutError(EvpLibrary, ReasonKeygenFailure);
    PutError(Pkcs12Library, ReasonPkcs12AlgorithmCipherInitError);
    PutError(Pkcs12Library, 117);
    LibreSsl::PutPemError(Asn1Library);
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

void* ReadGuestKeySlot(void** slot, const char* function) {
    void* value = nullptr;
    if (!GuestCopyIn_nid_no_patch(&value, slot, sizeof(value))) NotImplemented_nid_no_patch(function);
    return value;
}

void WriteGuestKeySlot(void** slot, void* value, const char* function) {
    if (!GuestCopyOut_nid_no_patch(slot, &value, sizeof(value))) NotImplemented_nid_no_patch(function);
}

void* DecodeRsaPrivateKey(void** reuse, const unsigned char** input, long length, const char* function) {
    void* const existing = reuse ? ReadGuestKeySlot(reuse, function) : nullptr;
    void* pkey = existing;
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
            if (existing != pkey) EVP_PKEY_free_nid_postfix(pkey);
            return nullptr;
        }
        EVP_PKEY_free_nid_postfix(pkey);
        pkey = Pkcs8ToKey(info, function);
    }
    if (reuse) WriteGuestKeySlot(reuse, pkey, function);
    return pkey;
}

}

extern "C" {

void* APS5_VABI PEM_read_bio_PrivateKey_nid_postfix(LibreSsl::Bio* bio, void** key, LibreSslPemPasswordCallback callback,
                                                      void* userData) {
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
                EVP_PKEY_free_nid_postfix(ReadGuestKeySlot(key, __func__));
                WriteGuestKeySlot(key, result, __func__);
            }
        }
    } else if (!std::strcmp(name, "ENCRYPTED PRIVATE KEY")) {
        Element sequence;
        if (!ReadTop(data, length, Sequence, sequence) || !sequence.constructed || sequence.indefinite ||
            sequence.end != data + length) {
            LibreSsl::PutPemError(Asn1Library);
            return nullptr;
        }
        Cursor fields {sequence.content, sequence.length};
        Algorithm algorithm;
        Element encrypted;
        if (!ReadAlgorithm(fields, algorithm, __func__) || !ReadPrimitive(fields, OctetString, encrypted, __func__) ||
            fields.length) {
            LibreSsl::PutPemError(Asn1Library);
            return nullptr;
        }
        char passbuf[1024];
        WipeGuard passGuard {passbuf, sizeof(passbuf)};
        int passlen = -1;
        if (callback != nullptr) {
            passlen = callback(passbuf, static_cast<int>(sizeof(passbuf)), 0, userData);
        } else if (userData != nullptr) {
            const char* const text = static_cast<const char*>(userData);
            std::size_t count = 0;
            while (count < sizeof(passbuf) && text[count]) ++count;
            std::memcpy(passbuf, text, count);
            passlen = static_cast<int>(count);
        }
        if (passlen > static_cast<int>(sizeof(passbuf))) passlen = -1;
        if (passlen <= 0) {
            mbedtls_platform_zeroize(passbuf, sizeof(passbuf));
            PutError(PemLibrary, ReasonBadPasswordRead);
            return nullptr;
        }
        std::vector<unsigned char> decrypted;
        VectorWipeGuard decryptedGuard {decrypted};
        const DecryptResult decryptResult = DecryptPrivateKey(algorithm, encrypted, passbuf, passlen, decrypted);
        if (decryptResult != DecryptResult::Success) {
            mbedtls_platform_zeroize(passbuf, sizeof(passbuf));
            mbedtls_platform_zeroize(decrypted.data(), decrypted.size());
            if (decryptResult == DecryptResult::BadDecrypt) PushDecryptFailure();
            else PushAlgorithmFailure(decryptResult);
            return nullptr;
        }
        const unsigned char* decryptedData = decrypted.data();
        LibreSsl::Pkcs8Key info;
        const bool decoded = LibreSsl::DecodePkcs8(&decryptedData, static_cast<long>(decrypted.size()), info, __func__);
        mbedtls_platform_zeroize(passbuf, sizeof(passbuf));
        if (!decoded) {
            if (!decrypted.empty() && decrypted[0] != 0x30) {
                PutError(Asn1Library, ReasonAsn1WrongTag);
                PutError(Asn1Library, ReasonAsn1NestedError);
            }
            mbedtls_platform_zeroize(decrypted.data(), decrypted.size());
            PutError(Pkcs12Library, ReasonPkcs12DecodeError);
            LibreSsl::PutPemError(Asn1Library);
            return nullptr;
        }
        result = Pkcs8ToKey(info, __func__);
        if (!result) {
            mbedtls_platform_zeroize(decrypted.data(), decrypted.size());
            LibreSsl::PutPemError(Asn1Library);
            return nullptr;
        }
        if (key) {
            EVP_PKEY_free_nid_postfix(ReadGuestKeySlot(key, __func__));
            WriteGuestKeySlot(key, result, __func__);
        }
    } else if (const int prefix = LibreSsl::PemCheckSuffix(name, "PRIVATE KEY"); prefix > 0) {
        const int type = LibreSsl::OldPrivateKeyType(name, prefix);
        if (type == KeyRsa) result = DecodeRsaPrivateKey(key, &data, length, __func__);
        else if (type) NotImplemented_nid_no_patch((std::string(__func__) + ": a traditional " + name + " record").c_str());
    }
    if (!result) LibreSsl::PutPemError(Asn1Library);
    return result;
}

}
