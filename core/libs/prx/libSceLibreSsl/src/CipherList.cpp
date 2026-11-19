/* $OpenBSD: ssl_ciph.c,v 1.96 2017/03/10 16:03:27 jsing Exp $ */
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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

/* $OpenBSD: s3_lib.c,v 1.138 2017/03/10 16:03:27 jsing Exp $ */
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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb):
 * https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_ciph.c
 * https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/s3_lib.c
 * Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com) */

#include "prx/libSceLibreSsl/include/CipherList.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct Cipher {
    std::int32_t valid;
    const char* name;
    std::uint64_t id;
    std::uint64_t keyExchange;
    std::uint64_t authentication;
    std::uint64_t encryption;
    std::uint64_t mac;
    std::uint64_t protocol;
    std::uint64_t strengthFlags;
    std::uint64_t extraFlags;
    std::int32_t strengthBits;
    std::int32_t algorithmBits;
};

static_assert(sizeof(Cipher) == 88);
static_assert(offsetof(Cipher, name) == 8);
static_assert(offsetof(Cipher, id) == 16);
static_assert(offsetof(Cipher, strengthBits) == 80);
static_assert(offsetof(Cipher, algorithmBits) == 84);

constexpr int ErrLibSsl = 20;
constexpr int ErrFunctionNone = 0xfff;
constexpr int MallocFailure = 65;
constexpr int NoCipherMatch = 185;
constexpr int InvalidCommand = 280;

constexpr std::uint64_t KeyRsa = 0x1;
constexpr std::uint64_t KeyDhe = 0x8;
constexpr std::uint64_t KeyEcdhe = 0x80;
constexpr std::uint64_t KeyGost = 0x200;

constexpr std::uint64_t AuthRsa = 0x1;
constexpr std::uint64_t AuthDss = 0x2;
constexpr std::uint64_t AuthNull = 0x4;
constexpr std::uint64_t AuthEcdsa = 0x40;
constexpr std::uint64_t AuthGost01 = 0x200;

constexpr std::uint64_t EncDes = 0x1;
constexpr std::uint64_t Enc3Des = 0x2;
constexpr std::uint64_t EncRc4 = 0x4;
constexpr std::uint64_t EncIdea = 0x8;
constexpr std::uint64_t EncNull = 0x10;
constexpr std::uint64_t EncAes128 = 0x20;
constexpr std::uint64_t EncAes256 = 0x40;
constexpr std::uint64_t EncCamellia128 = 0x80;
constexpr std::uint64_t EncCamellia256 = 0x100;
constexpr std::uint64_t EncGost89Cnt = 0x200;
constexpr std::uint64_t EncAes128Gcm = 0x400;
constexpr std::uint64_t EncAes256Gcm = 0x800;
constexpr std::uint64_t EncChaCha20Poly1305 = 0x1000;
constexpr std::uint64_t EncChaCha20Poly1305Old = 0x2000;
constexpr std::uint64_t EncAes = EncAes128 | EncAes256 | EncAes128Gcm | EncAes256Gcm;
constexpr std::uint64_t EncCamellia = EncCamellia128 | EncCamellia256;

constexpr std::uint64_t MacMd5 = 0x1;
constexpr std::uint64_t MacSha1 = 0x2;
constexpr std::uint64_t MacGost94 = 0x4;
constexpr std::uint64_t MacGost89 = 0x8;
constexpr std::uint64_t MacSha256 = 0x10;
constexpr std::uint64_t MacSha384 = 0x20;
constexpr std::uint64_t MacAead = 0x40;
constexpr std::uint64_t MacStreebog256 = 0x80;

constexpr std::uint64_t ProtocolSslV3 = 0x2;
constexpr std::uint64_t ProtocolTlsV1 = ProtocolSslV3;
constexpr std::uint64_t ProtocolTlsV12 = 0x4;

constexpr std::uint64_t StrongMask = 0x1fc;
constexpr std::uint64_t StrengthLow = 0x20;
constexpr std::uint64_t StrengthMedium = 0x40;
constexpr std::uint64_t StrengthHigh = 0x80;

