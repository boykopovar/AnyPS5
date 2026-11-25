/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a) src/lib/libcrypto/x509/x_all.c (x_all.c,v 1.32),
 * asn1/tasn_dec.c (tasn_dec.c,v 1.88), asn1/asn1_lib.c (asn1_lib.c,v 1.54), asn1/a_int.c (a_int.c,v 1.48)
 * and asn1/x_bignum.c (x_bignum.c,v 1.15) */
/* $OpenBSD: x_all.c,v 1.32 2024/06/19 08:00:53 tb Exp $ */
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
/* $OpenBSD: a_int.c,v 1.48 2024/07/08 14:52:31 beck Exp $ */
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
/* $OpenBSD: x_bignum.c,v 1.15 2024/07/08 16:24:22 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

constexpr int ErrLibAsn1 = 13;
constexpr int AnyFunction = 0xfff;

constexpr int ErrRNestedAsn1Error = 58;
constexpr int ErrRAsn1LengthMismatch = 62;

constexpr int Asn1RBadObjectHeader = 102;
constexpr int Asn1RBnLib = 105;
constexpr int Asn1RFieldMissing = 121;
constexpr int Asn1RMissingEoc = 137;
constexpr int Asn1RSequenceLengthMismatch = 148;
constexpr int Asn1RSequenceNotConstructed = 149;
constexpr int Asn1RTooLong = 155;
constexpr int Asn1RUnexpectedEoc = 159;
constexpr int Asn1RWrongTag = 168;
constexpr int Asn1RTypeNotPrimitive = 231;

constexpr std::uint32_t TagInteger = 2;
constexpr std::uint32_t TagSequence = 16;

using Bytes = std::span<const unsigned char>;

struct Header {
    bool constructed;
    bool indefinite;
    std::size_t length;
};

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(ErrLibAsn1, AnyFunction, reason);
}

bool TakeByte(Bytes& input, unsigned char& value) {
    if (input.empty()) return false;
    value = input.front();
    input = input.subspan(1);
    return true;
}

bool ReadIdentifier(Bytes& input, std::uint8_t& tagClass, bool& constructed, std::uint32_t& number) {
    unsigned char value = 0;
    if (!TakeByte(input, value)) return false;
    tagClass = static_cast<std::uint8_t>((value >> 6) & 0x3);
    constructed = ((value >> 5) & 0x1) != 0;
    number = value & 0x1f;
    if (number != 0x1f) return true;
    number = 0;
    do {
        if (!TakeByte(input, value)) return false;
        if (number > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
        number = number << 7 | (value & 0x7f);
    } while ((value & 0x80) != 0);
    return true;
}

bool ReadLength(Bytes& input, bool& indefinite, std::size_t& length) {
    indefinite = false;
    length = 0;
    unsigned char value = 0;
    if (!TakeByte(input, value)) return false;
    if (value < 0x80) {
        length = value;
        return true;
    }
    if (value == 0x80) {
        indefinite = true;
        return true;
    }
    std::size_t count = value & 0x7f;
    if (count == 0x7f) return false;
    while (count-- > 0) {
        if (!TakeByte(input, value)) return false;
        if (length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        length = (length << 8) | value;
    }
    return true;
}

bool CheckTag(Bytes& input, std::uint32_t expected, Header& header) {
    std::uint8_t tagClass = 0;
    std::uint32_t number = 0;
    if (!ReadIdentifier(input, tagClass, header.constructed, number)) {
        PutError(Asn1RBadObjectHeader);
        return false;
    }
    if (number != expected || tagClass != 0) {
        PutError(Asn1RWrongTag);
        return false;
    }
    if (!ReadLength(input, header.indefinite, header.length)) {
        PutError(Asn1RBadObjectHeader);
        return false;
    }
    if (header.indefinite && !header.constructed) {
        PutError(Asn1RBadObjectHeader);
        return false;
    }
    if (!header.indefinite && input.size() < header.length) {
        PutError(Asn1RTooLong);
        return false;
    }
    if (number > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        PutError(Asn1RTooLong);
        return false;
    }
    if (header.indefinite) header.length = input.size();
    return true;
}

bool TakeEndOfContents(Bytes& input) {
    if (input.size() < 2 || input[0] != 0 || input[1] != 0) return false;
    input = input.subspan(2);
    return true;
}

bool KeepTwosPadding(Bytes content) {
    if (content.size() <= 1 || content[0] != 0xff) return false;
    for (std::size_t index = 1; index < content.size(); ++index)
        if (content[index] != 0) return false;
    return true;
}

LibreSsl3Bignum* IntegerContentToBignum(Bytes content) {
    if (content.empty()) {
        PutError(ErrRAsn1LengthMismatch);
        return nullptr;
    }
    const bool negative = (content[0] >> 7) != 0;
    const unsigned char padding = negative ? 0xff : 0x00;
    if (content.size() > 1 && content[0] == padding && !KeepTwosPadding(content)) {
        content = content.subspan(1);
        if ((content[0] >> 7) == (padding >> 7)) {
            PutError(ErrRAsn1LengthMismatch);
            return nullptr;
        }
    }
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return nullptr;
    std::vector<unsigned char> magnitude(content.begin(), content.end());
    if (negative) {
        unsigned char carry = 1;
        for (std::size_t index = magnitude.size(); index-- > 0;) {
            magnitude[index] = static_cast<unsigned char>((magnitude[index] ^ 0xff) + carry);
            if (magnitude[index] != 0) carry = 0;
        }
    }
    auto* number = LibreSsl3::NewRsaBignum_nid_no_patch(magnitude.data(), magnitude.size());
    if (!number) {
        PutError(Asn1RBnLib);
        return nullptr;
    }
    number->negative = negative && number->top > 0 ? 1 : 0;
    return number;
}

LibreSsl3Bignum* DecodeIntegerField(Bytes& content) {
    Bytes object = content;
    Header header{};
    if (!CheckTag(object, TagInteger, header)) {
        PutError(ErrRNestedAsn1Error);
        PutError(ErrRNestedAsn1Error);
        return nullptr;
    }
    if (header.constructed) {
        PutError(Asn1RTypeNotPrimitive);
        PutError(ErrRNestedAsn1Error);
        return nullptr;
    }
    auto* number = IntegerContentToBignum(object.first(header.length));
    if (!number) {
        PutError(ErrRNestedAsn1Error);
        return nullptr;
    }
    content = object.subspan(header.length);
    return number;
}

LibreSsl3Rsa* DecodeRsaPublicKey(Bytes input) {
    Header header{};
    if (!CheckTag(input, TagSequence, header)) {
        PutError(ErrRNestedAsn1Error);
        return nullptr;
    }
    if (!header.constructed) {
        PutError(Asn1RSequenceNotConstructed);
        return nullptr;
    }
    Bytes content = input.first(header.length);
    bool endOfContentsNeeded = header.indefinite;
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    if (!rsa) {
        PutError(ErrRNestedAsn1Error);
        return nullptr;
    }
    LibreSsl3Bignum* fields[2] = {nullptr, nullptr};
    std::size_t decoded = 0;
    bool failed = false;
    for (; decoded < 2; ++decoded) {
        if (TakeEndOfContents(content)) {
            if (!header.indefinite) {
                PutError(Asn1RUnexpectedEoc);
                failed = true;
            }
            endOfContentsNeeded = false;
            break;
        }
        if (content.empty()) break;
        fields[decoded] = DecodeIntegerField(content);
        if (!fields[decoded]) {
            failed = true;
            break;
        }
    }
    if (!failed && endOfContentsNeeded && !TakeEndOfContents(content)) {
        PutError(Asn1RMissingEoc);
        failed = true;
    }
    if (!failed && !header.indefinite && !content.empty()) {
        PutError(Asn1RSequenceLengthMismatch);
        failed = true;
    }
    if (!failed && decoded < 2) {
        PutError(Asn1RFieldMissing);
        failed = true;
    }
    if (failed) {
        for (auto* field : fields) BN_clear_free_nid_postfix(field);
        RSA_free_nid_disambig1_nid_postfix(rsa);
        return nullptr;
    }
    rsa->n = fields[0];
    rsa->e = fields[1];
    return rsa;
}

}

extern "C" {

LibreSsl3Rsa* APS5_VABI d2i_RSAPublicKey_bio_nid_postfix(LibreSsl::Bio* bio, LibreSsl3Rsa** output) {
    std::vector<unsigned char> bytes;
    if (!ReadDerObjectFromBio_nid_no_patch(bio, bytes)) return nullptr;
    if (output) {
        LibreSsl3Rsa* previous = nullptr;
        if (!GuestCopyIn_nid_no_patch(&previous, output, sizeof(previous))) return nullptr;
        if (previous) {
            RSA_free_nid_disambig1_nid_postfix(previous);
            LibreSsl3Rsa* cleared = nullptr;
            if (!GuestCopyOut_nid_no_patch(output, &cleared, sizeof(cleared))) return nullptr;
        }
    }
    auto* rsa = DecodeRsaPublicKey(bytes);
    if (rsa && output && !GuestCopyOut_nid_no_patch(output, &rsa, sizeof(rsa))) {
        RSA_free_nid_disambig1_nid_postfix(rsa);
        return nullptr;
    }
    return rsa;
}

}
