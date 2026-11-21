/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_int.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_object.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_bitstr.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_dec.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/asn1_lib.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/asn1_types.c */
/* $OpenBSD: a_int.c,v 1.48 2024/07/08 14:52:31 beck Exp $ */
/* $OpenBSD: a_object.c,v 1.55 2024/07/08 14:52:31 beck Exp $ */
/* $OpenBSD: a_bitstr.c,v 1.43 2024/07/08 14:52:31 beck Exp $ */
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
/* $OpenBSD: asn1_types.c,v 1.3 2023/07/05 21:23:36 beck Exp $ */
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

#pragma once

#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace LibreSsl3Asn1 {

inline constexpr int Asn1Library = 13;
inline constexpr int AnyFunction = 0xfff;
inline constexpr int Asn1NestedError = 58;
inline constexpr int Asn1LengthMismatch = 62;
inline constexpr int Asn1BadObjectHeader = 102;
inline constexpr int Asn1BooleanIsWrongLength = 106;
inline constexpr int Asn1DataIsWrong = 109;
inline constexpr int Asn1FieldMissing = 121;
inline constexpr int Asn1NullIsWrongLength = 144;
inline constexpr int Asn1SequenceLengthMismatch = 148;
inline constexpr int Asn1SequenceNotConstructed = 149;
inline constexpr int Asn1StringTooShort = 152;
inline constexpr int Asn1TooLong = 155;
inline constexpr int Asn1TypeNotConstructed = 156;
inline constexpr int Asn1UnexpectedEoc = 159;
inline constexpr int Asn1WrongTag = 168;
inline constexpr int Asn1BmpStringIsWrongLength = 214;
inline constexpr int Asn1UniversalStringIsWrongLength = 215;
inline constexpr int Asn1InvalidObjectEncoding = 216;
inline constexpr int Asn1InvalidBitStringBitsLeft = 220;
inline constexpr int Asn1TooLarge = 223;
inline constexpr int Asn1TooSmall = 224;
inline constexpr int Asn1TypeNotPrimitive = 231;
inline constexpr int TypeBoolean = 1;
inline constexpr int TypeInteger = 2;
inline constexpr int TypeBitString = 3;
inline constexpr int TypeOctetString = 4;
inline constexpr int TypeNull = 5;
inline constexpr int TypeObject = 6;
inline constexpr int TypeReal = 9;
inline constexpr int TypeEnumerated = 10;
inline constexpr int TypeRelativeOid = 13;
inline constexpr int TypeTime = 14;
inline constexpr int TypeSequence = 16;
inline constexpr int TypeSet = 17;
inline constexpr int TypeUtcTime = 23;
inline constexpr int TypeGeneralizedTime = 24;
inline constexpr int TypeUniversalString = 28;
inline constexpr int TypeBmpString = 30;
inline constexpr int TypeOther = -3;
inline constexpr int TypeAny = -1;
inline constexpr int TypeNegativeFlag = 0x100;

inline void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, AnyFunction, reason);
}

struct Reader {
    const unsigned char* cursor;
    std::size_t remaining;
};

struct Header {
    unsigned tagClass = 0;
    bool constructed = false;
    std::uint32_t tagNumber = 0;
    bool indefinite = false;
    std::size_t length = 0;
};

struct Element {
    int type = 0;
    const unsigned char* encoded = nullptr;
    std::size_t encodedLength = 0;
    const unsigned char* content = nullptr;
    std::size_t length = 0;
};

struct Integer {
    std::vector<unsigned char> magnitude;
    bool negative = false;
};

inline bool TakeByte(Reader& reader, unsigned char& value) {
    if (reader.remaining == 0) return false;
    value = *reader.cursor;
    ++reader.cursor;
    --reader.remaining;
    return true;
}