const Cipher Ciphers[] = {
    {1, "NULL-MD5", 0x03000001, 0x1, 0x1, 0x10, 0x1, 0x2, 0x4, 0xC030, 0, 0},
    {1, "NULL-SHA", 0x03000002, 0x1, 0x1, 0x10, 0x2, 0x2, 0x4, 0xC030, 0, 0},
    {1, "RC4-MD5", 0x03000004, 0x1, 0x1, 0x4, 0x1, 0x2, 0x20, 0xC030, 128, 128},
    {1, "RC4-SHA", 0x03000005, 0x1, 0x1, 0x4, 0x2, 0x2, 0x20, 0xC030, 128, 128},
    {1, "DES-CBC-SHA", 0x03000009, 0x1, 0x1, 0x1, 0x2, 0x2, 0x20, 0xC030, 56, 56},
    {1, "DES-CBC3-SHA", 0x0300000A, 0x1, 0x1, 0x2, 0x2, 0x2, 0x40, 0xC030, 112, 168},
    {1, "EDH-DSS-DES-CBC-SHA", 0x03000012, 0x8, 0x2, 0x1, 0x2, 0x2, 0x20, 0xC030, 56, 56},
    {1, "EDH-DSS-DES-CBC3-SHA", 0x03000013, 0x8, 0x2, 0x2, 0x2, 0x2, 0x40, 0xC030, 112, 168},
    {1, "EDH-RSA-DES-CBC-SHA", 0x03000015, 0x8, 0x1, 0x1, 0x2, 0x2, 0x20, 0xC030, 56, 56},
    {1, "EDH-RSA-DES-CBC3-SHA", 0x03000016, 0x8, 0x1, 0x2, 0x2, 0x2, 0x40, 0xC030, 112, 168},
    {1, "ADH-RC4-MD5", 0x03000018, 0x8, 0x4, 0x4, 0x1, 0x2, 0x20, 0xC030, 128, 128},
    {1, "ADH-DES-CBC-SHA", 0x0300001A, 0x8, 0x4, 0x1, 0x2, 0x2, 0x20, 0xC030, 56, 56},
    {1, "ADH-DES-CBC3-SHA", 0x0300001B, 0x8, 0x4, 0x2, 0x2, 0x2, 0x40, 0xC030, 112, 168},
    {1, "AES128-SHA", 0x0300002F, 0x1, 0x1, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "DHE-DSS-AES128-SHA", 0x03000032, 0x8, 0x2, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "DHE-RSA-AES128-SHA", 0x03000033, 0x8, 0x1, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "ADH-AES128-SHA", 0x03000034, 0x8, 0x4, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "AES256-SHA", 0x03000035, 0x1, 0x1, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "DHE-DSS-AES256-SHA", 0x03000038, 0x8, 0x2, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "DHE-RSA-AES256-SHA", 0x03000039, 0x8, 0x1, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "ADH-AES256-SHA", 0x0300003A, 0x8, 0x4, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "NULL-SHA256", 0x0300003B, 0x1, 0x1, 0x10, 0x10, 0x4, 0x4, 0xC030, 0, 0},
    {1, "AES128-SHA256", 0x0300003C, 0x1, 0x1, 0x20, 0x10, 0x4, 0x80, 0xC030, 128, 128},
    {1, "AES256-SHA256", 0x0300003D, 0x1, 0x1, 0x40, 0x10, 0x4, 0x80, 0xC030, 256, 256},
    {1, "DHE-DSS-AES128-SHA256", 0x03000040, 0x8, 0x2, 0x20, 0x10, 0x4, 0x80, 0xC030, 128, 128},
    {1, "CAMELLIA128-SHA", 0x03000041, 0x1, 0x1, 0x80, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "DHE-DSS-CAMELLIA128-SHA", 0x03000044, 0x8, 0x2, 0x80, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "DHE-RSA-CAMELLIA128-SHA", 0x03000045, 0x8, 0x1, 0x80, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "ADH-CAMELLIA128-SHA", 0x03000046, 0x8, 0x4, 0x80, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "DHE-RSA-AES128-SHA256", 0x03000067, 0x8, 0x1, 0x20, 0x10, 0x4, 0x80, 0xC030, 128, 128},
    {1, "DHE-DSS-AES256-SHA256", 0x0300006A, 0x8, 0x2, 0x40, 0x10, 0x4, 0x80, 0xC030, 256, 256},
    {1, "DHE-RSA-AES256-SHA256", 0x0300006B, 0x8, 0x1, 0x40, 0x10, 0x4, 0x80, 0xC030, 256, 256},
    {1, "ADH-AES128-SHA256", 0x0300006C, 0x8, 0x4, 0x20, 0x10, 0x4, 0x80, 0xC030, 128, 128},
    {1, "ADH-AES256-SHA256", 0x0300006D, 0x8, 0x4, 0x40, 0x10, 0x4, 0x80, 0xC030, 256, 256},
    {1, "GOST2001-GOST89-GOST89", 0x03000081, 0x200, 0x200, 0x200, 0x8, 0x2, 0x80, 0x10044, 256, 256},
    {1, "GOST2001-NULL-GOST94", 0x03000083, 0x200, 0x200, 0x10, 0x4, 0x2, 0x4, 0x10040, 0, 0},
    {1, "CAMELLIA256-SHA", 0x03000084, 0x1, 0x1, 0x100, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "DHE-DSS-CAMELLIA256-SHA", 0x03000087, 0x8, 0x2, 0x100, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "DHE-RSA-CAMELLIA256-SHA", 0x03000088, 0x8, 0x1, 0x100, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "ADH-CAMELLIA256-SHA", 0x03000089, 0x8, 0x4, 0x100, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "AES128-GCM-SHA256", 0x0300009C, 0x1, 0x1, 0x400, 0x40, 0x4, 0x80, 0x2C20080, 128, 128},
    {1, "AES256-GCM-SHA384", 0x0300009D, 0x1, 0x1, 0x800, 0x40, 0x4, 0x80, 0x2C40100, 256, 256},
    {1, "DHE-RSA-AES128-GCM-SHA256", 0x0300009E, 0x8, 0x1, 0x400, 0x40, 0x4, 0x80, 0x2C20080, 128, 128},
    {1, "DHE-RSA-AES256-GCM-SHA384", 0x0300009F, 0x8, 0x1, 0x800, 0x40, 0x4, 0x80, 0x2C40100, 256, 256},
    {1, "DHE-DSS-AES128-GCM-SHA256", 0x030000A2, 0x8, 0x2, 0x400, 0x40, 0x4, 0x80, 0x2C20080, 128, 128},
    {1, "DHE-DSS-AES256-GCM-SHA384", 0x030000A3, 0x8, 0x2, 0x800, 0x40, 0x4, 0x80, 0x2C40100, 256, 256},
    {1, "ADH-AES128-GCM-SHA256", 0x030000A6, 0x8, 0x4, 0x400, 0x40, 0x4, 0x80, 0x2C20080, 128, 128},
    {1, "ADH-AES256-GCM-SHA384", 0x030000A7, 0x8, 0x4, 0x800, 0x40, 0x4, 0x80, 0x2C40100, 256, 256},
    {1, "CAMELLIA128-SHA256", 0x030000BA, 0x1, 0x1, 0x80, 0x10, 0x4, 0x80, 0x20080, 128, 128},
    {1, "DHE-DSS-CAMELLIA128-SHA256", 0x030000BD, 0x8, 0x2, 0x80, 0x10, 0x4, 0x80, 0x20080, 128, 128},
    {1, "DHE-RSA-CAMELLIA128-SHA256", 0x030000BE, 0x8, 0x1, 0x80, 0x10, 0x4, 0x80, 0x20080, 128, 128},
    {1, "ADH-CAMELLIA128-SHA256", 0x030000BF, 0x8, 0x4, 0x80, 0x10, 0x4, 0x80, 0x20080, 128, 128},
    {1, "CAMELLIA256-SHA256", 0x030000C0, 0x1, 0x1, 0x100, 0x10, 0x4, 0x80, 0x20080, 256, 256},
    {1, "DHE-DSS-CAMELLIA256-SHA256", 0x030000C3, 0x8, 0x2, 0x100, 0x10, 0x4, 0x80, 0x20080, 256, 256},
    {1, "DHE-RSA-CAMELLIA256-SHA256", 0x030000C4, 0x8, 0x1, 0x100, 0x10, 0x4, 0x80, 0x20080, 256, 256},
    {1, "ADH-CAMELLIA256-SHA256", 0x030000C5, 0x8, 0x4, 0x100, 0x10, 0x4, 0x80, 0x20080, 256, 256},
    {1, "ECDHE-ECDSA-NULL-SHA", 0x0300C006, 0x80, 0x40, 0x10, 0x2, 0x2, 0x4, 0xC030, 0, 0},
    {1, "ECDHE-ECDSA-RC4-SHA", 0x0300C007, 0x80, 0x40, 0x4, 0x2, 0x2, 0x20, 0xC030, 128, 128},
    {1, "ECDHE-ECDSA-DES-CBC3-SHA", 0x0300C008, 0x80, 0x40, 0x2, 0x2, 0x2, 0x40, 0xC030, 112, 168},
    {1, "ECDHE-ECDSA-AES128-SHA", 0x0300C009, 0x80, 0x40, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "ECDHE-ECDSA-AES256-SHA", 0x0300C00A, 0x80, 0x40, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "ECDHE-RSA-NULL-SHA", 0x0300C010, 0x80, 0x1, 0x10, 0x2, 0x2, 0x4, 0xC030, 0, 0},
    {1, "ECDHE-RSA-RC4-SHA", 0x0300C011, 0x80, 0x1, 0x4, 0x2, 0x2, 0x20, 0xC030, 128, 128},
    {1, "ECDHE-RSA-DES-CBC3-SHA", 0x0300C012, 0x80, 0x1, 0x2, 0x2, 0x2, 0x80, 0xC030, 112, 168},
    {1, "ECDHE-RSA-AES128-SHA", 0x0300C013, 0x80, 0x1, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "ECDHE-RSA-AES256-SHA", 0x0300C014, 0x80, 0x1, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "AECDH-NULL-SHA", 0x0300C015, 0x80, 0x4, 0x10, 0x2, 0x2, 0x4, 0xC030, 0, 0},
    {1, "AECDH-RC4-SHA", 0x0300C016, 0x80, 0x4, 0x4, 0x2, 0x2, 0x20, 0xC030, 128, 128},
    {1, "AECDH-DES-CBC3-SHA", 0x0300C017, 0x80, 0x4, 0x2, 0x2, 0x2, 0x40, 0xC030, 112, 168},
    {1, "AECDH-AES128-SHA", 0x0300C018, 0x80, 0x4, 0x20, 0x2, 0x2, 0x80, 0xC030, 128, 128},
    {1, "AECDH-AES256-SHA", 0x0300C019, 0x80, 0x4, 0x40, 0x2, 0x2, 0x80, 0xC030, 256, 256},
    {1, "ECDHE-ECDSA-AES128-SHA256", 0x0300C023, 0x80, 0x40, 0x20, 0x10, 0x4, 0x80, 0x20080, 128, 128},
    {1, "ECDHE-ECDSA-AES256-SHA384", 0x0300C024, 0x80, 0x40, 0x40, 0x20, 0x4, 0x80, 0x40100, 256, 256},
    {1, "ECDHE-RSA-AES128-SHA256", 0x0300C027, 0x80, 0x1, 0x20, 0x10, 0x4, 0x80, 0x20080, 128, 128},
    {1, "ECDHE-RSA-AES256-SHA384", 0x0300C028, 0x80, 0x1, 0x40, 0x20, 0x4, 0x80, 0x40100, 256, 256},
    {1, "ECDHE-ECDSA-AES128-GCM-SHA256", 0x0300C02B, 0x80, 0x40, 0x400, 0x40, 0x4, 0x80, 0x2C20080, 128, 128},
    {1, "ECDHE-ECDSA-AES256-GCM-SHA384", 0x0300C02C, 0x80, 0x40, 0x800, 0x40, 0x4, 0x80, 0x2C40100, 256, 256},
    {1, "ECDHE-RSA-AES128-GCM-SHA256", 0x0300C02F, 0x80, 0x1, 0x400, 0x40, 0x4, 0x80, 0x2C20080, 128, 128},
    {1, "ECDHE-RSA-AES256-GCM-SHA384", 0x0300C030, 0x80, 0x1, 0x800, 0x40, 0x4, 0x80, 0x2C40100, 256, 256},
    {1, "ECDHE-RSA-CHACHA20-POLY1305-OLD", 0x0300CC13, 0x80, 0x1, 0x2000, 0x40, 0x4, 0x80, 0x820080, 256, 256},
    {1, "ECDHE-ECDSA-CHACHA20-POLY1305-OLD", 0x0300CC14, 0x80, 0x40, 0x2000, 0x40, 0x4, 0x80, 0x820080, 256, 256},
    {1, "DHE-RSA-CHACHA20-POLY1305-OLD", 0x0300CC15, 0x8, 0x1, 0x2000, 0x40, 0x4, 0x80, 0x820080, 256, 256},
    {1, "ECDHE-RSA-CHACHA20-POLY1305", 0x0300CCA8, 0x80, 0x1, 0x1000, 0x40, 0x4, 0x80, 0x6820080, 256, 256},
    {1, "ECDHE-ECDSA-CHACHA20-POLY1305", 0x0300CCA9, 0x80, 0x40, 0x1000, 0x40, 0x4, 0x80, 0x6820080, 256, 256},
    {1, "DHE-RSA-CHACHA20-POLY1305", 0x0300CCAA, 0x8, 0x1, 0x1000, 0x40, 0x4, 0x80, 0x6820080, 256, 256},
    {1, "GOST2012256-GOST89-GOST89", 0x0300FF85, 0x200, 0x200, 0x200, 0x8, 0x2, 0x80, 0x80204, 256, 256},
    {1, "GOST2012256-NULL-STREEBOG256", 0x0300FF87, 0x200, 0x200, 0x10, 0x80, 0x2, 0x4, 0x80200, 0, 0},
};

