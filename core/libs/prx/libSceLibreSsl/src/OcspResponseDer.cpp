/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/ocsp/ocsp_asn.c (ocsp_asn.c,v 1.9),
 * asn1/tasn_dec.c (tasn_dec.c,v 1.34), asn1/tasn_new.c (tasn_new.c,v 1.17),
 * asn1/asn1_lib.c (asn1_lib.c,v 1.38), asn1/a_int.c (a_int.c,v 1.31) and
 * asn1/a_object.c (a_object.c,v 1.29) */
/* $OpenBSD: ocsp_asn.c,v 1.9 2016/11/04 18:35:30 jsing Exp $ */
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
/* $OpenBSD: tasn_dec.c,v 1.34 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: tasn_new.c,v 1.17 2017/01/29 17:49:22 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2004 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: asn1_lib.c,v 1.38 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: a_int.c,v 1.31 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: a_object.c,v 1.29 2017/01/29 17:49:22 beck Exp $ */
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
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/OcspResponse.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "prx/libc/include/Api.hpp"

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int ExplicitLengthMismatch = 119;
constexpr int ExplicitTagNotConstructed = 120;
constexpr int FieldMissing = 121;
constexpr int HeaderTooLong = 123;
constexpr int MissingEoc = 137;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int TooLong = 155;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int NestedAsn1String = 197;
constexpr int InvalidObjectEncoding = 216;

constexpr int ConstructedBit = 0x20;
constexpr int ClassMask = 0xc0;
constexpr int TagMask = 0x1f;
constexpr int ContextClass = 0x80;
constexpr int IntegerType = 2;
constexpr int OctetStringType = 4;
constexpr int ObjectType = 6;
constexpr int EnumeratedType = 10;
constexpr int SequenceType = 16;
constexpr int NegativeType = 0x100;
constexpr int MaxStringNest = 5;
constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicData = 0x08;
constexpr std::int64_t LongMax = std::numeric_limits<std::int64_t>::max();

struct Header {
    std::int64_t length;
    bool indefinite;
    bool constructed;
};

bool Fail(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
    return false;
}

void Wipe(void* pointer, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(pointer);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

bool ReadLength(const unsigned char*& in, int& indefinite, std::int64_t& result, int max) {
    const unsigned char* p = in;
    std::uint64_t value = 0;
    if (max-- < 1) return false;
    if (*p == 0x80) {
        indefinite = 1;
        ++p;
    } else {
        indefinite = 0;
        unsigned int count = *p & 0x7f;
        if (*(p++) & 0x80) {
            if (max < static_cast<int>(count)) return false;
            while (count && *p == 0) {
                ++p;
                --count;
            }
            if (count > sizeof(std::int64_t)) return false;
            while (count-- > 0) value = (value << 8) | *(p++);
        } else {
            value = count;
        }
    }
    if (value > static_cast<std::uint64_t>(LongMax)) return false;
    in = p;
    result = static_cast<std::int64_t>(value);
    return true;
}

int ReadIdentifier(const unsigned char*& in, std::int64_t& length, int& tag, int& objectClass, std::int64_t available) {
    const unsigned char* p = in;
    std::int64_t max = available;
    int indefinite = 0;
    if (!max) {
        Fail(HeaderTooLong);
        return 0x80;
    }
    int result = *p & ConstructedBit;
    const int identifierClass = *p & ClassMask;
    const int low = *p & TagMask;
    if (low == TagMask) {
        ++p;
        if (--max == 0) {
            Fail(HeaderTooLong);
            return 0x80;
        }
        std::int64_t value = 0;
        while (*p & 0x80) {
            value <<= 7;
            value |= *(p++) & 0x7f;
            if (--max == 0 || value > (INT_MAX >> 7)) {
                Fail(HeaderTooLong);
                return 0x80;
            }
        }
        value <<= 7;
        value |= *(p++) & 0x7f;
        tag = static_cast<int>(value);
        if (--max == 0) {
            Fail(HeaderTooLong);
            return 0x80;
        }
    } else {
        tag = low;
        ++p;
        if (--max == 0) {
            Fail(HeaderTooLong);
            return 0x80;
        }
    }
    objectClass = identifierClass;
    if (!ReadLength(p, indefinite, length, static_cast<int>(max)) || (indefinite && !(result & ConstructedBit))) {
        Fail(HeaderTooLong);
        return 0x80;
    }
    if (length > available - (p - in)) {
        Fail(TooLong);
        result |= 0x80;
    }
    in = p;
    return result | indefinite;
}

bool CheckHeader(Header& header, const unsigned char*& in, std::int64_t available, int expectedTag, int expectedClass) {
    const unsigned char* p = in;
    std::int64_t length = 0;
    int tag = 0;
    int objectClass = 0;
    const int result = ReadIdentifier(p, length, tag, objectClass, available);
    if (result & 0x80) return Fail(BadObjectHeader);
    if (expectedTag >= 0 && (expectedTag != tag || expectedClass != objectClass)) return Fail(WrongTag);
    if (result & 1) length = available - (p - in);
    header = {length, (result & 1) != 0, (result & ConstructedBit) != 0};
    in = p;
    return true;
}

bool CheckEoc(const unsigned char*& in, std::int64_t length) {
    if (length < 2 || in[0] || in[1]) return false;
    in += 2;
    return true;
}

bool Collect(std::vector<unsigned char>& buffer, const unsigned char*& in, std::int64_t length, bool indefinite, int depth) {
    const unsigned char* p = in;
    while (length > 0) {
        const unsigned char* const start = p;
        if (CheckEoc(p, length)) {
            if (!indefinite) return Fail(UnexpectedEoc);
            indefinite = false;
            break;
        }
        Header header {};
        if (!CheckHeader(header, p, length, -1, 0)) return Fail(NestedAsn1Error);
        if (header.constructed) {
            if (depth >= MaxStringNest) return Fail(NestedAsn1String);
            if (!Collect(buffer, p, header.length, header.indefinite, depth + 1)) return false;
        } else if (header.length) {
            buffer.insert(buffer.end(), p, p + header.length);
            p += header.length;
        }
        length -= p - start;
    }
    if (indefinite) return Fail(MissingEoc);
    in = p;
    return true;
}

Asn1String* NewString(int type) {
    auto* string = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!string) {
        Fail(MallocFailure);
        return nullptr;
    }
    *string = {0, type, nullptr, 0};
    return string;
}

