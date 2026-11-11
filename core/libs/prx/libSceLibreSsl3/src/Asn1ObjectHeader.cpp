/* $OpenBSD: asn1_lib.c,v 1.45 2020/12/08 15:06:42 tb Exp $ */
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
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int HeaderTooLong = 123;
constexpr int TooLong = 155;
constexpr int Constructed = 0x20;
constexpr int Private = 0xc0;
constexpr int PrimitiveTag = 0x1f;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

bool GetLength(const unsigned char** cursor, int* indefinite, std::int64_t* length, std::int64_t max) {
    const unsigned char* pointer = *cursor;
    std::uint64_t result = 0;
    if (max-- < 1) return false;
    if (*pointer == 0x80) {
        *indefinite = 1;
        ++pointer;
    } else {
        *indefinite = 0;
        unsigned int count = *pointer & 0x7f;
        if (*(pointer++) & 0x80) {
            if (max < static_cast<std::int64_t>(count)) return false;
            while (count && *pointer == 0) {
                ++pointer;
                --count;
            }
            if (count > 8) return false;
            while (count-- > 0) {
                result <<= 8;
                result |= *(pointer++);
            }
        } else {
            result = count;
        }
    }
    if (result > static_cast<std::uint64_t>(INT64_MAX)) return false;
    *cursor = pointer;
    *length = static_cast<std::int64_t>(result);
    return true;
}

}

extern "C" {

int APS5_VABI ASN1_get_object_nid_postfix(const unsigned char** pp, std::int64_t* plength, int* ptag, int* pclass, std::int64_t omax) {
    if (!pp || !plength || !ptag || !pclass) throw std::invalid_argument(std::string(__func__) + ": null output");
    if (omax < 0) throw std::invalid_argument(std::string(__func__) + ": negative input length");
    const unsigned char* pointer = *pp;
    if (omax > 0 && !pointer) throw std::invalid_argument(std::string(__func__) + ": null input");
    std::int64_t max = omax;
    if (!max) {
        PutError(HeaderTooLong);
        return 0x80;
    }
    int result = *pointer & Constructed;
    const int objectClass = *pointer & Private;
    int tag = *pointer & PrimitiveTag;
    if (tag == PrimitiveTag) {
        ++pointer;
        if (--max == 0) {
            PutError(HeaderTooLong);
            return 0x80;
        }
        std::int64_t value = 0;
        while (*pointer & 0x80) {
            value <<= 7;
            value |= *(pointer++) & 0x7f;
            if (--max == 0) {
                PutError(HeaderTooLong);
                return 0x80;
            }
            if (value > (INT_MAX >> 7)) {
                PutError(HeaderTooLong);
                return 0x80;
            }
        }
        value <<= 7;
        value |= *(pointer++) & 0x7f;
        tag = static_cast<int>(value);
        if (--max == 0) {
            PutError(HeaderTooLong);
            return 0x80;
        }
    } else {
        ++pointer;
        if (--max == 0) {
            PutError(HeaderTooLong);
            return 0x80;
        }
    }
    *ptag = tag;
    *pclass = objectClass;
    int indefinite = 0;
    if (!GetLength(&pointer, &indefinite, plength, max)) {
        PutError(HeaderTooLong);
        return 0x80;
    }
    if (indefinite && !(result & Constructed)) {
        PutError(HeaderTooLong);
        return 0x80;
    }
    if (*plength > omax - static_cast<std::int64_t>(pointer - *pp)) {
        PutError(TooLong);
        result |= 0x80;
    }
    *pp = pointer;
    return result | indefinite;
}

}
