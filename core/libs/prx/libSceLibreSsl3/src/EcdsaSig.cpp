/* $OpenBSD: ecdsa.c,v 1.19 2024/04/15 15:49:37 tb Exp $ */
/* ====================================================================
 * Copyright (c) 2000-2002 The OpenSSL Project.  All rights reserved.
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr std::int64_t NoFreeFlag = 0x10;
constexpr std::int32_t ObjectType = 6;
constexpr std::int32_t BooleanType = 1;
constexpr std::int32_t NullType = 5;
constexpr std::int32_t AnyType = -4;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void ClearAndFree(void* data, std::size_t size) {
    if (!data) return;
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
    free_nid_postfix(data);
}

void FreeString(Asn1String* value) {
    if (!value) return;
    if (value->data && !(value->flags & NoFreeFlag)) {
        if (value->length < 0) NotImplemented_nid_no_patch(__func__);
        ClearAndFree(value->data, static_cast<std::size_t>(value->length));
    }
    value->data = nullptr;
    value->length = 0;
    value->flags &= ~NoFreeFlag;
    free_nid_postfix(value);
}

void FreeObject(LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & 0x04) {
        free_nid_postfix(const_cast<char*>(object->shortName));
        free_nid_postfix(const_cast<char*>(object->longName));
        object->shortName = nullptr;
        object->longName = nullptr;
    }
    if (object->flags & 0x08) {
        if (object->data && object->length < 0) NotImplemented_nid_no_patch(__func__);
        ClearAndFree(const_cast<unsigned char*>(object->data), static_cast<std::size_t>(object->length));
        object->data = nullptr;
        object->length = 0;
    }
    if (object->flags & 0x01) free_nid_postfix(object);
}

}

extern "C" {

void APS5_VABI ECDSA_SIG_get0_nid_postfix(const LibreSsl3EcdsaSig* sig, const void** r, const void** s) {
    if (!sig && (r || s))
        throw std::invalid_argument("ECDSA_SIG_get0 requires a signature when outputs are requested");
    if (r)
        *r = sig->r;
    if (s)
        *s = sig->s;
}

void APS5_VABI ASN1_TYPE_free_nid_postfix(Asn1Type* value) {
    if (!value) return;
    void* element = value->value.pointer;
    if (element) {
        if (value->type == ObjectType) {
            FreeObject(static_cast<LibreSslAsn1Object*>(element));
        } else if (value->type == AnyType) {
            ASN1_TYPE_free_nid_postfix(static_cast<Asn1Type*>(element));
        } else if (value->type != BooleanType && value->type != NullType) {
            FreeString(static_cast<Asn1String*>(element));
        }
    }
    free_nid_postfix(value);
}

LibreSsl3EcdsaSig* APS5_VABI ECDSA_SIG_new_nid_postfix(void) {
    auto* signature = static_cast<LibreSsl3EcdsaSig*>(malloc_nid_postfix(sizeof(LibreSsl3EcdsaSig)));
    if (!signature) {
        PutError(MallocFailure);
        return nullptr;
    }
    signature->r = nullptr;
    signature->s = nullptr;
    return signature;
}

int APS5_VABI ECDSA_SIG_set0_nid_postfix(LibreSsl3EcdsaSig* signature, Bignum* r, Bignum* s) {
    if (!signature) throw std::invalid_argument(std::string(__func__) + ": null signature");
    if (!r || !s) return 0;
    BN_free_nid_postfix(static_cast<Bignum*>(signature->r));
    BN_free_nid_postfix(static_cast<Bignum*>(signature->s));
    signature->r = r;
    signature->s = s;
    return 1;
}

void APS5_VABI ECDSA_SIG_free_nid_postfix(LibreSsl3EcdsaSig* signature) {
    if (!signature) return;
    BN_free_nid_postfix(static_cast<Bignum*>(signature->r));
    BN_free_nid_postfix(static_cast<Bignum*>(signature->s));
    free_nid_postfix(signature);
}

}