void FreeString(Asn1String* string) {
    if (string->data && !(string->flags & 0x10)) {
        if (string->length < 0) throw std::invalid_argument("d2i_OCSP_RESPONSE: negative owned string length");
        Wipe(string->data, static_cast<std::size_t>(string->length));
        free_nid_postfix(string->data);
    }
    free_nid_postfix(string);
}

bool DecodeInteger(Asn1String*& slot, const unsigned char* content, std::int64_t length) {
    Asn1String* target = slot;
    if (!target && !(target = NewString(IntegerType))) return false;
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length) + 1));
    if (!data) {
        Fail(MallocFailure);
        if (target != slot) free_nid_postfix(target);
        return false;
    }
    if (!length) {
        target->type = IntegerType;
    } else if (content[0] & 0x80) {
        target->type = NegativeType | IntegerType;
        std::int64_t start = 0;
        if (content[0] == 0xff && length != 1) {
            start = 1;
            --length;
        }
        std::int64_t index = length;
        while (index && !content[start + index - 1]) {
            data[index - 1] = 0;
            --index;
        }
        if (!index) {
            data[0] = 1;
            data[length] = 0;
            ++length;
        } else {
            data[index - 1] = static_cast<unsigned char>((content[start + index - 1] ^ 0xff) + 1);
            for (--index; index > 0; --index) data[index - 1] = content[start + index - 1] ^ 0xff;
        }
    } else {
        target->type = IntegerType;
        std::int64_t start = 0;
        if (content[0] == 0 && length != 1) {
            start = 1;
            --length;
        }
        std::memcpy(data, content + start, static_cast<std::size_t>(length));
    }
    free_nid_postfix(target->data);
    target->data = data;
    target->length = static_cast<int>(length);
    target->type = EnumeratedType | (target->type & NegativeType);
    slot = target;
    return true;
}

