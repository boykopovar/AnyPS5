/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/dsa/dsa_ameth.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/dsa/dsa_asn1.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/dsa/dsa_lib.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_long.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_bignum.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_int.c */
/* $OpenBSD: dsa_ameth.c,v 1.59 2024/04/13 14:02:51 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: dsa_asn1.c,v 1.33 2024/07/08 17:11:05 beck Exp $ */
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
/* $OpenBSD: dsa_lib.c,v 1.48 2024/03/27 01:49:31 tb Exp $ */
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

/* Original version from Steven Schoch <schoch@sheba.arc.nasa.gov> */
/* $OpenBSD: x_long.c,v 1.21 2024/07/08 16:24:22 beck Exp $ */
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

#include "prx/libSceLibreSsl3/include/DsaKeys.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libSceLibreSsl3/src/Asn1Reader.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include <mbedtls/bignum.h>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace LibreSsl3Asn1;

namespace {

constexpr int DsaLibrary = 10;

constexpr int ErrRDsaLib = 10;
constexpr int ErrRMallocFailure = 65;

constexpr int DsaMissingParameters = 101;
constexpr int DsaBadQValue = 102;
constexpr int DsaModulusTooLarge = 103;
constexpr int DsaDecodeError = 104;
constexpr int DsaParameterEncodingError = 105;
constexpr int DsaBnDecodeError = 108;
constexpr int DsaInvalidParameters = 112;

constexpr int Asn1BnLib = 105;
constexpr int Asn1IntegerTooLargeForLong = 128;

constexpr std::int64_t LongUndefined = 0x7fffffff;
constexpr std::size_t DsaMaximumModulusBits = 10000;
constexpr std::size_t DsaMinimumModulusBits = 512;
constexpr std::int32_t DsaFlagCacheMontP = 0x01;
constexpr int PrimalityRounds = 64;

struct DsaObject {
    std::int32_t pad;
    std::int64_t version;
    LibreSsl3Bignum* p;
    LibreSsl3Bignum* q;
    LibreSsl3Bignum* g;
    LibreSsl3Bignum* pubKey;
    LibreSsl3Bignum* privKey;
    LibreSsl3Bignum* kinv;
    LibreSsl3Bignum* r;
    std::int32_t flags;
    void* methodMontP;
    std::int32_t references;
    void* extraData;
    const void* method;
};

static_assert(sizeof(DsaObject) == sizeof(LibreSsl3Dsa));
static_assert(sizeof(DsaObject) == 112);
static_assert(offsetof(DsaObject, version) == offsetof(LibreSsl3Dsa, version));
static_assert(offsetof(DsaObject, p) == offsetof(LibreSsl3Dsa, p));
static_assert(offsetof(DsaObject, q) == offsetof(LibreSsl3Dsa, q));
static_assert(offsetof(DsaObject, g) == offsetof(LibreSsl3Dsa, g));
static_assert(offsetof(DsaObject, pubKey) == offsetof(LibreSsl3Dsa, pubKey));
static_assert(offsetof(DsaObject, privKey) == offsetof(LibreSsl3Dsa, privKey));
static_assert(offsetof(DsaObject, kinv) == 56);
static_assert(offsetof(DsaObject, r) == 64);
static_assert(offsetof(DsaObject, flags) == 72);
static_assert(offsetof(DsaObject, methodMontP) == 80);
static_assert(offsetof(DsaObject, references) == 88);
static_assert(offsetof(DsaObject, extraData) == 96);
static_assert(offsetof(DsaObject, method) == 104);

constexpr std::size_t PrimeIndex = 0;
constexpr std::size_t SubprimeIndex = 1;
constexpr std::size_t GeneratorIndex = 2;
constexpr std::size_t PublicIndex = 3;
constexpr std::size_t PrivateIndex = 4;

struct Numbers {
    std::array<LibreSsl3Bignum*, 5> values{};

    Numbers() = default;
    Numbers(const Numbers&) = delete;
    Numbers& operator=(const Numbers&) = delete;

    ~Numbers() {
        for (LibreSsl3Bignum* value : values) BN_clear_free_nid_postfix(value);
    }
};

struct Mpi {
    mbedtls_mpi value;

