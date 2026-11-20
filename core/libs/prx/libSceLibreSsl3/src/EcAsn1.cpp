/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a) src/lib/libcrypto/ec/ec_asn1.c (ec_asn1.c,v 1.53),
 * asn1/tasn_dec.c (tasn_dec.c,v 1.88), asn1/asn1_lib.c (asn1_lib.c,v 1.54) and asn1/a_object.c (a_object.c,v 1.55) */
/* $OpenBSD: ec_asn1.c,v 1.53 2024/04/17 23:24:18 tb Exp $ */
/*
 * Written by Nils Larsch for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 2000-2003 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: tasn_dec.c,v 1.88 2023/07/28 10:00:10 tb Exp $ */
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

/* $OpenBSD: asn1_lib.c,v 1.54 2022/05/05 19:18:56 jsing Exp $ */
/*
 * Copyright (c) 2021 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $OpenBSD: a_object.c,v 1.55 2024/07/08 14:52:31 beck Exp $ */
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr int Asn1Library = 13;
constexpr int EcLibrary = 16;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int MallocFailure = 65;
constexpr int PassedNullParameter = 67;
constexpr int BadObjectHeader = 102;
constexpr int D2iEcpkParametersFailure = 117;
constexpr int GroupNewByNameFailure = 119;
constexpr int PkParametersToGroupFailure = 127;
constexpr int UnknownGroup = 129;
constexpr int NoMatchingChoiceType = 143;
constexpr int NullIsWrongLength = 144;
constexpr int SequenceNotConstructed = 149;
constexpr int TooLong = 155;
constexpr int InvalidObjectEncoding = 216;
constexpr int TypeNotPrimitive = 231;
constexpr std::uint32_t NullTag = 5;
constexpr std::uint32_t ObjectTag = 6;
constexpr std::uint32_t SequenceTag = 16;
constexpr std::int32_t NamedCurveFlag = 1;

constexpr std::pair<int, int> LibreSslCurves[] = {{409, 415}, {704, 716}, {739, 742}, {745, 745}, {922, 936}};

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(EcLibrary, AnyFunction, reason);
}

void PutAsn1Error(int reason, int nestedErrors) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
    for (int index = 0; index < nestedErrors; ++index) LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, NestedAsn1Error);
}

struct Header {
    std::uint8_t tagClass = 0;
    bool constructed = false;
    std::uint32_t tag = 0;
    bool indefinite = false;
    std::size_t length = 0;
};

struct Parameters {
    bool namedCurve = false;
    std::span<const unsigned char> object;
    std::size_t consumed = 0;
};

struct KeyDeleter {
    void operator()(EcKey* key) const {
        EC_KEY_free_nid_postfix(key);
    }
};

bool ReadByte(std::span<const unsigned char>& input, unsigned char& byte) {
    if (input.empty()) return false;
    byte = input.front();
    input = input.subspan(1);
    return true;
}

