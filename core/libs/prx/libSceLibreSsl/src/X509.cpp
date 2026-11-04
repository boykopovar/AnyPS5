/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/asn1/tasn_dec.c (v 1.34);
 * src/lib/libcrypto/asn1/tasn_new.c (v 1.17); src/lib/libcrypto/asn1/tasn_fre.c (v 1.15); src/lib/libcrypto/asn1/tasn_utl.c (v 1.12);
 * src/lib/libcrypto/asn1/asn1_lib.c (v 1.38); src/lib/libcrypto/asn1/a_object.c (v 1.29); src/lib/libcrypto/asn1/a_int.c (v 1.31);
 * src/lib/libcrypto/asn1/a_bitstr.c (v 1.24); src/lib/libcrypto/asn1/x_name.c (v 1.33); src/lib/libcrypto/asn1/x_x509.c (v 1.24);
 * src/lib/libcrypto/asn1/x_pubkey.c (v 1.26); src/lib/libcrypto/asn1/x_algor.c (v 1.21); src/lib/libcrypto/asn1/x_val.c (v 1.11);
 * src/lib/libcrypto/asn1/x_exten.c (v 1.16); src/lib/libcrypto/x509/x509_ext.c (v 1.9); src/lib/libcrypto/x509v3/v3_lib.c (v 1.17);
 * src/lib/libcrypto/x509v3/ext_dat.h (v 1.13); src/lib/libcrypto/x509v3/v3_genn.c (v 1.12); src/lib/libcrypto/x509v3/v3_bcons.c (v 1.15);
 * src/lib/libcrypto/x509v3/v3_akeya.c (v 1.7); src/lib/libcrypto/x509v3/v3_extku.c (v 1.15); src/lib/libcrypto/x509v3/v3_info.c (v 1.25);
 * src/lib/libcrypto/x509v3/v3_pku.c (v 1.13); src/lib/libcrypto/x509v3/v3_pcons.c (v 1.11); src/lib/libcrypto/x509v3/v3_pmaps.c (v 1.11);
 * src/lib/libcrypto/x509v3/v3_ncons.c (v 1.11); src/lib/libcrypto/x509v3/v3_pcia.c (v 1.6); src/lib/libcrypto/x509v3/v3_sxnet.c (v 1.19);
 * src/lib/libcrypto/x509v3/v3_crld.c (v 1.21); src/lib/libcrypto/x509v3/v3_ocsp.c (v 1.15); src/lib/libcrypto/ocsp/ocsp_asn.c (v 1.9) */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2000-2004 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 1999-2008 The OpenSSL Project.  All rights reserved.
 * Copyright (c) 2003 The OpenSSL Project.  All rights reserved.
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
/* Contributed to the OpenSSL Project 2004
 * by Richard Levitte (richard@levitte.org)
 */
