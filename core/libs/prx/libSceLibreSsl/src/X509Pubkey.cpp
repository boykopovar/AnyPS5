/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/x509/x509_cmp.c (v 1.27) L321-327;
 * src/lib/libcrypto/asn1/x_pubkey.c (v 1.26) L177-224 and L414-427; src/lib/libcrypto/rsa/rsa_ameth.c (v 1.18) L89-104;
 * src/lib/libcrypto/rsa/rsa_asn1.c (v 1.13) L167-199 and L285-290; src/lib/libcrypto/asn1/x_bignum.c (v 1.8) L87-165;
 * src/lib/libcrypto/asn1/tasn_dec.c (v 1.34) L295-435, L652-770 and L1073-1140; src/lib/libcrypto/asn1/asn1_lib.c (v 1.38) L95-190;
 * src/lib/libcrypto/evp/p_lib.c (v 1.17) L211-258 and L267-275; src/lib/libcrypto/asn1/ameth_lib.c (v 1.16) L84-112 and L165-214;
 * src/lib/libcrypto/ec/ec_ameth.c (v 1.18) L138-213; src/lib/libcrypto/ec/ec_asn1.c (v 1.23) L1539-1565;
 * src/lib/libcrypto/asn1/x_algor.c (v 1.21) L96-110 */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2000-2004 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "PkeyEc.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int EvpLibrary = 6;
constexpr int RsaLibrary = 4;
constexpr int EcLibrary = 16;
constexpr int X509Library = 11;
constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;

constexpr int NestedAsn1Error = 58;
constexpr int RsaLibraryError = 4;
constexpr int EcLibraryError = 16;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int EcDecodeError = 142;
constexpr int UnsupportedAlgorithm = 111;
constexpr int MethodNotSupported = 124;
constexpr int PublicKeyDecodeError = 125;
constexpr int FieldMissing = 121;
constexpr int HeaderTooLong = 123;
constexpr int MissingEoc = 137;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int TooLong = 155;
constexpr int EvpUnsupportedAlgorithm = 156;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int NestedAsn1String = 197;

constexpr int RsaEncryptionNid = 6;
constexpr int DsaEncryptionNid = 116;
constexpr int IdEcPublicKeyNid = 408;

constexpr int Universal = 0;
constexpr int Constructed = 0x20;
constexpr int HighTag = 0x1f;
constexpr int ClassMask = 0xc0;
constexpr int Object = 6;
constexpr int Integer = 2;
constexpr int Sequence = 16;
constexpr int MaximumStringNesting = 5;
constexpr std::size_t GrowthLimit = 0x5ffffffc;

constexpr int NamedCurveFlag = 1;

struct Pkey {
    int type;
    int saveType;
    int references;
    const void* ameth;
    void* engine;
    void* key;
    int saveParameters;
    void* attributes;
};

static_assert(sizeof(Pkey) == 56 && offsetof(Pkey, references) == 8 && offsetof(Pkey, key) == 32);

struct Buffer {
    std::uint64_t length;
    char* data;
    std::uint64_t max;
};

static_assert(sizeof(Buffer) == 24 && offsetof(Buffer, data) == 8 && offsetof(Buffer, max) == 16);

struct HeaderCache {
    bool valid;
    int result;
    std::int64_t length;
    int tag;
    int objectClass;
    int headerLength;
};

void Queue(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, AnyFunction, reason, __FILE__, __LINE__);
}

void Asn1Queue(int reason) {
    Queue(Asn1Library, reason);
}

Pkey* AsPkey(void* value) {
    return static_cast<Pkey*>(value);
}

void AddReference(void* value) {
    std::atomic_ref<int>(AsPkey(value)->references).fetch_add(1, std::memory_order_acq_rel);
}

bool Grow(Buffer* buffer, std::size_t length, bool clear) {
    if (buffer->length >= length || buffer->max >= length) {
        buffer->length = length;
        return true;
    }
    if (length > GrowthLimit) return false;
    const std::size_t capacity = ((length + 3) / 3) * 4;
    auto* const grown = static_cast<char*>(recallocarray_nid_postfix(buffer->data, buffer->max, capacity, 1));
    if (!grown) return false;
    buffer->data = grown;
    buffer->max = capacity;
    if (!clear) std::memset(buffer->data + buffer->length, 0, capacity - buffer->length);
    buffer->length = length;
    return true;
}