bool ReadIdentifier(std::span<const unsigned char>& input, Header& header) {
    unsigned char identifier;
    if (!ReadByte(input, identifier)) return false;
    header.tagClass = identifier >> 6;
    header.constructed = (identifier & 0x20) != 0;
    header.tag = identifier & 0x1f;
    if (header.tag != 0x1f) return true;
    header.tag = 0;
    unsigned char byte;
    do {
        if (!ReadByte(input, byte)) return false;
        if (header.tag > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
        header.tag = (header.tag << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    return true;
}

bool ReadLength(std::span<const unsigned char>& input, Header& header) {
    unsigned char first;
    if (!ReadByte(input, first)) return false;
    header.indefinite = first == 0x80;
    header.length = first < 0x80 ? first : 0;
    if (first <= 0x80) return true;
    const int count = first & 0x7f;
    if (count == 0x7f) return false;
    for (int index = 0; index < count; ++index) {
        unsigned char byte;
        if (!ReadByte(input, byte)) return false;
        if (header.length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        header.length = (header.length << 8) | byte;
    }
    return true;
}

bool ValidObjectEncoding(std::span<const unsigned char> content) {
    if (content.empty()) return false;
    bool arcStart = true;
    for (const unsigned char byte : content) {
        if (arcStart && byte == 0x80) return false;
        arcStart = (byte & 0x80) == 0;
    }
    return arcStart;
}

std::optional<Parameters> DecodeParameters(std::span<const unsigned char> input) {
    std::span<const unsigned char> cursor = input;
    Header header;
    if (!ReadIdentifier(cursor, header)) {
        PutAsn1Error(BadObjectHeader, 3);
        return std::nullopt;
    }
    if (header.tagClass != 0 || (header.tag != ObjectTag && header.tag != SequenceTag && header.tag != NullTag)) {
        PutAsn1Error(NoMatchingChoiceType, 0);
        return std::nullopt;
    }
    if (!ReadLength(cursor, header) || (header.indefinite && !header.constructed)) {
        PutAsn1Error(BadObjectHeader, 3);
        return std::nullopt;
    }
    if (!header.indefinite && header.length > cursor.size()) {
        PutAsn1Error(TooLong, 3);
        return std::nullopt;
    }
    if (header.tag == SequenceTag) {
        if (!header.constructed) {
            PutAsn1Error(SequenceNotConstructed, 2);
            return std::nullopt;
        }
        NotImplemented_nid_no_patch("d2i_ECParameters: explicit curve parameters");
    }
    if (header.constructed) {
        PutAsn1Error(TypeNotPrimitive, 2);
        return std::nullopt;
    }
    const std::span<const unsigned char> content = cursor.first(header.length);
    const std::size_t consumed = input.size() - cursor.size() + header.length;
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutAsn1Error(NestedAsn1Error, 1);
        return std::nullopt;
    }
    if (header.tag == NullTag) {
        if (!content.empty()) {
            PutAsn1Error(NullIsWrongLength, 2);
            return std::nullopt;
        }
        return Parameters{false, content, consumed};
    }
    if (!ValidObjectEncoding(content)) {
        PutAsn1Error(InvalidObjectEncoding, 2);
        return std::nullopt;
    }
    return Parameters{true, content, consumed};
}

EcGroup* NamedCurveGroup(std::span<const unsigned char> object) {
    const LibreSslAsn1Object decoded{nullptr, nullptr, 0, static_cast<std::int32_t>(object.size()), object.data(), 0};
    const int nid = LibreSsl3::ObjectNid(&decoded);
    for (const auto& [first, last] : LibreSslCurves) {
        if (nid >= first && nid <= last) return EC_GROUP_new_by_curve_name_nid_postfix(nid);
    }
    if (nid > 0) PutError(UnknownGroup);
    return nullptr;
}

EcGroup* DecodeGroup(const unsigned char** in, std::int64_t length) {
    const std::optional<Parameters> parameters = length < 0 ? std::nullopt : DecodeParameters({*in, static_cast<std::size_t>(length)});
    if (!parameters) {
        PutError(D2iEcpkParametersFailure);
        return nullptr;
    }
    *in += parameters->consumed;
    EcGroup* group = nullptr;
    if (parameters->namedCurve) {
        if ((group = NamedCurveGroup(parameters->object)) == nullptr) PutError(GroupNewByNameFailure);
        else group->asn1Flag = NamedCurveFlag;
    }
    if (group == nullptr) PutError(PkParametersToGroupFailure);
    return group;
}

}

extern "C" {

EcKey* APS5_VABI d2i_ECParameters_nid_postfix(EcKey** key, const unsigned char** in, std::int64_t length) {
    if (in == nullptr || *in == nullptr) {
        PutError(PassedNullParameter);
        return nullptr;
    }
    std::unique_ptr<EcKey, KeyDeleter> created;
    if (key == nullptr || *key == nullptr) created.reset(EC_KEY_new_nid_postfix());
    EcKey* result = created ? created.get() : *key;
    EcGroup* group = DecodeGroup(in, length);
    if (group == nullptr) {
        PutError(EcLibrary);
        return nullptr;
    }
    EC_GROUP_free_nid_postfix(result->group);
    result->group = group;
    created.release();
    if (key != nullptr) *key = result;
    return result;
}

EcKey* APS5_VABI o2i_ECPublicKey_nid_postfix(EcKey** key, const unsigned char** in, std::int64_t length) {
    if (key == nullptr || *key == nullptr || (*key)->group == nullptr) {
        PutError(PassedNullParameter);
        return nullptr;
    }
    if (in == nullptr) APS5_INVALID_ARG_EX;
    EcKey* result = *key;
    if (result->publicKey == nullptr && (result->publicKey = EC_POINT_new_nid_postfix(result->group)) == nullptr) {
        PutError(MallocFailure);
        return nullptr;
    }
    if (!EC_POINT_oct2point_nid_postfix(result->group, result->publicKey, *in, static_cast<std::size_t>(length), nullptr)) {
        PutError(EcLibrary);
        return nullptr;
    }
    result->conversionForm = (*in)[0] & ~1;
    *in += length;
    return result;
}

}