/* Copyright (c) 2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslAsn1Type) == 16 && offsetof(LibreSslAsn1Type, value) == 8);
static_assert(sizeof(LibreSslX509Algorithm) == 16 && offsetof(LibreSslX509Algorithm, parameter) == 8);
static_assert(sizeof(LibreSslX509Validity) == 16 && offsetof(LibreSslX509Validity, notAfter) == 8);
static_assert(sizeof(LibreSslX509PublicKey) == 24 && offsetof(LibreSslX509PublicKey, key) == 16);
static_assert(sizeof(LibreSslX509Extension) == 24 && offsetof(LibreSslX509Extension, critical) == 8 && offsetof(LibreSslX509Extension, value) == 16);
static_assert(sizeof(LibreSslAsn1Encoding) == 24 && offsetof(LibreSslAsn1Encoding, length) == 8 && offsetof(LibreSslAsn1Encoding, modified) == 16);
static_assert(sizeof(LibreSslX509CertificateInfo) == 104 && offsetof(LibreSslX509CertificateInfo, extensions) == 72 && offsetof(LibreSslX509CertificateInfo, encoding) == 80);
static_assert(sizeof(LibreSslX509) == 176 && offsetof(LibreSslX509, references) == 28 && offsetof(LibreSslX509, name) == 32);
static_assert(offsetof(LibreSslX509, exData) == 40 && offsetof(LibreSslX509, pathLength) == 48 && offsetof(LibreSslX509, extensionFlags) == 64);
static_assert(sizeof(LibreSslGeneralName) == 16 && offsetof(LibreSslGeneralName, value) == 8 && sizeof(LibreSslOtherName) == 16 && sizeof(LibreSslEdiPartyName) == 16);
static_assert(sizeof(LibreSslBasicConstraints) == 16 && offsetof(LibreSslBasicConstraints, pathLength) == 8 && sizeof(LibreSslAuthorityKeyId) == 24);
static_assert(sizeof(LibreSslAccessDescription) == 16 && sizeof(LibreSslPrivateKeyUsagePeriod) == 16 && sizeof(LibreSslPolicyConstraints) == 16);
static_assert(sizeof(LibreSslPolicyMapping) == 16 && sizeof(LibreSslGeneralSubtree) == 24 && sizeof(LibreSslNameConstraints) == 16);
static_assert(sizeof(LibreSslProxyPolicy) == 16 && sizeof(LibreSslProxyCertInfo) == 16 && sizeof(LibreSslSxnetId) == 16 && sizeof(LibreSslSxnet) == 16);
static_assert(sizeof(LibreSslDistPointName) == 24 && offsetof(LibreSslDistPointName, name) == 8 && offsetof(LibreSslDistPointName, dpName) == 16);
static_assert(sizeof(LibreSslDistPoint) == 32 && offsetof(LibreSslDistPoint, dpReasons) == 24);
static_assert(sizeof(LibreSslIssuingDistPoint) == 32 && offsetof(LibreSslIssuingDistPoint, onlyUser) == 8 && offsetof(LibreSslIssuingDistPoint, onlyCa) == 12);
static_assert(offsetof(LibreSslIssuingDistPoint, onlySomeReasons) == 16 && offsetof(LibreSslIssuingDistPoint, indirectCrl) == 24 && offsetof(LibreSslIssuingDistPoint, onlyAttributes) == 28);
static_assert(sizeof(LibreSslOcspCrlId) == 24 && sizeof(LibreSslOcspServiceLocator) == 16);
static_assert(offsetof(LibreSslX509, subjectKeyId) == 96 && offsetof(LibreSslX509, nameConstraints) == 136 && offsetof(LibreSslX509, sha1Hash) == 144 && offsetof(LibreSslX509, auxiliary) == 168);

namespace {

constexpr int BufferLibrary = 7;
constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int OcspLibrary = 39;
constexpr int MallocFailure = 65;
constexpr int AuxError = 100;
constexpr int BadObjectHeader = 102;
constexpr int BooleanIsWrongLength = 106;
constexpr int ExplicitLengthMismatch = 119;
constexpr int ExplicitTagNotConstructed = 120;
constexpr int FieldMissing = 121;
constexpr int HeaderTooLong = 123;
constexpr int IllegalNull = 125;
constexpr int IllegalOptionalAny = 126;
constexpr int IllegalTaggedAny = 127;
constexpr int MissingEoc = 137;
constexpr int MstringNotUniversal = 139;
constexpr int MstringWrongTag = 140;
constexpr int NoMatchingChoiceType = 143;
constexpr int NullIsWrongLength = 144;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int StringTooShort = 152;
constexpr int TooLong = 155;
constexpr int TypeNotConstructed = 156;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int IllegalOptionsOnItemTemplate = 170;
constexpr int NestedAsn1String = 197;
constexpr int BmpStringIsWrongLength = 214;
constexpr int UniversalStringIsWrongLength = 215;
constexpr int InvalidObjectEncoding = 216;

constexpr int Undefined = -1;
constexpr int Other = -3;
constexpr int Any = -4;
constexpr int Boolean = 1;
constexpr int Integer = 2;
constexpr int BitString = 3;
constexpr int OctetString = 4;
constexpr int Null = 5;
constexpr int Object = 6;
constexpr int Enumerated = 10;
constexpr int Utf8String = 12;
constexpr int Sequence = 16;
constexpr int Set = 17;
constexpr int Ia5String = 22;
constexpr int GeneralizedTime = 24;
constexpr int UniversalString = 28;
constexpr int BmpString = 30;
constexpr int Negative = 0x100;
constexpr int NegativeInteger = Integer | Negative;
constexpr int Universal = 0;
constexpr int Constructed = 0x20;
constexpr int HighTag = 0x1f;
constexpr int ClassMask = 0xc0;

constexpr std::uint32_t Optional = 0x1;
constexpr std::uint32_t SetOf = 0x2;
constexpr std::uint32_t SequenceOf = 0x4;
constexpr std::uint32_t StackMask = SetOf | SequenceOf;
constexpr std::uint32_t ImplicitTag = 0x8;
constexpr std::uint32_t ExplicitTag = 0x10;
constexpr std::uint32_t ContextClass = 0x80;

constexpr std::int64_t BitsLeft = 0x08;
constexpr std::int64_t IndefiniteString = 0x10;
constexpr std::int64_t MultiStringFlag = 0x40;
constexpr int DynamicObject = 0x01;
constexpr int DynamicStrings = 0x04;
constexpr int DynamicData = 0x08;

constexpr long UtcTimeBit = 0x4000;
constexpr long GeneralizedTimeBit = 0x8000;
constexpr long PrintableMask = 0x0001 | 0x0002 | 0x0004 | 0x0010 | 0x0400 | 0x0100 | 0x0800 | 0x2000 | 0x10000 | 0x1000;
constexpr long DirectoryStringMask = 0x0002 | 0x0004 | 0x0800 | 0x0100 | 0x2000;
constexpr long CanonicalMask = 0x2000 | 0x0800 | 0x0100 | 0x0002 | 0x0004 | 0x0010 | 0x0040;
constexpr long TagBits[31] = {
    0, 0, 0, 0x0400, 0x0200, 0, 0, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x2000, 0x1000, 0x1000, 0x1000,
    0x10000, 0, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x4000, 0x8000, 0x0020, 0x0040, 0x0080, 0x0100, 0x1000, 0x0800};

constexpr int MaximumStringNesting = 5;
constexpr int MinimumNodes = 4;
constexpr std::size_t GrowthLimit = 0x5ffffffc;

enum Operation { NewPre, NewPost, FreePre, FreePost, DecodePre, DecodePost };

using Value = void*;

struct TagCache {
    bool valid;
    int ret;
    std::int64_t length;
    int tag;
    int tagClass;
    int headerLength;
};

struct Buffer {
    std::uint64_t length;
    char* data;
    std::uint64_t max;
};

static_assert(sizeof(Buffer) == 24 && offsetof(Buffer, data) == 8 && offsetof(Buffer, max) == 16);

struct Item;

struct Template {
    std::uint32_t flags;
    int tag;
    std::size_t offset;
    const Item* item;
};

enum class Kind { Primitive, MultiString, Sequence, Choice, Extern };

struct Item {
    Kind kind;
    long type;
    const Template* templates = nullptr;
    int count = 0;
    std::int64_t size = 0;
    int (*callback)(Operation, Value*) = nullptr;
    int referenceOffset = -1;
    int encodingOffset = -1;
    bool (*create)(Value*) = nullptr;
    void (*release)(Value*) = nullptr;
    int (*decode)(Value*, const unsigned char**, std::int64_t, int, int, char, TagCache*) = nullptr;
    int selectorOffset = 0;
};

extern const Item IntegerItem;
extern const Item BitStringItem;
extern const Item AnyItem;
extern const Item NameEntryItem;
extern const Item NameInternalItem;
extern const Item NameItem;

void Asn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

long TagBit(int tag) {
    if (tag < 0 || tag > 30) return 0;
    return TagBits[tag];
}

std::int32_t* Selector(Value* value, const Item& item) {
    return reinterpret_cast<std::int32_t*>(static_cast<char*>(*value) + item.selectorOffset);
}

Value* Field(Value* parent, const Template& field) {
    return reinterpret_cast<Value*>(static_cast<char*>(*parent) + field.offset);
}

char* GrowArray(char* data, std::size_t oldSize, std::size_t newSize) {
    if (!data) return static_cast<char*>(calloc_nid_postfix(newSize, 1));
    auto* grown = static_cast<char*>(malloc_nid_postfix(newSize));
    if (!grown) return nullptr;
    std::memcpy(grown, data, oldSize);
    std::memset(grown + oldSize, 0, newSize - oldSize);
    std::memset(data, 0, oldSize);
    free_nid_postfix(data);
    return grown;
}

std::size_t Grow(Buffer* buffer, std::size_t length, bool clean) {
    if (buffer->length >= length) {
        if (clean && buffer->data) std::memset(buffer->data + length, 0, buffer->length - length);
        buffer->length = length;
        return length;
    }
    if (buffer->max >= length) {
        buffer->length = length;
        return length;
    }
    if (length > GrowthLimit) {
        LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure);
        return 0;
    }
    const std::size_t size = (length + 3) / 3 * 4;
    char* data = GrowArray(buffer->data, buffer->max, size);
    if (!data) {
        LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure);
        return 0;
    }
    buffer->data = data;
    buffer->max = size;
    buffer->length = length;
    return length;
}

void BufferFree(Buffer* buffer) {
    if (!buffer) return;
    if (buffer->data) {
        std::memset(buffer->data, 0, buffer->max);
        free_nid_postfix(buffer->data);
    }
    free_nid_postfix(buffer);
}

LibreSslStack* StackNew() {
    auto* stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(realloc_nid_postfix(nullptr, MinimumNodes * sizeof(char*)));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < MinimumNodes; ++index) stack->data[index] = nullptr;
    stack->comp = nullptr;
    stack->numAlloc = MinimumNodes;
    stack->num = 0;
    stack->sorted = 0;
    return stack;
}

bool StackPush(LibreSslStack* stack, Value element) {
    if (stack->numAlloc <= stack->num + 1) {
        auto* grown = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(element);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

Value StackPop(LibreSslStack* stack) {
    if (!stack || stack->num <= 0) return nullptr;
    return stack->data[--stack->num];
}

void StackFree(LibreSslStack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

Asn1String* StringNew(int type) {
    auto* string = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!string) {
        Asn1Error(MallocFailure);
        return nullptr;
    }
    *string = Asn1String{0, type, nullptr, 0};
    return string;
}

void StringFree(Asn1String* string) {
    if (!string) return;
    if (string->data && !(string->flags & IndefiniteString)) {
        std::memset(string->data, 0, static_cast<std::size_t>(string->length));
        free_nid_postfix(string->data);
    }
    free_nid_postfix(string);
}

bool StringSet(Asn1String* string, const void* data, int length) {
    if (length < 0) {
        if (!data) return false;
        length = static_cast<int>(std::strlen(static_cast<const char*>(data)));
    }
    if (string->length < length || !string->data) {
        auto* grown = static_cast<unsigned char*>(realloc_nid_postfix(string->data, static_cast<std::size_t>(length) + 1));
        if (!grown) {
            Asn1Error(MallocFailure);
            return false;
        }
        string->data = grown;
    }
    string->length = length;
    if (data) std::memmove(string->data, data, static_cast<std::size_t>(length));
    string->data[string->length] = 0;
    return true;
}

LibreSslAsn1Object* ObjectNew() {
    auto* object = static_cast<LibreSslAsn1Object*>(malloc_nid_postfix(sizeof(LibreSslAsn1Object)));
    if (!object) {
        Asn1Error(MallocFailure);
        return nullptr;
    }
    *object = LibreSslAsn1Object{nullptr, nullptr, 0, 0, nullptr, DynamicObject};
    return object;
}

void ObjectFree(LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & DynamicStrings) {
        free_nid_postfix(const_cast<char*>(object->shortName));
        free_nid_postfix(const_cast<char*>(object->longName));
        object->shortName = object->longName = nullptr;
    }
    if (object->flags & DynamicData) {
        if (object->data) std::memset(const_cast<unsigned char*>(object->data), 0, static_cast<std::size_t>(object->length));
        free_nid_postfix(const_cast<unsigned char*>(object->data));
        object->data = nullptr;
        object->length = 0;
    }
    if (object->flags & DynamicObject) free_nid_postfix(object);
}

bool ItemNew(Value* value, const Item& item);
void ItemFree(Value* value, const Item& item);
int ItemDecode(Value* value, const unsigned char** in, std::int64_t length, const Item& item, int tag, int tagClass, char optional, TagCache* cache);

Value NewValue(const Item& item) {
    Value value = nullptr;
    if (ItemNew(&value, item)) return value;
    return nullptr;
}

void FreeValue(Value value, const Item& item) {
    ItemFree(&value, item);
}

LibreSslAsn1Object* ObjectFromContent(LibreSslAsn1Object** target, const unsigned char** in, std::int64_t length) {
    const unsigned char* p;
    if (length <= 0 || length > INT_MAX || !in || !(p = *in) || (p[length - 1] & 0x80)) {
        Asn1Error(InvalidObjectEncoding);
        return nullptr;
    }
    const int size = static_cast<int>(length);
    for (int index = 0; index < size; ++index, ++p) {
        if (*p == 0x80 && (!index || !(p[-1] & 0x80))) {
            Asn1Error(InvalidObjectEncoding);
            return nullptr;
        }
    }
    LibreSslAsn1Object* object;
    if (!target || !*target || !((*target)->flags & DynamicObject)) {
        object = ObjectNew();
        if (!object) return nullptr;
    } else {
        object = *target;
    }
    p = *in;
    auto* data = const_cast<unsigned char*>(object->data);
    if (data) std::memset(data, 0, static_cast<std::size_t>(object->length));
    free_nid_postfix(data);
    object->data = nullptr;
    object->length = 0;
    data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(size)));
    if (!data) {
        Asn1Error(MallocFailure);
        if (!target || object != *target) ObjectFree(object);
        return nullptr;
    }
    std::memcpy(data, p, static_cast<std::size_t>(size));
    object->data = data;
    object->length = size;
    object->shortName = nullptr;
    object->longName = nullptr;
    object->flags |= DynamicData;
    p += size;
    if (target) *target = object;
    *in = p;
    return object;
}

Asn1String* IntegerFromContent(Asn1String** target, const unsigned char** in, std::int64_t length) {
    Asn1String* integer;
    if (!target || !*target) {
        integer = static_cast<Asn1String*>(NewValue(IntegerItem));
        if (!integer) return nullptr;
    } else {
        integer = *target;
    }
    const unsigned char* p = *in;
    const unsigned char* end = p + length;
    auto* bytes = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length) + 1));
    if (!bytes) {
        Asn1Error(MallocFailure);
        if (!target || *target != integer) FreeValue(integer, IntegerItem);
        return nullptr;
    }
    if (!length) {
        integer->type = Integer;
    } else if (*p & 0x80) {
        integer->type = NegativeInteger;
        if (*p == 0xff && length != 1) {
            ++p;
            --length;
        }
        std::int64_t remaining = length;
        std::int64_t at = remaining - 1;
        while (!p[at] && remaining) {
            bytes[at] = 0;
            --remaining;
            --at;
        }
        if (!remaining) {
            bytes[0] = 1;
            bytes[length] = 0;
            ++length;
        } else {
            bytes[at] = static_cast<unsigned char>((p[at] ^ 0xff) + 1);
            --remaining;
            --at;
            for (; remaining > 0; --remaining, --at) bytes[at] = p[at] ^ 0xff;
        }
    } else {
        integer->type = Integer;
        if (*p == 0 && length != 1) {
            ++p;
            --length;
        }
        std::memcpy(bytes, p, static_cast<std::size_t>(length));
    }
    free_nid_postfix(integer->data);
    integer->data = bytes;
    integer->length = static_cast<int>(length);
    if (target) *target = integer;
    *in = end;
    return integer;
}

Asn1String* BitStringFromContent(Asn1String** target, const unsigned char** in, std::int64_t length) {
    if (length < 1) {
        Asn1Error(StringTooShort);
        return nullptr;
    }
    Asn1String* bits;
    if (!target || !*target) {
        bits = static_cast<Asn1String*>(NewValue(BitStringItem));
        if (!bits) return nullptr;
    } else {
        bits = *target;
    }
    const unsigned char* p = *in;
    const int unused = *p++;
    bits->flags &= ~(BitsLeft | 0x07);
    bits->flags |= BitsLeft | (unused & 0x07);
    unsigned char* bytes = nullptr;
    if (length-- > 1) {
        bytes = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length)));
        if (!bytes) {
            Asn1Error(MallocFailure);
            if (!target || *target != bits) FreeValue(bits, BitStringItem);
            return nullptr;
        }
        std::memcpy(bytes, p, static_cast<std::size_t>(length));
        bytes[length - 1] &= static_cast<unsigned char>(0xffu << (unused & 31));
        p += length;
    }
    bits->length = static_cast<int>(length);
    free_nid_postfix(bits->data);
    bits->data = bytes;
    bits->type = BitString;
    if (target) *target = bits;
    *in = p;
    return bits;
}

void PrimitiveFree(Value* value, const Item* item) {
    int type;
    if (!item) {
        auto* any = static_cast<LibreSslAsn1Type*>(*value);
        type = any->type;
        value = &any->value;
        if (!*value) return;
    } else if (item->kind == Kind::MultiString) {
        type = Undefined;
        if (!*value) return;
    } else {
        type = static_cast<int>(item->type);
        if (type != Boolean && !*value) return;
    }
    switch (type) {
    case Object:
        ObjectFree(static_cast<LibreSslAsn1Object*>(*value));
        break;
    case Boolean:
        *reinterpret_cast<std::int32_t*>(value) = item ? static_cast<std::int32_t>(item->size) : -1;
        return;
    case Null:
        break;
    case Any:
        PrimitiveFree(value, nullptr);
        free_nid_postfix(*value);
        break;
    default:
        StringFree(static_cast<Asn1String*>(*value));
        break;
    }
    *value = nullptr;
}

void TypeSet(LibreSslAsn1Type* any, int type) {
    if (any->value) {
        Value holder = any;
        PrimitiveFree(&holder, nullptr);
    }
    any->type = type;
    if (type == Boolean) *reinterpret_cast<std::int32_t*>(&any->value) = 0;
    else any->value = nullptr;
}

bool PrimitiveNew(Value* value, const Item* item) {
    const int type = !item || item->kind == Kind::MultiString ? Undefined : static_cast<int>(item->type);
    switch (type) {
    case Object:
        *value = const_cast<LibreSslAsn1Object*>(&LibreSslObjects_nid_no_patch[0]);
        return true;
    case Boolean:
        *reinterpret_cast<std::int32_t*>(value) = static_cast<std::int32_t>(item->size);
        return true;
    case Null:
        *value = reinterpret_cast<Value>(1);
        return true;
    case Any: {
        auto* any = static_cast<LibreSslAsn1Type*>(malloc_nid_postfix(sizeof(LibreSslAsn1Type)));
        if (any) {
            any->value = nullptr;
            any->type = Undefined;
        }
        *value = any;
        break;
    }
    default: {
        auto* string = StringNew(type);
        if (item && item->kind == Kind::MultiString && string) string->flags |= MultiStringFlag;
        *value = string;
        break;
    }
    }
    return *value != nullptr;
}

void PrimitiveClear(Value* value, const Item* item) {
    const int type = !item || item->kind == Kind::MultiString ? Undefined : static_cast<int>(item->type);
    if (type == Boolean) *reinterpret_cast<std::int32_t*>(value) = static_cast<std::int32_t>(item->size);
    else *value = nullptr;
}

void ItemClear(Value* value, const Item& item);

void TemplateClear(Value* value, const Template& field) {
    if (field.flags & StackMask) *value = nullptr;
    else ItemClear(value, *field.item);
}

void ItemClear(Value* value, const Item& item) {
    switch (item.kind) {
    case Kind::Primitive:
        if (item.templates) TemplateClear(value, *item.templates);
        else PrimitiveClear(value, &item);
        break;
    case Kind::MultiString:
        PrimitiveClear(value, &item);
        break;
    case Kind::Sequence:
    case Kind::Choice:
    case Kind::Extern:
        *value = nullptr;
        break;
    }
}

bool TemplateNew(Value* value, const Template& field) {
    if (field.flags & Optional) {
        TemplateClear(value, field);
        return true;
    }
    if (field.flags & StackMask) {
        auto* stack = StackNew();
        if (!stack) {
            Asn1Error(MallocFailure);
            return false;
        }
        *value = stack;
        return true;
    }
    return ItemNew(value, *field.item);
}

int Lock(Value* value, int operation, const Item& item) {
    if (item.kind != Kind::Sequence || item.referenceOffset < 0) return 0;
    auto* references = reinterpret_cast<std::int32_t*>(static_cast<char*>(*value) + item.referenceOffset);
    if (operation == 0) {
        *references = 1;
        return 1;
    }
    return std::atomic_ref<std::int32_t>(*references).fetch_add(operation) + operation;
}

LibreSslAsn1Encoding* EncodingOf(Value* value, const Item& item) {
    if (item.encodingOffset < 0) return nullptr;
    return reinterpret_cast<LibreSslAsn1Encoding*>(static_cast<char*>(*value) + item.encodingOffset);
}

void EncodingInitialize(Value* value, const Item& item) {
    if (auto* encoding = EncodingOf(value, item)) {
        encoding->data = nullptr;
        encoding->length = 0;
        encoding->modified = 1;
    }
}

void EncodingFree(Value* value, const Item& item) {
    if (auto* encoding = EncodingOf(value, item)) {
        free_nid_postfix(encoding->data);
        encoding->data = nullptr;
        encoding->length = 0;
        encoding->modified = 1;
    }
}

bool EncodingSave(Value* value, const unsigned char* in, int length, const Item& item) {
    auto* encoding = EncodingOf(value, item);
    if (!encoding) return true;
    free_nid_postfix(encoding->data);
    encoding->data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length)));
    if (!encoding->data) return false;
    std::memcpy(encoding->data, in, static_cast<std::size_t>(length));
    encoding->length = length;
    encoding->modified = 0;
    return true;
}

bool ItemNew(Value* value, const Item& item) {
    *value = nullptr;
    auto memoryError = [] {
        Asn1Error(MallocFailure);
        return false;
    };
    switch (item.kind) {
    case Kind::Extern:
        if (!item.create(value)) return memoryError();
        break;
    case Kind::Primitive:
        if (item.templates) {
            if (!TemplateNew(value, *item.templates)) return memoryError();
        } else if (!PrimitiveNew(value, &item)) {
            return memoryError();
        }
        break;
    case Kind::MultiString:
        if (!PrimitiveNew(value, &item)) return memoryError();
        break;
    case Kind::Sequence:
        if (item.callback) {
            const int result = item.callback(NewPre, value);
            if (!result) {
                Asn1Error(AuxError);
                ItemFree(value, item);
                return false;
            }
            if (result == 2) return true;
        }
        *value = calloc_nid_postfix(1, static_cast<std::size_t>(item.size));
        if (!*value) return memoryError();
        Lock(value, 0, item);
        EncodingInitialize(value, item);
        for (int index = 0; index < item.count; ++index) {
            if (!TemplateNew(Field(value, item.templates[index]), item.templates[index])) return memoryError();
        }
        if (item.callback && !item.callback(NewPost, value)) {
            Asn1Error(AuxError);
            ItemFree(value, item);
            return false;
        }
        break;
    case Kind::Choice:
        if (item.callback) {
            const int result = item.callback(NewPre, value);
            if (!result) {
                Asn1Error(AuxError);
                ItemFree(value, item);
                return false;
            }
            if (result == 2) return true;
        }
        *value = calloc_nid_postfix(1, static_cast<std::size_t>(item.size));
        if (!*value) return memoryError();
        *Selector(value, item) = -1;
        if (item.callback && !item.callback(NewPost, value)) {
            Asn1Error(AuxError);
            ItemFree(value, item);
            return false;
        }
        break;
    }
    return true;
}

void TemplateFree(Value* value, const Template& field) {
    if (field.flags & StackMask) {
        auto* stack = static_cast<LibreSslStack*>(*value);
        for (int index = 0; stack && index < stack->num; ++index) {
            Value element = stack->data[index];
            ItemFree(&element, *field.item);
        }
        StackFree(stack);
        *value = nullptr;
    } else {
        ItemFree(value, *field.item);
    }
}

void ItemFree(Value* value, const Item& item) {
    if (!value || !*value) return;
    switch (item.kind) {
    case Kind::Primitive:
        if (item.templates) TemplateFree(value, *item.templates);
        else PrimitiveFree(value, &item);
        break;
    case Kind::MultiString:
        PrimitiveFree(value, &item);
        break;
    case Kind::Extern:
        item.release(value);
        break;
    case Kind::Sequence:
        if (Lock(value, -1, item) > 0) return;
        if (item.callback && item.callback(FreePre, value) == 2) return;
        EncodingFree(value, item);
        for (int index = item.count - 1; index >= 0; --index) TemplateFree(Field(value, item.templates[index]), item.templates[index]);
        if (item.callback) item.callback(FreePost, value);
        free_nid_postfix(*value);
        *value = nullptr;
        break;
    case Kind::Choice: {
        if (item.callback && item.callback(FreePre, value) == 2) return;
        const int selector = *Selector(value, item);
        if (selector >= 0 && selector < item.count) TemplateFree(Field(value, item.templates[selector]), item.templates[selector]);
        if (item.callback) item.callback(FreePost, value);
        free_nid_postfix(*value);
        *value = nullptr;
        break;
    }
    }
}

int GetLength(const unsigned char** in, int* indefinite, std::int64_t* length, int max) {
    const unsigned char* p = *in;
    std::uint64_t result = 0;
    if (max-- < 1) return 0;
    if (*p == 0x80) {
        *indefinite = 1;
        ++p;
    } else {
        *indefinite = 0;
        unsigned int count = *p & 0x7f;
        if (*p++ & 0x80) {
            if (max < static_cast<int>(count)) return 0;
            while (count && *p == 0) {
                ++p;
                --count;
            }
            if (count > sizeof(std::int64_t)) return 0;
            while (count-- > 0) {
                result <<= 8;
                result |= *p++;
            }
        } else {
            result = count;
        }
    }
    if (result > static_cast<std::uint64_t>(INT64_MAX)) return 0;
    *in = p;
    *length = static_cast<std::int64_t>(result);
    return 1;
}

int GetObject(const unsigned char** in, std::int64_t* length, int* tag, int* tagClass, std::int64_t limit) {
    auto headerError = [] {
        Asn1Error(HeaderTooLong);
        return 0x80;
    };
    const unsigned char* p = *in;
    std::int64_t max = limit;
    if (!max) return headerError();
    int result = *p & Constructed;
    const int objectClass = *p & ClassMask;
    const int low = *p & HighTag;
    int number;
    if (low == HighTag) {
        ++p;
        if (--max == 0) return headerError();
        std::int64_t value = 0;
        while (*p & 0x80) {
            value <<= 7;
            value |= *p++ & 0x7f;
            if (--max == 0) return headerError();
            if (value > (INT_MAX >> 7)) return headerError();
        }
        value <<= 7;
        value |= *p++ & 0x7f;
        number = static_cast<int>(value);
        if (--max == 0) return headerError();
    } else {
        number = low;
        ++p;
        if (--max == 0) return headerError();
    }
    *tag = number;
    *tagClass = objectClass;
    int indefinite = 0;
    if (!GetLength(&p, &indefinite, length, static_cast<int>(max))) return headerError();
    if (indefinite && !(result & Constructed)) return headerError();
    if (*length > limit - (p - *in)) {
        Asn1Error(TooLong);
        result |= 0x80;
    }
    *in = p;
    return result | indefinite;
}

int CheckTagLength(std::int64_t* outLength, int* outTag, unsigned char* outClass, char* indefinite, char* constructed, const unsigned char** in,
    std::int64_t length, int expectedTag, int expectedClass, char optional, TagCache* cache) {
    const unsigned char* p = *in;
    const unsigned char* q = p;
    int result;
    int tag = 0;
    int tagClass = 0;
    std::int64_t contentLength = 0;
    if (cache && cache->valid) {
        result = cache->ret;
        contentLength = cache->length;
        tagClass = cache->tagClass;
        tag = cache->tag;
        p += cache->headerLength;
    } else {
        result = GetObject(&p, &contentLength, &tag, &tagClass, length);
        if (cache) {
            cache->ret = result;
            cache->length = contentLength;
            cache->tagClass = tagClass;
            cache->tag = tag;
            cache->headerLength = static_cast<int>(p - q);
            cache->valid = true;
            if (!(result & 0x81) && contentLength + cache->headerLength > length) {
                Asn1Error(TooLong);
                cache->valid = false;
                return 0;
            }
        }
    }
    if (result & 0x80) {
        Asn1Error(BadObjectHeader);
        if (cache) cache->valid = false;
        return 0;
    }
    if (expectedTag >= 0) {
        if (expectedTag != tag || expectedClass != tagClass) {
            if (optional) return -1;
            if (cache) cache->valid = false;
            Asn1Error(WrongTag);
            return 0;
        }
        if (cache) cache->valid = false;
    }
    if (result & 1) contentLength = length - (p - q);
    if (indefinite) *indefinite = static_cast<char>(result & 1);
    if (constructed) *constructed = static_cast<char>(result & Constructed);
    if (outLength) *outLength = contentLength;
    if (outClass) *outClass = static_cast<unsigned char>(tagClass);
    if (outTag) *outTag = tag;
    *in = p;
    return 1;
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
            Asn1Error(NestedAsn1Error);
            return false;
        }
        if (indefinite) ++expected;
        else p += contentLength;
        length -= p - q;
    }
    if (expected) {
        Asn1Error(MissingEoc);
        return false;
    }
    *in = p;
    return true;
}

bool CollectData(Buffer* buffer, const unsigned char** p, std::int64_t length) {
    if (buffer) {
        const int used = static_cast<int>(buffer->length);
        if (!Grow(buffer, static_cast<std::size_t>(used + length), true)) {
            Asn1Error(MallocFailure);
            return false;
        }
        std::memcpy(buffer->data + used, *p, static_cast<std::size_t>(length));
    }
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
                Asn1Error(UnexpectedEoc);
                return false;
            }
            indefinite = 0;
            break;
        }
        std::int64_t contentLength = 0;
        char constructed = 0;
        char innerIndefinite = 0;
        if (!CheckTagLength(&contentLength, nullptr, nullptr, &innerIndefinite, &constructed, &p, length, tag, tagClass, 0, nullptr)) {
            Asn1Error(NestedAsn1Error);
            return false;
        }
        if (constructed) {
            if (depth >= MaximumStringNesting) {
                Asn1Error(NestedAsn1String);
                return false;
            }
            if (!Collect(buffer, &p, contentLength, innerIndefinite, tag, tagClass, depth + 1)) return false;
        } else if (contentLength && !CollectData(buffer, &p, contentLength)) {
            return false;
        }
        length -= p - q;
    }
    if (indefinite) {
        Asn1Error(MissingEoc);
        return false;
    }
    *in = p;
    return true;
}

bool StringContent(Value* value, const unsigned char* content, int length, int type, char* ownsContent) {
    if (type == BmpString && (length & 1)) {
        Asn1Error(BmpStringIsWrongLength);
        return false;
    }
    if (type == UniversalString && (length & 3)) {
        Asn1Error(UniversalStringIsWrongLength);
        return false;
    }
    Asn1String* string;
    if (!*value) {
        string = StringNew(type);
        if (!string) {
            Asn1Error(MallocFailure);
            return false;
        }
        *value = string;
    } else {
        string = static_cast<Asn1String*>(*value);
        string->type = type;
    }
    if (*ownsContent) {
        free_nid_postfix(string->data);
        string->data = const_cast<unsigned char*>(content);
        string->length = length;
        *ownsContent = 0;
    } else if (!StringSet(string, content, length)) {
        Asn1Error(MallocFailure);
        StringFree(string);
        *value = nullptr;
        return false;
    }
    return true;
}

bool ContentToValue(Value* value, const unsigned char* content, int length, int type, char* ownsContent, const Item& item) {
    Value* outer = nullptr;
    LibreSslAsn1Type* any = nullptr;
    auto fail = [&] {
        FreeValue(any, AnyItem);
        if (outer) *outer = nullptr;
        return false;
    };
    if (item.type == Any) {
        if (!*value) {
            any = static_cast<LibreSslAsn1Type*>(NewValue(AnyItem));
            if (!any) return fail();
            *value = any;
        } else {
            any = static_cast<LibreSslAsn1Type*>(*value);
        }
        if (type != any->type) TypeSet(any, type);
        outer = value;
        value = &any->value;
    }
    switch (type) {
    case Object:
        if (!ObjectFromContent(reinterpret_cast<LibreSslAsn1Object**>(value), &content, length)) return fail();
        break;
    case Null:
        if (length) {
            Asn1Error(NullIsWrongLength);
            return fail();
        }
        *value = reinterpret_cast<Value>(1);
        break;
    case Boolean:
        if (length != 1) {
            Asn1Error(BooleanIsWrongLength);
            return fail();
        }
        *reinterpret_cast<std::int32_t*>(value) = *content;
        break;
    case BitString:
        if (!BitStringFromContent(reinterpret_cast<Asn1String**>(value), &content, length)) return fail();
        break;
    case Integer:
    case Enumerated: {
        auto** integer = reinterpret_cast<Asn1String**>(value);
        if (!IntegerFromContent(integer, &content, length)) return fail();
        (*integer)->type = type | ((*integer)->type & Negative);
        break;
    }
    default:
        if (!StringContent(value, content, length, type, ownsContent)) return fail();
        break;
    }
    if (any && type == Null) any->value = nullptr;
    return true;
}

int PrimitiveDecode(Value* value, const unsigned char** in, std::int64_t inLength, const Item& item, int tag, int tagClass, char optional, TagCache* cache) {
    if (!value) {
        Asn1Error(IllegalNull);
        return 0;
    }
    int type;
    if (item.kind == Kind::MultiString) {
        type = tag;
        tag = -1;
    } else {
        type = static_cast<int>(item.type);
    }
    if (type == Any) {
        if (tag >= 0) {
            Asn1Error(IllegalTaggedAny);
            return 0;
        }
        if (optional) {
            Asn1Error(IllegalOptionalAny);
            return 0;
        }
        const unsigned char* p = *in;
        unsigned char objectClass = 0;
        if (!CheckTagLength(nullptr, &type, &objectClass, nullptr, nullptr, &p, inLength, -1, 0, 0, cache)) {
            Asn1Error(NestedAsn1Error);
            return 0;
        }
        if (objectClass != Universal) type = Other;
    }
    if (tag == -1) {
        tag = type;
        tagClass = Universal;
    }
    const unsigned char* p = *in;
    std::int64_t contentLength = 0;
    char indefinite = 0;
    char constructed = 0;
    const int found = CheckTagLength(&contentLength, nullptr, nullptr, &indefinite, &constructed, &p, inLength, tag, tagClass, optional, cache);
    if (!found) {
        Asn1Error(NestedAsn1Error);
        return 0;
    }
    if (found == -1) return -1;
    Buffer buffer{};
    char ownsContent = 0;
    auto finish = [&](int result) {
        if (ownsContent && buffer.data) free_nid_postfix(buffer.data);
        return result;
    };
    const unsigned char* content;
    std::int64_t length;
    if (type == Sequence || type == Set || type == Other) {
        if (type == Other) {
            if (cache) cache->valid = false;
        } else if (!constructed) {
            Asn1Error(TypeNotConstructed);
            return 0;
        }
        content = *in;
        if (indefinite) {
            if (!FindEnd(&p, contentLength, indefinite)) return finish(0);
            length = p - content;
        } else {
            length = p - content + contentLength;
            p += contentLength;
        }
    } else if (constructed) {
        ownsContent = 1;
        if (!Collect(&buffer, &p, contentLength, indefinite, -1, Universal, 0)) return finish(0);
        length = static_cast<std::int64_t>(buffer.length);
        if (!Grow(&buffer, static_cast<std::size_t>(length) + 1, true)) {
            Asn1Error(MallocFailure);
            return finish(0);
        }
        buffer.data[length] = 0;
        content = reinterpret_cast<const unsigned char*>(buffer.data);
    } else {
        content = p;
        length = contentLength;
        p += contentLength;
    }
    if (!ContentToValue(value, content, static_cast<int>(length), type, &ownsContent, item)) return finish(0);
    *in = p;
    return finish(1);
}

int TemplateDecodeUntagged(Value* value, const unsigned char** in, std::int64_t length, const Template& field, char optional, TagCache* cache) {
    if (!value) return 0;
    const int tagClass = static_cast<int>(field.flags & ClassMask);
    const unsigned char* p = *in;
    auto fail = [&] {
        TemplateFree(value, field);
        return 0;
    };
    if (field.flags & StackMask) {
        int tag;
        int stackClass;
        if (field.flags & ImplicitTag) {
            tag = field.tag;
            stackClass = tagClass;
        } else {
            stackClass = Universal;
            tag = field.flags & SetOf ? Set : Sequence;
        }
        char indefinite = 0;
        const int found = CheckTagLength(&length, nullptr, nullptr, &indefinite, nullptr, &p, length, tag, stackClass, optional, cache);
        if (!found) {
            Asn1Error(NestedAsn1Error);
            return 0;
        }
        if (found == -1) return -1;
        if (!*value) {
            *value = StackNew();
        } else {
            auto* existing = static_cast<LibreSslStack*>(*value);
            while (existing->num > 0) {
                Value element = StackPop(existing);
                ItemFree(&element, *field.item);
            }
        }
        if (!*value) {
            Asn1Error(MallocFailure);
            return fail();
        }
        auto* stack = static_cast<LibreSslStack*>(*value);
        while (length > 0) {
            const unsigned char* q = p;
            if (CheckEoc(&p, length)) {
                if (!indefinite) {
                    Asn1Error(UnexpectedEoc);
                    return fail();
                }
                length -= p - q;
                indefinite = 0;
                break;
            }
            Value element = nullptr;
            if (!ItemDecode(&element, &p, length, *field.item, -1, 0, 0, cache)) {
                Asn1Error(NestedAsn1Error);
                return fail();
            }
            length -= p - q;
            if (!StackPush(stack, element)) {
                Asn1Error(MallocFailure);
                ItemFree(&element, *field.item);
                return fail();
            }
        }
        if (indefinite) {
            Asn1Error(MissingEoc);
            return fail();
        }
    } else {
        const bool implicit = field.flags & ImplicitTag;
        const int found = ItemDecode(value, &p, length, *field.item, implicit ? field.tag : -1, implicit ? tagClass : 0, optional, cache);
        if (!found) {
            Asn1Error(NestedAsn1Error);
            return fail();
        }
        if (found == -1) return -1;
    }
    *in = p;
    return 1;
}

int TemplateDecode(Value* value, const unsigned char** in, std::int64_t inLength, const Template& field, char optional, TagCache* cache) {
    if (!value) return 0;
    if (!(field.flags & ExplicitTag)) return TemplateDecodeUntagged(value, in, inLength, field, optional, cache);
    const unsigned char* p = *in;
    std::int64_t length = 0;
    char indefinite = 0;
    char constructed = 0;
    const int found = CheckTagLength(&length, nullptr, nullptr, &indefinite, &constructed, &p, inLength, field.tag, static_cast<int>(field.flags & ClassMask), optional, cache);
    const unsigned char* q = p;
    if (!found) {
        Asn1Error(NestedAsn1Error);
        return 0;
    }
    if (found == -1) return -1;
    if (!constructed) {
        Asn1Error(ExplicitTagNotConstructed);
        return 0;
    }
    if (!TemplateDecodeUntagged(value, &p, length, field, 0, cache)) {
        Asn1Error(NestedAsn1Error);
        return 0;
    }
    length -= p - q;
    if (indefinite) {
        if (!CheckEoc(&p, length)) {
            Asn1Error(MissingEoc);
            TemplateFree(value, field);
            return 0;
        }
    } else if (length) {
        Asn1Error(ExplicitLengthMismatch);
        TemplateFree(value, field);
        return 0;
    }
    *in = p;
    return 1;
}

int SequenceDecode(Value* value, const unsigned char** in, std::int64_t length, const Item& item, int tag, int tagClass, char optional, TagCache* cache) {
    auto fail = [&] {
        ItemFree(value, item);
        return 0;
    };
    auto auxiliaryFail = [&] {
        Asn1Error(AuxError);
        return fail();
    };
    const unsigned char* p = *in;
    if (tag == -1) {
        tag = Sequence;
        tagClass = Universal;
    }
    char indefinite = 0;
    char constructed = 0;
    int found = CheckTagLength(&length, nullptr, nullptr, &indefinite, &constructed, &p, length, tag, tagClass, optional, cache);
    if (!found) {
        Asn1Error(NestedAsn1Error);
        return fail();
    }
    if (found == -1) return -1;
    const char unchecked = indefinite;
    if (!constructed) {
        Asn1Error(SequenceNotConstructed);
        return fail();
    }
    if (!*value && !ItemNew(value, item)) {
        Asn1Error(NestedAsn1Error);
        return fail();
    }
    if (item.callback && !item.callback(DecodePre, value)) return auxiliaryFail();
    int index = 0;
    for (; index < item.count; ++index) {
        const Template& field = item.templates[index];
        Value* member = Field(value, field);
        if (!length) break;
        const unsigned char* q = p;
        if (CheckEoc(&p, length)) {
            if (!indefinite) {
                Asn1Error(UnexpectedEoc);
                return fail();
            }
            length -= p - q;
            indefinite = 0;
            break;
        }
        const char fieldOptional = index == item.count - 1 ? 0 : static_cast<char>(field.flags & Optional);
        found = TemplateDecode(member, &p, length, field, fieldOptional, cache);
        if (!found) return fail();
        if (found == -1) {
            TemplateFree(member, field);
            continue;
        }
        length -= p - q;
    }
    if (indefinite && !CheckEoc(&p, length)) {
        Asn1Error(MissingEoc);
        return fail();
    }
    if (!unchecked && length) {
        Asn1Error(SequenceLengthMismatch);
        return fail();
    }
    for (; index < item.count; ++index) {
        const Template& field = item.templates[index];
        if (!(field.flags & Optional)) {
            Asn1Error(FieldMissing);
            return fail();
        }
        TemplateFree(Field(value, field), field);
    }
    if (!EncodingSave(value, *in, static_cast<int>(p - *in), item)) {
        Asn1Error(MallocFailure);
        return auxiliaryFail();
    }
    *in = p;
    if (item.callback && !item.callback(DecodePost, value)) return auxiliaryFail();
    return 1;
}

int ChoiceDecode(Value* value, const unsigned char** in, std::int64_t length, const Item& item, char optional, TagCache* cache) {
    auto fail = [&] {
        ItemFree(value, item);
        return 0;
    };
    if (item.callback && !item.callback(DecodePre, value)) {
        Asn1Error(AuxError);
        return fail();
    }
    if (*value) {
        std::int32_t* selector = Selector(value, item);
        if (*selector >= 0 && *selector < item.count) {
            TemplateFree(Field(value, item.templates[*selector]), item.templates[*selector]);
            *selector = -1;
        }
    } else if (!ItemNew(value, item)) {
        Asn1Error(NestedAsn1Error);
        return fail();
    }
    const unsigned char* p = *in;
    int index = 0;
    for (; index < item.count; ++index) {
        const Template& field = item.templates[index];
        const int found = TemplateDecode(Field(value, field), &p, length, field, 1, cache);
        if (found == -1) continue;
        if (found > 0) break;
        Asn1Error(NestedAsn1Error);
        return fail();
    }
    if (index == item.count) {
        if (optional) {
            ItemFree(value, item);
            return -1;
        }
        Asn1Error(NoMatchingChoiceType);
        return fail();
    }
    *Selector(value, item) = index;
    *in = p;
    if (item.callback && !item.callback(DecodePost, value)) {
        Asn1Error(AuxError);
        return fail();
    }
    return 1;
}

int ItemDecode(Value* value, const unsigned char** in, std::int64_t length, const Item& item, int tag, int tagClass, char optional, TagCache* cache) {
    if (!value) return 0;
    switch (item.kind) {
    case Kind::Primitive:
        if (item.templates) {
            if (tag != -1 || optional) {
                Asn1Error(IllegalOptionsOnItemTemplate);
                ItemFree(value, item);
                return 0;
            }
            return TemplateDecode(value, in, length, *item.templates, optional, cache);
        }
        return PrimitiveDecode(value, in, length, item, tag, tagClass, optional, cache);
    case Kind::MultiString: {
        const unsigned char* p = *in;
        int found = 0;
        unsigned char objectClass = 0;
        if (!CheckTagLength(nullptr, &found, &objectClass, nullptr, nullptr, &p, length, -1, 0, 1, cache)) {
            Asn1Error(NestedAsn1Error);
            ItemFree(value, item);
            return 0;
        }
        if (objectClass != Universal) {
            if (optional) return -1;
            Asn1Error(MstringNotUniversal);
            ItemFree(value, item);
            return 0;
        }
        if (!(TagBit(found) & item.type)) {
            if (optional) return -1;
            Asn1Error(MstringWrongTag);
            ItemFree(value, item);
            return 0;
        }
        return PrimitiveDecode(value, in, length, item, found, 0, 0, cache);
    }
    case Kind::Extern:
        return item.decode(value, in, length, tag, tagClass, optional, cache);
    case Kind::Sequence:
        return SequenceDecode(value, in, length, item, tag, tagClass, optional, cache);
    case Kind::Choice:
        return ChoiceDecode(value, in, length, item, optional, cache);
    }
    return 0;
}

bool NameCreate(Value* value) {
    auto* name = static_cast<LibreSslX509Name*>(malloc_nid_postfix(sizeof(LibreSslX509Name)));
    if (!name) {
        Asn1Error(MallocFailure);
        return false;
    }
    name->entries = StackNew();
    if (!name->entries) {
        Asn1Error(MallocFailure);
        free_nid_postfix(name);
        return false;
    }
    name->bytes = calloc_nid_postfix(1, sizeof(Buffer));
    if (!name->bytes) {
        LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure);
        Asn1Error(MallocFailure);
        StackFree(name->entries);
        free_nid_postfix(name);
        return false;
    }
    name->canonEnc = nullptr;
    name->canonEnclen = 0;
    name->modified = 1;
    *value = name;
    return true;
}

void NameRelease(Value* value) {
    if (!value || !*value) return;
    auto* name = static_cast<LibreSslX509Name*>(*value);
    BufferFree(static_cast<Buffer*>(name->bytes));
    if (auto* entries = name->entries) {
        for (int index = 0; index < entries->num; ++index) {
            Value entry = entries->data[index];
            ItemFree(&entry, NameEntryItem);
        }
        StackFree(entries);
    }
    free_nid_postfix(name->canonEnc);
    free_nid_postfix(name);
    *value = nullptr;
}

bool IsSpace(unsigned char character) {
    return character == ' ' || (character >= '\t' && character <= '\r');
}

void PutLength(std::vector<unsigned char>& out, int length) {
    if (length <= 127) {
        out.push_back(static_cast<unsigned char>(length));
        return;
    }
    int count = 0;
    for (int rest = length; rest > 0; rest >>= 8) ++count;
    out.push_back(static_cast<unsigned char>(count | 0x80));
    for (int index = count - 1; index >= 0; --index) out.push_back(static_cast<unsigned char>(length >> (8 * index)));
}

void PutHeader(std::vector<unsigned char>& out, bool constructed, std::size_t length, int tag) {
    const int leading = constructed ? Constructed : 0;
    if (tag < 31) {
        out.push_back(static_cast<unsigned char>(leading | (tag & HighTag)));
    } else {
        out.push_back(static_cast<unsigned char>(leading | HighTag));
        int count = 0;
        for (int rest = tag; rest > 0; rest >>= 7) ++count;
        for (int index = count - 1; index >= 0; --index) out.push_back(static_cast<unsigned char>(((tag >> (7 * index)) & 0x7f) | (index ? 0x80 : 0)));
    }
    PutLength(out, static_cast<int>(length));
}

struct CanonicalValue {
    int type;
    std::int64_t flags;
    std::vector<unsigned char> data;
};

bool CanonicalString(const Asn1String& in, CanonicalValue& out) {
    if (!(TagBit(in.type) & CanonicalMask)) {
        out.type = in.type;
        std::size_t length = static_cast<std::size_t>(in.length);
        if (in.length < 0) {
            if (!in.data) return false;
            length = std::strlen(reinterpret_cast<const char*>(in.data));
        }
        if (length) out.data.assign(in.data, in.data + length);
        out.flags = in.flags;
        return true;
    }
    out.type = Utf8String;
    out.flags = 0;
    unsigned char* text = nullptr;
    const int length = ASN1_STRING_to_UTF8_nid_postfix(&text, const_cast<Asn1String*>(&in));
    if (length == -1) return false;
    int start = 0;
    int remaining = length;
    while (remaining > 0 && !(text[start] & 0x80) && IsSpace(text[start])) {
        ++start;
        --remaining;
    }
    int end = start + remaining - 1;
    while (remaining > 0 && !(text[end] & 0x80) && IsSpace(text[end])) {
        --end;
        --remaining;
    }
    int from = start;
    for (int index = 0; index < remaining;) {
        if (text[from] & 0x80) {
            out.data.push_back(text[from++]);
            ++index;
        } else if (IsSpace(text[from])) {
            out.data.push_back(' ');
            do {
                ++from;
                ++index;
            } while (!(text[from] & 0x80) && IsSpace(text[from]));
        } else {
            const unsigned char character = text[from++];
            out.data.push_back(character >= 'A' && character <= 'Z' ? static_cast<unsigned char>(character + 32) : character);
            ++index;
        }
    }
    free_nid_postfix(text);
    return true;
}

std::vector<unsigned char> CanonicalContent(const CanonicalValue& value) {
    std::vector<unsigned char> content;
    switch (value.type) {
    case BitString: {
        int length = static_cast<int>(value.data.size());
        int unused = 0;
        if (length > 0) {
            if (value.flags & BitsLeft) {
                unused = static_cast<int>(value.flags & 0x07);
            } else {
                while (length > 0 && !value.data[length - 1]) --length;
                const int last = length > 0 ? value.data[length - 1] : 0;
                while (unused < 8 && !(last & (1 << unused))) ++unused;
                if (unused == 8) unused = 0;
            }
        }
        content.push_back(static_cast<unsigned char>(unused));
        content.insert(content.end(), value.data.begin(), value.data.begin() + length);
        if (length > 0) content.back() &= static_cast<unsigned char>(0xff << unused);
        return content;
    }
    case Integer:
    case Enumerated:
        if (value.data.empty()) return {0};
        if (value.data[0] > 127) content.push_back(0);
        content.insert(content.end(), value.data.begin(), value.data.end());
        return content;
    default:
        return value.data;
    }
}

std::vector<unsigned char> CanonicalEntry(const LibreSslAsn1Object& object, const CanonicalValue& value) {
    std::vector<unsigned char> fields;
    PutHeader(fields, false, static_cast<std::size_t>(object.length), Object);
    fields.insert(fields.end(), object.data, object.data + object.length);
    const auto content = CanonicalContent(value);
    if (value.type != Sequence && value.type != Set && value.type != Other) PutHeader(fields, false, content.size(), value.type);
    fields.insert(fields.end(), content.begin(), content.end());
    std::vector<unsigned char> entry;
    PutHeader(entry, true, fields.size(), Sequence);
    entry.insert(entry.end(), fields.begin(), fields.end());
    return entry;
}

int Canonicalize(LibreSslX509Name* name) {
    free_nid_postfix(name->canonEnc);
    name->canonEnc = nullptr;
    if (name->entries->num == 0) {
        name->canonEnclen = 0;
        return 1;
    }
    std::vector<std::vector<std::vector<unsigned char>>> sets;
    int set = -1;
    for (int index = 0; index < name->entries->num; ++index) {
        const auto* entry = reinterpret_cast<const LibreSslX509NameEntry*>(name->entries->data[index]);
        if (entry->set != set) {
            sets.emplace_back();
            set = entry->set;
        }
        CanonicalValue value{};
        if (!CanonicalString(*static_cast<const Asn1String*>(entry->value), value)) return 0;
        if (sets.empty()) return 0;
        sets.back().push_back(CanonicalEntry(*static_cast<const LibreSslAsn1Object*>(entry->object), value));
    }
    std::vector<unsigned char> encoding;
    for (auto& entries : sets) {
        if (entries.size() > 1) {
            std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
                const int order = std::memcmp(left.data(), right.data(), std::min(left.size(), right.size()));
                return order != 0 ? order < 0 : left.size() < right.size();
            });
        }
        std::size_t length = 0;
        for (const auto& entry : entries) length += entry.size();
        PutHeader(encoding, true, length, Set);
        for (const auto& entry : entries) encoding.insert(encoding.end(), entry.begin(), entry.end());
    }
    auto* canonical = static_cast<unsigned char*>(malloc_nid_postfix(encoding.size()));
    if (!canonical) return 0;
    std::memcpy(canonical, encoding.data(), encoding.size());
    name->canonEnc = canonical;
    name->canonEnclen = static_cast<int>(encoding.size());
    return 1;
}

int NameDecode(Value* value, const unsigned char** in, std::int64_t length, int tag, int tagClass, char optional, TagCache* cache) {
    const unsigned char* p = *in;
    const unsigned char* q = p;
    Value internal = nullptr;
    int result = ItemDecode(&internal, &p, length, NameInternalItem, tag, tagClass, optional, cache);
    if (result <= 0) return result;
    if (*value) NameRelease(value);
    Value created = nullptr;
    auto fail = [&] {
        ItemFree(&internal, NameInternalItem);
        NameRelease(&created);
        Asn1Error(NestedAsn1Error);
        return 0;
    };
    if (!NameCreate(&created)) return fail();
    auto* name = static_cast<LibreSslX509Name*>(created);
    auto* bytes = static_cast<Buffer*>(name->bytes);
    if (!Grow(bytes, static_cast<std::size_t>(p - q), false)) return fail();
    std::memcpy(bytes->data, q, static_cast<std::size_t>(p - q));
    auto* sets = static_cast<LibreSslStack*>(internal);
    for (int index = 0; index < sets->num; ++index) {
        auto* entries = reinterpret_cast<LibreSslStack*>(sets->data[index]);
        for (int position = 0; position < entries->num; ++position) {
            auto* entry = reinterpret_cast<LibreSslX509NameEntry*>(entries->data[position]);
            entry->set = index;
            if (!StackPush(name->entries, entry)) return fail();
            entries->data[position] = nullptr;
        }
        StackFree(entries);
        sets->data[index] = nullptr;
    }
    StackFree(sets);
    internal = nullptr;
    result = Canonicalize(name);
    if (!result) return fail();
    name->modified = 0;
    *value = created;
    *in = p;
    return result;
}

int CertificateCallback(Operation operation, Value* value) {
    auto* certificate = static_cast<LibreSslX509*>(*value);
    switch (operation) {
    case NewPost:
        certificate->valid = 0;
        certificate->name = nullptr;
        certificate->extensionFlags = 0;
        certificate->pathLength = -1;
        certificate->subjectKeyId = nullptr;
        certificate->authorityKeyId = nullptr;
        certificate->auxiliary = nullptr;
        certificate->crlDistributionPoints = nullptr;
        certificate->exData.stack = nullptr;
        break;
    case FreePre:
        if (certificate->exData.stack || certificate->auxiliary || certificate->subjectKeyId || certificate->authorityKeyId || certificate->crlDistributionPoints ||
            certificate->policyCache || certificate->alternativeNames || certificate->nameConstraints)
            NotImplemented_nid_no_patch("X509_free of a certificate with ex_data, auxiliary data or cached extensions");
        break;
    case DecodePost:
        free_nid_postfix(certificate->name);
        certificate->name = X509_NAME_oneline_nid_postfix(certificate->certificateInfo->subject, nullptr, 0);
        break;
    case FreePost:
        free_nid_postfix(certificate->name);
        certificate->name = nullptr;
        break;
    default:
        break;
    }
    return 1;
}

int PublicKeyCallback(Operation operation, Value* value) {
    if (operation == FreePre && static_cast<LibreSslX509PublicKey*>(*value)->key) NotImplemented_nid_no_patch("X509_PUBKEY_free of a decoded EVP_PKEY");
    return 1;
}

const Item IntegerItem{.kind = Kind::Primitive, .type = Integer};
const Item ObjectItem{.kind = Kind::Primitive, .type = Object};
const Item BooleanItem{.kind = Kind::Primitive, .type = Boolean, .size = -1};
const Item OctetStringItem{.kind = Kind::Primitive, .type = OctetString};
const Item BitStringItem{.kind = Kind::Primitive, .type = BitString};
const Item AnyItem{.kind = Kind::Primitive, .type = Any};
const Item TimeItem{.kind = Kind::MultiString, .type = UtcTimeBit | GeneralizedTimeBit, .size = sizeof(Asn1String)};
const Item PrintableItem{.kind = Kind::MultiString, .type = PrintableMask, .size = sizeof(Asn1String)};

const Template NameEntryTemplates[] = {
    {0, 0, offsetof(LibreSslX509NameEntry, object), &ObjectItem},
    {0, 0, offsetof(LibreSslX509NameEntry, value), &PrintableItem},
};
const Item NameEntryItem{.kind = Kind::Sequence, .type = Sequence, .templates = NameEntryTemplates, .count = 2, .size = sizeof(LibreSslX509NameEntry)};
const Template NameEntriesTemplate{SetOf, 0, 0, &NameEntryItem};
const Item NameEntriesItem{.kind = Kind::Primitive, .type = Undefined, .templates = &NameEntriesTemplate};
const Template NameInternalTemplate{SequenceOf, 0, 0, &NameEntriesItem};
const Item NameInternalItem{.kind = Kind::Primitive, .type = Undefined, .templates = &NameInternalTemplate};
const Item NameItem{.kind = Kind::Extern, .type = Sequence, .create = NameCreate, .release = NameRelease, .decode = NameDecode};

const Template AlgorithmTemplates[] = {
    {0, 0, offsetof(LibreSslX509Algorithm, algorithm), &ObjectItem},
    {Optional, 0, offsetof(LibreSslX509Algorithm, parameter), &AnyItem},
};
const Item AlgorithmItem{.kind = Kind::Sequence, .type = Sequence, .templates = AlgorithmTemplates, .count = 2, .size = sizeof(LibreSslX509Algorithm)};

const Template ValidityTemplates[] = {
    {0, 0, offsetof(LibreSslX509Validity, notBefore), &TimeItem},
    {0, 0, offsetof(LibreSslX509Validity, notAfter), &TimeItem},
};
const Item ValidityItem{.kind = Kind::Sequence, .type = Sequence, .templates = ValidityTemplates, .count = 2, .size = sizeof(LibreSslX509Validity)};

const Template PublicKeyTemplates[] = {
    {0, 0, offsetof(LibreSslX509PublicKey, algorithm), &AlgorithmItem},
    {0, 0, offsetof(LibreSslX509PublicKey, publicKey), &BitStringItem},
};
const Item PublicKeyItem{.kind = Kind::Sequence, .type = Sequence, .templates = PublicKeyTemplates, .count = 2, .size = sizeof(LibreSslX509PublicKey), .callback = PublicKeyCallback};

const Template ExtensionTemplates[] = {
    {0, 0, offsetof(LibreSslX509Extension, object), &ObjectItem},
    {Optional, 0, offsetof(LibreSslX509Extension, critical), &BooleanItem},
    {0, 0, offsetof(LibreSslX509Extension, value), &OctetStringItem},
};
const Item ExtensionItem{.kind = Kind::Sequence, .type = Sequence, .templates = ExtensionTemplates, .count = 3, .size = sizeof(LibreSslX509Extension)};

const Template CertificateInfoTemplates[] = {
    {ExplicitTag | ContextClass | Optional, 0, offsetof(LibreSslX509CertificateInfo, version), &IntegerItem},
    {0, 0, offsetof(LibreSslX509CertificateInfo, serialNumber), &IntegerItem},
    {0, 0, offsetof(LibreSslX509CertificateInfo, signature), &AlgorithmItem},
    {0, 0, offsetof(LibreSslX509CertificateInfo, issuer), &NameItem},
    {0, 0, offsetof(LibreSslX509CertificateInfo, validity), &ValidityItem},
    {0, 0, offsetof(LibreSslX509CertificateInfo, subject), &NameItem},
    {0, 0, offsetof(LibreSslX509CertificateInfo, key), &PublicKeyItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslX509CertificateInfo, issuerUid), &BitStringItem},
    {ImplicitTag | ContextClass | Optional, 2, offsetof(LibreSslX509CertificateInfo, subjectUid), &BitStringItem},
    {ExplicitTag | ContextClass | SequenceOf | Optional, 3, offsetof(LibreSslX509CertificateInfo, extensions), &ExtensionItem},
};
const Item CertificateInfoItem{.kind = Kind::Sequence, .type = Sequence, .templates = CertificateInfoTemplates, .count = 10,
    .size = sizeof(LibreSslX509CertificateInfo), .encodingOffset = offsetof(LibreSslX509CertificateInfo, encoding)};

const Template CertificateTemplates[] = {
    {0, 0, offsetof(LibreSslX509, certificateInfo), &CertificateInfoItem},
    {0, 0, offsetof(LibreSslX509, signatureAlgorithm), &AlgorithmItem},
    {0, 0, offsetof(LibreSslX509, signature), &BitStringItem},
};
const Item CertificateItem{.kind = Kind::Sequence, .type = Sequence, .templates = CertificateTemplates, .count = 3, .size = sizeof(LibreSslX509),
    .callback = CertificateCallback, .referenceOffset = offsetof(LibreSslX509, references)};

const Item FalseBooleanItem{.kind = Kind::Primitive, .type = Boolean};
const Item EnumeratedItem{.kind = Kind::Primitive, .type = Enumerated};
const Item Ia5StringItem{.kind = Kind::Primitive, .type = Ia5String};
const Item GeneralizedTimeItem{.kind = Kind::Primitive, .type = GeneralizedTime};
const Item NullItem{.kind = Kind::Primitive, .type = Null};
const Item SequenceStringItem{.kind = Kind::Primitive, .type = Sequence};
const Item DirectoryStringItem{.kind = Kind::MultiString, .type = DirectoryStringMask, .size = sizeof(Asn1String)};

const Template OtherNameTemplates[] = {
    {0, 0, offsetof(LibreSslOtherName, typeId), &ObjectItem},
    {ExplicitTag | ContextClass, 0, offsetof(LibreSslOtherName, value), &AnyItem},
};
const Item OtherNameItem{.kind = Kind::Sequence, .type = Sequence, .templates = OtherNameTemplates, .count = 2, .size = sizeof(LibreSslOtherName)};

const Template EdiPartyNameTemplates[] = {
    {ImplicitTag | ContextClass | Optional, 0, offsetof(LibreSslEdiPartyName, nameAssigner), &DirectoryStringItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslEdiPartyName, partyName), &DirectoryStringItem},
};
const Item EdiPartyNameItem{.kind = Kind::Sequence, .type = Sequence, .templates = EdiPartyNameTemplates, .count = 2, .size = sizeof(LibreSslEdiPartyName)};

const Template GeneralNameTemplates[] = {
    {ImplicitTag | ContextClass, 0, offsetof(LibreSslGeneralName, value), &OtherNameItem},
    {ImplicitTag | ContextClass, 1, offsetof(LibreSslGeneralName, value), &Ia5StringItem},
    {ImplicitTag | ContextClass, 2, offsetof(LibreSslGeneralName, value), &Ia5StringItem},
    {ImplicitTag | ContextClass, 3, offsetof(LibreSslGeneralName, value), &SequenceStringItem},
    {ExplicitTag | ContextClass, 4, offsetof(LibreSslGeneralName, value), &NameItem},
    {ImplicitTag | ContextClass, 5, offsetof(LibreSslGeneralName, value), &EdiPartyNameItem},
    {ImplicitTag | ContextClass, 6, offsetof(LibreSslGeneralName, value), &Ia5StringItem},
    {ImplicitTag | ContextClass, 7, offsetof(LibreSslGeneralName, value), &OctetStringItem},
    {ImplicitTag | ContextClass, 8, offsetof(LibreSslGeneralName, value), &ObjectItem},
};
const Item GeneralNameItem{.kind = Kind::Choice, .type = Undefined, .templates = GeneralNameTemplates, .count = 9, .size = sizeof(LibreSslGeneralName),
    .selectorOffset = offsetof(LibreSslGeneralName, type)};
const Template GeneralNamesTemplate{SequenceOf, 0, 0, &GeneralNameItem};
const Item GeneralNamesItem{.kind = Kind::Primitive, .type = Undefined, .templates = &GeneralNamesTemplate};

const Template BasicConstraintsTemplates[] = {
    {Optional, 0, offsetof(LibreSslBasicConstraints, ca), &FalseBooleanItem},
    {Optional, 0, offsetof(LibreSslBasicConstraints, pathLength), &IntegerItem},
};
const Item BasicConstraintsItem{.kind = Kind::Sequence, .type = Sequence, .templates = BasicConstraintsTemplates, .count = 2, .size = sizeof(LibreSslBasicConstraints)};

const Template AuthorityKeyIdTemplates[] = {
    {ImplicitTag | ContextClass | Optional, 0, offsetof(LibreSslAuthorityKeyId, keyId), &OctetStringItem},
    {ImplicitTag | ContextClass | SequenceOf | Optional, 1, offsetof(LibreSslAuthorityKeyId, issuer), &GeneralNameItem},
    {ImplicitTag | ContextClass | Optional, 2, offsetof(LibreSslAuthorityKeyId, serial), &IntegerItem},
};
const Item AuthorityKeyIdItem{.kind = Kind::Sequence, .type = Sequence, .templates = AuthorityKeyIdTemplates, .count = 3, .size = sizeof(LibreSslAuthorityKeyId)};

const Template ExtendedKeyUsageTemplate{SequenceOf, 0, 0, &ObjectItem};
const Item ExtendedKeyUsageItem{.kind = Kind::Primitive, .type = Undefined, .templates = &ExtendedKeyUsageTemplate};

const Template AccessDescriptionTemplates[] = {
    {0, 0, offsetof(LibreSslAccessDescription, method), &ObjectItem},
    {0, 0, offsetof(LibreSslAccessDescription, location), &GeneralNameItem},
};
const Item AccessDescriptionItem{.kind = Kind::Sequence, .type = Sequence, .templates = AccessDescriptionTemplates, .count = 2, .size = sizeof(LibreSslAccessDescription)};
const Template AuthorityInfoAccessTemplate{SequenceOf, 0, 0, &AccessDescriptionItem};
const Item AuthorityInfoAccessItem{.kind = Kind::Primitive, .type = Undefined, .templates = &AuthorityInfoAccessTemplate};

const Template PrivateKeyUsagePeriodTemplates[] = {
    {ImplicitTag | ContextClass | Optional, 0, offsetof(LibreSslPrivateKeyUsagePeriod, notBefore), &GeneralizedTimeItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslPrivateKeyUsagePeriod, notAfter), &GeneralizedTimeItem},
};
const Item PrivateKeyUsagePeriodItem{.kind = Kind::Sequence, .type = Sequence, .templates = PrivateKeyUsagePeriodTemplates, .count = 2,
    .size = sizeof(LibreSslPrivateKeyUsagePeriod)};

const Template PolicyConstraintsTemplates[] = {
    {ImplicitTag | ContextClass | Optional, 0, offsetof(LibreSslPolicyConstraints, requireExplicitPolicy), &IntegerItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslPolicyConstraints, inhibitPolicyMapping), &IntegerItem},
};
const Item PolicyConstraintsItem{.kind = Kind::Sequence, .type = Sequence, .templates = PolicyConstraintsTemplates, .count = 2, .size = sizeof(LibreSslPolicyConstraints)};

const Template PolicyMappingTemplates[] = {
    {0, 0, offsetof(LibreSslPolicyMapping, issuerDomainPolicy), &ObjectItem},
    {0, 0, offsetof(LibreSslPolicyMapping, subjectDomainPolicy), &ObjectItem},
};
const Item PolicyMappingItem{.kind = Kind::Sequence, .type = Sequence, .templates = PolicyMappingTemplates, .count = 2, .size = sizeof(LibreSslPolicyMapping)};
const Template PolicyMappingsTemplate{SequenceOf, 0, 0, &PolicyMappingItem};
const Item PolicyMappingsItem{.kind = Kind::Primitive, .type = Undefined, .templates = &PolicyMappingsTemplate};

const Template GeneralSubtreeTemplates[] = {
    {0, 0, offsetof(LibreSslGeneralSubtree, base), &GeneralNameItem},
    {ImplicitTag | ContextClass | Optional, 0, offsetof(LibreSslGeneralSubtree, minimum), &IntegerItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslGeneralSubtree, maximum), &IntegerItem},
};
const Item GeneralSubtreeItem{.kind = Kind::Sequence, .type = Sequence, .templates = GeneralSubtreeTemplates, .count = 3, .size = sizeof(LibreSslGeneralSubtree)};

const Template NameConstraintsTemplates[] = {
    {ImplicitTag | ContextClass | SequenceOf | Optional, 0, offsetof(LibreSslNameConstraints, permittedSubtrees), &GeneralSubtreeItem},
    {ImplicitTag | ContextClass | SequenceOf | Optional, 1, offsetof(LibreSslNameConstraints, excludedSubtrees), &GeneralSubtreeItem},
};
const Item NameConstraintsItem{.kind = Kind::Sequence, .type = Sequence, .templates = NameConstraintsTemplates, .count = 2, .size = sizeof(LibreSslNameConstraints)};

const Template ProxyPolicyTemplates[] = {
    {0, 0, offsetof(LibreSslProxyPolicy, policyLanguage), &ObjectItem},
    {Optional, 0, offsetof(LibreSslProxyPolicy, policy), &OctetStringItem},
};
const Item ProxyPolicyItem{.kind = Kind::Sequence, .type = Sequence, .templates = ProxyPolicyTemplates, .count = 2, .size = sizeof(LibreSslProxyPolicy)};

const Template ProxyCertInfoTemplates[] = {
    {Optional, 0, offsetof(LibreSslProxyCertInfo, pathLengthConstraint), &IntegerItem},
    {0, 0, offsetof(LibreSslProxyCertInfo, proxyPolicy), &ProxyPolicyItem},
};
const Item ProxyCertInfoItem{.kind = Kind::Sequence, .type = Sequence, .templates = ProxyCertInfoTemplates, .count = 2, .size = sizeof(LibreSslProxyCertInfo)};

const Template SxnetIdTemplates[] = {
    {0, 0, offsetof(LibreSslSxnetId, zone), &IntegerItem},
    {0, 0, offsetof(LibreSslSxnetId, user), &OctetStringItem},
};
const Item SxnetIdItem{.kind = Kind::Sequence, .type = Sequence, .templates = SxnetIdTemplates, .count = 2, .size = sizeof(LibreSslSxnetId)};

const Template SxnetTemplates[] = {
    {0, 0, offsetof(LibreSslSxnet, version), &IntegerItem},
    {SequenceOf, 0, offsetof(LibreSslSxnet, ids), &SxnetIdItem},
};
const Item SxnetItem{.kind = Kind::Sequence, .type = Sequence, .templates = SxnetTemplates, .count = 2, .size = sizeof(LibreSslSxnet)};

int DistPointNameCallback(Operation operation, Value* value) {
    auto* name = static_cast<LibreSslDistPointName*>(*value);
    if (operation == NewPost) {
        name->dpName = nullptr;
    } else if (operation == FreePost && name->dpName) {
        Value dpName = name->dpName;
        ItemFree(&dpName, NameItem);
    }
    return 1;
}

const Template DistPointNameTemplates[] = {
    {ImplicitTag | ContextClass | SequenceOf, 0, offsetof(LibreSslDistPointName, name), &GeneralNameItem},
    {ImplicitTag | ContextClass | SetOf, 1, offsetof(LibreSslDistPointName, name), &NameEntryItem},
};
const Item DistPointNameItem{.kind = Kind::Choice, .type = Undefined, .templates = DistPointNameTemplates, .count = 2, .size = sizeof(LibreSslDistPointName),
    .callback = DistPointNameCallback, .selectorOffset = offsetof(LibreSslDistPointName, type)};

const Template DistPointTemplates[] = {
    {ExplicitTag | ContextClass | Optional, 0, offsetof(LibreSslDistPoint, distPoint), &DistPointNameItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslDistPoint, reasons), &BitStringItem},
    {ImplicitTag | ContextClass | SequenceOf | Optional, 2, offsetof(LibreSslDistPoint, crlIssuer), &GeneralNameItem},
};
const Item DistPointItem{.kind = Kind::Sequence, .type = Sequence, .templates = DistPointTemplates, .count = 3, .size = sizeof(LibreSslDistPoint)};
const Template CrlDistPointsTemplate{SequenceOf, 0, 0, &DistPointItem};
const Item CrlDistPointsItem{.kind = Kind::Primitive, .type = Undefined, .templates = &CrlDistPointsTemplate};

const Template IssuingDistPointTemplates[] = {
    {ExplicitTag | ContextClass | Optional, 0, offsetof(LibreSslIssuingDistPoint, distPoint), &DistPointNameItem},
    {ImplicitTag | ContextClass | Optional, 1, offsetof(LibreSslIssuingDistPoint, onlyUser), &FalseBooleanItem},
    {ImplicitTag | ContextClass | Optional, 2, offsetof(LibreSslIssuingDistPoint, onlyCa), &FalseBooleanItem},
    {ImplicitTag | ContextClass | Optional, 3, offsetof(LibreSslIssuingDistPoint, onlySomeReasons), &BitStringItem},
    {ImplicitTag | ContextClass | Optional, 4, offsetof(LibreSslIssuingDistPoint, indirectCrl), &FalseBooleanItem},
    {ImplicitTag | ContextClass | Optional, 5, offsetof(LibreSslIssuingDistPoint, onlyAttributes), &FalseBooleanItem},
};
const Item IssuingDistPointItem{.kind = Kind::Sequence, .type = Sequence, .templates = IssuingDistPointTemplates, .count = 6, .size = sizeof(LibreSslIssuingDistPoint)};

const Template OcspCrlIdTemplates[] = {
    {ExplicitTag | ContextClass | Optional, 0, offsetof(LibreSslOcspCrlId, crlUrl), &Ia5StringItem},
    {ExplicitTag | ContextClass | Optional, 1, offsetof(LibreSslOcspCrlId, crlNumber), &IntegerItem},
    {ExplicitTag | ContextClass | Optional, 2, offsetof(LibreSslOcspCrlId, crlTime), &GeneralizedTimeItem},
};
const Item OcspCrlIdItem{.kind = Kind::Sequence, .type = Sequence, .templates = OcspCrlIdTemplates, .count = 3, .size = sizeof(LibreSslOcspCrlId)};

const Template OcspServiceLocatorTemplates[] = {
    {0, 0, offsetof(LibreSslOcspServiceLocator, issuer), &NameItem},
    {SequenceOf | Optional, 0, offsetof(LibreSslOcspServiceLocator, locator), &AccessDescriptionItem},
};
const Item OcspServiceLocatorItem{.kind = Kind::Sequence, .type = Sequence, .templates = OcspServiceLocatorTemplates, .count = 2, .size = sizeof(LibreSslOcspServiceLocator)};

enum class Decoder { Template, Nonce, Policies };

struct ExtensionMethod {
    int nid;
    Decoder decoder;
    const Item* item;
};

const ExtensionMethod ExtensionMethods[] = {
    {71, Decoder::Template, &BitStringItem},
    {72, Decoder::Template, &Ia5StringItem},
    {73, Decoder::Template, &Ia5StringItem},
    {74, Decoder::Template, &Ia5StringItem},
    {75, Decoder::Template, &Ia5StringItem},
    {76, Decoder::Template, &Ia5StringItem},
    {77, Decoder::Template, &Ia5StringItem},
    {78, Decoder::Template, &Ia5StringItem},
    {82, Decoder::Template, &OctetStringItem},
    {83, Decoder::Template, &BitStringItem},
    {84, Decoder::Template, &PrivateKeyUsagePeriodItem},
    {85, Decoder::Template, &GeneralNamesItem},
    {86, Decoder::Template, &GeneralNamesItem},
    {87, Decoder::Template, &BasicConstraintsItem},
    {88, Decoder::Template, &IntegerItem},
    {89, Decoder::Policies, nullptr},
    {90, Decoder::Template, &AuthorityKeyIdItem},
    {103, Decoder::Template, &CrlDistPointsItem},
    {126, Decoder::Template, &ExtendedKeyUsageItem},
    {140, Decoder::Template, &IntegerItem},
    {141, Decoder::Template, &EnumeratedItem},
    {142, Decoder::Template, &GeneralizedTimeItem},
    {143, Decoder::Template, &SxnetItem},
    {177, Decoder::Template, &AuthorityInfoAccessItem},
    {366, Decoder::Nonce, nullptr},
    {367, Decoder::Template, &OcspCrlIdItem},
    {368, Decoder::Template, &ExtendedKeyUsageItem},
    {369, Decoder::Template, &NullItem},
    {370, Decoder::Template, &GeneralizedTimeItem},
    {371, Decoder::Template, &OcspServiceLocatorItem},
    {398, Decoder::Template, &AuthorityInfoAccessItem},
    {401, Decoder::Template, &PolicyConstraintsItem},
    {430, Decoder::Template, &ObjectItem},
    {663, Decoder::Template, &ProxyCertInfoItem},
    {666, Decoder::Template, &NameConstraintsItem},
    {747, Decoder::Template, &PolicyMappingsItem},
    {748, Decoder::Template, &IntegerItem},
    {770, Decoder::Template, &IssuingDistPointItem},
    {771, Decoder::Template, &GeneralNamesItem},
    {857, Decoder::Template, &CrlDistPointsItem},
};

const ExtensionMethod* FindExtensionMethod(int nid) {
    const auto* end = std::end(ExtensionMethods);
    const auto* method = std::lower_bound(std::begin(ExtensionMethods), end, nid, [](const ExtensionMethod& entry, int key) { return entry.nid < key; });
    return method != end && method->nid == nid ? method : nullptr;
}

Value NonceDecode(const Asn1String* content) {
    Asn1String* nonce = StringNew(OctetString);
    if (nonce && StringSet(nonce, content->data, content->length)) return nonce;
    if (nonce) StringFree(nonce);
    LibreSslPutError_nid_no_patch(OcspLibrary, AnyFunction, MallocFailure);
    return nullptr;
}

Value ExtensionDecode(const LibreSslX509Extension* extension) {
    const ExtensionMethod* method = FindExtensionMethod(LibreSslObjectNid_nid_no_patch(extension->object));
    if (!method) return nullptr;
    if (!extension->value) throw std::invalid_argument("X509_get_ext_d2i: an extension without a value");
    switch (method->decoder) {
    case Decoder::Policies:
        NotImplemented_nid_no_patch("X509_get_ext_d2i of certificatePolicies, whose qualifiers need ANY DEFINED BY decoding");
    case Decoder::Nonce:
        return NonceDecode(extension->value);
    case Decoder::Template:
        break;
    }
    const unsigned char* p = extension->value->data;
    Value value = nullptr;
    TagCache cache{};
    if (ItemDecode(&value, &p, extension->value->length, *method->item, -1, 0, 0, &cache) > 0) return value;
    return nullptr;
}

} // namespace

extern "C" {

LibreSslX509* APS5_VABI d2i_X509_nid_postfix(LibreSslX509** certificate, const unsigned char** input, std::int64_t length) {
    if (!input) throw std::invalid_argument(std::string(__func__) + ": null input pointer");
    if (!*input && length != 0) throw std::invalid_argument(std::string(__func__) + ": null input with a nonzero length");
    Value scratch = nullptr;
    Value* value = certificate ? reinterpret_cast<Value*>(certificate) : &scratch;
    TagCache cache{};
    if (ItemDecode(value, input, length, CertificateItem, -1, 0, 0, &cache) > 0) return static_cast<LibreSslX509*>(*value);
    return nullptr;
}

void APS5_VABI X509_free_nid_postfix(LibreSslX509* certificate) {
    FreeValue(certificate, CertificateItem);
}


void* APS5_VABI X509_get_ext_d2i_nid_postfix(const LibreSslX509* certificate, int nid, int* critical, int* index) {
    if (!certificate || !certificate->certificateInfo) throw std::invalid_argument(std::string(__func__) + ": null certificate");
    const LibreSslStack* extensions = certificate->certificateInfo->extensions;
    if (!extensions) {
        if (index) *index = -1;
        if (critical) *critical = -1;
        return nullptr;
    }
    const std::int64_t start = index ? static_cast<std::int64_t>(*index) + 1 : 0;
    const LibreSslX509Extension* found = nullptr;
    for (std::int64_t position = std::max<std::int64_t>(start, 0); position < extensions->num; ++position) {
        const auto* extension = reinterpret_cast<const LibreSslX509Extension*>(extensions->data[position]);
        if (!extension) throw std::invalid_argument(std::string(__func__) + ": a null extension");
        if (LibreSslObjectNid_nid_no_patch(extension->object) != nid) continue;
        if (index) {
            *index = static_cast<int>(position);
            found = extension;
            break;
        }
        if (found) {
            if (critical) *critical = -2;
            return nullptr;
        }
        found = extension;
    }
    if (!found) {
        if (index) *index = -1;
        if (critical) *critical = -1;
        return nullptr;
    }
    if (critical) *critical = found->critical > 0 ? 1 : 0;
    return ExtensionDecode(found);
}

}