int GetLength(const unsigned char** in, int* indefinite, std::int64_t* length, int maximum) {
    const unsigned char* p = *in;
    std::uint64_t result = 0;
    int count = 0;
    if (maximum-- < 1) return 0;
    if (*p == 0x80) {
        *indefinite = 1;
        p++;
    } else {
        *indefinite = 0;
        count = *p & 0x7f;
        if (*(p++) & 0x80) {
            if (maximum < count) return 0;
            while (count && *p == 0) {
                ++p;
                --count;
            }
            if (count > static_cast<int>(sizeof(std::int64_t))) return 0;
            while (count-- > 0) {
                result <<= 8;
                result |= *p++;
            }
        } else {
            result = count;
        }
    }
    if (result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return 0;
    *in = p;
    *length = static_cast<std::int64_t>(result);
    return 1;
}

int GetObject(const unsigned char** in, std::int64_t* length, int* tag, int* objectClass, std::int64_t limit) {
    auto headerError = [] {
        Asn1Queue(HeaderTooLong);
        return 0x80;
    };
    const unsigned char* p = *in;
    std::int64_t maximum = limit;
    if (!maximum) return headerError();
    int result = *p & Constructed;
    const int objectClassNumber = *p & ClassMask;
    const int low = *p & HighTag;
    int number;
    if (low == HighTag) {
        ++p;
        if (--maximum == 0) return headerError();
        std::int64_t value = 0;
        while (*p & 0x80) {
            value <<= 7;
            value |= *p++ & 0x7f;
            if (--maximum == 0) return headerError();
            if (value > (std::numeric_limits<int>::max() >> 7)) return headerError();
        }
        value <<= 7;
        value |= *p++ & 0x7f;
        number = static_cast<int>(value);
        if (--maximum == 0) return headerError();
    } else {
        number = low;
        ++p;
        if (--maximum == 0) return headerError();
    }
    *tag = number;
    *objectClass = objectClassNumber;
    int indefinite = 0;
    if (!GetLength(&p, &indefinite, length, static_cast<int>(maximum))) return headerError();
    if (indefinite && !(result & Constructed)) return headerError();
    if (*length > limit - (p - *in)) {
        Asn1Queue(TooLong);
        result |= 0x80;
    }
    *in = p;
    return result | indefinite;
}

bool CheckEoc(const unsigned char** in, std::int64_t length) {
    if (length < 2) return false;
    const unsigned char* p = *in;
    if (!p[0] && !p[1]) {
        *in += 2;
        return true;
    }
    return false;
}

int CheckTagLength(std::int64_t* length, int* tag, unsigned char* objectClass, char* indefinite, char* constructed, const unsigned char** in,
    std::int64_t limit, int expectedTag, int expectedClass, char optional, HeaderCache* cache) {
    const unsigned char* p = *in;
    const unsigned char* start = p;
    int result;
    int foundTag = 0;
    int foundClass = 0;
    std::int64_t contentLength = 0;
    if (cache && cache->valid) {
        result = cache->result;
        contentLength = cache->length;
        foundClass = cache->objectClass;
        foundTag = cache->tag;
        p += cache->headerLength;
    } else {
        result = GetObject(&p, &contentLength, &foundTag, &foundClass, limit);
        if (cache) {
            cache->result = result;
            cache->length = contentLength;
            cache->objectClass = foundClass;
            cache->tag = foundTag;
            cache->headerLength = static_cast<int>(p - start);
            cache->valid = true;
            if (!(result & 0x81) && contentLength + cache->headerLength > limit) {
                Asn1Queue(TooLong);
                cache->valid = false;
                return 0;
            }
        }
    }
    if (result & 0x80) {
        Asn1Queue(BadObjectHeader);
        if (cache) cache->valid = false;
        return 0;
    }
    if (expectedTag >= 0) {
        if (expectedTag != foundTag || expectedClass != foundClass) {
            if (optional) return -1;
            if (cache) cache->valid = false;
            Asn1Queue(WrongTag);
            return 0;
        }
        if (cache) cache->valid = false;
    }
    if (result & 1) contentLength = limit - (p - start);
    if (indefinite) *indefinite = static_cast<char>(result & 1);
    if (constructed) *constructed = static_cast<char>(result & Constructed);
    if (length) *length = contentLength;
    if (objectClass) *objectClass = static_cast<unsigned char>(foundClass);
    if (tag) *tag = foundTag;
    *in = p;
    return 1;
}

bool CollectData(Buffer* buffer, const unsigned char** p, std::int64_t length) {
    const std::uint64_t used = buffer->length;
    if (!Grow(buffer, used + static_cast<std::uint64_t>(length), true)) {
        Asn1Queue(MallocFailure);
        return false;
    }
    std::memcpy(buffer->data + used, *p, static_cast<std::size_t>(length));
    *p += length;
    return true;
}

bool Collect(Buffer* buffer, const unsigned char** in, std::int64_t length, char indefinite, int tag, int tagClass, int depth) {
    const unsigned char* p = *in;
    indefinite &= 1;
    if (!buffer && !indefinite) {
        *in += length;
        return true;
    }
    while (length > 0) {
        const unsigned char* q = p;
        if (CheckEoc(&p, length)) {
            if (!indefinite) {
                Asn1Queue(UnexpectedEoc);
                return false;
            }
            indefinite = 0;
            break;
        }
        std::int64_t contentLength = 0;
        char constructed = 0;
        char innerIndefinite = 0;
        if (!CheckTagLength(&contentLength, nullptr, nullptr, &innerIndefinite, &constructed, &p, length, tag, tagClass, 0, nullptr)) {
            Asn1Queue(NestedAsn1Error);
            return false;
        }
        if (constructed) {
            if (depth >= MaximumStringNesting) {
                Asn1Queue(NestedAsn1String);
                return false;
            }
            if (!Collect(buffer, &p, contentLength, innerIndefinite, tag, tagClass, depth + 1)) return false;
        } else if (contentLength && !CollectData(buffer, &p, contentLength)) {
            return false;
        }
        length -= p - q;
    }
    if (indefinite) {
        Asn1Queue(MissingEoc);
        return false;
    }
    *in = p;
    return true;
}

bool FindEnd(const unsigned char** in, std::int64_t length, char indefinite) {
    if (indefinite == 0) {
        *in += length;
        return true;
    }
    int expected = 1;
    const unsigned char* p = *in;
    while (length > 0) {
        if (CheckEoc(&p, length)) {
            if (--expected == 0) break;
            length -= 2;
            continue;
        }
        const unsigned char* q = p;
        std::int64_t contentLength = 0;
        if (!CheckTagLength(&contentLength, nullptr, nullptr, &indefinite, nullptr, &p, length, -1, 0, 0, nullptr)) {
            Asn1Queue(NestedAsn1Error);
            return false;
        }
        if (indefinite) ++expected;
        else p += contentLength;
        length -= p - q;
    }
    if (expected) {
        Asn1Queue(MissingEoc);
        return false;
    }
    *in = p;
    return true;
}

bool DecodeBignum(const unsigned char** in, std::int64_t limit, void** number, HeaderCache* cache) {
    const unsigned char* p = *in;
    std::int64_t contentLength = 0;
    char indefinite = 0;
    char constructed = 0;
    if (CheckTagLength(&contentLength, nullptr, nullptr, &indefinite, &constructed, &p, limit, Integer, Universal, 0, cache) != 1) {
        Asn1Queue(NestedAsn1Error);
        return false;
    }
    const unsigned char* content;
    std::int64_t length;
    Buffer collected{};
    bool ownsContent = false;
    auto release = [&] {
        if (ownsContent && collected.data) free_nid_postfix(collected.data);
    };
    if (constructed) {
        if (!Collect(&collected, &p, contentLength, indefinite, -1, Universal, 0)) {
            release();
            return false;
        }
        length = static_cast<std::int64_t>(collected.length);
        if (!Grow(&collected, static_cast<std::size_t>(length) + 1, true)) {
            Asn1Queue(MallocFailure);
            release();
            return false;
        }
        collected.data[length] = 0;
        content = reinterpret_cast<const unsigned char*>(collected.data);
        ownsContent = true;
    } else {
        content = p;
        length = contentLength;
        p += contentLength;
    }
    if (length > std::numeric_limits<int>::max()) NotImplemented_nid_no_patch("X509_get_pubkey: an INTEGER longer than INT_MAX bytes");
    *number = BN_bin2bn_nid_postfix(content, static_cast<int>(length), nullptr);
    release();
    if (!*number) return false;
    *in = p;
    return true;
}

struct Numbers {
    void* items[2] = {nullptr, nullptr};
    ~Numbers() {
        for (void* item : items)
            if (item) BN_free_nid_postfix(static_cast<Bignum*>(item));
    }
};

LibreSslRsa* DecodePublicKey(const unsigned char* input, std::int64_t limit) {
    HeaderCache cache{};
    const unsigned char* p = input;
    std::int64_t length = 0;
    char indefinite = 0;
    char constructed = 0;
    if (CheckTagLength(&length, nullptr, nullptr, &indefinite, &constructed, &p, limit, Sequence, Universal, 0, &cache) != 1) {
        Asn1Queue(NestedAsn1Error);
        return nullptr;
    }
    if (!constructed) {
        Asn1Queue(SequenceNotConstructed);
        return nullptr;
    }
    auto* rsa = RSA_new_nid_postfix();
    if (!rsa) {
        Asn1Queue(MallocFailure);
        return nullptr;
    }
    auto release = [&] {
        RSA_free_nid_postfix(rsa);
        return nullptr;
    };
    bool indefiniteContent = indefinite != 0;
    std::int64_t remaining = length;
    Numbers numbers;
    int index = 0;
    for (; index < 2; ++index) {
        if (!remaining) break;
        const unsigned char* q = p;
        if (CheckEoc(&p, remaining)) {
            if (!indefiniteContent) {
                Asn1Queue(UnexpectedEoc);
                return release();
            }
            remaining -= p - q;
            indefiniteContent = false;
            break;
        }
        if (!DecodeBignum(&p, remaining, &numbers.items[index], &cache)) {
            Asn1Queue(NestedAsn1Error);
            return release();
        }
        remaining -= p - q;
    }
    if (indefiniteContent && !CheckEoc(&p, remaining)) {
        Asn1Queue(MissingEoc);
        return release();
    }
    if (!indefiniteContent && remaining) {
        Asn1Queue(SequenceLengthMismatch);
        return release();
    }
    if (index < 2) {
        Asn1Queue(FieldMissing);
        return release();
    }
    rsa->n = numbers.items[0];
    rsa->e = numbers.items[1];
    numbers.items[0] = numbers.items[1] = nullptr;
    return rsa;
}

bool RsaPubDecode(void* pkey, LibreSslX509PublicKey* key) {
    const unsigned char* data = key->publicKey->data;
    const std::int64_t length = key->publicKey->length;
    if (!data && length != 0) throw std::invalid_argument("X509_get_pubkey: public key bits without content");
    auto* rsa = DecodePublicKey(data, length);
    if (!rsa) {
        Queue(RsaLibrary, RsaLibraryError);
        return false;
    }
    if (!EVP_PKEY_set1_RSA_nid_postfix(pkey, rsa)) {
        RSA_free_nid_postfix(rsa);
        return false;
    }
    RSA_free_nid_postfix(rsa);
    return true;
}

bool EcType2Param(LibreSslX509PublicKey* key, EcKey** result) {
    const LibreSslAsn1Type* parameter = key->algorithm->parameter;
    if (!parameter) {
        Queue(EcLibrary, EcDecodeError);
        return false;
    }
    if (parameter->type == Sequence) NotImplemented_nid_no_patch("X509_get_pubkey: elliptic curve parameters given as an explicit SEQUENCE");
    if (parameter->type != Object) {
        Queue(EcLibrary, EcDecodeError);
        return false;
    }
    EcKey* ecKey = EC_KEY_new_nid_postfix();
    if (!ecKey) {
        Queue(EcLibrary, MallocFailure);
        return false;
    }
    EcGroup* group = nullptr;
    try {
        group = EC_GROUP_new_by_curve_name_nid_postfix(OBJ_obj2nid_nid_postfix(static_cast<const LibreSslAsn1Object*>(parameter->value)));
        if (!group) {
            EC_KEY_free_nid_postfix(ecKey);
            return false;
        }
        group->asn1Flag = NamedCurveFlag;
        if (EC_KEY_set_group_nid_postfix(ecKey, group) == 0) {
            EC_GROUP_free_nid_postfix(group);
            EC_KEY_free_nid_postfix(ecKey);
            return false;
        }
        EC_GROUP_free_nid_postfix(group);
        group = nullptr;
    } catch (...) {
        if (group) EC_GROUP_free_nid_postfix(group);
        EC_KEY_free_nid_postfix(ecKey);
        throw;
    }
    *result = ecKey;
    return true;
}

bool EcPubDecode(void* pkey, LibreSslX509PublicKey* key) {
    EcKey* ecKey = nullptr;
    if (!EcType2Param(key, &ecKey)) {
        Queue(EcLibrary, EcLibraryError);
        return false;
    }
    EcPoint* point = nullptr;
    try {
        const EcGroup* curve = EC_KEY_get0_group_nid_postfix(ecKey);
        point = EC_POINT_new_nid_postfix(curve);
        if (!point) {
            Queue(EcLibrary, MallocFailure);
            Queue(EcLibrary, EcLibraryError);
            EC_KEY_free_nid_postfix(ecKey);
            return false;
        }
        const unsigned char* data = key->publicKey->data;
        const std::int64_t length = key->publicKey->length;
        if (length < 0) throw std::invalid_argument("X509_get_pubkey: negative public key bit length");
        if (!data && length != 0) throw std::invalid_argument("X509_get_pubkey: public key bits without content");
        if (EC_POINT_oct2point_nid_postfix(curve, point, data, static_cast<std::size_t>(length), nullptr) != 1) {
            EC_POINT_free_nid_postfix(point);
            EC_KEY_free_nid_postfix(ecKey);
            Queue(EcLibrary, EcLibraryError);
            Queue(EcLibrary, EcDecodeError);
            return false;
        }
        ecKey->conversionForm = data[0] & ~0x01;
        const bool assigned = EC_KEY_set_public_key_nid_postfix(ecKey, point) == 1 && LibreSsl::PkeyAssignEcKey_nid_no_patch(pkey, ecKey);
        EC_POINT_free_nid_postfix(point);
        point = nullptr;
        if (!assigned) {
            EC_KEY_free_nid_postfix(ecKey);
            Queue(EcLibrary, EcLibraryError);
            return false;
        }
        return true;
    } catch (...) {
        if (point) EC_POINT_free_nid_postfix(point);
        EC_KEY_free_nid_postfix(ecKey);
        throw;
    }
}

void* PubkeyGet(LibreSslX509PublicKey* key) {
    if (!key) return nullptr;
    std::atomic_ref<void*> cache(key->key);
    void* cached = cache.load(std::memory_order_acquire);
    if (cached) {
        AddReference(cached);
        return cached;
    }
    if (!key->publicKey) return nullptr;
    if (!key->algorithm || !key->algorithm->algorithm) throw std::invalid_argument("X509_get_pubkey: public key without an algorithm");
    void* pkey = EVP_PKEY_new_nid_postfix();
    if (!pkey) {
        Queue(X509Library, MallocFailure);
        return nullptr;
    }
    try {
        const int nid = OBJ_obj2nid_nid_postfix(key->algorithm->algorithm);
        const bool rsaKey = nid == RsaEncryptionNid;
        if (rsaKey || nid == IdEcPublicKeyNid) {
            const bool decoded = rsaKey ? RsaPubDecode(pkey, key) : EcPubDecode(pkey, key);
            if (!decoded) {
                Queue(X509Library, PublicKeyDecodeError);
                EVP_PKEY_free_nid_postfix(pkey);
                return nullptr;
            }
        } else if (nid == DsaEncryptionNid) {
            NotImplemented_nid_no_patch("X509_get_pubkey: a DSA public key this library cannot hold");
        } else {
            Queue(EvpLibrary, EvpUnsupportedAlgorithm);
            Queue(X509Library, UnsupportedAlgorithm);
            EVP_PKEY_free_nid_postfix(pkey);
            return nullptr;
        }
    } catch (...) {
        EVP_PKEY_free_nid_postfix(pkey);
        throw;
    }
    void* expected = nullptr;
    if (!cache.compare_exchange_strong(expected, pkey, std::memory_order_acq_rel, std::memory_order_acquire)) {
        EVP_PKEY_free_nid_postfix(pkey);
        pkey = expected;
    }
    AddReference(pkey);
    return pkey;
}

} // namespace

extern "C" {

void* APS5_VABI X509_get_pubkey_nid_postfix(LibreSslX509* certificate) {
    if (!certificate || !certificate->certificateInfo) return nullptr;
    return PubkeyGet(certificate->certificateInfo->key);
}

}