inline bool ReadIdentifier(Reader& reader, Header& header) {
    unsigned char value = 0;
    if (!TakeByte(reader, value)) return false;
    header.tagClass = (value >> 6) & 0x3u;
    header.constructed = ((value >> 5) & 0x1u) != 0;
    std::uint32_t number = value & 0x1fu;
    if (number == 0x1f) {
        number = 0;
        do {
            if (!TakeByte(reader, value)) return false;
            if (number > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
            number = (number << 7) | (value & 0x7fu);
        } while ((value & 0x80u) != 0);
    }
    header.tagNumber = number;
    return true;
}

inline bool ReadLength(Reader& reader, Header& header) {
    unsigned char value = 0;
    header.indefinite = false;
    header.length = 0;
    if (!TakeByte(reader, value)) return false;
    if (value < 0x80) {
        header.length = value;
        return true;
    }
    if (value == 0x80) {
        header.indefinite = true;
        return true;
    }
    unsigned count = value & 0x7fu;
    if (count == 0x7f) return false;
    std::size_t length = 0;
    while (count-- > 0) {
        if (!TakeByte(reader, value)) return false;
        if (length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        length = (length << 8) | value;
    }
    header.length = length;
    return true;
}

inline int CheckTag(Reader& reader, Header& header, int expectedType, bool optional) {
    if (!ReadIdentifier(reader, header)) {
        PutError(Asn1Library, Asn1BadObjectHeader);
        return 0;
    }
    if (expectedType >= 0 && (header.tagNumber != static_cast<std::uint32_t>(expectedType) || header.tagClass != 0)) {
        if (optional) return -1;
        PutError(Asn1Library, Asn1WrongTag);
        return 0;
    }
    if (!ReadLength(reader, header) || (header.indefinite && !header.constructed)) {
        PutError(Asn1Library, Asn1BadObjectHeader);
        return 0;
    }
    if ((!header.indefinite && reader.remaining < header.length)
        || header.tagNumber > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        PutError(Asn1Library, Asn1TooLong);
        return 0;
    }
    return 1;
}

inline bool MustBePrimitive(int type) {
    if (type == (TypeInteger | TypeNegativeFlag) || type == (TypeEnumerated | TypeNegativeFlag))
        type &= ~TypeNegativeFlag;
    return type == TypeBoolean || type == TypeInteger || type == TypeNull || type == TypeObject || type == TypeReal
        || type == TypeEnumerated || type == TypeRelativeOid || type == TypeTime;
}

inline bool MustBeConstructed(int type) {
    return type == TypeSequence || type == TypeSet;
}

inline bool ValidObjectIdentifier(const unsigned char* data, std::size_t length) {
    if (length == 0) return false;
    std::size_t index = 0;
    while (index < length) {
        std::uint64_t arc = 0;
        unsigned char value = 0;
        do {
            if (index == length) return false;
            value = data[index];
            ++index;
            if (arc == 0 && value == 0x80) return false;
            arc = (arc << 7) | (value & 0x7fu);
        } while ((value & 0x80u) != 0);
    }
    return true;
}

inline bool KeepTwosPadding(const unsigned char* data, std::size_t length) {
    if (length <= 1 || data[0] != 0xff) return false;
    for (std::size_t index = 1; index < length; ++index) {
        if (data[index] != 0) return false;
    }
    return true;
}

inline bool DecodeInteger(const unsigned char* data, std::size_t length, Integer* integer) {
    if (length == 0) return false;
    const unsigned char first = data[0];
    const bool negative = (first >> 7) != 0;
    const unsigned char padding = negative ? 0xff : 0x00;
    if (length > 1 && first == padding && !KeepTwosPadding(data, length)) {
        ++data;
        --length;
        if ((data[0] >> 7) == (padding >> 7)) return false;
    }
    if (integer == nullptr) return true;
    integer->negative = negative;
    integer->magnitude.assign(data, data + length);
    if (negative) {
        unsigned char carry = 1;
        for (std::size_t index = integer->magnitude.size(); index-- > 0;) {
            integer->magnitude[index] = static_cast<unsigned char>((integer->magnitude[index] ^ 0xffu) + carry);
            if (integer->magnitude[index] != 0) carry = 0;
        }
    }
    return true;
}

inline bool IntegerToInt64(const Integer& integer, std::int64_t& result) {
    std::uint64_t value = 0;
    for (const unsigned char byte : integer.magnitude) {
        if (value > (std::numeric_limits<std::uint64_t>::max() >> 8)) {
            PutError(Asn1Library, Asn1TooLarge);
            return false;
        }
        value = (value << 8) | byte;
    }
    if (integer.negative) {
        if (value > (std::uint64_t{1} << 63)) {
            PutError(Asn1Library, Asn1TooSmall);
            return false;
        }
        result = value == (std::uint64_t{1} << 63) ? std::numeric_limits<std::int64_t>::min()
                                                 : -static_cast<std::int64_t>(value);
        return true;
    }
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        PutError(Asn1Library, Asn1TooLarge);
        return false;
    }
    result = static_cast<std::int64_t>(value);
    return true;
}

inline std::int64_t IntegerValue(const Integer& integer) {
    std::int64_t value = 0;
    return IntegerToInt64(integer, value) ? value : -1;
}

inline bool ValidContent(int type, const unsigned char* content, std::size_t length) {
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    int reason = 0;
    switch (type) {
    case TypeObject:
        if (!ValidObjectIdentifier(content, length)) reason = Asn1InvalidObjectEncoding;
        break;
    case TypeNull:
        if (length != 0) reason = Asn1NullIsWrongLength;
        break;
    case TypeBoolean:
        if (length != 1) reason = Asn1BooleanIsWrongLength;
        break;
    case TypeBitString:
        if (length == 0) {
            reason = Asn1StringTooShort;
        } else if (content[0] > 7) {
            reason = Asn1InvalidBitStringBitsLeft;
        }
        break;
    case TypeEnumerated:
    case TypeInteger:
        if (!DecodeInteger(content, length, nullptr)) reason = Asn1LengthMismatch;
        break;
    case TypeUtcTime:
    case TypeGeneralizedTime:
        NotImplemented_nid_no_patch("ASN1_item_d2i: UTCTime or GeneralizedTime inside an AlgorithmIdentifier");
    case TypeBmpString:
        if ((length & 1u) != 0) reason = Asn1BmpStringIsWrongLength;
        break;
    case TypeUniversalString:
        if ((length & 3u) != 0) reason = Asn1UniversalStringIsWrongLength;
        break;
    default:
        break;
    }
    if (reason == 0) return true;
    PutError(Asn1Library, reason);
    return false;
}

inline int ReadField(Reader& reader, int expectedType, bool optional, Element& element) {
    Reader probe = reader;
    Header header;
    const int checked = CheckTag(probe, header, expectedType, optional);
    if (checked == -1) return -1;
    if (checked != 1) {
        PutError(Asn1Library, Asn1NestedError);
        PutError(Asn1Library, Asn1NestedError);
        return 0;
    }
    int type = expectedType;
    if (expectedType < 0) type = header.tagClass == 0 ? static_cast<int>(header.tagNumber) : TypeOther;
    if (MustBeConstructed(type) && !header.constructed) {
        PutError(Asn1Library, Asn1TypeNotConstructed);
        PutError(Asn1Library, Asn1NestedError);
        return 0;
    }
    if (MustBePrimitive(type) && header.constructed) {
        PutError(Asn1Library, Asn1TypeNotPrimitive);
        PutError(Asn1Library, Asn1NestedError);
        return 0;
    }
    if (header.indefinite) NotImplemented_nid_no_patch("ASN1_item_d2i: indefinite length");
    const std::size_t headerLength = static_cast<std::size_t>(probe.cursor - reader.cursor);
    element.type = type;
    element.encoded = reader.cursor;
    element.encodedLength = headerLength + header.length;
    element.content = probe.cursor;
    element.length = header.length;
    bool valid = true;
    if (type == TypeSequence || type == TypeSet || type == TypeOther) {
        valid = element.encodedLength <= static_cast<std::size_t>(std::numeric_limits<int>::max());
    } else {
        if (header.constructed) NotImplemented_nid_no_patch("ASN1_item_d2i: constructed string");
        valid = ValidContent(type, element.content, element.length);
    }
    if (!valid) {
        PutError(Asn1Library, Asn1NestedError);
        return 0;
    }
    reader.cursor += element.encodedLength;
    reader.remaining -= element.encodedLength;
    return 1;
}

inline bool StartsWithEndOfContents(const Reader& reader) {
    return reader.remaining >= 2 && reader.cursor[0] == 0 && reader.cursor[1] == 0;
}

inline bool EnterSequence(Reader& reader, Reader& content) {
    Reader probe = reader;
    Header header;
    if (CheckTag(probe, header, TypeSequence, false) != 1) {
        PutError(Asn1Library, Asn1NestedError);
        return false;
    }
    if (!header.constructed) {
        PutError(Asn1Library, Asn1SequenceNotConstructed);
        return false;
    }
    if (header.indefinite) NotImplemented_nid_no_patch("ASN1_item_d2i: indefinite length");
    content = Reader{probe.cursor, header.length};
    reader.cursor = probe.cursor + header.length;
    reader.remaining = probe.remaining - header.length;
    return true;
}

inline bool AtField(const Reader& inner, bool& present) {
    if (StartsWithEndOfContents(inner)) {
        PutError(Asn1Library, Asn1UnexpectedEoc);
        return false;
    }
    present = inner.remaining != 0;
    return true;
}

inline bool MissingField() {
    PutError(Asn1Library, Asn1FieldMissing);
    return false;
}

inline bool SequenceComplete(const Reader& inner) {
    if (inner.remaining == 0) return true;
    PutError(Asn1Library, Asn1SequenceLengthMismatch);
    return false;
}

inline void StoreInteger(const Element& element, Integer& integer) {
    if (!DecodeInteger(element.content, element.length, &integer))
        throw std::logic_error("ASN1_item_d2i: INTEGER accepted then rejected");
}

} // namespace LibreSsl3Asn1
