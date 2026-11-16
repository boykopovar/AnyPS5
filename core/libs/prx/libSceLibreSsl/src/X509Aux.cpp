/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/asn1/x_x509.c (v 1.24) L308-335;
 * src/lib/libcrypto/asn1/x_x509a.c (v 1.14) L74-118; src/lib/libcrypto/asn1/tasn_dec.c (v 1.34) L295-449, L524-640, L641-770, L800-905 and L1073-1148;
 * src/lib/libcrypto/asn1/a_object.c (v 1.29) L281-350; src/lib/libcrypto/asn1/asn1_lib.c (v 1.38) L95-190 and L374-399 */
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
#include "X509Aux.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace LibreSsl {
namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;

constexpr int NestedAsn1Error = 58;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int HeaderTooLong = 123;
constexpr int MissingEoc = 137;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int TooLong = 155;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int NestedAsn1String = 197;
constexpr int InvalidObjectEncoding = 216;

constexpr int Universal = 0;
constexpr int ContextClass = 0x80;
constexpr int Constructed = 0x20;
constexpr int HighTag = 0x1f;
constexpr int ClassMask = 0xc0;
constexpr int OctetString = 4;
constexpr int ObjectId = 6;
constexpr int Utf8String = 12;
constexpr int Sequence = 16;
constexpr int RejectTag = 0;
constexpr int OtherTag = 1;
constexpr int FieldCount = 5;
constexpr int MaximumStringNesting = 5;
constexpr int MinimumNodes = 4;
constexpr std::size_t GrowthLimit = 0x5ffffffc;

constexpr int DynamicObject = 0x01;
constexpr int DynamicData = 0x08;

struct CertAux {
    LibreSslStack* trust;
    LibreSslStack* reject;
    Asn1String* alias;
    Asn1String* keyid;
    LibreSslStack* other;
};

static_assert(sizeof(CertAux) == 40);
static_assert(offsetof(CertAux, trust) == 0 && offsetof(CertAux, reject) == 8 && offsetof(CertAux, alias) == 16 &&
    offsetof(CertAux, keyid) == 24 && offsetof(CertAux, other) == 32);

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

void Queue(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason, __FILE__, __LINE__);
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
        Queue(HeaderTooLong);
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
        Queue(TooLong);
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

int CheckTagLength(std::int64_t* length, int* tag, int* objectClass, char* indefinite, char* constructed, const unsigned char** in,
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
                Queue(TooLong);
                cache->valid = false;
                return 0;
            }
        }
    }
    if (result & 0x80) {
        Queue(BadObjectHeader);
        if (cache) cache->valid = false;
        return 0;
    }
    if (expectedTag >= 0) {
        if (expectedTag != foundTag || expectedClass != foundClass) {
            if (optional) return -1;
            if (cache) cache->valid = false;
            Queue(WrongTag);
            return 0;
        }
        if (cache) cache->valid = false;
    }
    if (result & 1) contentLength = limit - (p - start);
    if (indefinite) *indefinite = static_cast<char>(result & 1);
    if (constructed) *constructed = static_cast<char>(result & Constructed);
    if (length) *length = contentLength;
    if (objectClass) *objectClass = foundClass;
    if (tag) *tag = foundTag;
    *in = p;
    return 1;
}

bool CollectData(Buffer* buffer, const unsigned char** p, std::int64_t length) {
    const std::uint64_t used = buffer->length;
    if (!Grow(buffer, used + static_cast<std::uint64_t>(length), true)) {
        Queue(MallocFailure);
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
                Queue(UnexpectedEoc);
                return false;
            }
            indefinite = 0;
            break;
        }
        std::int64_t contentLength = 0;
        char constructed = 0;
        char innerIndefinite = 0;
        if (!CheckTagLength(&contentLength, nullptr, nullptr, &innerIndefinite, &constructed, &p, length, tag, tagClass, 0, nullptr)) {
            Queue(NestedAsn1Error);
            return false;
        }
        if (constructed) {
            if (depth >= MaximumStringNesting) {
                Queue(NestedAsn1String);
                return false;
            }
            if (!Collect(buffer, &p, contentLength, innerIndefinite, tag, tagClass, depth + 1)) return false;
        } else if (contentLength && !CollectData(buffer, &p, contentLength)) {
            return false;
        }
        length -= p - q;
    }
    if (indefinite) {
        Queue(MissingEoc);
        return false;
    }
    *in = p;
    return true;
}