constexpr int CipherCount = static_cast<int>(std::size(Ciphers));

const Cipher Aliases[] = {
    {.name = "ALL", .encryption = ~EncNull},
    {.name = "COMPLEMENTOFALL", .encryption = EncNull},
    {.name = "COMPLEMENTOFDEFAULT", .keyExchange = KeyDhe | KeyEcdhe, .authentication = AuthNull, .encryption = ~EncNull},
    {.name = "kRSA", .keyExchange = KeyRsa},
    {.name = "kEDH", .keyExchange = KeyDhe},
    {.name = "DH", .keyExchange = KeyDhe},
    {.name = "kEECDH", .keyExchange = KeyEcdhe},
    {.name = "ECDH", .keyExchange = KeyEcdhe},
    {.name = "kGOST", .keyExchange = KeyGost},
    {.name = "aRSA", .authentication = AuthRsa},
    {.name = "aDSS", .authentication = AuthDss},
    {.name = "DSS", .authentication = AuthDss},
    {.name = "aNULL", .authentication = AuthNull},
    {.name = "aECDSA", .authentication = AuthEcdsa},
    {.name = "ECDSA", .authentication = AuthEcdsa},
    {.name = "aGOST01", .authentication = AuthGost01},
    {.name = "aGOST", .authentication = AuthGost01},
    {.name = "DHE", .keyExchange = KeyDhe, .authentication = ~AuthNull},
    {.name = "EDH", .keyExchange = KeyDhe, .authentication = ~AuthNull},
    {.name = "ECDHE", .keyExchange = KeyEcdhe, .authentication = ~AuthNull},
    {.name = "EECDH", .keyExchange = KeyEcdhe, .authentication = ~AuthNull},
    {.name = "NULL", .encryption = EncNull},
    {.name = "RSA", .keyExchange = KeyRsa, .authentication = AuthRsa},
    {.name = "ADH", .keyExchange = KeyDhe, .authentication = AuthNull},
    {.name = "AECDH", .keyExchange = KeyEcdhe, .authentication = AuthNull},
    {.name = "DES", .encryption = EncDes},
    {.name = "3DES", .encryption = Enc3Des},
    {.name = "RC4", .encryption = EncRc4},
    {.name = "IDEA", .encryption = EncIdea},
    {.name = "eNULL", .encryption = EncNull},
    {.name = "AES128", .encryption = EncAes128 | EncAes128Gcm},
    {.name = "AES256", .encryption = EncAes256 | EncAes256Gcm},
    {.name = "AES", .encryption = EncAes},
    {.name = "AESGCM", .encryption = EncAes128Gcm | EncAes256Gcm},
    {.name = "CAMELLIA128", .encryption = EncCamellia128},
    {.name = "CAMELLIA256", .encryption = EncCamellia256},
    {.name = "CAMELLIA", .encryption = EncCamellia},
    {.name = "CHACHA20", .encryption = EncChaCha20Poly1305 | EncChaCha20Poly1305Old},
    {.name = "AEAD", .mac = MacAead},
    {.name = "MD5", .mac = MacMd5},
    {.name = "SHA1", .mac = MacSha1},
    {.name = "SHA", .mac = MacSha1},
    {.name = "GOST94", .mac = MacGost94},
    {.name = "GOST89MAC", .mac = MacGost89},
    {.name = "SHA256", .mac = MacSha256},
    {.name = "SHA384", .mac = MacSha384},
    {.name = "STREEBOG256", .mac = MacStreebog256},
    {.name = "SSLv3", .protocol = ProtocolSslV3},
    {.name = "TLSv1", .protocol = ProtocolTlsV1},
    {.name = "TLSv1.2", .protocol = ProtocolTlsV12},
    {.name = "LOW", .strengthFlags = StrengthLow},
    {.name = "MEDIUM", .strengthFlags = StrengthMedium},
    {.name = "HIGH", .strengthFlags = StrengthHigh},
};

