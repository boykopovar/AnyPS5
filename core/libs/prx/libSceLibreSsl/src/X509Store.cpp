/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/x509/x509_lu.c (v 1.23);
 * src/lib/libcrypto/x509/x509_vpm.c (v 1.15); src/lib/libcrypto/x509/by_file.c (v 1.21); src/lib/libcrypto/x509/x509_cmp.c (v 1.27);
 * src/lib/libcrypto/stack/stack.c (v 1.19); src/lib/libcrypto/objects/obj_dat.c (v 1.39) */
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
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
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
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

static_assert(sizeof(LibreSslX509VerifyParamId) == 56);
static_assert(offsetof(LibreSslX509VerifyParamId, hostFlags) == 8);
static_assert(offsetof(LibreSslX509VerifyParamId, peerName) == 16);
static_assert(offsetof(LibreSslX509VerifyParamId, ip) == 40);
static_assert(sizeof(LibreSslX509VerifyParam) == 64);
static_assert(offsetof(LibreSslX509VerifyParam, flags) == 24);
static_assert(offsetof(LibreSslX509VerifyParam, purpose) == 32);
static_assert(offsetof(LibreSslX509VerifyParam, depth) == 40);
static_assert(offsetof(LibreSslX509VerifyParam, policies) == 48);
static_assert(offsetof(LibreSslX509VerifyParam, id) == 56);
static_assert(sizeof(LibreSslX509Store) == 136);
static_assert(offsetof(LibreSslX509Store, objects) == 8);
static_assert(offsetof(LibreSslX509Store, lookupMethods) == 16);
static_assert(offsetof(LibreSslX509Store, param) == 24);
static_assert(offsetof(LibreSslX509Store, callbacks) == 32);
static_assert(offsetof(LibreSslX509Store, exData) == 120);
static_assert(offsetof(LibreSslX509Store, references) == 128);
static_assert(sizeof(LibreSslX509LookupMethod) == 80);
static_assert(offsetof(LibreSslX509LookupMethod, shutdown) == 32);
static_assert(offsetof(LibreSslX509LookupMethod, ctrl) == 40);
static_assert(sizeof(LibreSslX509Lookup) == 32);
static_assert(offsetof(LibreSslX509Lookup, method) == 8);
static_assert(offsetof(LibreSslX509Lookup, methodData) == 16);
static_assert(offsetof(LibreSslX509Lookup, storeContext) == 24);

namespace {

constexpr int MinimumNodes = 4;
constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t PolicyMask = 0x80 | 0x100 | 0x200 | 0x400;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int CertificateObject = 1;
constexpr int CrlObject = 2;

using Compare = int (APS5_VABI*)(const void*, const void*);

struct Object {
    std::int32_t type;
    LibreSslX509* certificate;
};

static_assert(sizeof(Object) == 16);
static_assert(offsetof(Object, certificate) == 8);

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    Compare comp;
};

static_assert(sizeof(Stack) == 32);
static_assert(offsetof(Stack, data) == 8);
static_assert(offsetof(Stack, numAlloc) == 20);
static_assert(offsetof(Stack, comp) == 24);

const LibreSslX509Name* Subject(const Object* object) {
    const auto* certificate = object->certificate;
    if (!certificate || !certificate->certificateInfo || !certificate->certificateInfo->subject) NotImplemented_nid_no_patch("X509_STORE object comparison");
    return certificate->certificateInfo->subject;
}

int CompareNames(const LibreSslX509Name* left, const LibreSslX509Name* right) {
    if (left->modified || right->modified) NotImplemented_nid_no_patch("X509_NAME_cmp: re-encoding a modified name");
    const int difference = left->canonEnclen - right->canonEnclen;
    if (difference) return difference;
    if (left->canonEnclen <= 0) return 0;
    return std::memcmp(left->canonEnc, right->canonEnc, static_cast<std::size_t>(left->canonEnclen));
}

int APS5_VABI CompareObjects(const void* leftPointer, const void* rightPointer) {
    const auto* left = *static_cast<const Object* const*>(leftPointer);
    const auto* right = *static_cast<const Object* const*>(rightPointer);
    const int difference = left->type - right->type;
    if (difference) return difference;
    if (left->type == CrlObject) NotImplemented_nid_no_patch("X509_STORE CRL comparison");
    if (left->type != CertificateObject) return 0;
    return CompareNames(Subject(left), Subject(right));
}

int APS5_VABI LoadFile(LibreSslX509Lookup*, int, const char*, std::int64_t, char**) {
    NotImplemented_nid_no_patch("X509_LOOKUP_file: loading certificates and CRLs from a file");
    return 0;
}

LibreSslX509LookupMethod fileLookup{"Load file into cache", nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(LoadFile), nullptr, nullptr, nullptr, nullptr};