LibreSslStack* StackNew() {
    auto* stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * MinimumNodes));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < MinimumNodes; ++index) stack->data[index] = nullptr;
    stack->comp = nullptr;
    stack->num = 0;
    stack->numAlloc = MinimumNodes;
    stack->sorted = 0;
    return stack;
}

bool StackPush(LibreSslStack* stack, void* element) {
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

void ObjectFree(LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & DynamicData) {
        if (object->data) std::memset(const_cast<unsigned char*>(object->data), 0, static_cast<std::size_t>(object->length));
        free_nid_postfix(const_cast<unsigned char*>(object->data));
    }
    free_nid_postfix(object);
}

void ObjectStackFree(LibreSslStack* stack) {
    if (!stack) return;
    if (stack->data) {
        for (int index = 0; index < stack->num; ++index)
            ObjectFree(reinterpret_cast<LibreSslAsn1Object*>(stack->data[index]));
    }
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

void StringFree(Asn1String* string) {
    if (!string) return;
    if (string->data) {
        std::memset(string->data, 0, static_cast<std::size_t>(string->length));
        free_nid_postfix(string->data);
    }
    free_nid_postfix(string);
}

void AuxFree(CertAux* aux) {
    if (!aux) return;
    ObjectStackFree(aux->trust);
    ObjectStackFree(aux->reject);
    StringFree(aux->alias);
    StringFree(aux->keyid);
    free_nid_postfix(aux);
}

LibreSslAsn1Object* ObjectFromContent(const unsigned char** in, std::int64_t length) {
    const unsigned char* p;
    if (length <= 0 || length > std::numeric_limits<int>::max() || !in || !(p = *in) || (p[length - 1] & 0x80)) {
        Queue(InvalidObjectEncoding);
        return nullptr;
    }
    const int size = static_cast<int>(length);
    for (int index = 0; index < size; ++index, ++p) {
        if (*p == 0x80 && (!index || !(p[-1] & 0x80))) {
            Queue(InvalidObjectEncoding);
            return nullptr;
        }
    }
    auto* object = static_cast<LibreSslAsn1Object*>(malloc_nid_postfix(sizeof(LibreSslAsn1Object)));
    if (!object) {
        Queue(MallocFailure);
        return nullptr;
    }
    *object = LibreSslAsn1Object{nullptr, nullptr, 0, 0, nullptr, DynamicObject};
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(size)));
    if (!data) {
        free_nid_postfix(object);
        Queue(MallocFailure);
        return nullptr;
    }
    std::memcpy(data, *in, static_cast<std::size_t>(size));
    object->data = data;
    object->length = size;
    object->flags |= DynamicData;
    *in += size;
    return object;
}

Asn1String* StringNew(int type) {
    auto* string = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!string) return nullptr;
    string->length = 0;
    string->type = type;
    string->data = nullptr;
    string->flags = 0;
    return string;
}

bool StringSet(Asn1String* string, const unsigned char* data, std::int64_t length) {
    if (length > std::numeric_limits<int>::max()) NotImplemented_nid_no_patch("X509_CERT_AUX: a string longer than INT_MAX bytes");
    const int size = static_cast<int>(length);
    if (string->length < size || !string->data) {
        auto* grown = static_cast<unsigned char*>(realloc_nid_postfix(string->data, static_cast<std::size_t>(size) + 1));
        if (!grown) return false;
        string->data = grown;
    }
    string->length = size;
    std::memmove(string->data, data, static_cast<std::size_t>(size));
    string->data[size] = 0;
    return true;
}