constexpr int AliasCount = static_cast<int>(std::size(Aliases));

constexpr const char* DefaultCipherList = "ALL:!aNULL:!eNULL:!SSLv2";

constexpr bool AesAccelerated = true;

enum CipherIndex : int {
    CipherDes = 0,
    Cipher3Des = 1,
    CipherRc4 = 2,
    CipherIdea = 3,
    CipherNull = 4,
    CipherAes128 = 5,
    CipherAes256 = 6,
    CipherCamellia128 = 7,
    CipherCamellia256 = 8,
    CipherGost89 = 9,
    CipherAes128Gcm = 10,
    CipherAes256Gcm = 11,
    CipherMethodCount = 12
};

enum DigestIndex : int {
    DigestMd5 = 0,
    DigestSha1 = 1,
    DigestGost94 = 2,
    DigestGost89Mac = 3,
    DigestSha256 = 4,
    DigestSha384 = 5,
    DigestStreebog256 = 6,
    DigestMethodCount = 7
};

constexpr bool GostPublicKeyMethodAvailable = true;

constexpr bool CipherMethodAvailable[CipherMethodCount] = {
    true, true, true, false, false, true, true, true, true, true, true, true
};

constexpr bool DigestMethodAvailable[DigestMethodCount] = {
    true, true, true, true, true, true, true
};

