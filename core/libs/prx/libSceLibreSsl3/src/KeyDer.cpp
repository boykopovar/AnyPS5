/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a) src/lib/libcrypto/asn1/x_pubkey.c (x_pubkey.c,v 1.37),
 * src/lib/libcrypto/asn1/p8_pkey.c (p8_pkey.c,v 1.25),
 * src/lib/libcrypto/evp/evp_pkey.c (evp_pkey.c,v 1.32),
 * src/lib/libcrypto/asn1/a_pkey.c (a_pkey.c,v 1.8)
 * and src/lib/libcrypto/asn1/asn1_item.c (asn1_item.c,v 1.21) */
/* $OpenBSD: p8_pkey.c,v 1.25 2024/07/08 14:48:49 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: evp_pkey.c,v 1.32 2024/08/31 10:25:38 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: a_pkey.c,v 1.8 2024/04/09 13:52:41 beck Exp $ */
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

/* $OpenBSD: asn1_item.c,v 1.21 2024/04/09 13:55:02 beck Exp $ */
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
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

/* $OpenBSD: x_pubkey.c,v 1.37 2024/07/08 14:48:49 beck Exp $ */
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
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace {

constexpr int ErrLibRsa = 4;
constexpr int ErrLibEvp = 6;
constexpr int ErrLibX509 = 11;
constexpr int ErrLibAsn1 = 13;
constexpr int ErrLibEc = 16;
constexpr int AnyFunction = 0xfff;

constexpr int ErrRRsaLib = 4;
constexpr int ErrRNestedAsn1Error = 58;
constexpr int ErrRMallocFailure = 65;

constexpr int Asn1RBadObjectHeader = 102;
constexpr int Asn1RHeaderTooLong = 123;
constexpr int Asn1RNotEnoughData = 142;
constexpr int Asn1RSequenceLengthMismatch = 148;
constexpr int Asn1RSequenceNotConstructed = 149;
constexpr int Asn1RTooLong = 155;
constexpr int Asn1RUnsupportedPublicKeyType = 167;
constexpr int Asn1RWrongTag = 168;

constexpr int EvpRUnsupportedPrivateKeyAlgorithm = 118;
constexpr int EvpRPrivateKeyDecodeError = 145;
constexpr int EvpRPrivateKeyEncodeError = 146;
constexpr int EvpRUnsupportedAlgorithm = 156;

constexpr int X509RUnsupportedAlgorithm = 111;
constexpr int X509RPublicKeyDecodeError = 125;

constexpr int EcRDecodeError = 142;

constexpr int Asn1TypeNull = 5;
constexpr int Asn1TypeObject = 6;
constexpr int Asn1TypeOctetString = 4;
constexpr int Asn1ObjDynamic = 0x01;
constexpr int Asn1ObjDynamicData = 0x08;

constexpr int RsaType = 6;
constexpr int EcType = 408;
constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr int PointConversionUncompressed = 4;
constexpr int EcNamedCurveFlag = 1;

constexpr std::array<unsigned char, 9> RsaOid{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
constexpr std::array<unsigned char, 7> EcPublicKeyOid{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
constexpr std::array<unsigned char, 8> P256Oid{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
constexpr std::array<unsigned char, 5> P384Oid{0x2b, 0x81, 0x04, 0x00, 0x22};

struct Tlv {
    unsigned char tag;
    std::span<const unsigned char> content;
};

void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, AnyFunction, reason);
}

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value) {
    if (input.size() < 2) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return false;
    }
    value.tag = input[0];
    std::size_t offset = 2;
    std::size_t length = input[1];
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(std::size_t) || input.size() < 2 + count || input[2] == 0) {
            PutError(ErrLibAsn1, Asn1RBadObjectHeader);
            return false;
        }
        length = 0;
        for (std::size_t i = 0; i < count; ++i) {
            if (length > (std::numeric_limits<std::size_t>::max() >> 8)) {
                PutError(ErrLibAsn1, Asn1RBadObjectHeader);
                return false;
            }
            length = (length << 8) | input[2 + i];
        }
        if (length < 128) {
            PutError(ErrLibAsn1, Asn1RBadObjectHeader);
            return false;
        }
        offset += count;
    }
    if (length > input.size() - offset) {
        PutError(ErrLibAsn1, Asn1RTooLong);
        return false;
    }
    value.content = input.subspan(offset, length);
    input = input.subspan(offset + length);
    return true;
}

bool Next(std::span<const unsigned char>& input, unsigned char tag, Tlv& value) {
    if (!ReadTlv(input, value)) return false;
    if (value.tag != tag) {
        if (tag == 0x30 && (value.tag & 0x1f) == 0x10 && (value.tag & 0x20) == 0) {
            PutError(ErrLibAsn1, Asn1RSequenceNotConstructed);
        } else {
            PutError(ErrLibAsn1, Asn1RWrongTag);
        }
        return false;
    }
    return true;
}

bool ReadOuterSequence(std::span<const unsigned char>& input, std::span<const unsigned char>& content) {
    Tlv value{};
    if (!Next(input, 0x30, value)) return false;
    content = value.content;
    return true;
}

bool ReadExactSequence(std::span<const unsigned char> input, std::span<const unsigned char>& content) {
    Tlv value{};
    if (!Next(input, 0x30, value)) return false;
    if (!input.empty()) {
        PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
        return false;
    }
    content = value.content;
    return true;
}