    Mpi() { mbedtls_mpi_init(&value); }
    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;
    ~Mpi() { mbedtls_mpi_free(&value); }
};

void RequireMbedtls(int status) {
    if (status != 0) throw std::runtime_error("DSA: mbedtls bignum operation failed");
}

void Load(Mpi& out, const LibreSsl3Bignum* number) {
    if (number->top < 0 || number->top > std::numeric_limits<int>::max() / 64
        || (number->top != 0 && number->data == nullptr))
        throw std::invalid_argument("DSA: malformed BIGNUM");
    if (number->top == 0) {
        RequireMbedtls(mbedtls_mpi_lset(&out.value, 0));
        return;
    }
    RequireMbedtls(mbedtls_mpi_read_binary_le(&out.value, reinterpret_cast<const unsigned char*>(number->data),
        static_cast<std::size_t>(number->top) * sizeof(std::uint64_t)));
    if (number->negative != 0 && mbedtls_mpi_cmp_int(&out.value, 0) != 0) {
        Mpi zero;
        RequireMbedtls(mbedtls_mpi_lset(&zero.value, 0));
        RequireMbedtls(mbedtls_mpi_sub_mpi(&out.value, &zero.value, &out.value));
    }
}

bool IsOdd(const Mpi& number) {
    return mbedtls_mpi_get_bit(&number.value, 0) != 0;
}

int EntropySource(void*, unsigned char* output, std::size_t length) {
    HostEntropy_nid_no_patch(output, length);
    return 0;
}

bool IsPrime(const Mpi& candidate) {
    const int status = mbedtls_mpi_is_prime_ext(&candidate.value, PrimalityRounds, EntropySource, nullptr);
    if (status == 0) return true;
    if (status == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) return false;
    throw std::runtime_error("DSA: mbedtls primality test failed");
}

LibreSsl3Bignum* NewBignum(const Integer& integer) {
    LibreSsl3Bignum* number =
        LibreSsl3::NewRsaBignum_nid_no_patch(integer.magnitude.data(), integer.magnitude.size());
    if (number != nullptr) number->negative = integer.negative && number->top > 0 ? 1 : 0;
    return number;
}

bool ReadLong(Reader& reader, std::int64_t& value) {
    Reader probe = reader;
    Header header;
    if (CheckTag(probe, header, TypeInteger, false) != 1) {
        PutError(Asn1Library, Asn1NestedError);
        PutError(Asn1Library, Asn1NestedError);
        return false;
    }
    if (header.constructed) {
        PutError(Asn1Library, Asn1TypeNotPrimitive);
        PutError(Asn1Library, Asn1NestedError);
        return false;
    }
    value = 0;
    bool valid = header.length <= static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (valid && header.length != 0) {
        Integer integer;
        if (!DecodeInteger(probe.cursor, header.length, &integer)) {
            PutError(Asn1Library, Asn1LengthMismatch);
            valid = false;
        } else if (!IntegerToInt64(integer, value)) {
            valid = false;
        }
    }
    if (valid && value == LongUndefined) {
        PutError(Asn1Library, Asn1IntegerTooLargeForLong);
        valid = false;
    }
    if (!valid) {
        PutError(Asn1Library, Asn1NestedError);
        return false;
    }
    reader.cursor = probe.cursor + header.length;
    reader.remaining = probe.remaining - header.length;
    return true;
}

bool ReadBignum(Reader& reader, LibreSsl3Bignum*& number) {
    Element element;
    if (ReadField(reader, TypeInteger, false, element) != 1) return false;
    Integer integer;
    StoreInteger(element, integer);
    number = NewBignum(integer);
    if (number == nullptr) {
        PutError(Asn1Library, Asn1BnLib);
        PutError(Asn1Library, Asn1NestedError);
        return false;
    }
    return true;
}

bool ReadDsaSequence(Reader& reader, bool privateKey, std::int64_t& version, Numbers& numbers) {
    Reader inner{nullptr, 0};
    if (!EnterSequence(reader, inner)) return false;
    bool present = false;
    if (privateKey) {
        if (!AtField(inner, present)) return false;
        if (!present) return MissingField();
        if (!ReadLong(inner, version)) return false;
    }
    const std::size_t count = privateKey ? 5 : 3;
    for (std::size_t index = 0; index < count; ++index) {
        if (!AtField(inner, present)) return false;
        if (!present) return MissingField();
        if (!ReadBignum(inner, numbers.values[index])) return false;
    }
    return SequenceComplete(inner);
}

bool ReadBareInteger(const unsigned char* der, std::size_t length, Integer& integer) {
    Reader probe{der, length};
    Header header;
    if (CheckTag(probe, header, TypeInteger, false) != 1) {
        PutError(Asn1Library, Asn1NestedError);
        return false;
    }
    if (header.constructed) {
        PutError(Asn1Library, Asn1TypeNotPrimitive);
        return false;
    }
    if (header.length > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    if (!DecodeInteger(probe.cursor, header.length, &integer)) {
        PutError(Asn1Library, Asn1LengthMismatch);
        return false;
    }
    return true;
}

bool InvalidParameters() {
    PutError(DsaLibrary, DsaInvalidParameters);
    return false;
}

bool BadSubprime() {
    PutError(DsaLibrary, DsaBadQValue);
    return false;
}

bool CheckKey(const Numbers& numbers) {
    const LibreSsl3Bignum* const prime = numbers.values[PrimeIndex];
    const LibreSsl3Bignum* const subprime = numbers.values[SubprimeIndex];
    const LibreSsl3Bignum* const generator = numbers.values[GeneratorIndex];
    if (prime == nullptr || subprime == nullptr || generator == nullptr) {
        PutError(DsaLibrary, DsaMissingParameters);
        return false;
    }
    Mpi p;
    Mpi q;
    Mpi g;
    Load(p, prime);
    Load(q, subprime);
    Load(g, generator);
    if (!IsOdd(p) || !IsOdd(q)) return InvalidParameters();
    if (mbedtls_mpi_cmp_int(&g.value, 1) <= 0 || mbedtls_mpi_cmp_mpi(&g.value, &p.value) >= 0)
        return InvalidParameters();
    if (mbedtls_mpi_cmp_int(&q.value, 0) < 0) return BadSubprime();
    const std::size_t subprimeBits = mbedtls_mpi_bitlen(&q.value);
    if (subprimeBits != 160 && subprimeBits != 224 && subprimeBits != 256) return BadSubprime();
    const std::size_t primeBits = mbedtls_mpi_bitlen(&p.value);
    if (primeBits > DsaMaximumModulusBits) {
        PutError(DsaLibrary, DsaModulusTooLarge);
        return false;
    }
    if (primeBits < DsaMinimumModulusBits) return InvalidParameters();
    if (numbers.values[PublicIndex] != nullptr) {
        Mpi publicKey;
        Load(publicKey, numbers.values[PublicIndex]);
        if (mbedtls_mpi_cmp_int(&publicKey.value, 1) <= 0 || mbedtls_mpi_cmp_mpi(&publicKey.value, &p.value) >= 0)
            return InvalidParameters();
    }
    if (numbers.values[PrivateIndex] != nullptr) {
        Mpi privateKey;
        Load(privateKey, numbers.values[PrivateIndex]);
        if (mbedtls_mpi_cmp_int(&privateKey.value, 1) < 0 || mbedtls_mpi_cmp_mpi(&privateKey.value, &q.value) >= 0)
            return InvalidParameters();
    }
    return true;
}

bool CheckOldKeyConsistency(const Numbers& numbers) {
    Mpi p;
    Mpi q;
    Mpi g;
    Mpi result;
    Load(p, numbers.values[PrimeIndex]);
    Load(q, numbers.values[SubprimeIndex]);
    Load(g, numbers.values[GeneratorIndex]);
    RequireMbedtls(mbedtls_mpi_sub_int(&result.value, &p.value, 1));
    RequireMbedtls(mbedtls_mpi_mod_mpi(&result.value, &result.value, &q.value));
    if (mbedtls_mpi_cmp_int(&result.value, 0) != 0) return BadSubprime();
    RequireMbedtls(mbedtls_mpi_exp_mod(&result.value, &g.value, &q.value, &p.value, nullptr));
    if (mbedtls_mpi_cmp_int(&result.value, 1) != 0) return InvalidParameters();
    if (!IsPrime(q)) return BadSubprime();
    return true;
}

LibreSsl3Bignum* PublicFromPrivate(const Numbers& numbers) {
    Mpi p;
    Mpi g;
    Mpi privateKey;
    Mpi result;
    Load(p, numbers.values[PrimeIndex]);
    Load(g, numbers.values[GeneratorIndex]);
    Load(privateKey, numbers.values[PrivateIndex]);
    RequireMbedtls(mbedtls_mpi_exp_mod(&result.value, &g.value, &privateKey.value, &p.value, nullptr));
    std::vector<unsigned char> bytes(mbedtls_mpi_size(&result.value));
    if (!bytes.empty()) RequireMbedtls(mbedtls_mpi_write_binary(&result.value, bytes.data(), bytes.size()));
    return LibreSsl3::NewRsaBignum_nid_no_patch(bytes.data(), bytes.size());
}

void TwosComplement(std::vector<unsigned char>& bytes) {
    unsigned char carry = 1;
    for (std::size_t index = bytes.size(); index-- > 0;) {
        bytes[index] = static_cast<unsigned char>((bytes[index] ^ 0xffu) + carry);
        if (bytes[index] != 0) carry = 0;
    }
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    std::array<unsigned char, sizeof(std::size_t)> bytes{};
    std::size_t count = 0;
    while (length != 0) {
        bytes[bytes.size() - ++count] = static_cast<unsigned char>(length);
        length >>= 8;
    }
    output.push_back(static_cast<unsigned char>(0x80 | count));
    output.insert(output.end(), bytes.end() - static_cast<std::ptrdiff_t>(count), bytes.end());
}

void AppendInteger(std::vector<unsigned char>& output, const LibreSsl3Bignum* number) {
    if (number->top < 0 || number->top > std::numeric_limits<int>::max() / 64
        || (number->top != 0 && number->data == nullptr))
        throw std::invalid_argument("DSA: malformed BIGNUM");
    std::vector<unsigned char> content;
    bool significant = false;
    for (std::size_t limb = static_cast<std::size_t>(number->top); limb-- > 0;) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            const auto byte = static_cast<unsigned char>(number->data[limb] >> shift);
            if (byte != 0) significant = true;
            if (significant) content.push_back(byte);
        }
    }
    if (content.empty()) {
        content.push_back(0);
    } else {
        const bool negative = number->negative != 0;
        if (negative) TwosComplement(content);
        const bool highBit = (content.front() & 0x80u) != 0;
        if (!negative && highBit) content.insert(content.begin(), 0x00);
        if (negative && !highBit) content.insert(content.begin(), 0xff);
    }
    output.push_back(0x02);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

void Wipe(void* data, std::size_t length) {
    auto* const bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = 0;
}

LibreSsl3EvpPkey* NewDsaPkey(std::int64_t version, Numbers& numbers) {
    auto* const dsa = static_cast<DsaObject*>(calloc_nid_postfix(1, sizeof(DsaObject)));
    if (dsa == nullptr) {
        PutError(DsaLibrary, ErrRMallocFailure);
        return nullptr;
    }
    dsa->flags = DsaFlagCacheMontP;
    dsa->references = 1;
    LibreSsl3EvpPkey* const key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    if (key == nullptr || !EVP_PKEY_assign_nid_postfix(key, LibreSsl3PkeyDsa, dsa)) {
        if (key != nullptr) EVP_PKEY_free_nid_disambig1_nid_postfix(key);
        free_nid_postfix(dsa);
        return nullptr;
    }
    dsa->version = version;
    dsa->p = numbers.values[PrimeIndex];
    dsa->q = numbers.values[SubprimeIndex];
    dsa->g = numbers.values[GeneratorIndex];
    dsa->pubKey = numbers.values[PublicIndex];
    dsa->privKey = numbers.values[PrivateIndex];
    numbers.values = {};
    return key;
}

}