bool DecodeObject(LibreSslAsn1Object*& slot, const unsigned char* content, std::int64_t length) {
    if (length <= 0 || length > INT_MAX || !content || (content[length - 1] & 0x80)) return Fail(InvalidObjectEncoding);
    for (std::int64_t index = 0; index < length; ++index)
        if (content[index] == 0x80 && (!index || !(content[index - 1] & 0x80))) return Fail(InvalidObjectEncoding);
    LibreSslAsn1Object* target = slot;
    if (!target || !(target->flags & ObjectDynamic)) {
        target = static_cast<LibreSslAsn1Object*>(malloc_nid_postfix(sizeof(LibreSslAsn1Object)));
        if (!target) return Fail(MallocFailure);
        *target = {nullptr, nullptr, 0, 0, nullptr, ObjectDynamic};
    }
    if (target->data) {
        if (target->length < 0) throw std::invalid_argument("d2i_OCSP_RESPONSE: negative owned object length");
        Wipe(const_cast<unsigned char*>(target->data), static_cast<std::size_t>(target->length));
    }
    free_nid_postfix(const_cast<unsigned char*>(target->data));
    target->data = nullptr;
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length)));
    if (!data) {
        Fail(MallocFailure);
        if (target != slot) free_nid_postfix(target);
        return false;
    }
    std::memcpy(data, content, static_cast<std::size_t>(length));
    target->data = data;
    target->length = static_cast<int>(length);
    target->shortName = nullptr;
    target->longName = nullptr;
    target->flags |= ObjectDynamicData;
    slot = target;
    return true;
}

bool DecodeOctetString(Asn1String*& slot, const unsigned char* content, std::int64_t length, unsigned char* collected) {
    if (!slot) {
        if (!(slot = NewString(OctetStringType))) {
            free_nid_postfix(collected);
            return Fail(MallocFailure);
        }
    } else {
        slot->type = OctetStringType;
    }
    if (collected) {
        free_nid_postfix(slot->data);
        slot->data = collected;
        slot->length = static_cast<int>(length);
        return true;
    }
    if (slot->length < length || !slot->data) {
        auto* grown = static_cast<unsigned char*>(realloc_nid_postfix(slot->data, static_cast<std::size_t>(length) + 1));
        if (!grown) {
            Fail(MallocFailure);
            Fail(MallocFailure);
            FreeString(slot);
            slot = nullptr;
            return false;
        }
        slot->data = grown;
    }
    slot->length = static_cast<int>(length);
    std::memmove(slot->data, content, static_cast<std::size_t>(length));
    slot->data[length] = 0;
    return true;
}

template<typename TValue> bool DecodePrimitive(TValue*& slot, const unsigned char*& in, std::int64_t available, int type) {
    const unsigned char* p = in;
    Header header {};
    if (!CheckHeader(header, p, available, type, 0)) return Fail(NestedAsn1Error);
    std::vector<unsigned char> buffer;
    const unsigned char* content = p;
    std::int64_t length = header.length;
    unsigned char* collected = nullptr;
    if (header.constructed) {
        if (!Collect(buffer, p, header.length, header.indefinite, 0)) return false;
        length = static_cast<std::int64_t>(buffer.size());
        if (length > INT_MAX) throw std::length_error("d2i_OCSP_RESPONSE: content longer than INT_MAX bytes");
        content = buffer.data();
        if (type == OctetStringType) {
            collected = static_cast<unsigned char*>(malloc_nid_postfix(buffer.size() + 1));
            if (!collected) return Fail(MallocFailure);
            if (!buffer.empty()) std::memcpy(collected, buffer.data(), buffer.size());
            collected[buffer.size()] = 0;
            content = collected;
        }
    } else {
        if (length > INT_MAX) throw std::length_error("d2i_OCSP_RESPONSE: content longer than INT_MAX bytes");
        p += header.length;
    }
    bool decoded = false;
    if constexpr (std::is_same_v<TValue, LibreSslAsn1Object>) decoded = DecodeObject(slot, content, length);
    else if (type == EnumeratedType) decoded = DecodeInteger(slot, content, length);
    else decoded = DecodeOctetString(slot, content, length, collected);
    if (!decoded) return false;
    in = p;
    return true;
}

bool NewResponseBytes(LibreSslOcspResponseBytes*& bytes) {
    bytes = static_cast<LibreSslOcspResponseBytes*>(calloc_nid_postfix(1, sizeof(LibreSslOcspResponseBytes)));
    if (!bytes) return Fail(MallocFailure);
    if (!(bytes->response = NewString(OctetStringType))) {
        Fail(MallocFailure);
        return Fail(MallocFailure);
    }
    return true;
}