bool Integer(std::span<const unsigned char>& input, std::span<const unsigned char>& content) {
    Tlv value{};
    if (!Next(input, 0x02, value)) return false;
    if (value.content.empty() || (value.content[0] & 0x80)) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return false;
    }
    content = value.content;
    if (content.size() > 1 && content[0] == 0 && (content[1] & 0x80) == 0) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return false;
    }
    while (content.size() > 1 && content[0] == 0) content = content.subspan(1);
    return true;
}

bool ValidateTlvSequence(std::span<const unsigned char> bytes) {
    while (!bytes.empty()) {
        Tlv item{};
        if (!ReadTlv(bytes, item)) return false;
    }
    return true;
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 128) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    std::array<unsigned char, sizeof(std::size_t)> bytes{};
    std::size_t count = 0;
    while (length) {
        bytes[bytes.size() - ++count] = static_cast<unsigned char>(length);
        length >>= 8;
    }
    output.push_back(static_cast<unsigned char>(0x80 | count));
    output.insert(output.end(), bytes.end() - static_cast<std::ptrdiff_t>(count), bytes.end());
}

std::vector<unsigned char> Wrap(unsigned char tag, std::span<const unsigned char> content) {
    std::vector<unsigned char> output;
    output.reserve(content.size() + 1 + sizeof(std::size_t) + 1);
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
    return output;
}

void Append(std::vector<unsigned char>& output, const std::vector<unsigned char>& value) {
    output.insert(output.end(), value.begin(), value.end());
}

std::vector<unsigned char> EncodeInteger(std::span<const unsigned char> value) {
    while (value.size() > 1 && value.front() == 0) value = value.subspan(1);
    std::vector<unsigned char> content(value.begin(), value.end());
    if (content.empty()) content.push_back(0);
    if (content.front() & 0x80) content.insert(content.begin(), 0);
    return Wrap(0x02, content);
}

std::vector<unsigned char> EncodeSmallInteger(unsigned char value) {
    return Wrap(0x02, std::span<const unsigned char>(&value, 1));
}

bool IsOid(std::span<const unsigned char> value, std::span<const unsigned char> oid) {
    return value.size() == oid.size() && std::equal(value.begin(), value.end(), oid.begin());
}

LibreSsl3Bignum* DecodeBn(std::span<const unsigned char> value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return nullptr;
    return reinterpret_cast<LibreSsl3Bignum*>(
        BN_bin2bn_nid_postfix(value.data(), static_cast<int>(value.size()), nullptr));
}

bool EncodeBn(const LibreSsl3Bignum* value, std::vector<unsigned char>& bytes) {
    if (!value) return false;
    const auto* bn = reinterpret_cast<const Bignum*>(value);
    const int length = BN_num_bits_nid_postfix(bn);
    if (length < 0) return false;
    bytes.resize(static_cast<std::size_t>((length + 7) / 8));
    if (!bytes.empty() && BN_bn2bin_nid_postfix(bn, bytes.data()) != static_cast<int>(bytes.size())) return false;
    return true;
}

LibreSsl3EvpPkey* AssignRsaToPkey(std::array<LibreSsl3Bignum*, 8>& components, bool privateKey) {
    auto* pkey = EVP_PKEY_new_nid_disambig1_nid_postfix();
    if (!pkey) {
        for (auto* component : components) BN_clear_free_nid_postfix(component);
        return nullptr;
    }
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    if (!rsa) {
        EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
        for (auto* component : components) BN_clear_free_nid_postfix(component);
        return nullptr;
    }
    if (!EVP_PKEY_assign_nid_postfix(pkey, LibreSsl3PkeyRsa, rsa)) {
        EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
        for (auto* component : components) BN_clear_free_nid_postfix(component);
        return nullptr;
    }
    pkey->method = FindNativePkeyAsn1Method_nid_no_patch(LibreSsl3PkeyRsa);
    if (!RSA_set0_key_nid_postfix(rsa, components[0], components[1], privateKey ? components[2] : nullptr)) {
        for (auto* component : components) BN_clear_free_nid_postfix(component);
        EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
        return nullptr;
    }
    components[0] = nullptr;
    components[1] = nullptr;
    if (privateKey) {
        components[2] = nullptr;
        if (!RSA_set0_factors_nid_postfix(rsa, components[3], components[4])) {
            for (std::size_t i = 3; i < 8; ++i) BN_clear_free_nid_postfix(components[i]);
            EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
            return nullptr;
        }
        components[3] = nullptr;
        components[4] = nullptr;
        if (!RSA_set0_crt_params_nid_postfix(rsa, components[5], components[6], components[7])) {
            for (std::size_t i = 5; i < 8; ++i) BN_clear_free_nid_postfix(components[i]);
            EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
            return nullptr;
        }
        components[5] = nullptr;
        components[6] = nullptr;
        components[7] = nullptr;
    }
    return pkey;
}