LibreSsl3EvpPkey* DsaDecodeTraditionalKey_nid_no_patch(const unsigned char* der, std::size_t length) {
    Reader reader{der, length};
    Numbers numbers;
    std::int64_t version = 0;
    if (!ReadDsaSequence(reader, true, version, numbers)) {
        PutError(DsaLibrary, ErrRDsaLib);
        return nullptr;
    }
    if (!CheckKey(numbers)) return nullptr;
    if (!CheckOldKeyConsistency(numbers)) return nullptr;
    return NewDsaPkey(version, numbers);
}

LibreSsl3EvpPkey* DsaDecodePkcs8Key_nid_no_patch(const LibreSsl3X509Algor* algorithm, const unsigned char* key,
    std::size_t keyLength) {
    if (algorithm == nullptr) throw std::invalid_argument("dsa_priv_decode: null algorithm");
    const LibreSsl3Asn1Type* const parameter = algorithm->parameter;
    if (parameter == nullptr || parameter->type != TypeSequence) {
        PutError(DsaLibrary, DsaParameterEncodingError);
        return nullptr;
    }
    const auto* const sequence = static_cast<const LibreSsl3Asn1String*>(parameter->value);
    if (sequence == nullptr || sequence->length < 0 || (sequence->data == nullptr && sequence->length > 0))
        throw std::invalid_argument("dsa_priv_decode: malformed parameter string");
    Numbers numbers;
    std::int64_t version = 0;
    Reader reader{sequence->data, static_cast<std::size_t>(sequence->length)};
    if (!ReadDsaSequence(reader, false, version, numbers)) {
        PutError(DsaLibrary, DsaDecodeError);
        return nullptr;
    }
    Integer integer;
    if (!ReadBareInteger(key, keyLength, integer)) {
        PutError(DsaLibrary, DsaDecodeError);
        return nullptr;
    }
    numbers.values[PrivateIndex] = NewBignum(integer);
    if (numbers.values[PrivateIndex] == nullptr) {
        PutError(DsaLibrary, DsaBnDecodeError);
        return nullptr;
    }
    if (!CheckKey(numbers)) return nullptr;
    numbers.values[PublicIndex] = PublicFromPrivate(numbers);
    if (numbers.values[PublicIndex] == nullptr) {
        PutError(DsaLibrary, ErrRMallocFailure);
        return nullptr;
    }
    return NewDsaPkey(0, numbers);
}