enum Rule : int {
    RuleAdd = 1,
    RuleKill = 2,
    RuleDel = 3,
    RuleOrd = 4,
    RuleSpecial = 5
};

struct CipherOrder {
    const Cipher* cipher;
    int active;
    CipherOrder* next;
    CipherOrder* prev;
};

struct DisabledMasks {
    std::uint64_t keyExchange;
    std::uint64_t authentication;
    std::uint64_t encryption;
    std::uint64_t mac;
    std::uint64_t protocol;
};

constexpr int StackMinNodes = 4;

bool IsItemSeparator(char ch) {
    return ch == ':' || ch == ' ' || ch == ';' || ch == ',';
}

template <typename T>
bool AllocateScratch(std::vector<T>& scratch, std::size_t count) {
    try {
        scratch.assign(count, T{});
    } catch (const std::bad_alloc&) {
        return false;
    }
    return true;
}

LibreSslStack* StackNewNull() {
    auto* const stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    auto* const data = static_cast<char**>(malloc_nid_postfix(StackMinNodes * sizeof(char*)));
    if (!data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < StackMinNodes; ++index) data[index] = nullptr;
    stack->data = data;
    stack->comp = nullptr;
    stack->numAlloc = StackMinNodes;
    stack->num = 0;
    stack->sorted = 0;
    return stack;
}

void StackFree(LibreSslStack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

int StackPush(LibreSslStack* stack, void* element) {
    if (stack->numAlloc <= stack->num + 1) {
        const std::size_t oldCount = static_cast<std::size_t>(stack->numAlloc);
        auto* const grown = static_cast<char**>(realloc_nid_postfix(stack->data, oldCount * 2 * sizeof(char*)));
        if (!grown) return 0;
        std::memset(grown + oldCount, 0, oldCount * sizeof(char*));
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(element);
    ++stack->num;
    stack->sorted = 0;
    return stack->num;
}

LibreSslStack* StackDup(const LibreSslStack* source) {
    LibreSslStack* const copy = StackNewNull();
    if (!copy) return nullptr;
    copy->comp = source->comp;
    const std::size_t count = static_cast<std::size_t>(source->numAlloc);
    auto* const data = static_cast<char**>(realloc_nid_postfix(copy->data, count * sizeof(char*)));
    if (!data) {
        StackFree(copy);
        return nullptr;
    }
    copy->data = data;
    copy->num = source->num;
    std::memset(copy->data, 0, count * sizeof(char*));
    std::memcpy(copy->data, source->data, sizeof(char*) * static_cast<std::size_t>(source->num));
    copy->sorted = source->sorted;
    copy->numAlloc = source->numAlloc;
    copy->comp = source->comp;
    return copy;
}

void StackSetCompare(LibreSslStack* stack, StackCompare compare) {
    if (stack->comp != compare) stack->sorted = 0;
    stack->comp = compare;
}

void StackSort(LibreSslStack* stack) {
    if (stack && !stack->sorted) {
        qsort_nid_postfix(stack->data, static_cast<std::size_t>(stack->num), sizeof(char*), stack->comp);
        stack->sorted = 1;
    }
}

int APS5_VABI CipherPointerIdCompare(const void* left, const void* right) {
    const Cipher* const leftCipher = *static_cast<const Cipher* const*>(left);
    const Cipher* const rightCipher = *static_cast<const Cipher* const*>(right);
    const auto difference = static_cast<std::int64_t>(leftCipher->id - rightCipher->id);
    if (difference == 0) return 0;
    return difference > 0 ? 1 : -1;
}

void LinkAppendTail(CipherOrder** head, CipherOrder* current, CipherOrder** tail) {
    if (current == *tail) return;
    if (current == *head) *head = current->next;
    if (current->prev != nullptr) current->prev->next = current->next;
    if (current->next != nullptr) current->next->prev = current->prev;
    (*tail)->next = current;
    current->prev = *tail;
    current->next = nullptr;
    *tail = current;
}

void LinkAppendHead(CipherOrder** head, CipherOrder* current, CipherOrder** tail) {
    if (current == *head) return;
    if (current == *tail) *tail = current->prev;
    if (current->next != nullptr) current->next->prev = current->prev;
    if (current->prev != nullptr) current->prev->next = current->next;
    (*head)->prev = current;
    current->next = *head;
    current->prev = nullptr;
    *head = current;
}

DisabledMasks GetDisabled() {
    DisabledMasks disabled{};

    if (!GostPublicKeyMethodAvailable) {
        disabled.authentication |= AuthGost01;
        disabled.keyExchange |= KeyGost;
    }

    disabled.encryption |= !CipherMethodAvailable[CipherDes] ? EncDes : 0;
    disabled.encryption |= !CipherMethodAvailable[Cipher3Des] ? Enc3Des : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherRc4] ? EncRc4 : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherIdea] ? EncIdea : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherAes128] ? EncAes128 : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherAes256] ? EncAes256 : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherAes128Gcm] ? EncAes128Gcm : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherAes256Gcm] ? EncAes256Gcm : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherCamellia128] ? EncCamellia128 : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherCamellia256] ? EncCamellia256 : 0;
    disabled.encryption |= !CipherMethodAvailable[CipherGost89] ? EncGost89Cnt : 0;

    disabled.mac |= !DigestMethodAvailable[DigestMd5] ? MacMd5 : 0;
    disabled.mac |= !DigestMethodAvailable[DigestSha1] ? MacSha1 : 0;
    disabled.mac |= !DigestMethodAvailable[DigestSha256] ? MacSha256 : 0;
    disabled.mac |= !DigestMethodAvailable[DigestSha384] ? MacSha384 : 0;
    disabled.mac |= !DigestMethodAvailable[DigestGost94] ? MacGost94 : 0;
    disabled.mac |= !DigestMethodAvailable[DigestGost89Mac] ? MacGost89 : 0;
    disabled.mac |= !DigestMethodAvailable[DigestStreebog256] ? MacStreebog256 : 0;

    return disabled;
}

