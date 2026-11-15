/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/asn1/x_name.c (v 1.33) L236-239;
 * src/lib/libcrypto/asn1/a_dup.c (v 1.14) L99-118; src/lib/libcrypto/asn1/x_name.c (v 1.33) L242-265 and L286-360 */
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
#include "X509NameDup.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int Asn1Library = 13;
constexpr int BufferLibrary = 7;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int MinimumNodes = 4;
constexpr std::int32_t DynamicObject = 0x01;
constexpr std::int32_t DynamicData = 0x08;

struct Buffer {
    std::uint64_t length;
    char* data;
    std::uint64_t max;
};

static_assert(sizeof(Buffer) == 24 && offsetof(Buffer, data) == 8 && offsetof(Buffer, max) == 16);
static_assert(sizeof(LibreSslX509Name) == 40 && offsetof(LibreSslX509Name, modified) == 8 && offsetof(LibreSslX509Name, bytes) == 16);
static_assert(offsetof(LibreSslX509Name, canonEnc) == 24 && offsetof(LibreSslX509Name, canonEnclen) == 32);
static_assert(sizeof(LibreSslX509NameEntry) == 24 && offsetof(LibreSslX509NameEntry, value) == 8 && offsetof(LibreSslX509NameEntry, set) == 16);
static_assert(offsetof(LibreSslX509NameEntry, size) == 20);
static_assert(sizeof(LibreSslStack) == 32 && offsetof(LibreSslStack, data) == 8 && offsetof(LibreSslStack, numAlloc) == 20);
static_assert(sizeof(LibreSslAsn1Object) == 40 && offsetof(LibreSslAsn1Object, nid) == 16 && offsetof(LibreSslAsn1Object, length) == 20);
static_assert(offsetof(LibreSslAsn1Object, data) == 24 && offsetof(LibreSslAsn1Object, flags) == 32);
static_assert(sizeof(Asn1String) == 24 && offsetof(Asn1String, type) == 4 && offsetof(Asn1String, data) == 8 && offsetof(Asn1String, flags) == 16);

void Asn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason, __FILE__, __LINE__);
}

LibreSslStack* StackNew() {
    auto* const stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * MinimumNodes));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < MinimumNodes; ++index)
        stack->data[index] = nullptr;
    stack->num = 0;
    stack->sorted = 0;
    stack->numAlloc = MinimumNodes;
    stack->comp = nullptr;
    return stack;
}

bool StackPush(LibreSslStack* stack, void* element) {
    if (stack->numAlloc <= stack->num + 1) {
        auto* const grown = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(element);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

void StackFree(LibreSslStack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

LibreSslAsn1Object* ObjectCopy(const LibreSslAsn1Object* source) {
    auto* const object = static_cast<LibreSslAsn1Object*>(calloc_nid_postfix(1, sizeof(LibreSslAsn1Object)));
    if (!object) {
        Asn1Error(MallocFailure);
        return nullptr;
    }
    object->nid = source->nid;
    object->length = source->length;
    object->flags = source->flags | DynamicObject | DynamicData;
    if (source->length > 0) {
        auto* const data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
        if (!data) {
            Asn1Error(MallocFailure);
            free_nid_postfix(object);
            return nullptr;
        }
        std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
        object->data = data;
    }
    return object;
}

Asn1String* StringCopy(const Asn1String* source) {
    auto* const string = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!string) {
        Asn1Error(MallocFailure);
        return nullptr;
    }
    string->length = source->length;
    string->type = source->type;
    string->flags = source->flags;
    if (source->length > 0) {
        auto* const data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
        if (!data) {
            Asn1Error(MallocFailure);
            free_nid_postfix(string);
            return nullptr;
        }
        std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
        string->data = data;
    }
    return string;
}

Buffer* BufferCopy(const Buffer* source) {
    auto* const buffer = static_cast<Buffer*>(calloc_nid_postfix(1, sizeof(Buffer)));
    if (!buffer) {
        LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure, __FILE__, __LINE__);
        return nullptr;
    }
    buffer->length = source->length;
    buffer->max = source->length;
    if (source->length > 0) {
        auto* const data = static_cast<char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
        if (!data) {
            LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure, __FILE__, __LINE__);
            free_nid_postfix(buffer);
            return nullptr;
        }
        std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
        buffer->data = data;
    }
    return buffer;
}