LibreSsl3EvpPkey* DecodeRsaPkey(std::span<const unsigned char>& input, bool privateKey, bool allowTrailing) {
    std::span<const unsigned char> body;
    if (allowTrailing) {
        if (!ReadOuterSequence(input, body)) return nullptr;
    } else {
        if (!ReadExactSequence(input, body)) return nullptr;
        input = {};
    }
    std::array<LibreSsl3Bignum*, 8> components{};
    if (privateKey) {
        std::span<const unsigned char> version;
        if (!Integer(body, version) || version.size() != 1 || version[0] != 0) {
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
    }
    const std::size_t count = privateKey ? 8 : 2;
    for (std::size_t i = 0; i < count; ++i) {
        std::span<const unsigned char> bytes;
        if (!Integer(body, bytes)) {
            for (auto* component : components) BN_clear_free_nid_postfix(component);
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
        components[i] = DecodeBn(bytes);
        if (!components[i]) {
            for (auto* component : components) BN_clear_free_nid_postfix(component);
            PutError(ErrLibEvp, ErrRMallocFailure);
            return nullptr;
        }
    }
    if (!body.empty()) {
        for (auto* component : components) BN_clear_free_nid_postfix(component);
        PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    return AssignRsaToPkey(components, privateKey);
}

bool EncodeRsa(const LibreSsl3Rsa* rsa, bool privateKey, std::vector<unsigned char>& out) {
    const void* n = nullptr;
    const void* e = nullptr;
    const void* d = nullptr;
    const void* p = nullptr;
    const void* q = nullptr;
    const void* dmp1 = nullptr;
    const void* dmq1 = nullptr;
    const void* iqmp = nullptr;
    RSA_get0_key_nid_postfix(rsa, &n, &e, &d);
    if (privateKey) {
        RSA_get0_factors_nid_postfix(rsa, &p, &q);
        RSA_get0_crt_params_nid_postfix(rsa, &dmp1, &dmq1, &iqmp);
    }
    std::vector<unsigned char> body;
    if (privateKey) Append(body, EncodeSmallInteger(0));
    std::array<const void*, 8> fields{n, e, d, p, q, dmp1, dmq1, iqmp};
    const std::size_t count = privateKey ? 8 : 2;
    for (std::size_t i = 0; i < count; ++i) {
        std::vector<unsigned char> bytes;
        if (!EncodeBn(static_cast<const LibreSsl3Bignum*>(fields[i]), bytes)) return false;
        Append(body, EncodeInteger(bytes));
    }
    out = Wrap(0x30, body);
    return true;
}

int CurveFromOid(std::span<const unsigned char> oid) {
    if (IsOid(oid, P256Oid)) return Secp256r1;
    if (IsOid(oid, P384Oid)) return Secp384r1;
    return 0;
}

bool ParseAlgorithm(std::span<const unsigned char> body, int& type, int& curve) {
    Tlv oid{};
    if (!Next(body, 0x06, oid)) {
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return false;
    }
    if (IsOid(oid.content, RsaOid)) {
        type = RsaType;
        curve = 0;
        if (!body.empty()) {
            Tlv nullParameter{};
            if (!Next(body, 0x05, nullParameter) || !nullParameter.content.empty()) {
                PutError(ErrLibAsn1, ErrRNestedAsn1Error);
                return false;
            }
        }
        if (!body.empty()) {
            PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return false;
        }
        return true;
    }
    if (IsOid(oid.content, EcPublicKeyOid)) {
        type = EcType;
        curve = 0;
        if (!body.empty()) {
            Tlv curveOid{};
            if (!Next(body, 0x06, curveOid)) {
                PutError(ErrLibEc, EcRDecodeError);
                return false;
            }
            if (!body.empty()) {
                PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
                PutError(ErrLibAsn1, ErrRNestedAsn1Error);
                return false;
            }
            curve = CurveFromOid(curveOid.content);
        }
        if (curve == 0) {
            PutError(ErrLibEc, EcRDecodeError);
            return false;
        }
        return true;
    }
    PutError(ErrLibEvp, EvpRUnsupportedAlgorithm);
    return false;
}

LibreSsl3EvpPkey* DecodeSpki(std::span<const unsigned char>& input) {
    std::span<const unsigned char> remaining = input;
    std::span<const unsigned char> body;
    Tlv algorithm{};
    Tlv bits{};
    if (!ReadOuterSequence(remaining, body)) return nullptr;
    if (!Next(body, 0x30, algorithm)) {
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    if (!Next(body, 0x03, bits) || !body.empty()) {
        if (!body.empty()) PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    if (bits.content.empty() || bits.content[0] != 0) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    int type = 0;
    int curve = 0;
    if (!ParseAlgorithm(algorithm.content, type, curve)) {
        if (type == 0) PutError(ErrLibX509, X509RUnsupportedAlgorithm);
        else PutError(ErrLibX509, X509RPublicKeyDecodeError);
        return nullptr;
    }
    auto keyBits = bits.content.subspan(1);
    if (type == RsaType) {
        auto* key = DecodeRsaPkey(keyBits, false, false);
        if (!key) {
            PutError(ErrLibRsa, ErrRRsaLib);
            PutError(ErrLibX509, X509RPublicKeyDecodeError);
            return nullptr;
        }
        input = remaining;
        return key;
    }
    if (type == EcType) {
        EcKey* ec = EC_KEY_new_by_curve_name_nid_postfix(curve);
        if (!ec) {
            PutError(ErrLibX509, X509RPublicKeyDecodeError);
            return nullptr;
        }
        EC_KEY_set_asn1_flag_nid_postfix(ec, EcNamedCurveFlag);
        const unsigned char* cursor = keyBits.data();
        if (!o2i_ECPublicKey_nid_postfix(&ec, &cursor, static_cast<std::int64_t>(keyBits.size()))
            || cursor != keyBits.data() + keyBits.size()) {
            EC_KEY_free_nid_postfix(ec);
            PutError(ErrLibEc, EcRDecodeError);
            PutError(ErrLibX509, X509RPublicKeyDecodeError);
            return nullptr;
        }
        auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
        if (!key || !EVP_PKEY_assign_nid_postfix(key, EcType, ec)) {
            if (key) EVP_PKEY_free_nid_disambig1_nid_postfix(key);
            EC_KEY_free_nid_postfix(ec);
            return nullptr;
        }
        input = remaining;
        return key;
    }
    return nullptr;
}

EcKey* DecodeEcPrivateKey(std::span<const unsigned char>& input, int expectedCurve, bool allowTrailing) {
    std::span<const unsigned char> remaining = input;
    std::span<const unsigned char> body;
    if (allowTrailing) {
        if (!ReadOuterSequence(remaining, body)) return nullptr;
    } else {
        if (!ReadExactSequence(remaining, body)) return nullptr;
        remaining = {};
    }
    std::span<const unsigned char> version;
    Tlv privOctet{};
    if (!Integer(body, version) || version.size() != 1 || version[0] != 1
        || !Next(body, 0x04, privOctet) || privOctet.content.empty()) {
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }

    int curve = expectedCurve;
    if (!body.empty() && body[0] == 0xa0) {
        Tlv paramsTlv{};
        Tlv curveOid{};
        if (!Next(body, 0xa0, paramsTlv)) {
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
        std::span<const unsigned char> paramsContent = paramsTlv.content;
        if (!Next(paramsContent, 0x06, curveOid) || !paramsContent.empty()) {
            PutError(ErrLibEc, EcRDecodeError);
            return nullptr;
        }
        const int paramCurve = CurveFromOid(curveOid.content);
        if (paramCurve == 0 || (curve != 0 && paramCurve != curve)) {
            PutError(ErrLibEc, EcRDecodeError);
            return nullptr;
        }
        curve = paramCurve;
    }
    if (curve == 0) {
        PutError(ErrLibEc, EcRDecodeError);
        return nullptr;
    }

    std::span<const unsigned char> pubBits{};
    if (!body.empty() && body[0] == 0xa1) {
        Tlv pubTlv{};
        Tlv bitsTlv{};
        if (!Next(body, 0xa1, pubTlv)) {
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
        std::span<const unsigned char> pubContent = pubTlv.content;
        if (!Next(pubContent, 0x03, bitsTlv) || !pubContent.empty()
            || bitsTlv.content.empty() || bitsTlv.content[0] != 0) {
            PutError(ErrLibEc, EcRDecodeError);
            return nullptr;
        }
        pubBits = bitsTlv.content.subspan(1);
    }
    if (!body.empty()) {
        PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }

    EcKey* ec = EC_KEY_new_by_curve_name_nid_postfix(curve);
    if (!ec) return nullptr;
    EC_KEY_set_asn1_flag_nid_postfix(ec, EcNamedCurveFlag);
    if (!EC_KEY_generate_key_nid_postfix(ec)) {
        EC_KEY_free_nid_postfix(ec);
        return nullptr;
    }
    if (mbedtls_mpi_read_binary(&ec->privateKey->value, privOctet.content.data(), privOctet.content.size()) != 0
        || mbedtls_mpi_cmp_int(&ec->privateKey->value, 0) <= 0
        || mbedtls_mpi_cmp_mpi(&ec->privateKey->value, &ec->group->group.N) >= 0) {
        EC_KEY_free_nid_postfix(ec);
        PutError(ErrLibEc, EcRDecodeError);
        return nullptr;
    }
    if (!pubBits.empty()) {
        const unsigned char* cursor = pubBits.data();
        if (!o2i_ECPublicKey_nid_postfix(&ec, &cursor, static_cast<std::int64_t>(pubBits.size()))
            || cursor != pubBits.data() + pubBits.size()) {
            EC_KEY_free_nid_postfix(ec);
            PutError(ErrLibEc, EcRDecodeError);
            return nullptr;
        }
    } else {
        if (privOctet.content.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            EC_KEY_free_nid_postfix(ec);
            PutError(ErrLibEc, EcRDecodeError);
            return nullptr;
        }
        Bignum* privBn = BN_bin2bn_nid_postfix(
            privOctet.content.data(), static_cast<int>(privOctet.content.size()), nullptr);
        if (!privBn) {
            EC_KEY_free_nid_postfix(ec);
            return nullptr;
        }
        const int mulOk = EC_POINT_mul_nid_postfix(ec->group, ec->publicKey, privBn, nullptr, nullptr, nullptr);
        BN_clear_free_nid_postfix(reinterpret_cast<LibreSsl3Bignum*>(privBn));
        if (!mulOk) {
            EC_KEY_free_nid_postfix(ec);
            PutError(ErrLibEc, EcRDecodeError);
            return nullptr;
        }
    }
    if (!EC_KEY_check_key_nid_postfix(ec)) {
        EC_KEY_free_nid_postfix(ec);
        PutError(ErrLibEc, EcRDecodeError);
        return nullptr;
    }
    input = remaining;
    return ec;
}

bool EncodeEcPrivateKey(const EcKey* ec, int& curveOut, std::vector<unsigned char>& out) {
    if (!ec || !ec->group || !ec->privateKey) return false;
    curveOut = ec->group->curve.nid;
    if (curveOut != Secp256r1 && curveOut != Secp384r1) return false;
    const std::size_t orderBytes = (mbedtls_mpi_bitlen(&ec->group->group.N) + 7) / 8;
    std::vector<unsigned char> privBytes(orderBytes);
    if (mbedtls_mpi_write_binary(&ec->privateKey->value, privBytes.data(), privBytes.size()) != 0) return false;

    std::vector<unsigned char> body;
    Append(body, EncodeSmallInteger(1));
    Append(body, Wrap(0x04, privBytes));

    if (ec->publicKey) {
        const std::size_t pubLen = EC_POINT_point2oct_nid_postfix(
            ec->group, ec->publicKey, PointConversionUncompressed, nullptr, 0, nullptr);
        if (pubLen == 0) return false;
        std::vector<unsigned char> bitString(1 + pubLen);
        bitString[0] = 0;
        if (EC_POINT_point2oct_nid_postfix(
                ec->group, ec->publicKey, PointConversionUncompressed, bitString.data() + 1, pubLen, nullptr)
            != pubLen)
            return false;
        Append(body, Wrap(0xa1, Wrap(0x03, bitString)));
    }
    out = Wrap(0x30, body);
    return true;
}

Asn1Object_nid_no_patch* CreateOidObject(std::span<const unsigned char> oidBytes, int nid) {
    if (oidBytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return nullptr;
    auto* obj = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!obj) {
        PutError(ErrLibAsn1, ErrRMallocFailure);
        return nullptr;
    }
    if (!oidBytes.empty()) {
        auto* data = static_cast<unsigned char*>(malloc_nid_postfix(oidBytes.size()));
        if (!data) {
            free_nid_postfix(obj);
            PutError(ErrLibAsn1, ErrRMallocFailure);
            return nullptr;
        }
        std::memcpy(data, oidBytes.data(), oidBytes.size());
        obj->data = data;
    }
    obj->nid = nid;
    obj->length = static_cast<int>(oidBytes.size());
    obj->flags = Asn1ObjDynamic | Asn1ObjDynamicData;
    return obj;
}

void FreeAlgor(LibreSsl3X509Algor* algor) {
    if (!algor) return;
    ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(algor->algorithm));
    ASN1_TYPE_free_nid_postfix(reinterpret_cast<Asn1Type*>(algor->parameter));
    free_nid_postfix(algor);
}

void FreePkcs8(LibreSsl3Pkcs8PrivKeyInfo* info) {
    if (!info) return;
    ASN1_INTEGER_free_nid_postfix(info->version);
    FreeAlgor(info->algorithm);
    ASN1_OCTET_STRING_free_nid_postfix(info->privateKey);
    ASN1_OCTET_STRING_free_nid_postfix(static_cast<Asn1String*>(info->attributes));
    auto* bytes = reinterpret_cast<volatile unsigned char*>(info);
    for (std::size_t i = 0; i < sizeof(*info); ++i) bytes[i] = 0;
    free_nid_postfix(info);
}

LibreSsl3X509Algor* BuildAlgor(
    std::span<const unsigned char> oidBytes, int oidNid,
    bool hasNullParam, std::span<const unsigned char> paramOidBytes, int paramNid) {
    auto* algor = static_cast<LibreSsl3X509Algor*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Algor)));
    if (!algor) {
        PutError(ErrLibAsn1, ErrRMallocFailure);
        return nullptr;
    }
    algor->algorithm = CreateOidObject(oidBytes, oidNid);
    if (!algor->algorithm) {
        free_nid_postfix(algor);
        return nullptr;
    }
    if (hasNullParam || !paramOidBytes.empty()) {
        algor->parameter = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
        if (!algor->parameter) {
            FreeAlgor(algor);
            PutError(ErrLibAsn1, ErrRMallocFailure);
            return nullptr;
        }
        if (hasNullParam) {
            algor->parameter->type = Asn1TypeNull;
        } else {
            algor->parameter->type = Asn1TypeObject;
            algor->parameter->value = CreateOidObject(paramOidBytes, paramNid);
            if (!algor->parameter->value) {
                FreeAlgor(algor);
                return nullptr;
            }
        }
    }
    return algor;
}

LibreSsl3Pkcs8PrivKeyInfo* NewPkcs8(
    LibreSsl3X509Algor* algor, std::span<const unsigned char> key,
    std::span<const unsigned char> attributesContent) {
    if (key.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || attributesContent.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        FreeAlgor(algor);
        return nullptr;
    }
    auto* info = static_cast<LibreSsl3Pkcs8PrivKeyInfo*>(
        calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs8PrivKeyInfo)));
    if (!info) {
        FreeAlgor(algor);
        PutError(ErrLibAsn1, ErrRMallocFailure);
        return nullptr;
    }
    info->algorithm = algor;
    info->version = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    info->privateKey = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!info->version || !info->privateKey
        || !ASN1_INTEGER_set_nid_postfix(info->version, 0)) {
        FreePkcs8(info);
        PutError(ErrLibAsn1, ErrRMallocFailure);
        return nullptr;
    }
    info->privateKey->type = Asn1TypeOctetString;
    if (!ASN1_STRING_set_nid_postfix(info->privateKey, key.data(), static_cast<int>(key.size()))) {
        FreePkcs8(info);
        return nullptr;
    }
    if (!attributesContent.empty()) {
        auto* attrs = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
        if (!attrs) {
            FreePkcs8(info);
            PutError(ErrLibAsn1, ErrRMallocFailure);
            return nullptr;
        }
        attrs->type = 0xa0;
        info->attributes = attrs;
        if (!ASN1_STRING_set_nid_postfix(
                attrs, attributesContent.data(), static_cast<int>(attributesContent.size()))) {
            FreePkcs8(info);
            return nullptr;
        }
    }
    return info;
}