void CollectCiphers(int numberOfCiphers, const DisabledMasks& disabled, std::vector<CipherOrder>& coList,
    CipherOrder** headOut, CipherOrder** tailOut) {
    int coListNum = 0;
    for (int index = 0; index < numberOfCiphers; ++index) {
        const Cipher* const cipher = static_cast<const Cipher*>(LibreSslCipherAt_nid_no_patch(static_cast<unsigned int>(index)));
        if (cipher != nullptr && cipher->valid &&
            !(cipher->keyExchange & disabled.keyExchange) &&
            !(cipher->authentication & disabled.authentication) &&
            !(cipher->encryption & disabled.encryption) &&
            !(cipher->mac & disabled.mac) &&
            !(cipher->protocol & disabled.protocol)) {
            coList[coListNum].cipher = cipher;
            coList[coListNum].next = nullptr;
            coList[coListNum].prev = nullptr;
            coList[coListNum].active = 0;
            ++coListNum;
        }
    }

    if (coListNum > 0) {
        coList[0].prev = nullptr;

        if (coListNum > 1) {
            coList[0].next = &coList[1];

            for (int index = 1; index < coListNum - 1; ++index) {
                coList[index].prev = &coList[index - 1];
                coList[index].next = &coList[index + 1];
            }

            coList[coListNum - 1].prev = &coList[coListNum - 2];
        }

        coList[coListNum - 1].next = nullptr;

        *headOut = &coList[0];
        *tailOut = &coList[coListNum - 1];
    }
}

void CollectAliases(std::vector<const Cipher*>& caList, int numberOfGroupAliases, const DisabledMasks& disabled,
    CipherOrder* head) {
    const std::uint64_t maskKeyExchange = ~disabled.keyExchange;
    const std::uint64_t maskAuthentication = ~disabled.authentication;
    const std::uint64_t maskEncryption = ~disabled.encryption;
    const std::uint64_t maskMac = ~disabled.mac;
    const std::uint64_t maskProtocol = ~disabled.protocol;

    std::size_t current = 0;
    for (CipherOrder* order = head; order != nullptr; order = order->next) {
        caList[current] = order->cipher;
        ++current;
    }

    for (int index = 0; index < numberOfGroupAliases; ++index) {
        const std::uint64_t keyExchange = Aliases[index].keyExchange;
        const std::uint64_t authentication = Aliases[index].authentication;
        const std::uint64_t encryption = Aliases[index].encryption;
        const std::uint64_t mac = Aliases[index].mac;
        const std::uint64_t protocol = Aliases[index].protocol;

        if (keyExchange)
            if ((keyExchange & maskKeyExchange) == 0)
                continue;

        if (authentication)
            if ((authentication & maskAuthentication) == 0)
                continue;

        if (encryption)
            if ((encryption & maskEncryption) == 0)
                continue;

        if (mac)
            if ((mac & maskMac) == 0)
                continue;

        if (protocol)
            if ((protocol & maskProtocol) == 0)
                continue;

        caList[current] = &Aliases[index];
        ++current;
    }

    caList[current] = nullptr;
}

void ApplyRule(std::uint64_t keyExchange, std::uint64_t authentication, std::uint64_t encryption, std::uint64_t mac,
    std::uint64_t protocol, std::uint64_t strengthFlags, int rule, int strengthBits, CipherOrder** headOut,
    CipherOrder** tailOut) {
    CipherOrder* head;
    CipherOrder* tail;
    CipherOrder* current;
    CipherOrder* next;
    CipherOrder* last;
    const Cipher* cipher;
    bool reverse = false;

    if (rule == RuleDel) reverse = true;

    head = *headOut;
    tail = *tailOut;

    if (reverse) {
        next = tail;
        last = head;
    } else {
        next = head;
        last = tail;
    }

    current = nullptr;
    for (;;) {
        if (current == last) break;
        current = next;
        next = reverse ? current->prev : current->next;

        cipher = current->cipher;

        if (strengthBits >= 0) {
            if (strengthBits != cipher->strengthBits) continue;
        } else {
            if (keyExchange && !(keyExchange & cipher->keyExchange)) continue;
            if (authentication && !(authentication & cipher->authentication)) continue;
            if (encryption && !(encryption & cipher->encryption)) continue;
            if (mac && !(mac & cipher->mac)) continue;
            if (protocol && !(protocol & cipher->protocol)) continue;
            if ((strengthFlags & StrongMask) && !(strengthFlags & StrongMask & cipher->strengthFlags)) continue;
        }

        if (rule == RuleAdd) {
            if (!current->active) {
                LinkAppendTail(&head, current, &tail);
                current->active = 1;
            }
        } else if (rule == RuleOrd) {
            if (current->active) {
                LinkAppendTail(&head, current, &tail);
            }
        } else if (rule == RuleDel) {
            if (current->active) {
                LinkAppendHead(&head, current, &tail);
                current->active = 0;
            }
        } else if (rule == RuleKill) {
            if (head == current)
                head = current->next;
            else
                current->prev->next = current->next;
            if (tail == current) tail = current->prev;
            current->active = 0;
            if (current->next != nullptr) current->next->prev = current->prev;
            if (current->prev != nullptr) current->prev->next = current->next;
            current->next = nullptr;
            current->prev = nullptr;
        }
    }

    *headOut = head;
    *tailOut = tail;
}

