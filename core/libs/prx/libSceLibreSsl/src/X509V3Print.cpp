/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/x509v3/v3_prn.c (v3_prn.c,v 1.19),
 * x509v3/v3_lib.c (v3_lib.c,v 1.17), x509v3/ext_dat.h (ext_dat.h,v 1.13), x509v3/v3_ocsp.c (v3_ocsp.c,v 1.15),
 * asn1/f_string.c (f_string.c,v 1.17), asn1/f_int.c (f_int.c,v 1.18), asn1/asn1_lib.c (asn1_lib.c,v 1.38),
 * bio/b_dump.c (b_dump.c,v 1.21) and bio/b_print.c (b_print.c,v 1.25) */
/* $OpenBSD: v3_prn.c,v 1.19 2016/12/30 15:54:49 jsing Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
/* X509 v3 extension utilities */
/* $OpenBSD: v3_lib.c,v 1.17 2017/01/29 17:49:23 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
/* X509 v3 extension utilities */
/* $OpenBSD: ext_dat.h,v 1.13 2016/12/21 15:49:29 jsing Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: v3_ocsp.c,v 1.15 2017/01/29 17:49:23 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: f_string.c,v 1.17 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: f_int.c,v 1.18 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: b_dump.c,v 1.21 2015/04/23 06:11:19 deraadt Exp $ */
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
/*
 * Stolen from tjh's ssl/ssl_trc.c stuff.
 */
/* $OpenBSD: b_print.c,v 1.25 2014/06/12 15:49:28 deraadt Exp $ */
/* Theo de Raadt places this file in the public domain. */
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Asn1Der.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/OcspResponse.hpp"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int OcspLibrary = 39;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int OctetStringType = 4;
constexpr int NegativeFlag = 0x100;
constexpr std::uint64_t UnknownMask = std::uint64_t {0xf} << 16;
constexpr std::uint64_t DefaultUnknown = 0;
constexpr std::uint64_t ErrorUnknown = std::uint64_t {1} << 16;
constexpr std::uint64_t ParseUnknown = std::uint64_t {2} << 16;
constexpr std::uint64_t DumpUnknown = std::uint64_t {3} << 16;
constexpr int DumpWidth = 16;
constexpr int MaximumDumpIndent = 128;

struct Extension {
    LibreSslAsn1Object* object;
    int critical;
    Asn1String* value;
};

static_assert(sizeof(Extension) == 24 && offsetof(Extension, critical) == 8 && offsetof(Extension, value) == 16);

enum class Method { Unported, Nonce, CrlId, GeneralizedTime, NoCheck, Object };

struct StandardExtension {
    int nid;
    Method method;
};

constexpr StandardExtension StandardExtensions[] {
    {71, Method::Unported}, {72, Method::Unported}, {73, Method::Unported}, {74, Method::Unported},
    {75, Method::Unported}, {76, Method::Unported}, {77, Method::Unported}, {78, Method::Unported},
    {82, Method::Unported}, {83, Method::Unported}, {84, Method::Unported}, {85, Method::Unported},
    {86, Method::Unported}, {87, Method::Unported}, {88, Method::Unported}, {89, Method::Unported},
    {90, Method::Unported}, {103, Method::Unported}, {126, Method::Unported}, {140, Method::Unported},
    {141, Method::Unported}, {142, Method::GeneralizedTime}, {143, Method::Unported}, {177, Method::Unported},
    {366, Method::Nonce}, {367, Method::CrlId}, {368, Method::Unported}, {369, Method::NoCheck},
    {370, Method::GeneralizedTime}, {371, Method::Unported}, {398, Method::Unported}, {401, Method::Unported},
    {430, Method::Object}, {663, Method::Unported}, {666, Method::Unported}, {747, Method::Unported},
    {748, Method::Unported}, {770, Method::Unported}, {771, Method::Unported}, {857, Method::Unported},
};

static_assert(std::is_sorted(std::begin(StandardExtensions), std::end(StandardExtensions),
    [](const StandardExtension& left, const StandardExtension& right) { return left.nid < right.nid; }));

const StandardExtension* FindExtension(int nid) {
    if (nid < 0) return nullptr;
    const auto* found = std::lower_bound(std::begin(StandardExtensions), std::end(StandardExtensions), nid,
        [](const StandardExtension& entry, int value) { return entry.nid < value; });
    if (found == std::end(StandardExtensions) || found->nid != nid) return nullptr;
    return found;
}