LibreSsl3X509Algor* DecodePkcs8Algor(std::span<const unsigned char> body) {
    Tlv oid{};
    if (!Next(body, 0x06, oid)) {
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    int oidNid = 0;
    if (IsOid(oid.content, RsaOid)) oidNid = RsaType;
    else if (IsOid(oid.content, EcPublicKeyOid)) oidNid = EcType;

    bool hasNullParam = false;
    std::span<const unsigned char> paramOidBytes{};
    int paramNid = 0;
    if (!body.empty()) {
        Tlv param{};
        if (!ReadTlv(body, param)) {
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
        if (!body.empty()) {
            PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
        if (param.tag == 0x05 && param.content.empty()) {
            hasNullParam = true;
        } else if (param.tag == 0x06) {
            paramOidBytes = param.content;
            paramNid = CurveFromOid(paramOidBytes);
        } else {
            PutError(ErrLibAsn1, Asn1RWrongTag);
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
    }
    return BuildAlgor(oid.content, oidNid, hasNullParam, paramOidBytes, paramNid);
}

LibreSsl3Pkcs8PrivKeyInfo* DecodePkcs8(std::span<const unsigned char>& input) {
    std::span<const unsigned char> remaining = input;
    std::span<const unsigned char> body;
    std::span<const unsigned char> version;
    Tlv algorithm{};
    Tlv privateKey{};
    if (!ReadOuterSequence(remaining, body)) return nullptr;
    if (!Integer(body, version) || version.size() != 1 || version[0] != 0
        || !Next(body, 0x30, algorithm) || !Next(body, 0x04, privateKey)) {
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    std::span<const unsigned char> attrsContent{};
    if (!body.empty() && body[0] == 0xa0) {
        Tlv attributes{};
        if (!Next(body, 0xa0, attributes) || !ValidateTlvSequence(attributes.content)) {
            PutError(ErrLibAsn1, ErrRNestedAsn1Error);
            return nullptr;
        }
        attrsContent = attributes.content;
    }
    if (!body.empty()) {
        PutError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    auto* algor = DecodePkcs8Algor(algorithm.content);
    if (!algor) {
        PutError(ErrLibAsn1, ErrRNestedAsn1Error);
        return nullptr;
    }
    auto* info = NewPkcs8(algor, privateKey.content, attrsContent);
    if (!info) return nullptr;
    input = remaining;
    return info;
}

std::vector<unsigned char> EncodePkcs8(const LibreSsl3Pkcs8PrivKeyInfo* info) {
    if (!info || !info->version || !info->algorithm || !info->algorithm->algorithm || !info->privateKey) return {};
    const auto* oidObj = static_cast<const Asn1Object_nid_no_patch*>(info->algorithm->algorithm);
    if (!oidObj->data || oidObj->length <= 0) return {};
    if (info->privateKey->length < 0 || (!info->privateKey->data && info->privateKey->length > 0)) return {};

    std::vector<unsigned char> body;
    std::span<const unsigned char> verBytes(
        info->version->data, info->version->length > 0 ? static_cast<std::size_t>(info->version->length) : 0);
    Append(body, EncodeInteger(verBytes));

    std::vector<unsigned char> algorithm;
    Append(algorithm, Wrap(0x06, std::span<const unsigned char>(
        oidObj->data, static_cast<std::size_t>(oidObj->length))));
    if (info->algorithm->parameter) {
        if (info->algorithm->parameter->type == Asn1TypeNull) {
            Append(algorithm, Wrap(0x05, {}));
        } else if (info->algorithm->parameter->type == Asn1TypeObject && info->algorithm->parameter->value) {
            const auto* paramObj = static_cast<const Asn1Object_nid_no_patch*>(info->algorithm->parameter->value);
            if (!paramObj->data || paramObj->length <= 0) return {};
            Append(algorithm, Wrap(0x06, std::span<const unsigned char>(
                paramObj->data, static_cast<std::size_t>(paramObj->length))));
        } else {
            return {};
        }
    }
    Append(body, Wrap(0x30, algorithm));
    Append(body, Wrap(0x04, std::span<const unsigned char>(
        info->privateKey->data, static_cast<std::size_t>(info->privateKey->length))));
    if (info->attributes) {
        const auto* attrs = static_cast<const Asn1String*>(info->attributes);
        if (attrs->length < 0 || (!attrs->data && attrs->length > 0)) return {};
        Append(body, Wrap(0xa0, std::span<const unsigned char>(
            attrs->data, static_cast<std::size_t>(attrs->length))));
    }
    return Wrap(0x30, body);
}

bool ReadExactBytesFromBio(LibreSsl::Bio* bio, unsigned char* dst, std::size_t count) {
    std::size_t total = 0;
    while (total < count) {
        const int want = static_cast<int>(std::min<std::size_t>(
            count - total, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const int got = bio->method->read(bio, reinterpret_cast<char*>(dst + total), want);
        if (got <= 0) return false;
        total += static_cast<std::size_t>(got);
    }
    return true;
}

bool ReadSingleDerObjectFromBio(LibreSsl::Bio* bio, std::vector<unsigned char>& result) {
    ERR_clear_error_nid_postfix();
    result.clear();
    if (!bio || !bio->method || !bio->method->read) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return false;
    }
    unsigned char tag = 0;
    const int tagRead = bio->method->read(bio, reinterpret_cast<char*>(&tag), 1);
    if (tagRead == 0) {
        PutError(ErrLibAsn1, Asn1RHeaderTooLong);
        return false;
    }
    if (tagRead < 0) {
        PutError(ErrLibAsn1, Asn1RNotEnoughData);
        return false;
    }
    unsigned char lenByte = 0;
    if (bio->method->read(bio, reinterpret_cast<char*>(&lenByte), 1) <= 0) {
        PutError(ErrLibAsn1, Asn1RNotEnoughData);
        return false;
    }
    result.push_back(tag);
    result.push_back(lenByte);

    std::size_t contentLength = lenByte;
    if (lenByte & 0x80) {
        const std::size_t count = lenByte & 0x7f;
        if (count == 0) {
            PutError(ErrLibAsn1, Asn1RBadObjectHeader);
            return false;
        }
        if (count > sizeof(std::size_t)) {
            PutError(ErrLibAsn1, Asn1RHeaderTooLong);
            return false;
        }
        std::array<unsigned char, sizeof(std::size_t)> lenBytes{};
        if (!ReadExactBytesFromBio(bio, lenBytes.data(), count)) {
            PutError(ErrLibAsn1, Asn1RNotEnoughData);
            return false;
        }
        result.insert(result.end(), lenBytes.begin(), lenBytes.begin() + static_cast<std::ptrdiff_t>(count));
        if (lenBytes[0] == 0) {
            PutError(ErrLibAsn1, Asn1RBadObjectHeader);
            return false;
        }
        contentLength = 0;
        for (std::size_t i = 0; i < count; ++i) {
            if (contentLength > (std::numeric_limits<std::size_t>::max() >> 8)) {
                PutError(ErrLibAsn1, Asn1RHeaderTooLong);
                return false;
            }
            contentLength = (contentLength << 8) | lenBytes[i];
        }
        if (contentLength < 128) {
            PutError(ErrLibAsn1, Asn1RBadObjectHeader);
            return false;
        }
    }
    const std::size_t headerSize = result.size();
    if (contentLength > static_cast<std::size_t>(std::numeric_limits<int>::max()) - headerSize) {
        PutError(ErrLibAsn1, Asn1RTooLong);
        return false;
    }
    result.resize(headerSize + contentLength);
    if (!ReadExactBytesFromBio(bio, result.data() + headerSize, contentLength)) {
        PutError(ErrLibAsn1, Asn1RNotEnoughData);
        return false;
    }
    return true;
}

template<typename T> void ReplaceOutput(T** output, T* decoded, void (*freeValue)(T*)) {
    if (!output) return;
    if (*output) freeValue(*output);
    *output = decoded;
}

}

extern "C" {

LibreSsl3EvpPkey* APS5_VABI d2i_PUBKEY_nid_postfix(
    LibreSsl3EvpPkey** output, const unsigned char** input, std::int64_t length) {
    if (!input || !*input || length < 0) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return nullptr;
    }
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    auto* key = DecodeSpki(remaining);
    if (!key) return nullptr;
    ReplaceOutput(output, key, EVP_PKEY_free_nid_disambig1_nid_postfix);
    *input += static_cast<std::size_t>(length) - remaining.size();
    return key;
}

LibreSsl3Pkcs8PrivKeyInfo* APS5_VABI d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
    LibreSsl3Pkcs8PrivKeyInfo** output, const unsigned char** input, std::int64_t length) {
    if (!input || !*input || length < 0) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return nullptr;
    }
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    auto* info = DecodePkcs8(remaining);
    if (!info) return nullptr;
    ReplaceOutput(output, info, FreePkcs8);
    *input += static_cast<std::size_t>(length) - remaining.size();
    return info;
}

LibreSsl3Pkcs8PrivKeyInfo* APS5_VABI d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(
    LibreSsl::Bio* bio, LibreSsl3Pkcs8PrivKeyInfo** output) {
    std::vector<unsigned char> bytes;
    if (!ReadSingleDerObjectFromBio(bio, bytes)) return nullptr;
    const unsigned char* input = bytes.data();
    return d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(output, &input, static_cast<std::int64_t>(bytes.size()));
}

LibreSsl3EvpPkey* APS5_VABI d2i_PrivateKey_bio_nid_postfix(LibreSsl::Bio* bio, LibreSsl3EvpPkey** output) {
    std::vector<unsigned char> bytes;
    if (!ReadSingleDerObjectFromBio(bio, bytes)) return nullptr;

    std::span<const unsigned char> seqSpan(bytes);
    std::span<const unsigned char> seqBody;
    std::size_t elementCount = 0;
    if (ReadOuterSequence(seqSpan, seqBody)) {
        std::span<const unsigned char> counter = seqBody;
        while (!counter.empty()) {
            Tlv item{};
            if (!ReadTlv(counter, item)) {
                elementCount = 0;
                break;
            }
            ++elementCount;
        }
    }
    ERR_clear_error_nid_postfix();

    auto tryPkcs8 = [&](bool putUnsupportedOnDecodeFail) -> LibreSsl3EvpPkey* {
        std::span<const unsigned char> pkcs8Span(bytes);
        auto* info = DecodePkcs8(pkcs8Span);
        if (!info) {
            if (putUnsupportedOnDecodeFail) PutError(ErrLibAsn1, Asn1RUnsupportedPublicKeyType);
            return nullptr;
        }
        auto* key = EVP_PKCS82PKEY_nid_postfix(info);
        FreePkcs8(info);
        if (!key) return nullptr;
        ERR_clear_error_nid_postfix();
        ReplaceOutput(output, key, EVP_PKEY_free_nid_disambig1_nid_postfix);
        return key;
    };

    if (elementCount == 3) return tryPkcs8(true);

    if (elementCount == 4) {
        std::span<const unsigned char> ecSpan(bytes);
        if (auto* ec = DecodeEcPrivateKey(ecSpan, 0, true)) {
            auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
            if (!key || !EVP_PKEY_assign_nid_postfix(key, EcType, ec)) {
                if (key) EVP_PKEY_free_nid_disambig1_nid_postfix(key);
                EC_KEY_free_nid_postfix(ec);
                return nullptr;
            }
            ReplaceOutput(output, key, EVP_PKEY_free_nid_disambig1_nid_postfix);
            return key;
        }
        return tryPkcs8(false);
    }

    std::span<const unsigned char> rsaSpan(bytes);
    if (auto* key = DecodeRsaPkey(rsaSpan, true, true)) {
        ReplaceOutput(output, key, EVP_PKEY_free_nid_disambig1_nid_postfix);
        return key;
    }
    PutError(ErrLibRsa, ErrRRsaLib);
    return tryPkcs8(false);
}

int APS5_VABI i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(
    const LibreSsl3Pkcs8PrivKeyInfo* info, unsigned char** output) {
    if (!info) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return -1;
    }
    const auto encoded = EncodePkcs8(info);
    if (encoded.empty() || encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        PutError(ErrLibAsn1, Asn1RBadObjectHeader);
        return -1;
    }
    if (!output) return static_cast<int>(encoded.size());
    const bool advanceOutput = (*output != nullptr);
    if (!*output) {
        *output = static_cast<unsigned char*>(malloc_nid_postfix(encoded.size()));
        if (!*output) {
            PutError(ErrLibAsn1, ErrRMallocFailure);
            return -1;
        }
    }
    std::memcpy(*output, encoded.data(), encoded.size());
    if (advanceOutput) *output += encoded.size();
    return static_cast<int>(encoded.size());
}

void APS5_VABI PKCS8_PRIV_KEY_INFO_free_nid_postfix(LibreSsl3Pkcs8PrivKeyInfo* info) {
    FreePkcs8(info);
}

LibreSsl3EvpPkey* APS5_VABI EVP_PKCS82PKEY_nid_postfix(const LibreSsl3Pkcs8PrivKeyInfo* info) {
    if (!info || !info->algorithm || !info->algorithm->algorithm || !info->privateKey
        || info->privateKey->length < 0 || (!info->privateKey->data && info->privateKey->length > 0)) {
        PutError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
        return nullptr;
    }
    const auto* oidObj = static_cast<const Asn1Object_nid_no_patch*>(info->algorithm->algorithm);
    if (!oidObj->data || oidObj->length <= 0) {
        PutError(ErrLibEvp, EvpRUnsupportedAlgorithm);
        PutError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
        return nullptr;
    }
    std::span<const unsigned char> oidSpan(oidObj->data, static_cast<std::size_t>(oidObj->length));
    std::span<const unsigned char> keyBytes(
        info->privateKey->data, static_cast<std::size_t>(info->privateKey->length));

    if (IsOid(oidSpan, RsaOid)) {
        if (info->algorithm->parameter && info->algorithm->parameter->type != Asn1TypeNull) {
            PutError(ErrLibRsa, ErrRRsaLib);
            PutError(ErrLibEvp, EvpRPrivateKeyDecodeError);
            return nullptr;
        }
        auto* key = DecodeRsaPkey(keyBytes, true, false);
        if (!key) {
            PutError(ErrLibRsa, ErrRRsaLib);
            PutError(ErrLibEvp, EvpRPrivateKeyDecodeError);
            return nullptr;
        }
        return key;
    }
    if (IsOid(oidSpan, EcPublicKeyOid)) {
        int curve = 0;
        if (info->algorithm->parameter && info->algorithm->parameter->type == Asn1TypeObject
            && info->algorithm->parameter->value) {
            const auto* curveObj = static_cast<const Asn1Object_nid_no_patch*>(info->algorithm->parameter->value);
            if (curveObj->data && curveObj->length > 0) {
                curve = CurveFromOid(std::span<const unsigned char>(
                    curveObj->data, static_cast<std::size_t>(curveObj->length)));
            }
        }
        if (curve == 0) {
            PutError(ErrLibEc, EcRDecodeError);
            PutError(ErrLibEvp, EvpRPrivateKeyDecodeError);
            return nullptr;
        }
        auto* ec = DecodeEcPrivateKey(keyBytes, curve, false);
        if (!ec) {
            PutError(ErrLibEc, EcRDecodeError);
            PutError(ErrLibEvp, EvpRPrivateKeyDecodeError);
            return nullptr;
        }
        auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
        if (!key || !EVP_PKEY_assign_nid_postfix(key, EcType, ec)) {
            if (key) EVP_PKEY_free_nid_disambig1_nid_postfix(key);
            EC_KEY_free_nid_postfix(ec);
            return nullptr;
        }
        return key;
    }
    PutError(ErrLibEvp, EvpRUnsupportedAlgorithm);
    PutError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
    return nullptr;
}

LibreSsl3Pkcs8PrivKeyInfo* APS5_VABI EVP_PKEY2PKCS8_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key || !key->key || (!key->method && key->type != EcType)) {
        PutError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
        return nullptr;
    }
    if (key->type == RsaType) {
        std::vector<unsigned char> encoded;
        if (!EncodeRsa(static_cast<const LibreSsl3Rsa*>(key->key), true, encoded)) {
            PutError(ErrLibEvp, EvpRPrivateKeyEncodeError);
            return nullptr;
        }
        auto* algor = BuildAlgor(RsaOid, RsaType, true, {}, 0);
        if (!algor) return nullptr;
        return NewPkcs8(algor, encoded, {});
    }
    if (key->type == EcType) {
        int curve = 0;
        std::vector<unsigned char> encoded;
        if (!EncodeEcPrivateKey(static_cast<const EcKey*>(key->key), curve, encoded)) {
            PutError(ErrLibEvp, EvpRPrivateKeyEncodeError);
            return nullptr;
        }
        std::span<const unsigned char> curveOid = curve == Secp256r1 ? std::span<const unsigned char>(P256Oid)
            : std::span<const unsigned char>(P384Oid);
        auto* algor = BuildAlgor(EcPublicKeyOid, EcType, false, curveOid, curve);
        if (!algor) return nullptr;
        return NewPkcs8(algor, encoded, {});
    }
    PutError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
    return nullptr;
}

}