int StrengthSort(CipherOrder** headOut, CipherOrder** tailOut) {
    int maxStrengthBits;
    CipherOrder* current;
    std::vector<int> numberUses;

    maxStrengthBits = 0;
    current = *headOut;
    while (current != nullptr) {
        if (current->active && (current->cipher->strengthBits > maxStrengthBits))
            maxStrengthBits = current->cipher->strengthBits;
        current = current->next;
    }

    if (!AllocateScratch(numberUses, static_cast<std::size_t>(maxStrengthBits) + 1)) {
        LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, MallocFailure, __FILE__, __LINE__);
        return 0;
    }

    current = *headOut;
    while (current != nullptr) {
        if (current->active) numberUses[current->cipher->strengthBits]++;
        current = current->next;
    }

    for (int index = maxStrengthBits; index >= 0; index--)
        if (numberUses[index] > 0) ApplyRule(0, 0, 0, 0, 0, 0, RuleOrd, index, headOut, tailOut);

    return 1;
}

int ProcessRuleString(const char* ruleString, CipherOrder** headOut, CipherOrder** tailOut,
    const std::vector<const Cipher*>& caList) {
    std::uint64_t keyExchange, authentication, encryption, mac, protocol;
    std::uint64_t strengthFlags;
    int multi, found = 0, rule, retval, ok, buflen = 0;
    std::size_t j;
    const char* l;
    const char* buf;
    char ch;

    retval = 1;
    l = ruleString;
    for (;;) {
        ch = *l;

        if (ch == '\0') break;

        if (ch == '-') {
            rule = RuleDel;
            l++;
        } else if (ch == '+') {
            rule = RuleOrd;
            l++;
        } else if (ch == '!') {
            rule = RuleKill;
            l++;
        } else if (ch == '@') {
            rule = RuleSpecial;
            l++;
        } else {
            rule = RuleAdd;
        }

        if (IsItemSeparator(ch)) {
            l++;
            continue;
        }

        keyExchange = 0;
        authentication = 0;
        encryption = 0;
        mac = 0;
        protocol = 0;
        strengthFlags = 0;

        for (;;) {
            ch = *l;
            buf = l;
            buflen = 0;
            while ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || ch == '-' ||
                ch == '.') {
                ch = *(++l);
                buflen++;
            }

            if (buflen == 0) {
                LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, InvalidCommand, __FILE__, __LINE__);
                retval = found = 0;
                if (*l != '\0') l++;
                break;
            }

            if (rule == RuleSpecial) {
                found = 0;
                break;
            }

            if (ch == '+') {
                multi = 1;
                l++;
            } else {
                multi = 0;
            }

            j = 0;
            found = 0;
            while (caList[j]) {
                if (!std::strncmp(buf, caList[j]->name, static_cast<std::size_t>(buflen)) &&
                    (caList[j]->name[buflen] == '\0')) {
                    found = 1;
                    break;
                } else {
                    j++;
                }
            }

            if (!found) break;

            if (caList[j]->keyExchange) {
                if (keyExchange) {
                    keyExchange &= caList[j]->keyExchange;
                    if (!keyExchange) {
                        found = 0;
                        break;
                    }
                } else {
                    keyExchange = caList[j]->keyExchange;
                }
            }

            if (caList[j]->authentication) {
                if (authentication) {
                    authentication &= caList[j]->authentication;
                    if (!authentication) {
                        found = 0;
                        break;
                    }
                } else {
                    authentication = caList[j]->authentication;
                }
            }

            if (caList[j]->encryption) {
                if (encryption) {
                    encryption &= caList[j]->encryption;
                    if (!encryption) {
                        found = 0;
                        break;
                    }
                } else {
                    encryption = caList[j]->encryption;
                }
            }

            if (caList[j]->mac) {
                if (mac) {
                    mac &= caList[j]->mac;
                    if (!mac) {
                        found = 0;
                        break;
                    }
                } else {
                    mac = caList[j]->mac;
                }
            }

            if (caList[j]->strengthFlags & StrongMask) {
                if (strengthFlags & StrongMask) {
                    strengthFlags &= (caList[j]->strengthFlags & StrongMask) | ~StrongMask;
                    if (!(strengthFlags & StrongMask)) {
                        found = 0;
                        break;
                    }
                } else {
                    strengthFlags |= caList[j]->strengthFlags & StrongMask;
                }
            }

            if (!caList[j]->valid && caList[j]->protocol) {
                if (protocol) {
                    protocol &= caList[j]->protocol;
                    if (!protocol) {
                        found = 0;
                        break;
                    }
                } else {
                    protocol = caList[j]->protocol;
                }
            }

            if (!multi) break;
        }

        if (rule == RuleSpecial) {
            ok = 0;
            if ((buflen == 8) && !std::strncmp(buf, "STRENGTH", 8))
                ok = StrengthSort(headOut, tailOut);
            else
                LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, InvalidCommand, __FILE__, __LINE__);
            if (ok == 0) retval = 0;
            while ((*l != '\0') && !IsItemSeparator(*l)) l++;
        } else if (found) {
            ApplyRule(keyExchange, authentication, encryption, mac, protocol, strengthFlags, rule, -1, headOut, tailOut);
        } else {
            while ((*l != '\0') && !IsItemSeparator(*l)) l++;
        }
        if (*l == '\0') break;
    }

    return retval;
}

}