int Indented(LibreSsl::Bio* out, int indent, std::string_view text) {
    if (indent == INT_MIN) throw std::invalid_argument("X509V3_EXT_print: indent INT_MIN has no field width");
    const std::size_t padding = static_cast<std::size_t>(indent < 0 ? -indent : indent);
    if (padding + text.size() > static_cast<std::size_t>(INT_MAX)) return -1;
    std::string line(padding, ' ');
    line.append(text);
    LibreSslBioWrite_nid_no_patch(out, line.data(), static_cast<int>(line.size()));
    return static_cast<int>(line.size());
}

int WriteHex(LibreSsl::Bio* out, const Asn1String& string, int written) {
    static constexpr char digits[] = "0123456789ABCDEF";
    for (int index = 0; index < string.length; ++index) {
        if (index != 0 && index % 35 == 0) {
            if (LibreSslBioWrite_nid_no_patch(out, "\\\n", 2) != 2) return -1;
            written += 2;
        }
        const char pair[2] {digits[(string.data[index] >> 4) & 0x0f], digits[string.data[index] & 0x0f]};
        if (LibreSslBioWrite_nid_no_patch(out, pair, 2) != 2) return -1;
        written += 2;
    }
    return written;
}

int PrintString(LibreSsl::Bio* out, const Asn1String& string) {
    if (string.length == 0) return LibreSslBioWrite_nid_no_patch(out, "0", 1) == 1 ? 1 : -1;
    return WriteHex(out, string, 0);
}

int PrintInteger(LibreSsl::Bio* out, const Asn1String& integer) {
    int written = 0;
    if (integer.type & NegativeFlag) {
        if (LibreSslBioWrite_nid_no_patch(out, "-", 1) != 1) return -1;
        written = 1;
    }
    if (integer.length == 0) return LibreSslBioWrite_nid_no_patch(out, "00", 2) == 2 ? written + 2 : -1;
    return WriteHex(out, integer, written);
}

int Dump(LibreSsl::Bio* out, const unsigned char* data, int length, int indent) {
    int trailing = 0;
    for (; length > 0 && (data[length - 1] == ' ' || data[length - 1] == '\0'); --length) ++trailing;
    indent = std::clamp(indent, 0, MaximumDumpIndent);
    const std::string prefix(static_cast<std::size_t>(indent), ' ');
    const int width = DumpWidth - ((indent - (indent > 6 ? 6 : indent) + 3) / 4);
    if (width == 0) throw std::domain_error("X509V3_EXT_print: BIO_dump_indent divides by a zero dump width");
    int rows = length / width;
    if (rows * width < length) ++rows;
    int result = 0;
    char field[20];
    for (int row = 0; row < rows; ++row) {
        std::string line = prefix;
        std::snprintf(field, sizeof field, "%04x - ", static_cast<unsigned>(row * width));
        line += field;
        for (int column = 0; column < width; ++column) {
            if (row * width + column >= length) {
                line += "   ";
            } else {
                std::snprintf(field, sizeof field, "%02x%c", data[row * width + column], column == 7 ? '-' : ' ');
                line += field;
            }
        }
        line += "  ";
        for (int column = 0; column < width; ++column) {
            if (row * width + column >= length) break;
            const unsigned char character = data[row * width + column];
            line += character >= ' ' && character <= '~' ? static_cast<char>(character) : '.';
        }
        line += "\n";
        result += LibreSslBioWrite_nid_no_patch(out, line.data(), static_cast<int>(line.size()));
    }
    if (trailing > 0) {
        std::snprintf(field, sizeof field, "%04x", static_cast<unsigned>(length + trailing));
        const std::string line = prefix + field + " - <SPACES/NULS>\n";
        result += LibreSslBioWrite_nid_no_patch(out, line.data(), static_cast<int>(line.size()));
    }
    return result;
}

const Asn1String& ExtensionValue(const Extension& extension) {
    const Asn1String* value = extension.value;
    if (!value) throw std::invalid_argument("X509V3_EXT_print: null extension value");
    if (value->length < 0) throw std::invalid_argument("X509V3_EXT_print: negative extension value length");
    if (!value->data && value->length) throw std::invalid_argument("X509V3_EXT_print: null extension value data");
    return *value;
}

int PrintUnknown(LibreSsl::Bio* out, const Extension& extension, std::uint64_t flag, int indent, bool supported) {
    switch (flag & UnknownMask) {
    case DefaultUnknown:
        return 0;
    case ErrorUnknown:
        Indented(out, indent, supported ? "<Parse Error>" : "<Not Supported>");
        return 1;
    case ParseUnknown:
        ExtensionValue(extension);
        NotImplemented_nid_no_patch("X509V3_EXT_print: ASN1_parse_dump of an extension value");
    case DumpUnknown: {
        const Asn1String& value = ExtensionValue(extension);
        return Dump(out, value.data, value.length, indent);
    }
    default:
        return 1;
    }
}