const unsigned char* FieldContent(Buffer* collected, const unsigned char** in, std::int64_t contentLength, char indefinite,
    char constructed, std::int64_t& length) {
    if (constructed) {
        if (!Collect(collected, in, contentLength, indefinite, -1, Universal, 0)) return nullptr;
        const std::int64_t collectedLength = static_cast<std::int64_t>(collected->length);
        if (!Grow(collected, static_cast<std::size_t>(collectedLength) + 1, true)) {
            Queue(MallocFailure);
            return nullptr;
        }
        collected->data[collectedLength] = 0;
        collected->length = static_cast<std::size_t>(collectedLength);
        length = collectedLength;
        return reinterpret_cast<const unsigned char*>(collected->data);
    }
    length = contentLength;
    const unsigned char* content = *in;
    *in += contentLength;
    return content;
}

LibreSslAsn1Object* DecodeObject(const unsigned char** in, std::int64_t limit, HeaderCache* cache) {
    const unsigned char* p = *in;
    std::int64_t contentLength = 0;
    char indefinite = 0;
    char constructed = 0;
    if (CheckTagLength(&contentLength, nullptr, nullptr, &indefinite, &constructed, &p, limit, ObjectId, Universal, 0, cache) != 1) {
        Queue(NestedAsn1Error);
        return nullptr;
    }
    Buffer collected{};
    std::int64_t length = 0;
    const unsigned char* content = FieldContent(&collected, &p, contentLength, indefinite, constructed, length);
    auto* object = content ? ObjectFromContent(&content, length) : nullptr;
    if (collected.data) free_nid_postfix(collected.data);
    if (!object) return nullptr;
    *in = p;
    return object;
}

int DecodeObjectStack(LibreSslStack** target, const unsigned char** in, std::int64_t limit, int sequenceTag, int sequenceClass,
    char optional, HeaderCache* cache) {
    const unsigned char* p = *in;
    std::int64_t contentLength = 0;
    char indefinite = 0;
    const int checked = CheckTagLength(&contentLength, nullptr, nullptr, &indefinite, nullptr, &p, limit, sequenceTag, sequenceClass,
        optional, cache);
    if (checked < 0) return -1;
    if (checked == 0) {
        Queue(NestedAsn1Error);
        return 0;
    }
    auto* stack = StackNew();
    if (!stack) {
        Queue(MallocFailure);
        return 0;
    }
    bool indefiniteContent = indefinite != 0;
    std::int64_t remaining = contentLength;
    while (remaining > 0) {
        const unsigned char* q = p;
        if (CheckEoc(&p, remaining)) {
            if (!indefiniteContent) {
                Queue(UnexpectedEoc);
                ObjectStackFree(stack);
                return 0;
            }
            remaining -= p - q;
            indefiniteContent = false;
            break;
        }
        auto* object = DecodeObject(&p, remaining, cache);
        if (!object) {
            Queue(NestedAsn1Error);
            ObjectStackFree(stack);
            return 0;
        }
        remaining -= p - q;
        if (!StackPush(stack, object)) {
            ObjectFree(object);
            Queue(MallocFailure);
            ObjectStackFree(stack);
            return 0;
        }
    }
    if (indefiniteContent) {
        Queue(MissingEoc);
        ObjectStackFree(stack);
        return 0;
    }
    *target = stack;
    *in = p;
    return 1;
}

int DecodeStringField(Asn1String** target, const unsigned char** in, std::int64_t limit, int expectedTag, char optional,
    HeaderCache* cache) {
    const unsigned char* p = *in;
    std::int64_t contentLength = 0;
    char indefinite = 0;
    char constructed = 0;
    const int checked = CheckTagLength(&contentLength, nullptr, nullptr, &indefinite, &constructed, &p, limit, expectedTag, Universal,
        optional, cache);
    if (checked < 0) return -1;
    if (checked == 0) {
        Queue(NestedAsn1Error);
        return 0;
    }
    Buffer collected{};
    std::int64_t length = 0;
    const unsigned char* content = FieldContent(&collected, &p, contentLength, indefinite, constructed, length);
    if (!content) {
        if (collected.data) free_nid_postfix(collected.data);
        return 0;
    }
    auto* string = StringNew(expectedTag);
    if (!string) {
        Queue(MallocFailure);
        if (collected.data) free_nid_postfix(collected.data);
        return 0;
    }
    if (!StringSet(string, content, length)) {
        Queue(MallocFailure);
        StringFree(string);
        if (collected.data) free_nid_postfix(collected.data);
        return 0;
    }
    if (collected.data) free_nid_postfix(collected.data);
    *target = string;
    *in = p;
    return 1;
}