extern "C" {

int LibreSslCipherCount_nid_no_patch() {
    return CipherCount;
}

const void* LibreSslCipherAt_nid_no_patch(unsigned int index) {
    if (index < static_cast<unsigned int>(CipherCount)) return &Ciphers[CipherCount - 1 - static_cast<int>(index)];
    return nullptr;
}

const void* LibreSslCipherById_nid_no_patch(std::uint64_t id) {
    for (const Cipher& cipher : Ciphers) {
        if (cipher.id == id) return &cipher;
    }
    return nullptr;
}

LibreSslStack* LibreSslCreateCipherList_nid_no_patch(LibreSslStack** cipherList, LibreSslStack** cipherListById,
    const char* rules) {
    int ok, numberOfCiphers, numberOfAliasMax, numberOfGroupAliases;
    LibreSslStack* cipherStack;
    LibreSslStack* tmpCipherList;
    const char* ruleP;
    std::vector<CipherOrder> coList;
    CipherOrder* head = nullptr;
    CipherOrder* tail = nullptr;
    CipherOrder* current;
    std::vector<const Cipher*> caList;

    if (rules == nullptr || cipherList == nullptr || cipherListById == nullptr) return nullptr;

    const DisabledMasks disabled = GetDisabled();

    numberOfCiphers = LibreSslCipherCount_nid_no_patch();
    if (!AllocateScratch(coList, static_cast<std::size_t>(numberOfCiphers))) {
        LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, MallocFailure, __FILE__, __LINE__);
        return nullptr;
    }

    CollectCiphers(numberOfCiphers, disabled, coList, &head, &tail);

    ApplyRule(KeyEcdhe, 0, 0, 0, 0, 0, RuleAdd, -1, &head, &tail);
    ApplyRule(KeyEcdhe, 0, 0, 0, 0, 0, RuleDel, -1, &head, &tail);

    if (AesAccelerated) {
        ApplyRule(0, 0, EncAes, 0, 0, 0, RuleAdd, -1, &head, &tail);
        ApplyRule(0, 0, EncChaCha20Poly1305, 0, 0, 0, RuleAdd, -1, &head, &tail);
        ApplyRule(0, 0, EncChaCha20Poly1305Old, 0, 0, 0, RuleAdd, -1, &head, &tail);
    } else {
        ApplyRule(0, 0, EncChaCha20Poly1305, 0, 0, 0, RuleAdd, -1, &head, &tail);
        ApplyRule(0, 0, EncChaCha20Poly1305Old, 0, 0, 0, RuleAdd, -1, &head, &tail);
        ApplyRule(0, 0, EncAes, 0, 0, 0, RuleAdd, -1, &head, &tail);
    }

    ApplyRule(0, 0, 0, 0, 0, 0, RuleAdd, -1, &head, &tail);
    ApplyRule(0, 0, 0, MacMd5, 0, 0, RuleOrd, -1, &head, &tail);
    ApplyRule(0, AuthNull, 0, 0, 0, 0, RuleOrd, -1, &head, &tail);
    ApplyRule(KeyRsa, 0, 0, 0, 0, 0, RuleOrd, -1, &head, &tail);
    ApplyRule(0, 0, EncRc4, 0, 0, 0, RuleOrd, -1, &head, &tail);

    if (!StrengthSort(&head, &tail)) return nullptr;

    ApplyRule(0, 0, 0, 0, 0, 0, RuleDel, -1, &head, &tail);

    numberOfGroupAliases = AliasCount;
    numberOfAliasMax = numberOfCiphers + numberOfGroupAliases + 1;
    if (!AllocateScratch(caList, static_cast<std::size_t>(numberOfAliasMax))) {
        LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, MallocFailure, __FILE__, __LINE__);
        return nullptr;
    }
    CollectAliases(caList, numberOfGroupAliases, disabled, head);

    ok = 1;
    ruleP = rules;
    if (std::strncmp(rules, "DEFAULT", 7) == 0) {
        ok = ProcessRuleString(DefaultCipherList, &head, &tail, caList);
        ruleP += 7;
        if (*ruleP == ':') ruleP++;
    }

    if (ok && (std::strlen(ruleP) > 0)) ok = ProcessRuleString(ruleP, &head, &tail, caList);

    if (!ok) return nullptr;

    if ((cipherStack = StackNewNull()) == nullptr) return nullptr;

    for (current = head; current != nullptr; current = current->next) {
        if (current->active) StackPush(cipherStack, const_cast<Cipher*>(current->cipher));
    }

    tmpCipherList = StackDup(cipherStack);
    if (tmpCipherList == nullptr) {
        StackFree(cipherStack);
        return nullptr;
    }
    StackFree(*cipherList);
    *cipherList = cipherStack;
    StackFree(*cipherListById);
    *cipherListById = tmpCipherList;
    StackSetCompare(*cipherListById, CipherPointerIdCompare);

    StackSort(*cipherListById);
    return cipherStack;
}

int APS5_VABI SSL_CTX_set_cipher_list_nid_postfix(LibreSslCtx* ctx, const char* rules) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (!ctx->internal) throw std::invalid_argument(std::string(__func__) + ": null internal state");
    auto* cipherList = static_cast<LibreSslStack*>(ctx->cipherList);
    auto* cipherListById = static_cast<LibreSslStack*>(ctx->internal->cipherListById);
    LibreSslStack* const stack = LibreSslCreateCipherList_nid_no_patch(&cipherList, &cipherListById, rules);
    ctx->cipherList = cipherList;
    ctx->internal->cipherListById = cipherListById;
    if (stack == nullptr) return 0;
    if (stack->num == 0) {
        LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, NoCipherMatch, __FILE__, __LINE__);
        return 0;
    }
    return 1;
}

}