bool DecodeResponseBytes(LibreSslOcspResponseBytes*& bytes, const unsigned char*& in, std::int64_t length) {
    const unsigned char* p = in;
    Header header {};
    if (!CheckHeader(header, p, length, SequenceType, 0)) return Fail(NestedAsn1Error);
    bool eoc = header.indefinite;
    const bool noLength = eoc;
    length = header.length;
    if (!header.constructed) return Fail(SequenceNotConstructed);
    if (!bytes && !NewResponseBytes(bytes)) return Fail(NestedAsn1Error);
    int field = 0;
    for (; field < 2; ++field) {
        if (!length) break;
        const unsigned char* const start = p;
        if (CheckEoc(p, length)) {
            if (!eoc) return Fail(UnexpectedEoc);
            length -= p - start;
            eoc = false;
            break;
        }
        const bool decoded = field == 0 ? DecodePrimitive(bytes->responseType, p, length, ObjectType)
            : DecodePrimitive(bytes->response, p, length, OctetStringType);
        if (!decoded) return Fail(NestedAsn1Error);
        length -= p - start;
    }
    if (eoc && !CheckEoc(p, length)) return Fail(MissingEoc);
    if (!noLength && length) return Fail(SequenceLengthMismatch);
    if (field < 2) return Fail(FieldMissing);
    in = p;
    return true;
}

bool DecodeExplicitResponseBytes(LibreSslOcspResponse* response, const unsigned char*& in, std::int64_t available) {
    const unsigned char* p = in;
    Header header {};
    if (!CheckHeader(header, p, available, 0, ContextClass)) return Fail(NestedAsn1Error);
    const unsigned char* const start = p;
    if (!header.constructed) return Fail(ExplicitTagNotConstructed);
    auto* bytes = static_cast<LibreSslOcspResponseBytes*>(response->responseBytes);
    const bool decoded = DecodeResponseBytes(bytes, p, header.length);
    response->responseBytes = bytes;
    if (!decoded) {
        Fail(NestedAsn1Error);
        return Fail(NestedAsn1Error);
    }
    std::int64_t length = header.length - (p - start);
    if (header.indefinite) {
        if (!CheckEoc(p, length)) return Fail(MissingEoc);
    } else if (length) {
        return Fail(ExplicitLengthMismatch);
    }
    in = p;
    return true;
}

bool NewResponse(LibreSslOcspResponse*& response) {
    response = static_cast<LibreSslOcspResponse*>(calloc_nid_postfix(1, sizeof(LibreSslOcspResponse)));
    if (!response) return Fail(MallocFailure);
    if (!(response->responseStatus = NewString(EnumeratedType))) {
        Fail(MallocFailure);
        return Fail(MallocFailure);
    }
    return true;
}

bool DecodeResponseFields(LibreSslOcspResponse*& response, const unsigned char*& in, std::int64_t length) {
    const unsigned char* p = in;
    Header header {};
    if (!CheckHeader(header, p, length, SequenceType, 0)) return Fail(NestedAsn1Error);
    bool eoc = header.indefinite;
    const bool noLength = eoc;
    length = header.length;
    if (!header.constructed) return Fail(SequenceNotConstructed);
    if (!response && !NewResponse(response)) return Fail(NestedAsn1Error);
    int field = 0;
    for (; field < 2; ++field) {
        if (!length) break;
        const unsigned char* const start = p;
        if (CheckEoc(p, length)) {
            if (!eoc) return Fail(UnexpectedEoc);
            length -= p - start;
            eoc = false;
            break;
        }
        if (field == 0) {
            if (!DecodePrimitive(response->responseStatus, p, length, EnumeratedType)) return Fail(NestedAsn1Error);
        } else if (!DecodeExplicitResponseBytes(response, p, length)) {
            return false;
        }
        length -= p - start;
    }
    if (eoc && !CheckEoc(p, length)) return Fail(MissingEoc);
    if (!noLength && length) return Fail(SequenceLengthMismatch);
    if (field == 0) return Fail(FieldMissing);
    if (field == 1) {
        LibreSslOcspResponseBytesFree_nid_no_patch(static_cast<LibreSslOcspResponseBytes*>(response->responseBytes));
        response->responseBytes = nullptr;
    }
    in = p;
    return true;
}

}

extern "C" {

LibreSslOcspResponse* APS5_VABI d2i_OCSP_RESPONSE_nid_postfix(LibreSslOcspResponse** response, const unsigned char** input, std::int64_t length) {
    if (!input) throw std::invalid_argument(std::string(__func__) + ": null input pointer");
    if (!*input && length) throw std::invalid_argument(std::string(__func__) + ": null input");
    LibreSslOcspResponse* decoded = nullptr;
    LibreSslOcspResponse*& target = response ? *response : decoded;
    if (DecodeResponseFields(target, *input, length)) return target;
    OCSP_RESPONSE_free_nid_postfix(target);
    target = nullptr;
    return nullptr;
}

}