CertAux* DecodeAux(const unsigned char** in, std::int64_t limit) {
    HeaderCache cache{};
    const unsigned char* p = *in;
    std::int64_t length = limit;
    char indefinite = 0;
    char constructed = 0;
    if (CheckTagLength(&length, nullptr, nullptr, &indefinite, &constructed, &p, length, Sequence, Universal, 0, &cache) != 1) {
        Queue(NestedAsn1Error);
        return nullptr;
    }
    if (!constructed) {
        Queue(SequenceNotConstructed);
        return nullptr;
    }
    bool indefiniteContent = indefinite != 0;
    const bool sequenceWithoutLength = indefiniteContent;
    auto* aux = static_cast<CertAux*>(calloc_nid_postfix(1, sizeof(CertAux)));
    if (!aux) {
        Queue(NestedAsn1Error);
        return nullptr;
    }
    auto release = [&] {
        AuxFree(aux);
        return nullptr;
    };
    std::int64_t remaining = length;
    try {
        for (int field = 0; field < FieldCount; ++field) {
            if (remaining <= 0) break;
            const unsigned char* q = p;
            if (CheckEoc(&p, remaining)) {
                if (!indefiniteContent) {
                    Queue(UnexpectedEoc);
                    return release();
                }
                remaining -= p - q;
                indefiniteContent = false;
                break;
            }
            const char optional = field == FieldCount - 1 ? 0 : 1;
            int decoded = 0;
            if (field == 0) decoded = DecodeObjectStack(&aux->trust, &p, remaining, Sequence, Universal, optional, &cache);
            else if (field == 1) decoded = DecodeObjectStack(&aux->reject, &p, remaining, RejectTag, ContextClass, optional, &cache);
            else if (field == 2) decoded = DecodeStringField(&aux->alias, &p, remaining, Utf8String, optional, &cache);
            else if (field == 3) decoded = DecodeStringField(&aux->keyid, &p, remaining, OctetString, optional, &cache);
            else {
                std::int64_t otherLength = 0;
                char otherIndefinite = 0;
                if (CheckTagLength(&otherLength, nullptr, nullptr, &otherIndefinite, nullptr, &p, remaining, OtherTag, ContextClass, 0,
                        &cache) != 1) {
                    Queue(NestedAsn1Error);
                    return release();
                }
                NotImplemented_nid_no_patch("d2i_X509_AUX of a certificate with other auxiliary information");
            }
            if (decoded == 0) return release();
            if (decoded == 1) remaining -= p - q;
        }
        if (indefiniteContent && !CheckEoc(&p, remaining)) {
            Queue(MissingEoc);
            return release();
        }
        if (!sequenceWithoutLength && remaining) {
            Queue(SequenceLengthMismatch);
            return release();
        }
    } catch (...) {
        AuxFree(aux);
        throw;
    }
    *in = p;
    return aux;
}

}  // namespace

void X509AuxiliaryFree(void* auxiliary) {
    AuxFree(static_cast<CertAux*>(auxiliary));
}

LibreSslX509* X509AuxRead_nid_no_patch(LibreSslX509** certificate, const unsigned char** input, std::int64_t length) {
    const unsigned char* p = *input;
    LibreSslX509* certificateRead = d2i_X509_nid_postfix(nullptr, &p, length);
    if (!certificateRead) return nullptr;
    length -= static_cast<std::int64_t>(p - *input);
    *input = p;
    if (length > 0) {
        CertAux* aux = nullptr;
        try {
            aux = DecodeAux(&p, length);
        } catch (...) {
            X509_free_nid_postfix(certificateRead);
            throw;
        }
        if (!aux) {
            X509_free_nid_postfix(certificateRead);
            return nullptr;
        }
        certificateRead->auxiliary = aux;
    }
    if (certificate) {
        X509_free_nid_postfix(*certificate);
        *certificate = certificateRead;
    }
    *input = p;
    return certificateRead;
}

}