Stack* NewStack(Compare compare) {
    auto* stack = static_cast<Stack*>(malloc_nid_postfix(sizeof(Stack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * MinimumNodes));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    std::fill_n(stack->data, MinimumNodes, nullptr);
    stack->comp = compare;
    stack->numAlloc = MinimumNodes;
    stack->num = 0;
    stack->sorted = 0;
    return stack;
}

void FreeStack(Stack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

char** Elements(const Stack* stack, const char* function) {
    if (!stack->data) NotImplemented_nid_no_patch(function);
    return stack->data;
}

bool Push(Stack* stack, void* element, const char* function) {
    if (!stack) return false;
    if (stack->numAlloc <= stack->num + 1) {
        if (stack->numAlloc <= 0 || stack->num < 0) NotImplemented_nid_no_patch(function);
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

void Sort(Stack* stack, const char* function) {
    if (stack->sorted) return;
    if (stack->num > 0) {
        char** const elements = Elements(stack, function);
        const Compare compare = stack->comp;
        std::sort(elements, elements + stack->num, [compare](char* const& left, char* const& right) { return compare(&left, &right) < 0; });
    }
    stack->sorted = 1;
}

int Find(Stack* stack, void* element, const char* function) {
    if (!stack) return -1;
    if (!stack->comp) {
        for (int index = 0; index < stack->num; ++index)
            if (Elements(stack, function)[index] == element) return index;
        return -1;
    }
    Sort(stack, function);
    if (!element || stack->num == 0) return -1;
    char** const elements = Elements(stack, function);
    int low = 0;
    int high = stack->num;
    int index = 0;
    int result = 0;
    while (low < high) {
        index = (low + high) / 2;
        result = stack->comp(&element, &elements[index]);
        if (result < 0) high = index;
        else if (result > 0) low = index + 1;
        else break;
    }
    if (result != 0) return -1;
    while (index > 0 && stack->comp(&element, &elements[index - 1]) == 0) --index;
    return index;
}

void FreeObject(Object* object) {
    if (object->type == CertificateObject) X509_free_nid_postfix(object->certificate);
    else if (object->type == CrlObject) NotImplemented_nid_no_patch("X509_STORE_free: stored CRLs");
    free_nid_postfix(object);
}

void FreeLookup(LibreSslX509Lookup* lookup, bool shutdown, const char* function) {
    if (!lookup) NotImplemented_nid_no_patch(function);
    const auto* method = lookup->method;
    if (shutdown && method && method->shutdown) NotImplemented_nid_no_patch("X509_LOOKUP shutdown callback");
    if (method && method->free) NotImplemented_nid_no_patch("X509_LOOKUP free callback");
    free_nid_postfix(lookup);
}

bool HoldsElements(const Stack* stack, const char* function) {
    if (!stack || stack->num <= 0) return false;
    char** const elements = Elements(stack, function);
    return std::any_of(elements, elements + stack->num, [](const char* element) { return element != nullptr; });
}

void FreeStrings(Stack* stack, const char* function) {
    if (!stack) return;
    if (stack->num > 0) {
        char** const elements = Elements(stack, function);
        std::for_each(elements, elements + stack->num, [](char* element) { free_nid_postfix(element); });
    }
    FreeStack(stack);
}

LibreSslX509VerifyParam* NewVerifyParam() {
    auto* param = static_cast<LibreSslX509VerifyParam*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParam)));
    if (!param) return nullptr;
    auto* id = static_cast<LibreSslX509VerifyParamId*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParamId)));
    if (!id) {
        free_nid_postfix(param);
        return nullptr;
    }
    param->id = id;
    param->depth = -1;
    return param;
}

void FreeVerifyParam(LibreSslX509VerifyParam* param, const char* function) {
    if (!param) return;
    auto* policies = static_cast<Stack*>(param->policies);
    if (HoldsElements(policies, function)) NotImplemented_nid_no_patch("X509_VERIFY_PARAM policy objects");
    FreeStack(policies);
    auto* id = param->id;
    if (!id) NotImplemented_nid_no_patch(function);
    FreeStrings(static_cast<Stack*>(id->hosts), function);
    free_nid_postfix(id->peerName);
    free_nid_postfix(id->email);
    free_nid_postfix(id->ip);
    free_nid_postfix(id);
    free_nid_postfix(param);
}

} // namespace