Asn1String* DecodeNonce(const Asn1String& value) {
    Asn1String* nonce = LibreSslDer::NewString(OctetStringType);
    if (nonce) {
        auto* data = static_cast<unsigned char*>(realloc_nid_postfix(nullptr, static_cast<std::size_t>(value.length) + 1));
        if (data) {
            nonce->data = data;
            nonce->length = value.length;
            if (value.data) {
                std::memcpy(data, value.data, static_cast<std::size_t>(value.length));
                data[value.length] = 0;
            }
            return nonce;
        }
        LibreSslDer::Fail(MallocFailure);
        LibreSslAsn1StringFree_nid_no_patch(nonce);
    }
    LibreSslPutError_nid_no_patch(OcspLibrary, AnyFunction, MallocFailure);
    return nullptr;
}

LibreSslExtensionValue ValueType(Method method) {
    switch (method) {
    case Method::GeneralizedTime:
        return LibreSslExtensionValue::GeneralizedTime;
    case Method::NoCheck:
        return LibreSslExtensionValue::Null;
    case Method::Object:
        return LibreSslExtensionValue::Object;
    case Method::CrlId:
        return LibreSslExtensionValue::CrlId;
    default:
        throw std::logic_error("X509V3_EXT_print: method without an ASN.1 item");
    }
}

int PrintCrlId(LibreSsl::Bio* out, const LibreSslOcspCrlId& crlId, int indent) {
    if (crlId.crlUrl) {
        if (Indented(out, indent, "crlUrl: ") <= 0) return 0;
        if (!ASN1_STRING_print_nid_postfix(out, crlId.crlUrl)) return 0;
        if (LibreSslBioWrite_nid_no_patch(out, "\n", 1) <= 0) return 0;
    }
    if (crlId.crlNum) {
        if (Indented(out, indent, "crlNum: ") <= 0) return 0;
        if (PrintInteger(out, *crlId.crlNum) <= 0) return 0;
        if (LibreSslBioWrite_nid_no_patch(out, "\n", 1) <= 0) return 0;
    }
    if (crlId.crlTime) {
        if (Indented(out, indent, "crlTime: ") <= 0) return 0;
        if (!ASN1_TIME_print_nid_postfix(out, crlId.crlTime)) return 0;
        if (LibreSslBioWrite_nid_no_patch(out, "\n", 1) <= 0) return 0;
    }
    return 1;
}

int PrintValue(LibreSsl::Bio* out, Method method, void* value, int indent) {
    switch (method) {
    case Method::Nonce:
        if (Indented(out, indent, "") <= 0) return 0;
        return PrintString(out, *static_cast<Asn1String*>(value)) > 0;
    case Method::CrlId:
        return PrintCrlId(out, *static_cast<LibreSslOcspCrlId*>(value), indent);
    case Method::GeneralizedTime:
        if (Indented(out, indent, "") <= 0) return 0;
        return ASN1_TIME_print_nid_postfix(out, static_cast<Asn1String*>(value)) != 0;
    case Method::NoCheck:
        return 1;
    case Method::Object:
        if (Indented(out, indent, "") <= 0) return 0;
        return i2a_ASN1_OBJECT_nid_postfix(out, static_cast<LibreSslAsn1Object*>(value)) > 0;
    case Method::Unported:
        break;
    }
    throw std::logic_error("X509V3_EXT_print: method without a printer");
}

}

extern "C" {

int APS5_VABI X509V3_EXT_print_nid_postfix(LibreSsl::Bio* out, void* extension, std::uint64_t flag, int indent) {
    if (!extension) throw std::invalid_argument(std::string(__func__) + ": null extension");
    const auto& ext = *static_cast<const Extension*>(extension);
    const int nid = LibreSslObjectNid_nid_no_patch(ext.object);
    const StandardExtension* standard = nid == 0 ? nullptr : FindExtension(nid);
    if (!standard) return PrintUnknown(out, ext, flag, indent, false);
    const Asn1String& value = ExtensionValue(ext);
    if (standard->method == Method::Unported)
        NotImplemented_nid_no_patch(("X509V3_EXT_print: the extension method of NID " + std::to_string(nid)).c_str());
    void* decoded = standard->method == Method::Nonce ? DecodeNonce(value)
        : LibreSslExtensionValueDecode_nid_no_patch(ValueType(standard->method), value.data, value.length);
    if (!decoded) return PrintUnknown(out, ext, flag, indent, true);
    const int result = PrintValue(out, standard->method, decoded, indent);
    if (standard->method == Method::Nonce) LibreSslAsn1StringFree_nid_no_patch(static_cast<Asn1String*>(decoded));
    else LibreSslExtensionValueFree_nid_no_patch(ValueType(standard->method), decoded);
    return result;
}

}
