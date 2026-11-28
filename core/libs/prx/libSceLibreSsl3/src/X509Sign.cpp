/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x_all.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/asn1_item.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_x509.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_int.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_bitstr.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_name.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_val.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_pubkey.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_exten.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_enc.c */
/* $OpenBSD: x_x509.c,v 1.39 2024/07/08 14:48:49 beck Exp $ */
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
/* ====================================================================
 * Copyright (c) 1998-2004 The OpenSSL Project.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR ITS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl3/include/X509.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {

constexpr int TypeOther = -3;
constexpr int BooleanType = 1;
constexpr int IntegerType = 2;
constexpr int SignatureStringType = 3;
constexpr int OctetStringType = 4;
constexpr int NullType = 5;
constexpr int ObjectType = 6;
constexpr int Utf8StringType = 12;
constexpr int SequenceType = 16;
constexpr int SetType = 17;
constexpr int NumericStringType = 18;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;
constexpr int UniversalStringType = 28;
constexpr int BmpStringType = 30;
constexpr int NegativeIntegerFlag = 0x100;
constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr std::int64_t BitsLeftMask = BitsLeftFlag | 0x07;
constexpr int RsaKeyType = 6;
constexpr int Ed25519KeyType = 952;
constexpr unsigned char BooleanTag = 0x01;
constexpr unsigned char IntegerTag = 0x02;
constexpr unsigned char BitStringTag = 0x03;
constexpr unsigned char OctetStringTag = 0x04;
constexpr unsigned char NullTag = 0x05;
constexpr unsigned char ObjectTag = 0x06;
constexpr unsigned char SequenceTag = 0x30;
constexpr unsigned char SetTag = 0x31;
constexpr unsigned char IssuerUniqueIdTag = 0x81;
constexpr unsigned char SubjectUniqueIdTag = 0x82;
constexpr unsigned char VersionTag = 0xa0;
constexpr unsigned char ExtensionsTag = 0xa3;

struct Encoding {
    const unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

struct NameBuffer {
    std::size_t length;
    char* data;
    std::size_t max;
};

static_assert(sizeof(Encoding) == sizeof(LibreSsl3CertificateInfo::encoding));
static_assert(sizeof(NameBuffer) == 24);
static_assert(sizeof(LibreSsl3X509) <= sizeof(LibreSsl3Certificate));
static_assert(offsetof(LibreSsl3X509, certInfo) == offsetof(LibreSsl3Certificate, certInfo));
static_assert(offsetof(LibreSsl3X509, sigAlg) == offsetof(LibreSsl3Certificate, sigAlg));
static_assert(offsetof(LibreSsl3X509, signature) == 16);
static_assert(offsetof(Encoding, modified) == 16);

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename Value>
Value CopyIn(const void* source, const char* caller) {
    PreservedErrno guard;
    Value value {};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename Value>
void CopyOut(void* destination, const Value& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(value))) NotImplemented_nid_no_patch(caller);
}

std::vector<unsigned char> CopyGuestBytes(const unsigned char* source, std::int64_t length, const char* caller) {
    if (length < 0) NotImplemented_nid_no_patch(caller);
    if (length == 0) return {};
    if (!source) NotImplemented_nid_no_patch(caller);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    PreservedErrno guard;
    if (!GuestCopyIn_nid_no_patch(bytes.data(), source, bytes.size())) NotImplemented_nid_no_patch(caller);
    return bytes;
}

int SignatureNid(int digestNid, int keyType) {
    for (int nid = 1; nid < LibreSsl3::ObjectCount; ++nid) {
        int digest = 0;
        int key = 0;
        if (OBJ_find_sigid_algs_nid_postfix(nid, &digest, &key) == 0) continue;
        if (digest == digestNid && key == keyType) return nid;
    }
    return 0;
}

std::vector<unsigned char> CertificateInfoEncoding(const Encoding& encoding) {
    return CopyGuestBytes(encoding.bytes, encoding.length, "X509_sign: certificate info without a current cached encoding");
}

void AppendElement(std::vector<unsigned char>& output, unsigned char tag, const std::vector<unsigned char>& content) {
    output.push_back(tag);
    if (content.size() < 0x80) {
        output.push_back(static_cast<unsigned char>(content.size()));
    } else {
        unsigned char bytes[sizeof(std::size_t)];
        int count = 0;
        for (std::size_t rest = content.size(); rest != 0; rest >>= 8) bytes[count++] = static_cast<unsigned char>(rest);
        output.push_back(static_cast<unsigned char>(0x80 | count));
        while (count > 0) output.push_back(bytes[--count]);
    }
    output.insert(output.end(), content.begin(), content.end());
}

std::vector<unsigned char> Element(unsigned char tag, const std::vector<unsigned char>& content) {
    std::vector<unsigned char> encoded;
    AppendElement(encoded, tag, content);
    return encoded;
}

std::vector<unsigned char> ObjectEncoding(const void* guestObject, const char* caller) {
    if (!guestObject) NotImplemented_nid_no_patch(caller);
    const auto object = CopyIn<LibreSslAsn1Object>(guestObject, caller);
    if (object.length <= 0 || !object.data) NotImplemented_nid_no_patch(caller);
    return Element(ObjectTag, CopyGuestBytes(object.data, object.length, caller));
}

std::vector<unsigned char> AlgorithmEncoding(const LibreSslAsn1Object& object, bool nullParameter) {
    if (object.length <= 0 || !object.data)
        NotImplemented_nid_no_patch("X509_sign: signature algorithm without an object encoding");
    auto content = Element(ObjectTag, CopyGuestBytes(object.data, object.length, "X509_sign"));
    if (nullParameter) {
        content.push_back(NullTag);
        content.push_back(0);
    }
    return Element(SequenceTag, content);
}

std::vector<unsigned char> BitStringContent(const LibreSsl3Asn1String& bits, const char* caller) {
    if (bits.length < 0 || bits.length == std::numeric_limits<int>::max()) NotImplemented_nid_no_patch(caller);
    const auto data = CopyGuestBytes(bits.data, bits.length, caller);
    std::size_t used = data.size();
    int unused = 0;
    if (bits.length > 0 && (bits.flags & BitsLeftFlag) != 0) {
        unused = static_cast<int>(bits.flags & 0x07);
    } else {
        while (used > 0 && data[used - 1] == 0) --used;
        if (used > 0) {
            const unsigned char last = data[used - 1];
            while (unused < 7 && ((last >> unused) & 1) == 0) ++unused;
        }
    }
    std::vector<unsigned char> content;
    content.reserve(data.size() + 1);
    content.push_back(static_cast<unsigned char>(unused));
    content.insert(content.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(used));
    if (used > 0) content.back() &= static_cast<unsigned char>(0xffu << unused);
    content.resize(data.size() + 1, 0);
    return content;
}

std::vector<unsigned char> IntegerEncoding(const void* guestInteger, const char* caller) {
    const auto integer = CopyIn<LibreSsl3Asn1String>(guestInteger, caller);
    if ((integer.type & ~NegativeIntegerFlag) != IntegerType) NotImplemented_nid_no_patch(caller);
    const auto raw = CopyGuestBytes(integer.data, integer.length, caller);
    std::size_t firstNonZero = 0;
    while (firstNonZero < raw.size() && raw[firstNonZero] == 0) ++firstNonZero;
    if (firstNonZero == raw.size()) return Element(IntegerTag, {0x00});
    std::vector<unsigned char> magnitude(raw.begin() + static_cast<std::ptrdiff_t>(firstNonZero), raw.end());
    const bool negative = (integer.type & NegativeIntegerFlag) != 0;
    if (negative) {
        unsigned char carry = 1;
        for (std::size_t index = magnitude.size(); index-- > 0;) {
            const unsigned int sum = static_cast<unsigned int>(magnitude[index] ^ 0xffu) + carry;
            magnitude[index] = static_cast<unsigned char>(sum & 0xffu);
            carry = sum > 0xffu ? 1 : 0;
        }
    }
    const unsigned char msb = (magnitude[0] >> 7) & 1;
    if ((!negative && msb == 1) || (negative && msb == 0)) {
        magnitude.insert(magnitude.begin(), negative ? static_cast<unsigned char>(0xff) : static_cast<unsigned char>(0x00));
    }
    return Element(IntegerTag, magnitude);
}

bool IsStringType(int type) {
    return type == OctetStringType || type == Utf8StringType
        || (type >= NumericStringType && type <= UniversalStringType) || type == BmpStringType;
}

std::vector<unsigned char> AnyEncoding(const LibreSsl3Asn1Type* guestParameter, const char* caller) {
    const auto parameter = CopyIn<LibreSsl3Asn1Type>(guestParameter, caller);
    switch (parameter.type) {
    case NullType:
        return Element(NullTag, {});
    case ObjectType:
        return ObjectEncoding(parameter.value, caller);
    case BooleanType: {
        const auto value = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(parameter.value));
        if (value == -1) return {};
        return Element(BooleanTag, {static_cast<unsigned char>(value)});
    }
    case SignatureStringType:
        if (!parameter.value) NotImplemented_nid_no_patch(caller);
        return Element(BitStringTag, BitStringContent(CopyIn<LibreSsl3Asn1String>(parameter.value, caller), caller));
    default:
        break;
    }
    if (parameter.type != SequenceType && parameter.type != SetType && parameter.type != TypeOther
        && !IsStringType(parameter.type))
        NotImplemented_nid_no_patch(caller);
    if (!parameter.value) NotImplemented_nid_no_patch(caller);
    const auto value = CopyIn<LibreSsl3Asn1String>(parameter.value, caller);
    const auto bytes = CopyGuestBytes(value.data, value.length, caller);
    if (parameter.type == SequenceType || parameter.type == SetType || parameter.type == TypeOther) return bytes;
    return Element(static_cast<unsigned char>(parameter.type), bytes);
}

struct Span {
    std::size_t begin;
    std::size_t content;
    std::size_t end;
};

Span ReadElement(const std::vector<unsigned char>& bytes, std::size_t offset, std::size_t limit, unsigned char tag,
    const char* caller) {
    if (limit - offset < 2 || bytes[offset] != tag) NotImplemented_nid_no_patch(caller);
    std::size_t length = bytes[offset + 1];
    std::size_t content = offset + 2;
    if ((length & 0x80) != 0) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(std::size_t) || limit - content < count) NotImplemented_nid_no_patch(caller);
        length = 0;
        for (std::size_t index = 0; index < count; ++index) length = (length << 8) | bytes[content++];
    }
    if (limit - content < length) NotImplemented_nid_no_patch(caller);
    return {offset, content, content + length};
}

std::vector<unsigned char> NameValueEncoding(const LibreSsl3Asn1String& value, const char* caller) {
    if (value.type == SignatureStringType) return Element(BitStringTag, BitStringContent(value, caller));
    if (value.type == SequenceType) return CopyGuestBytes(value.data, value.length, caller);
    if (!IsStringType(value.type)) NotImplemented_nid_no_patch(caller);
    return Element(static_cast<unsigned char>(value.type), CopyGuestBytes(value.data, value.length, caller));
}

std::vector<unsigned char> NameEncoding(const void* guestName) {
    constexpr const char* Caller = "X509_sign: certificate info with invalid X509_NAME";
    const auto name = CopyIn<LibreSslX509Name>(guestName, Caller);
    if (name.modified == 0 && name.bytes != nullptr) {
        const auto buffer = CopyIn<NameBuffer>(name.bytes, Caller);
        if (buffer.length > 0 && buffer.data != nullptr) {
            if (buffer.length > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
                NotImplemented_nid_no_patch(Caller);
            auto cached = CopyGuestBytes(reinterpret_cast<const unsigned char*>(buffer.data),
                static_cast<std::int64_t>(buffer.length), Caller);
            if (ReadElement(cached, 0, cached.size(), SequenceTag, Caller).end != cached.size())
                NotImplemented_nid_no_patch(Caller);
            return cached;
        }
    }
    if (!name.entries) NotImplemented_nid_no_patch(Caller);
    const auto stack = CopyIn<LibreSslStack>(name.entries, Caller);
    if (stack.num < 0 || (stack.num > 0 && !stack.data)) NotImplemented_nid_no_patch(Caller);
    if (stack.num == 0) return Element(SequenceTag, {});

    std::vector<const LibreSslX509NameEntry*> pointers(static_cast<std::size_t>(stack.num));
    {
        PreservedErrno guard;
        if (!GuestCopyIn_nid_no_patch(pointers.data(), stack.data, pointers.size() * sizeof(pointers[0])))
            NotImplemented_nid_no_patch(Caller);
    }

    int currentSet = -1;
    std::vector<std::vector<std::vector<unsigned char>>> groups;
    for (const auto* entryPointer : pointers) {
        if (!entryPointer) NotImplemented_nid_no_patch(Caller);
        const auto entry = CopyIn<LibreSslX509NameEntry>(entryPointer, Caller);
        if (!entry.object || !entry.value || entry.set < 0) NotImplemented_nid_no_patch(Caller);
        std::vector<unsigned char> entryContent = ObjectEncoding(entry.object, Caller);
        const auto encodedValue = NameValueEncoding(CopyIn<LibreSsl3Asn1String>(entry.value, Caller), Caller);
        entryContent.insert(entryContent.end(), encodedValue.begin(), encodedValue.end());
        auto entryBytes = Element(SequenceTag, entryContent);
        if (groups.empty() || entry.set != currentSet) {
            currentSet = entry.set;
            groups.emplace_back();
        }
        groups.back().push_back(std::move(entryBytes));
    }

    std::vector<unsigned char> content;
    for (auto& group : groups) {
        std::sort(group.begin(), group.end(), [](const auto& left, const auto& right) {
            const std::size_t common = std::min(left.size(), right.size());
            const int order = std::memcmp(left.data(), right.data(), common);
            if (order != 0) return order < 0;
            return left.size() < right.size();
        });
        std::vector<unsigned char> rdn;
        for (const auto& entryBytes : group) rdn.insert(rdn.end(), entryBytes.begin(), entryBytes.end());
        AppendElement(content, SetTag, rdn);
    }
    return Element(SequenceTag, content);
}

std::vector<unsigned char> TimeEncoding(const void* guestTime, const char* caller) {
    if (!guestTime) NotImplemented_nid_no_patch(caller);
    const auto time = CopyIn<LibreSsl3Asn1String>(guestTime, caller);
    if ((time.type != UtcTimeType && time.type != GeneralizedTimeType) || time.length <= 0 || !time.data)
        NotImplemented_nid_no_patch(caller);
    return Element(static_cast<unsigned char>(time.type), CopyGuestBytes(time.data, time.length, caller));
}

std::vector<unsigned char> ValidityEncoding(const void* guestValidity) {
    constexpr const char* Caller = "X509_sign: certificate info with invalid validity";
    const auto validity = CopyIn<LibreSsl3X509Validity>(guestValidity, Caller);
    std::vector<unsigned char> content = TimeEncoding(validity.notBefore, Caller);
    const auto notAfter = TimeEncoding(validity.notAfter, Caller);
    content.insert(content.end(), notAfter.begin(), notAfter.end());
    return Element(SequenceTag, content);
}

std::vector<unsigned char> PublicKeyEncoding(const LibreSsl3X509Pubkey* guestKey) {
    constexpr const char* Caller = "X509_sign: certificate info with invalid public key";
    const auto key = CopyIn<LibreSsl3X509Pubkey>(guestKey, Caller);
    if (!key.algor || !key.publicKey) NotImplemented_nid_no_patch(Caller);
    const auto algor = CopyIn<LibreSsl3X509Algor>(key.algor, Caller);
    std::vector<unsigned char> algContent = ObjectEncoding(algor.algorithm, Caller);
    if (algor.parameter) {
        const auto param = AnyEncoding(algor.parameter, Caller);
        algContent.insert(algContent.end(), param.begin(), param.end());
    }
    std::vector<unsigned char> content = Element(SequenceTag, algContent);
    const auto bits = Element(BitStringTag, BitStringContent(CopyIn<LibreSsl3Asn1String>(key.publicKey, Caller), Caller));
    content.insert(content.end(), bits.begin(), bits.end());
    return Element(SequenceTag, content);
}

std::vector<unsigned char> ExtensionsEncoding(const LibreSsl3Stack* guestExtensions) {
    constexpr const char* Caller = "X509_sign: certificate info with invalid extensions";
    const auto stack = CopyIn<LibreSslStack>(guestExtensions, Caller);
    if (stack.num < 0 || (stack.num > 0 && !stack.data)) NotImplemented_nid_no_patch(Caller);
    if (stack.num == 0) return Element(SequenceTag, {});

    std::vector<const LibreSsl3X509Extension*> pointers(static_cast<std::size_t>(stack.num));
    {
        PreservedErrno guard;
        if (!GuestCopyIn_nid_no_patch(pointers.data(), stack.data, pointers.size() * sizeof(pointers[0])))
            NotImplemented_nid_no_patch(Caller);
    }

    std::vector<unsigned char> seqContent;
    for (const auto* extensionPointer : pointers) {
        if (!extensionPointer) NotImplemented_nid_no_patch(Caller);
        const auto extension = CopyIn<LibreSsl3X509Extension>(extensionPointer, Caller);
        if (!extension.value) NotImplemented_nid_no_patch(Caller);
        std::vector<unsigned char> extContent = ObjectEncoding(extension.object, Caller);
        if (extension.critical > 0) AppendElement(extContent, BooleanTag, {0xff});
        const auto value = CopyIn<LibreSsl3Asn1String>(extension.value, Caller);
        AppendElement(extContent, OctetStringTag, CopyGuestBytes(value.data, value.length, Caller));
        AppendElement(seqContent, SequenceTag, extContent);
    }
    return Element(SequenceTag, seqContent);
}

std::vector<unsigned char> EncodeCertificateInfo(const LibreSsl3CertificateInfo& info,
    const std::vector<unsigned char>& algorithm) {
    if (!info.serialNumber || !info.issuer || !info.validity || !info.subject || !info.key)
        NotImplemented_nid_no_patch("X509_sign: certificate info missing required fields");

    std::vector<unsigned char> content;
    if (info.version) {
        AppendElement(content, VersionTag, IntegerEncoding(info.version, "X509_sign: certificate info with invalid version"));
    }
    const auto serial = IntegerEncoding(info.serialNumber, "X509_sign: certificate info with invalid serial number");
    content.insert(content.end(), serial.begin(), serial.end());
    content.insert(content.end(), algorithm.begin(), algorithm.end());
    const auto issuer = NameEncoding(info.issuer);
    content.insert(content.end(), issuer.begin(), issuer.end());
    const auto validity = ValidityEncoding(info.validity);
    content.insert(content.end(), validity.begin(), validity.end());
    const auto subject = NameEncoding(info.subject);
    content.insert(content.end(), subject.begin(), subject.end());
    const auto pubkey = PublicKeyEncoding(info.key);
    content.insert(content.end(), pubkey.begin(), pubkey.end());
    if (info.issuerUid) {
        AppendElement(content, IssuerUniqueIdTag,
            BitStringContent(CopyIn<LibreSsl3Asn1String>(info.issuerUid, "X509_sign"), "X509_sign"));
    }
    if (info.subjectUid) {
        AppendElement(content, SubjectUniqueIdTag,
            BitStringContent(CopyIn<LibreSsl3Asn1String>(info.subjectUid, "X509_sign"), "X509_sign"));
    }
    if (info.extensions) {
        AppendElement(content, ExtensionsTag, ExtensionsEncoding(info.extensions));
    }
    return Element(SequenceTag, content);
}

std::vector<unsigned char> ReencodeCertificateInfo(const std::vector<unsigned char>& cached,
    const std::vector<unsigned char>& algorithm) {
    constexpr const char* Malformed = "X509_sign: cached certificate info that is not a TBSCertificate";
    const Span whole = ReadElement(cached, 0, cached.size(), SequenceTag, Malformed);
    if (whole.end != cached.size()) NotImplemented_nid_no_patch(Malformed);
    std::size_t cursor = whole.content;
    if (cursor < whole.end && cached[cursor] == VersionTag) cursor = ReadElement(cached, cursor, whole.end, VersionTag, Malformed).end;
    cursor = ReadElement(cached, cursor, whole.end, IntegerTag, Malformed).end;
    const Span signature = ReadElement(cached, cursor, whole.end, SequenceTag, Malformed);
    std::vector<unsigned char> content(cached.begin() + static_cast<std::ptrdiff_t>(whole.content),
        cached.begin() + static_cast<std::ptrdiff_t>(signature.begin));
    content.insert(content.end(), algorithm.begin(), algorithm.end());
    content.insert(content.end(), cached.begin() + static_cast<std::ptrdiff_t>(signature.end), cached.end());
    return Element(SequenceTag, content);
}

bool SignBytes(const std::vector<unsigned char>& tbs, LibreSsl3EvpPkey* key, const LibreSsl3::DigestMethod* digest,
    std::vector<unsigned char>& signature) {
    const int keySize = EVP_PKEY_size_nid_postfix(key);
    if (keySize <= 0) NotImplemented_nid_no_patch("X509_sign: digest signing through EVP");
    auto* context = EVP_MD_CTX_new_nid_postfix();
    if (!context) return false;
    signature.assign(static_cast<std::size_t>(keySize), 0);
    std::size_t length = signature.size();
    int signedOk = 0;
    try {
        if (EVP_DigestSignInit_nid_postfix(context, nullptr, digest, nullptr, key) == 1 && context->update
            && context->update(context, tbs.data(), tbs.size()) == 1)
            signedOk = EVP_DigestSignFinal_nid_postfix(context, signature.data(), &length);
    } catch (...) {
        EVP_MD_CTX_free_nid_postfix(context);
        throw;
    }
    EVP_MD_CTX_free_nid_postfix(context);
    if (signedOk != 1 || length == 0 || length > static_cast<std::size_t>(keySize)) return false;
    signature.resize(length);
    return true;
}

LibreSsl3Asn1Type* NewNullParameter() {
    auto* const allocated = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
    if (!allocated) return nullptr;
    allocated->type = NullType;
    allocated->value = nullptr;
    return allocated;
}

struct ParameterPlan {
    bool needsNew = false;
    bool updateInPlace = false;
    bool freeOld = false;
    LibreSsl3Asn1Type* oldParameter = nullptr;
    LibreSsl3Asn1Type* newParameter = nullptr;
};

bool PlanAlgorithmParameter(const LibreSsl3X509Algor& algor, bool nullParameter, ParameterPlan& plan) {
    plan.oldParameter = algor.parameter;
    if (!nullParameter) {
        plan.freeOld = algor.parameter != nullptr;
        return true;
    }
    if (!algor.parameter) {
        plan.needsNew = true;
    } else {
        const auto existing = CopyIn<LibreSsl3Asn1Type>(algor.parameter, "X509_sign");
        if (existing.value) {
            plan.needsNew = true;
            plan.freeOld = true;
        } else {
            plan.updateInPlace = true;
        }
    }
    if (!plan.needsNew) return true;
    plan.newParameter = NewNullParameter();
    return plan.newParameter != nullptr;
}

void WriteAlgorithmPlan(LibreSsl3X509Algor* guestAlgor, const void* object, ParameterPlan& plan) {
    CopyOut(&guestAlgor->algorithm, object, "X509_sign");
    if (plan.newParameter) {
        CopyOut(&guestAlgor->parameter, plan.newParameter, "X509_sign");
        plan.newParameter = nullptr;
    } else if (plan.updateInPlace) {
        CopyOut(plan.oldParameter, LibreSsl3Asn1Type {NullType, nullptr}, "X509_sign");
    } else if (plan.freeOld) {
        CopyOut(&guestAlgor->parameter, static_cast<LibreSsl3Asn1Type*>(nullptr), "X509_sign");
    }
}

} // namespace

extern "C" int APS5_VABI X509_sign_nid_postfix(LibreSsl3Certificate* certificate, LibreSsl3EvpPkey* key,
    const EvpMd* digest) {
    if (!certificate || !key) return 0;
    const int keyType = EVP_PKEY_base_id_nid_postfix(key);
    if (!digest) {
        if (keyType == Ed25519KeyType) NotImplemented_nid_no_patch("X509_sign: Ed25519 signing");
        return 0;
    }
    const auto fields = CopyIn<LibreSsl3Certificate>(certificate, "X509_sign");
    const auto view = CopyIn<LibreSsl3X509>(certificate, "X509_sign");
    if (!fields.certInfo || !fields.sigAlg) return 0;
    const auto info = CopyIn<LibreSsl3CertificateInfo>(fields.certInfo, "X509_sign");
    if (!info.signature) return 0;
    const auto encoding = CopyIn<Encoding>(info.encoding, "X509_sign");

    const auto* const digestMethod = reinterpret_cast<const LibreSsl3::DigestMethod*>(digest);
    const int digestNid = digestMethod->type;
    const int signatureNid = SignatureNid(digestNid, keyType);
    if (signatureNid == 0) NotImplemented_nid_no_patch("X509_sign: digest and key type without a signature algorithm");
    const auto* const object = LibreSsl3::ObjectByNid(signatureNid);
    if (!object) NotImplemented_nid_no_patch("X509_sign: digest and key type without a signature algorithm");

    const auto outerCopy = CopyIn<LibreSsl3X509Algor>(fields.sigAlg, "X509_sign");
    const auto innerCopy = CopyIn<LibreSsl3X509Algor>(info.signature, "X509_sign");
    const bool nullParameter = keyType == RsaKeyType;
    const auto algorithm = AlgorithmEncoding(*object, nullParameter);

    const bool hasCached = encoding.modified == 0 && encoding.bytes != nullptr && encoding.length > 0;
    const bool hasStructuredFields = info.serialNumber != nullptr && info.issuer != nullptr && info.validity != nullptr
        && info.subject != nullptr && info.key != nullptr;
    const bool reencodeFull = !hasCached || hasStructuredFields;

    std::vector<unsigned char> cached;
    std::vector<unsigned char> tbs;
    if (!reencodeFull) {
        if (LibreSsl3::ObjectNid(static_cast<const LibreSslAsn1Object*>(innerCopy.algorithm)) != signatureNid)
            NotImplemented_nid_no_patch("X509_sign: certificate info signed with another signature algorithm");
        cached = CertificateInfoEncoding(encoding);
        tbs = ReencodeCertificateInfo(cached, algorithm);
    } else {
        tbs = EncodeCertificateInfo(info, algorithm);
        if (hasCached && static_cast<std::size_t>(encoding.length) == tbs.size()) {
            cached = CertificateInfoEncoding(encoding);
        }
    }

    if (tbs.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
        NotImplemented_nid_no_patch("X509_sign: TBSCertificate exceeds INT64_MAX bytes");

    std::vector<unsigned char> signature;
    if (!SignBytes(tbs, key, digestMethod, signature)) return 0;
    if (signature.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        NotImplemented_nid_no_patch("X509_sign: signature exceeds INT_MAX bytes");

    ParameterPlan outerPlan;
    ParameterPlan innerPlan;
    unsigned char* reencoded = nullptr;
    unsigned char* sigData = nullptr;
    LibreSsl3Asn1String* sigStruct = nullptr;
    const auto release = [&] {
        free_nid_postfix(outerPlan.newParameter);
        free_nid_postfix(innerPlan.newParameter);
        free_nid_postfix(reencoded);
        free_nid_postfix(sigData);
        free_nid_postfix(sigStruct);
    };

    try {
        if (!PlanAlgorithmParameter(outerCopy, nullParameter, outerPlan)
            || !PlanAlgorithmParameter(innerCopy, nullParameter, innerPlan)) {
            release();
            return 0;
        }
        const bool needNewEncoding = !hasCached || tbs != cached;
        if (needNewEncoding) {
            reencoded = static_cast<unsigned char*>(malloc_nid_postfix(tbs.size()));
            if (!reencoded) {
                release();
                return 0;
            }
            std::memcpy(reencoded, tbs.data(), tbs.size());
        }
        sigData = static_cast<unsigned char*>(malloc_nid_postfix(signature.size()));
        if (!sigData) {
            release();
            return 0;
        }
        std::memcpy(sigData, signature.data(), signature.size());
        if (!view.signature) {
            sigStruct = static_cast<LibreSsl3Asn1String*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1String)));
            if (!sigStruct) {
                release();
                return 0;
            }
        }

        auto* const outer = const_cast<LibreSsl3X509Algor*>(fields.sigAlg);
        auto* const innerAlgor = static_cast<LibreSsl3X509Algor*>(info.signature);
        auto* const encodingField = reinterpret_cast<unsigned char*>(const_cast<LibreSsl3CertificateInfo*>(fields.certInfo))
            + offsetof(LibreSsl3CertificateInfo, encoding);
        auto* const certSigField = reinterpret_cast<unsigned char*>(certificate) + offsetof(LibreSsl3X509, signature);

        LibreSsl3Asn1String bits {};
        if (view.signature) {
            bits = CopyIn<LibreSsl3Asn1String>(view.signature, "X509_sign");
        }

        const auto requireWritable = [](void* address, std::size_t size) {
            PreservedErrno guard;
            if (!GuestWritable_nid_no_patch(address, size)) NotImplemented_nid_no_patch("X509_sign");
        };
        requireWritable(&outer->algorithm, sizeof(outer->algorithm));
        requireWritable(&outer->parameter, sizeof(outer->parameter));
        if (outerPlan.updateInPlace) requireWritable(outerPlan.oldParameter, sizeof(LibreSsl3Asn1Type));
        requireWritable(&innerAlgor->algorithm, sizeof(innerAlgor->algorithm));
        requireWritable(&innerAlgor->parameter, sizeof(innerAlgor->parameter));
        if (innerPlan.updateInPlace) requireWritable(innerPlan.oldParameter, sizeof(LibreSsl3Asn1Type));
        requireWritable(encodingField, sizeof(Encoding));
        if (view.signature) {
            requireWritable(view.signature, sizeof(LibreSsl3Asn1String));
        } else {
            requireWritable(certSigField, sizeof(void*));
        }

        WriteAlgorithmPlan(outer, object, outerPlan);
        WriteAlgorithmPlan(innerAlgor, object, innerPlan);
        const unsigned char* oldEncodingBytes = nullptr;
        if (reencoded) {
            CopyOut(encodingField, Encoding {reencoded, static_cast<std::int64_t>(tbs.size()), 0, encoding.padding},
                "X509_sign");
            reencoded = nullptr;
            oldEncodingBytes = encoding.bytes;
        }
        void* const previousSigData = bits.data;
        void* sigTarget = view.signature;
        if (!sigTarget) {
            CopyOut(certSigField, static_cast<void*>(sigStruct), "X509_sign");
            sigTarget = sigStruct;
            sigStruct = nullptr;
        }
        bits.data = sigData;
        bits.length = static_cast<int>(signature.size());
        bits.type = SignatureStringType;
        bits.flags = (bits.flags & ~BitsLeftMask) | BitsLeftFlag;
        CopyOut(sigTarget, bits, "X509_sign");
        sigData = nullptr;

        if (outerPlan.freeOld && outerPlan.oldParameter) {
            ASN1_TYPE_free_nid_postfix(reinterpret_cast<Asn1Type*>(outerPlan.oldParameter));
        }
        if (innerPlan.freeOld && innerPlan.oldParameter && innerPlan.oldParameter != outerPlan.oldParameter) {
            ASN1_TYPE_free_nid_postfix(reinterpret_cast<Asn1Type*>(innerPlan.oldParameter));
        }
        if (outerCopy.algorithm && outerCopy.algorithm != object) {
            ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(const_cast<void*>(outerCopy.algorithm)));
        }
        if (innerCopy.algorithm && innerCopy.algorithm != object && innerCopy.algorithm != outerCopy.algorithm) {
            ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(const_cast<void*>(innerCopy.algorithm)));
        }
        if (oldEncodingBytes) free_nid_postfix(const_cast<unsigned char*>(oldEncodingBytes));
        if (previousSigData) free_nid_postfix(previousSigData);
    } catch (...) {
        release();
        throw;
    }

    return static_cast<int>(signature.size());
}