extern "C" {

LibreSslX509Store* APS5_VABI X509_STORE_new_nid_postfix(void) {
    auto* store = static_cast<LibreSslX509Store*>(malloc_nid_postfix(sizeof(LibreSslX509Store)));
    if (!store) return nullptr;
    auto* objects = NewStack(CompareObjects);
    auto* lookupMethods = NewStack(nullptr);
    store->objects = objects;
    store->cache = 1;
    store->lookupMethods = lookupMethods;
    store->param = NewVerifyParam();
    if (!store->param) {
        FreeStack(lookupMethods);
        FreeStack(objects);
        free_nid_postfix(store);
        return nullptr;
    }
    std::fill(std::begin(store->callbacks), std::end(store->callbacks), nullptr);
    store->exData = nullptr;
    store->references = 1;
    return store;
}

void APS5_VABI X509_STORE_free_nid_postfix(LibreSslX509Store* store) {
    if (!store) return;
    if (std::atomic_ref<std::int32_t>(store->references).fetch_sub(1) - 1 > 0) return;
    auto* lookupMethods = static_cast<Stack*>(store->lookupMethods);
    if (lookupMethods && lookupMethods->num > 0) {
        char** const lookups = Elements(lookupMethods, __func__);
        for (int index = 0; index < lookupMethods->num; ++index)
            FreeLookup(reinterpret_cast<LibreSslX509Lookup*>(lookups[index]), true, __func__);
    }
    FreeStack(lookupMethods);
    auto* objects = static_cast<Stack*>(store->objects);
    if (objects && objects->num > 0) {
        char** const elements = Elements(objects, __func__);
        for (int index = 0; index < objects->num; ++index)
            if (elements[index]) FreeObject(reinterpret_cast<Object*>(elements[index]));
    }
    FreeStack(objects);
    FreeStack(static_cast<Stack*>(store->exData));
    FreeVerifyParam(store->param, __func__);
    free_nid_postfix(store);
}

int APS5_VABI X509_STORE_set_flags_nid_postfix(LibreSslX509Store* store, std::uint64_t flags) {
    if (!store || !store->param) NotImplemented_nid_no_patch(__func__);
    store->param->flags |= flags;
    if (flags & PolicyMask) store->param->flags |= PolicyCheck;
    return 1;
}

int APS5_VABI X509_STORE_add_cert_nid_postfix(LibreSslX509Store* store, LibreSslX509* certificate) {
    if (!certificate) return 0;
    if (!store) NotImplemented_nid_no_patch(__func__);
    auto* object = static_cast<Object*>(malloc_nid_postfix(sizeof(Object)));
    if (!object) {
        LibreSslPutError_nid_no_patch(X509Library, AnyFunction, MallocFailure);
        return 0;
    }
    object->type = CertificateObject;
    object->certificate = certificate;
    std::atomic_ref<std::int32_t>(certificate->references).fetch_add(1);
    const auto release = [&] {
        std::atomic_ref<std::int32_t>(certificate->references).fetch_sub(1);
        free_nid_postfix(object);
    };
    auto* objects = static_cast<Stack*>(store->objects);
    bool pushed = false;
    try {
        if (Find(objects, object, __func__) != -1) NotImplemented_nid_no_patch("X509_STORE_add_cert: X509_cmp of certificates with the same subject");
        pushed = Push(objects, object, __func__);
    } catch (...) {
        release();
        throw;
    }
    if (pushed) return 1;
    LibreSslPutError_nid_no_patch(X509Library, AnyFunction, MallocFailure);
    release();
    return 0;
}

LibreSslX509LookupMethod* APS5_VABI X509_LOOKUP_file_nid_postfix(void) {
    return &fileLookup;
}

LibreSslX509Lookup* APS5_VABI X509_STORE_add_lookup_nid_postfix(LibreSslX509Store* store, LibreSslX509LookupMethod* method) {
    if (!store) NotImplemented_nid_no_patch(__func__);
    auto* lookupMethods = static_cast<Stack*>(store->lookupMethods);
    if (lookupMethods && lookupMethods->num > 0) {
        char** const lookups = Elements(lookupMethods, __func__);
        for (int index = 0; index < lookupMethods->num; ++index) {
            auto* lookup = reinterpret_cast<LibreSslX509Lookup*>(lookups[index]);
            if (!lookup) NotImplemented_nid_no_patch(__func__);
            if (lookup->method == method) return lookup;
        }
    }
    if (!method) NotImplemented_nid_no_patch(__func__);
    if (method->newItem) NotImplemented_nid_no_patch("X509_LOOKUP new_item callback");
    auto* lookup = static_cast<LibreSslX509Lookup*>(malloc_nid_postfix(sizeof(LibreSslX509Lookup)));
    if (!lookup) return nullptr;
    lookup->init = 0;
    lookup->skip = 0;
    lookup->method = method;
    lookup->methodData = nullptr;
    lookup->storeContext = store;
    if (Push(lookupMethods, lookup, __func__)) return lookup;
    FreeLookup(lookup, false, __func__);
    return nullptr;
}

int APS5_VABI X509_VERIFY_PARAM_set1_host_nid_postfix(LibreSslX509VerifyParam* param, const char* name, std::size_t length) {
    if (!param || !param->id) NotImplemented_nid_no_patch(__func__);
    auto* id = param->id;
    if (name && length == 0) length = std::strlen(name);
    if (name && std::memchr(name, '\0', length)) return 0;
    if (id->hosts) {
        FreeStrings(static_cast<Stack*>(id->hosts), __func__);
        id->hosts = nullptr;
    }
    if (!name || length == 0) return 1;
    char* copy = strndup_nid_postfix(name, length);
    if (!copy) return 0;
    if (!id->hosts && !(id->hosts = NewStack(nullptr))) {
        free_nid_postfix(copy);
        return 0;
    }
    auto* hosts = static_cast<Stack*>(id->hosts);
    if (!Push(hosts, copy, __func__)) {
        free_nid_postfix(copy);
        if (hosts->num == 0) {
            FreeStack(hosts);
            id->hosts = nullptr;
        }
        return 0;
    }
    return 1;
}

}