LibreSslX509NameEntry* EntryCopy(const LibreSslX509NameEntry* source) {
    auto* const entry = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    if (!entry) {
        Asn1Error(MallocFailure);
        return nullptr;
    }
    entry->set = source->set;
    entry->size = source->size;
    if (source->object) {
        entry->object = ObjectCopy(static_cast<const LibreSslAsn1Object*>(source->object));
        if (!entry->object) {
            free_nid_postfix(entry);
            return nullptr;
        }
    }
    if (source->value) {
        entry->value = StringCopy(static_cast<const Asn1String*>(source->value));
        if (!entry->value) {
            auto* const object = static_cast<LibreSslAsn1Object*>(entry->object);
            free_nid_postfix(const_cast<unsigned char*>(object->data));
            free_nid_postfix(object);
            free_nid_postfix(entry);
            return nullptr;
        }
    }
    return entry;
}

void EntryFree(LibreSslX509NameEntry* entry) {
    if (!entry) return;
    if (entry->object) {
        auto* const object = static_cast<LibreSslAsn1Object*>(entry->object);
        free_nid_postfix(const_cast<unsigned char*>(object->data));
        free_nid_postfix(object);
    }
    if (entry->value) {
        auto* const value = static_cast<Asn1String*>(entry->value);
        free_nid_postfix(value->data);
        free_nid_postfix(value);
    }
    free_nid_postfix(entry);
}

void NameRelease(LibreSslX509Name* name) {
    if (!name) return;
    if (name->entries) {
        for (int index = 0; index < name->entries->num; ++index)
            EntryFree(reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[index]));
        StackFree(name->entries);
    }
    if (name->bytes) {
        auto* const bytes = static_cast<Buffer*>(name->bytes);
        free_nid_postfix(bytes->data);
        free_nid_postfix(bytes);
    }
    free_nid_postfix(name->canonEnc);
    free_nid_postfix(name);
}

}  // namespace

namespace LibreSsl {

LibreSslX509Name* X509NameDup_nid_no_patch(const LibreSslX509Name* name) {
    if (!name) return nullptr;
    if (name->modified) NotImplemented_nid_no_patch("X509_NAME_dup of a modified name");
    if (!name->entries) throw std::invalid_argument("X509_NAME_dup: a name without entries");
    auto* const copy = static_cast<LibreSslX509Name*>(calloc_nid_postfix(1, sizeof(LibreSslX509Name)));
    if (!copy) {
        Asn1Error(MallocFailure);
        return nullptr;
    }
    const auto release = [&] {
        NameRelease(copy);
        return nullptr;
    };
    copy->entries = StackNew();
    if (!copy->entries) {
        Asn1Error(MallocFailure);
        free_nid_postfix(copy);
        return nullptr;
    }
    if (name->bytes) {
        copy->bytes = BufferCopy(static_cast<const Buffer*>(name->bytes));
        if (!copy->bytes) return release();
    }
    if (name->canonEnclen > 0) {
        copy->canonEnc = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(name->canonEnclen)));
        if (!copy->canonEnc) {
            Asn1Error(MallocFailure);
            return release();
        }
        std::memcpy(copy->canonEnc, name->canonEnc, static_cast<std::size_t>(name->canonEnclen));
    }
    copy->canonEnclen = name->canonEnclen;
    for (int index = 0; index < name->entries->num; ++index) {
        auto* source = reinterpret_cast<const LibreSslX509NameEntry*>(name->entries->data[index]);
        if (!source) throw std::invalid_argument("X509_NAME_dup: a null name entry");
        auto* entry = EntryCopy(source);
        if (!entry) return release();
        if (!StackPush(copy->entries, entry)) {
            EntryFree(entry);
            Asn1Error(MallocFailure);
            return release();
        }
    }
    return copy;
}

void X509NameFree_nid_no_patch(LibreSslX509Name* name) {
    NameRelease(name);
}

LibreSslStack* X509NameStackNew_nid_no_patch() {
    auto* const stack = StackNew();
    if (!stack) Asn1Error(MallocFailure);
    return stack;
}

int X509NameStackPush_nid_no_patch(LibreSslStack* stack, LibreSslX509Name* name) {
    return StackPush(stack, name) ? 1 : 0;
}

}  // namespace