void DsaEncodePkcs8Parts_nid_no_patch(const LibreSsl3Dsa* key, std::vector<unsigned char>& parameters,
    std::vector<unsigned char>& privateKey) {
    if (key == nullptr) throw std::invalid_argument("dsa_priv_encode: null key");
    const auto* const dsa = reinterpret_cast<const DsaObject*>(key);
    if (dsa->privKey == nullptr) throw std::invalid_argument("dsa_priv_encode: null private key");
    std::vector<unsigned char> body;
    for (const LibreSsl3Bignum* number : {dsa->p, dsa->q, dsa->g}) {
        if (number != nullptr) AppendInteger(body, number);
    }
    parameters.clear();
    parameters.push_back(0x30);
    AppendLength(parameters, body.size());
    parameters.insert(parameters.end(), body.begin(), body.end());
    privateKey.clear();
    AppendInteger(privateKey, dsa->privKey);
}

void DsaFree_nid_no_patch(LibreSsl3Dsa* key) {
    if (key == nullptr) return;
    auto* const dsa = reinterpret_cast<DsaObject*>(key);
    DsaObject copy{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&copy, dsa, sizeof(copy));
    const bool writable = readable && GuestWritable_nid_no_patch(dsa, sizeof(*dsa));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("DSA_free: unreadable dsa");
    if (!writable) throw std::invalid_argument("DSA_free: unwritable dsa");
    const int remaining = std::atomic_ref<std::int32_t>(dsa->references).fetch_add(-1) - 1;
    if (remaining > 0) return;
    if (copy.method != nullptr || copy.methodMontP != nullptr || copy.extraData != nullptr)
        NotImplemented_nid_no_patch("DSA_free of a key with a method, a Montgomery context or extra data");
    for (LibreSsl3Bignum* number : {copy.p, copy.q, copy.g, copy.pubKey, copy.privKey, copy.kinv, copy.r})
        BN_clear_free_nid_postfix(number);
    Wipe(dsa, sizeof(*dsa));
    free_nid_postfix(dsa);
}
