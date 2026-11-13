/* $OpenBSD: rsa_eay.c,v 1.65 2023/08/09 12:09:06 tb Exp $ */
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
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: rsa_lib.c,v 1.50 2024/03/27 01:22:30 tb Exp $ */
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

/* $OpenBSD: rsa_chk.c,v 1.18 2023/07/08 12:26:45 beck Exp $ */
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
 *    openssl-core@OpenSSL.org.
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
 */

/* $OpenBSD: rsa_oaep.c,v 1.39 2024/03/26 05:37:28 joshua Exp $ */
/*
 * Copyright 1999-2018 The OpenSSL Project Authors. All Rights Reserved.
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

/* EME-OAEP as defined in RFC 2437 (PKCS #1 v2.0) */

/* See Victor Shoup, "OAEP reconsidered," Nov. 2000,
 * <URL: http://www.shoup.net/papers/oaep.ps.Z>
 * for problems with the security proof for the
 * original OAEP scheme, which EME-OAEP is based on.
 *
 * A new proof can be found in E. Fujisaki, T. Okamoto,
 * D. Pointcheval, J. Stern, "RSA-OEAP is Still Alive!",
 * Dec. 2000, <URL: http://eprint.iacr.org/2000/061/>.
 * The new proof has stronger requirements for the
 * underlying permutation: "partial-one-wayness" instead
 * of one-wayness.  For the RSA function, this is
 * an equivalent notion.
 */

/* $OpenBSD: rsa_pk1.c,v 1.17 2024/03/30 04:34:17 jsing Exp $ */
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

/* $OpenBSD: rsa_pss.c,v 1.19 2024/03/26 05:26:27 joshua Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2005.
 */
/* ====================================================================
 * Copyright (c) 2005 The OpenSSL Project.  All rights reserved.
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

/* $OpenBSD: rsa_sign.c,v 1.36 2023/07/08 12:26:45 beck Exp $ */
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

/* $OpenBSD: rsa_none.c,v 1.12 2023/07/08 12:26:45 beck Exp $ */
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

#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "mbedtls/bignum.h"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"

namespace {

using LibreSsl3::DigestContext;
using LibreSsl3::DigestMethod;

constexpr int RsaLibrary = 4;
constexpr int BigNumberLibrary = 3;
constexpr int AnyFunction = 0xfff;
constexpr int AllocationFailure = 65;
constexpr int InternalFailure = 68;

constexpr int ReasonBadExponentValue = 101;
constexpr int ReasonBadFixedHeaderDecrypt = 102;
constexpr int ReasonBadPadByteCount = 103;
constexpr int ReasonBadSignature = 104;
constexpr int ReasonModulusTooLarge = 105;
constexpr int ReasonBlockTypeIsNotOne = 106;
constexpr int ReasonBlockTypeIsNotTwo = 107;
constexpr int ReasonDataGreaterThanModulusLength = 108;
constexpr int ReasonDataTooLarge = 109;
constexpr int ReasonDataTooLargeForKeySize = 110;
constexpr int ReasonDigestTooBigForRsaKey = 112;
constexpr int ReasonNullBeforeBlockMissing = 113;
constexpr int ReasonPaddingCheckFailed = 114;
constexpr int ReasonObjectIdentifierNotKnownForDigest = 116;
constexpr int ReasonUnknownAlgorithmType = 117;
constexpr int ReasonUnknownPaddingType = 118;
constexpr int ReasonWrongSignatureLength = 119;
constexpr int ReasonKeySizeTooSmall = 120;
constexpr int ReasonOaepDecodingError = 121;
constexpr int ReasonDataTooSmallForKeySize = 122;
constexpr int ReasonInvalidMessageLength = 131;
constexpr int ReasonDataTooLargeForModulus = 132;
constexpr int ReasonFirstOctetInvalid = 133;
constexpr int ReasonLastOctetInvalid = 134;
constexpr int ReasonSlenRecoveryFailed = 135;
constexpr int ReasonSlenCheckFailed = 136;
constexpr int ReasonInvalidDigestLength = 143;
constexpr int ReasonValueMissing = 147;
constexpr int ReasonDeNotCongruentToOne = 123;
constexpr int ReasonDmp1NotCongruentToD = 124;
constexpr int ReasonDmq1NotCongruentToD = 125;
constexpr int ReasonIqmpNotInverseOfQ = 126;
constexpr int ReasonModulusNotProductOfPrimes = 127;
constexpr int ReasonPNotPrime = 128;
constexpr int ReasonQNotPrime = 129;
constexpr int BigNumberReasonDivisionByZero = 103;
constexpr int BigNumberReasonNoInverse = 108;

constexpr int Md5Sha1Type = 114;
constexpr int Md5Sha1Length = 36;

constexpr int MaximumModulusBits = 16384;
constexpr int SmallModulusBits = 3072;
constexpr int MaximumPublicExponentBits = 64;
constexpr int BlindingDraws = 33;
constexpr int PrimalityRounds = 64;
constexpr int DefaultPublicExponent = 0x10001;
constexpr int BignumAllocated = 1;

void QueueError(int reason) {
    LibreSslPutError_nid_no_patch(RsaLibrary, AnyFunction, reason);
}

void QueueBigNumberError(int reason) {
    LibreSslPutError_nid_no_patch(BigNumberLibrary, AnyFunction, reason);
}

[[noreturn]] void Reject(const std::string& message) {
    throw std::invalid_argument("libSceLibreSsl3 RSA: " + message);
}

void Wipe(void* memory, std::size_t bytes) {
    auto* target = static_cast<volatile unsigned char*>(memory);
    for (std::size_t index = 0; index < bytes; ++index) target[index] = 0;
}

const LibreSsl3Bignum& RequireComponent(const void* component, const char* function, const char* name) {
    if (!component) Reject(std::string(function) + ": null " + name);
    const auto& value = *static_cast<const LibreSsl3Bignum*>(component);
    if (value.top < 0 || value.top > std::numeric_limits<int>::max() / 64)
        Reject(std::string(function) + ": " + name + " limb count out of range");
    if (value.capacity < value.top)
        Reject(std::string(function) + ": " + name + " limb capacity is below its limb count");
    if (value.top != 0 && value.data == nullptr) Reject(std::string(function) + ": null " + name + " limbs");
    if (value.negative != 0)
        NotImplemented_nid_no_patch((std::string(function) + ": a negative " + name).c_str());
    return value;
}

int BitCount(const LibreSsl3Bignum& value) {
    if (value.top == 0) return 0;
    return (value.top - 1) * 64 + static_cast<int>(std::bit_width(value.data[value.top - 1]));
}

int ByteCount(const LibreSsl3Bignum& value) {
    return (BitCount(value) + 7) / 8;
}

int SignificantLimbs(int bits) {
    return (bits + 63) / 64;
}

int CompareComponents(const LibreSsl3Bignum& left, const LibreSsl3Bignum& right) {
    const int leftBits = BitCount(left);
    const int rightBits = BitCount(right);
    if (leftBits != rightBits) return leftBits < rightBits ? -1 : 1;
    for (int index = SignificantLimbs(leftBits) - 1; index >= 0; --index) {
        if (left.data[index] != right.data[index]) return left.data[index] > right.data[index] ? 1 : -1;
    }
    return 0;
}

void CheckNumberSize(const LibreSsl3Bignum& value, const char* what) {
    if (BitCount(value) > MBEDTLS_MPI_MAX_BITS)
        NotImplemented_nid_no_patch((std::string("libSceLibreSsl3 RSA: a ") + what + " over " +
            std::to_string(MBEDTLS_MPI_MAX_BITS) + " bits").c_str());
}

bool HandleMbedtls(int status, const char* function) {
    if (status == 0) return true;
    if (status == MBEDTLS_ERR_MPI_ALLOC_FAILED) {
        QueueBigNumberError(AllocationFailure);
        return false;
    }
    if (status == MBEDTLS_ERR_MPI_BAD_INPUT_DATA || status == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) return false;
    NotImplemented_nid_no_patch((std::string(function) + ": unexpected mbedTLS status " + std::to_string(status)).c_str());
}

struct Number {
    mbedtls_mpi value;

    Number() {
        mbedtls_mpi_init(&value);
    }

    Number(const Number&) = delete;
    Number& operator=(const Number&) = delete;

    Number(Number&& other) noexcept : value(other.value) {
        mbedtls_mpi_init(&other.value);
    }

    Number& operator=(Number&& other) noexcept {
        if (this != &other) {
            mbedtls_mpi_free(&value);
            value = other.value;
            mbedtls_mpi_init(&other.value);
        }
        return *this;
    }

    ~Number() {
        mbedtls_mpi_free(&value);
    }
};

Number ComponentNumber(const LibreSsl3Bignum& component, const char* function) {
    CheckNumberSize(component, "key component");
    Number number;
    if (!HandleMbedtls(mbedtls_mpi_read_binary_le(&number.value, reinterpret_cast<const unsigned char*>(component.data),
            static_cast<std::size_t>(component.top) * 8), function))
        Reject(std::string(function) + ": mbedTLS rejected a key component");
    return number;
}

Number BytesNumber(const unsigned char* bytes, std::size_t length, const char* function) {
    Number number;
    if (!HandleMbedtls(mbedtls_mpi_read_binary(&number.value, bytes, length), function))
        Reject(std::string(function) + ": mbedTLS rejected the input");
    return number;
}

bool WriteBytes(const Number& number, unsigned char* output, std::size_t length, const char* function) {
    return HandleMbedtls(mbedtls_mpi_write_binary(&number.value, output, length), function);
}

bool Power(const Number& base, const Number& exponent, const Number& modulus, Number& result, const char* function) {
    return HandleMbedtls(mbedtls_mpi_exp_mod(&result.value, &base.value, &exponent.value, &modulus.value, nullptr), function);
}

bool Remainder(Number& result, const Number& value, const Number& modulus, const char* function) {
    Number temporary;
    if (!HandleMbedtls(mbedtls_mpi_mod_mpi(&temporary.value, &value.value, &modulus.value), function)) return false;
    result = std::move(temporary);
    return true;
}

bool MultiplyRemainder(Number& result, const Number& left, const Number& right, const Number& modulus,
    const char* function) {
    Number product;
    Number temporary;
    if (!HandleMbedtls(mbedtls_mpi_mul_mpi(&product.value, &left.value, &right.value), function)) return false;
    if (!HandleMbedtls(mbedtls_mpi_mod_mpi(&temporary.value, &product.value, &modulus.value), function)) return false;
    result = std::move(temporary);
    return true;
}

bool Subtract(Number& result, const Number& left, const Number& right, const char* function) {
    Number temporary;
    if (!HandleMbedtls(mbedtls_mpi_sub_mpi(&temporary.value, &left.value, &right.value), function)) return false;
    result = std::move(temporary);
    return true;
}

bool InverseModulo(Number& result, const Number& value, const Number& modulus, const char* function) {
    Number temporary;
    const int status = mbedtls_mpi_inv_mod(&temporary.value, &value.value, &modulus.value);
    if (status == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) return false;
    if (!HandleMbedtls(status, function)) return false;
    result = std::move(temporary);
    return true;
}

bool IsOne(const Number& number) {
    return mbedtls_mpi_cmp_int(&number.value, 1) == 0;
}

bool IsOdd(const Number& number) {
    return mbedtls_mpi_get_bit(&number.value, 0) == 1;
}

int EntropySource(void*, unsigned char* output, std::size_t length) {
    HostEntropy_nid_no_patch(output, length);
    return 0;
}

int PrimalityCheck(const Number& candidate, const char* function) {
    if (mbedtls_mpi_cmp_int(&candidate.value, 2) == 0) return 1;
    if (mbedtls_mpi_cmp_int(&candidate.value, 1) <= 0 || !IsOdd(candidate)) return 0;
    const int status = mbedtls_mpi_is_prime_ext(&candidate.value, PrimalityRounds, EntropySource, nullptr);
    if (status == 0) return 1;
    if (status == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) return 0;
    HandleMbedtls(status, function);
    return -1;
}

bool CheckedRemainder(Number& result, const Number& value, const Number& modulus, const char* function) {
    if (mbedtls_mpi_cmp_int(&modulus.value, 0) == 0) {
        QueueBigNumberError(BigNumberReasonDivisionByZero);
        return false;
    }
    return Remainder(result, value, modulus, function);
}

bool RandomBelow(Number& result, const Number& modulus, const char* function) {
    const int bits = static_cast<int>(mbedtls_mpi_bitlen(&modulus.value));
    if (bits <= 0) return false;
    const int bytes = (bits + 7) / 8;
    const int spare = bytes * 8 - bits;
    Number candidate;
    do {
        if (!HandleMbedtls(mbedtls_mpi_lset(&candidate.value, 0), function)) return false;
        if (!HandleMbedtls(mbedtls_mpi_fill_random(&candidate.value, bytes, EntropySource, nullptr), function)) return false;
        if (spare > 0 && !HandleMbedtls(mbedtls_mpi_shift_r(&candidate.value, static_cast<size_t>(spare)), function)) return false;
    } while (mbedtls_mpi_cmp_mpi(&candidate.value, &modulus.value) >= 0);
    result = std::move(candidate);
    return true;
}

bool BlindingExponent(const LibreSsl3Rsa& rsa, Number& exponent, const char* function) {
    if (rsa.e) {
        exponent = ComponentNumber(RequireComponent(rsa.e, function, "exponent"), function);
        return true;
    }
    return HandleMbedtls(mbedtls_mpi_lset(&exponent.value, DefaultPublicExponent), function);
}

struct Blinding {
    Number factor;
    Number unblind;
    bool applied = false;
};

bool SetupBlinding(const LibreSsl3Rsa& rsa, const Number& modulusNumber, Blinding& blinding, const char* function) {
    if (rsa.blinding || rsa.mtBlinding)
        NotImplemented_nid_no_patch((std::string(function) + ": blinding state held by the key").c_str());
    if (rsa.methodModN || rsa.methodModP || rsa.methodModQ)
        NotImplemented_nid_no_patch((std::string(function) + ": a method Montgomery context").c_str());
    if (IsOne(modulusNumber)) return true;
    if (!IsOdd(modulusNumber)) return false;
    blinding.applied = true;
    Number exponent;
    if (!BlindingExponent(rsa, exponent, function)) return false;
    Number random;
    bool usable = false;
    for (int draw = 0; draw < BlindingDraws && !usable; ++draw) {
        usable = RandomBelow(random, modulusNumber, function) && InverseModulo(blinding.unblind, random, modulusNumber, function);
    }
    if (!usable) return false;
    Number blinded;
    if (!Power(random, exponent, modulusNumber, blinded, function)) return false;
    blinding.factor = std::move(blinded);
    return true;
}

bool CombinedChineseRemainder(Number& result, const Number& powerP, const Number& powerQ, const LibreSsl3Bignum& coefficient,
    const LibreSsl3Bignum& p, const LibreSsl3Bignum& q, const char* function) {
    const Number factor = ComponentNumber(coefficient, function);
    const Number primeP = ComponentNumber(p, function);
    const Number primeQ = ComponentNumber(q, function);
    Number difference;
    Number scaled;
    Number total;
    if (!Subtract(difference, powerP, powerQ, function)) return false;
    if (!MultiplyRemainder(scaled, difference, factor, primeP, function)) return false;
    if (!HandleMbedtls(mbedtls_mpi_mul_mpi(&total.value, &scaled.value, &primeQ.value), function)) return false;
    if (!HandleMbedtls(mbedtls_mpi_add_mpi(&total.value, &total.value, &powerQ.value), function)) return false;
    result = std::move(total);
    return true;
}

bool RemainderOfComponent(Number& result, const Number& value, const LibreSsl3Bignum& modulus, const char* function) {
    const Number divisor = ComponentNumber(modulus, function);
    return Remainder(result, value, divisor, function);
}

bool ChineseRemainderPower(const LibreSsl3Rsa& rsa, const Number& value, const Number& modulusNumber, Number& result,
    const char* function) {
    if (rsa.methodModP || rsa.methodModQ)
        NotImplemented_nid_no_patch((std::string(function) + ": a method Montgomery context").c_str());
    const auto& p = RequireComponent(rsa.p, function, "p");
    const auto& q = RequireComponent(rsa.q, function, "q");
    if (p.top == 0 || q.top == 0) return false;
    Number powerP;
    Number powerQ;
    Number reducedP;
    Number reducedQ;
    if (!RemainderOfComponent(reducedP, value, p, function)) return false;
    if (!Power(reducedP, ComponentNumber(RequireComponent(rsa.dmp1, function, "dmp1"), function), ComponentNumber(p, function),
            powerP, function))
        return false;
    if (!RemainderOfComponent(reducedQ, value, q, function)) return false;
    if (!Power(reducedQ, ComponentNumber(RequireComponent(rsa.dmq1, function, "dmq1"), function), ComponentNumber(q, function),
            powerQ, function))
        return false;
    if (!CombinedChineseRemainder(result, powerP, powerQ, RequireComponent(rsa.iqmp, function, "iqmp"), p, q, function))
        return false;
    if (rsa.e) {
        Number check;
        if (!Power(result, ComponentNumber(RequireComponent(rsa.e, function, "exponent"), function), modulusNumber, check,
                function))
            return false;
        if (mbedtls_mpi_cmp_mpi(&check.value, &value.value) == 0) return true;
    }
    if (!rsa.d) return true;
    return Power(value, ComponentNumber(RequireComponent(rsa.d, function, "private exponent"), function), modulusNumber, result,
        function);
}

bool PrivatePower(const LibreSsl3Rsa& rsa, const Number& value, Number& result, const char* function) {
    const auto& modulus = RequireComponent(rsa.n, function, "modulus");
    CheckNumberSize(modulus, "modulus");
    const Number modulusNumber = ComponentNumber(modulus, function);
    Blinding blinding;
    Number blinded;
    if ((rsa.flags & LibreSsl3RsaFlagNoBlinding) == 0) {
        if (!SetupBlinding(rsa, modulusNumber, blinding, function)) {
            LibreSslPutError_nid_no_patch(RsaLibrary, AnyFunction, InternalFailure);
            return false;
        }
        if (blinding.applied && !MultiplyRemainder(blinded, value, blinding.factor, modulusNumber, function)) return false;
    }
    const bool usesChineseRemainder = (rsa.flags & LibreSsl3RsaFlagExternalPrivateKey) != 0 ||
        (rsa.p && rsa.q && rsa.dmp1 && rsa.dmq1 && rsa.iqmp);
    Number worked;
    if (usesChineseRemainder) {
        if (!ChineseRemainderPower(rsa, blinding.applied ? blinded : value, modulusNumber, worked, function)) return false;
    } else {
        const auto& exponent = RequireComponent(rsa.d, function, "private exponent");
        if (!Power(blinding.applied ? blinded : value, ComponentNumber(exponent, function), modulusNumber, worked, function))
            return false;
    }
    if (!blinding.applied) {
        result = std::move(worked);
        return true;
    }
    return MultiplyRemainder(result, worked, blinding.unblind, modulusNumber, function);
}

bool PublicPower(const LibreSsl3Rsa& rsa, const Number& value, Number& result, const char* function) {
    const auto& modulus = RequireComponent(rsa.n, function, "modulus");
    const auto& exponent = RequireComponent(rsa.e, function, "exponent");
    if (rsa.methodModN) NotImplemented_nid_no_patch((std::string(function) + ": a method Montgomery context").c_str());
    return Power(value, ComponentNumber(exponent, function), ComponentNumber(modulus, function), result, function);
}

bool PublicComponents(const LibreSsl3Rsa* rsa, const char* function, const LibreSsl3Bignum*& modulus,
    const LibreSsl3Bignum*& exponent) {
    if (!rsa) Reject(std::string(function) + ": null key");
    modulus = &RequireComponent(rsa->n, function, "modulus");
    if (BitCount(*modulus) > MaximumModulusBits) {
        QueueError(ReasonModulusTooLarge);
        return false;
    }
    exponent = &RequireComponent(rsa->e, function, "exponent");
    if (CompareComponents(*modulus, *exponent) <= 0) {
        QueueError(ReasonBadExponentValue);
        return false;
    }
    if (BitCount(*modulus) > SmallModulusBits && BitCount(*exponent) > MaximumPublicExponentBits) {
        QueueError(ReasonBadExponentValue);
        return false;
    }
    return true;
}

bool WriteOutput(unsigned char* to, int size, const Number& result, const char* function) {
    if (size > 0 && !to) Reject(std::string(function) + ": null output");
    return WriteBytes(result, to, static_cast<std::size_t>(size), function);
}

int ModulusByteCount(const LibreSsl3Rsa* rsa, const char* function) {
    if (!rsa) Reject(std::string(function) + ": null key");
    return ByteCount(RequireComponent(rsa->n, function, "modulus"));
}

int DigestSize(const DigestMethod* digest) {
    return digest ? digest->digestSize : -1;
}

bool RunDigest(const DigestMethod* digest, std::initializer_list<std::span<const unsigned char>> spans,
    unsigned char* output) {
    if (!digest || !digest->init || !digest->update || !digest->finalize) return false;
    if (digest->contextSize < 0) return false;
    std::vector<unsigned char> storage(static_cast<std::size_t>(digest->contextSize));
    DigestContext context {};
    context.digest = digest;
    context.data = storage.empty() ? nullptr : storage.data();
    if (digest->init(&context) != 1) return false;
    for (const auto& span : spans) {
        if (span.empty()) continue;
        if (digest->update(&context, span.data(), span.size()) != 1) return false;
    }
    return digest->finalize(&context, output) == 1;
}

std::vector<unsigned char> DigestBytes(const DigestMethod* digest, std::span<const unsigned char> input, int size) {
    std::vector<unsigned char> output(static_cast<std::size_t>(size));
    if (!RunDigest(digest, {input}, output.data())) return {};
    return output;
}

bool ApplyPadding(int padding, bool privateKey, unsigned char* block, int size, const unsigned char* from, int length,
    const char* function) {
    switch (padding) {
    case LibreSsl3RsaPaddingPkcs1:
        if (privateKey) return LibreSsl3::RsaPaddingAddPkcs1Type1_nid_no_patch(block, size, from, length) > 0;
        return LibreSsl3::RsaPaddingAddPkcs1Type2_nid_no_patch(block, size, from, length) > 0;
    case LibreSsl3RsaPaddingX931:
        if (!privateKey) break;
        NotImplemented_nid_no_patch((std::string(function) + ": X9.31 padding").c_str());
    case LibreSsl3RsaPaddingOaep:
        if (privateKey) break;
        return LibreSsl3::RsaPaddingAddPkcs1OaepMgf1_nid_no_patch(block, size, from, length, nullptr, 0, nullptr,
            nullptr) > 0;
    case LibreSsl3RsaPaddingNone:
        return LibreSsl3::RsaPaddingAddNone_nid_no_patch(block, size, from, length) > 0;
    default:
        break;
    }
    QueueError(ReasonUnknownPaddingType);
    return false;
}

bool RecoverPadding(int padding, bool publicKey, unsigned char* to, int size, const unsigned char* block, int length,
    int modulusLength, const char* function, int& recovered) {
    switch (padding) {
    case LibreSsl3RsaPaddingPkcs1:
        recovered = publicKey ? LibreSsl3::RsaPaddingCheckPkcs1Type1_nid_no_patch(to, size, block, length, modulusLength)
                              : LibreSsl3::RsaPaddingCheckPkcs1Type2_nid_no_patch(to, size, block, length, modulusLength);
        break;
    case LibreSsl3RsaPaddingX931:
        if (!publicKey) {
            QueueError(ReasonUnknownPaddingType);
            return false;
        }
        NotImplemented_nid_no_patch((std::string(function) + ": X9.31 padding").c_str());
    case LibreSsl3RsaPaddingOaep:
        if (publicKey) {
            QueueError(ReasonUnknownPaddingType);
            return false;
        }
        recovered = LibreSsl3::RsaPaddingCheckPkcs1OaepMgf1_nid_no_patch(to, size, block, length, modulusLength, nullptr, 0,
            nullptr, nullptr);
        break;
    case LibreSsl3RsaPaddingNone:
        recovered = LibreSsl3::RsaPaddingCheckNone_nid_no_patch(to, size, block, length, modulusLength);
        break;
    default:
        QueueError(ReasonUnknownPaddingType);
        return false;
    }
    if (recovered < 0) QueueError(ReasonPaddingCheckFailed);
    return true;
}

bool DecodedTail(const Number& result, std::vector<unsigned char>& block, int size, int& length, const char* function) {
    length = static_cast<int>((mbedtls_mpi_bitlen(&result.value) + 7) / 8);
    if (size < length) return false;
    return WriteBytes(result, block.data(), static_cast<std::size_t>(length), function);
}

std::span<const unsigned char> DigestIdentifier(int type) {
    static constexpr unsigned char Md4[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x04};
    static constexpr unsigned char Md5[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05};
    static constexpr unsigned char Sha1[] = {0x2b, 0x0e, 0x03, 0x02, 0x1a};
    static constexpr unsigned char Ripemd160[] = {0x2b, 0x24, 0x03, 0x02, 0x01};
    static constexpr unsigned char Sha224[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04};
    static constexpr unsigned char Sha256[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
    static constexpr unsigned char Sha384[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02};
    static constexpr unsigned char Sha512[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03};
    switch (type) {
    case 4: return Md5;
    case 64: return Sha1;
    case 117: return Ripemd160;
    case 257: return Md4;
    case 672: return Sha256;
    case 673: return Sha384;
    case 674: return Sha512;
    case 675: return Sha224;
    default: return {};
    }
}

void AppendDerLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(std::size_t)];
    int count = 0;
    for (auto remaining = length; remaining; remaining >>= 8) bytes[count++] = static_cast<unsigned char>(remaining);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count) output.push_back(bytes[--count]);
}

bool EncodeDigestInfo(int type, const unsigned char* message, unsigned int messageLength,
    std::vector<unsigned char>& encoded) {
    if (type == 0) {
        QueueError(ReasonObjectIdentifierNotKnownForDigest);
        return false;
    }
    const auto identifier = DigestIdentifier(type);
    if (identifier.empty()) {
        NotImplemented_nid_no_patch(("RSA DigestInfo for an object without a known digest OID, NID " +
            std::to_string(type)).c_str());
    }
    std::vector<unsigned char> algorithm {0x06};
    AppendDerLength(algorithm, identifier.size());
    algorithm.insert(algorithm.end(), identifier.begin(), identifier.end());
    algorithm.push_back(0x05);
    algorithm.push_back(0x00);
    std::vector<unsigned char> content {0x30};
    AppendDerLength(content, algorithm.size());
    content.insert(content.end(), algorithm.begin(), algorithm.end());
    content.push_back(0x04);
    AppendDerLength(content, messageLength);
    if (messageLength) content.insert(content.end(), message, message + messageLength);
    encoded.assign(1, 0x30);
    AppendDerLength(encoded, content.size());
    encoded.insert(encoded.end(), content.begin(), content.end());
    Wipe(content.data(), content.size());
    return true;
}

void CheckSignatureMethod(const LibreSsl3Rsa* rsa, const char* function) {
    if (!rsa) Reject(std::string(function) + ": a null key");
    if (rsa->flags & LibreSsl3RsaFlagSignVerify)
        NotImplemented_nid_no_patch((std::string(function) + ": a key flagged for method signatures").c_str());
}

} // namespace

namespace LibreSsl3 {

int RsaByteCount_nid_no_patch(const LibreSsl3Rsa* rsa) {
    return ModulusByteCount(rsa, "RSA_size");
}

int RsaBitCount_nid_no_patch(const LibreSsl3Rsa* rsa) {
    if (!rsa) Reject("RSA_bits: null key");
    return BitCount(RequireComponent(rsa->n, "RSA_bits", "modulus"));
}

int RsaComponentBits_nid_no_patch(const void* component) {
    return BitCount(RequireComponent(component, "BN_num_bits", "number"));
}

LibreSsl3Bignum* NewRsaBignum_nid_no_patch(const unsigned char* bytes, std::size_t length) {
    auto* number = static_cast<LibreSsl3Bignum*>(malloc_nid_postfix(sizeof(LibreSsl3Bignum)));
    if (!number) return nullptr;
    *number = {nullptr, 0, 0, 0, BignumAllocated};
    if (length == 0) return number;
    const std::size_t limbs = (length - 1) / 8 + 1;
    if (limbs > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        free_nid_postfix(number);
        return nullptr;
    }
    number->data = static_cast<std::uint64_t*>(malloc_nid_postfix(limbs * sizeof(std::uint64_t)));
    if (!number->data) {
        free_nid_postfix(number);
        return nullptr;
    }
    number->capacity = static_cast<std::int32_t>(limbs);
    for (std::size_t limb = 0; limb < limbs; ++limb) {
        std::uint64_t value = 0;
        for (std::size_t byte = 0; byte < 8; ++byte) {
            const std::size_t position = limb * 8 + byte;
            if (position < length) value |= static_cast<std::uint64_t>(bytes[length - 1 - position]) << (8 * byte);
        }
        number->data[limb] = value;
    }
    number->top = static_cast<std::int32_t>(limbs);
    while (number->top > 0 && number->data[number->top - 1] == 0) --number->top;
    return number;
}

LibreSsl3Bignum* DuplicateRsaBignum_nid_no_patch(const LibreSsl3Bignum* number) {
    if (!number || number->top < 0 || number->capacity < number->top) return nullptr;
    if (number->top > 0 && !number->data) return nullptr;
    const std::size_t limbs = number->top == 0 ? 1 : static_cast<std::size_t>(number->top);
    auto* copy = static_cast<LibreSsl3Bignum*>(malloc_nid_postfix(sizeof(LibreSsl3Bignum)));
    if (!copy) return nullptr;
    copy->data = static_cast<std::uint64_t*>(malloc_nid_postfix(limbs * sizeof(std::uint64_t)));
    if (!copy->data) {
        free_nid_postfix(copy);
        return nullptr;
    }
    for (std::size_t index = 0; index < limbs; ++index) copy->data[index] = number->data ? number->data[index] : 0;
    copy->top = number->top;
    copy->capacity = static_cast<std::int32_t>(limbs);
    copy->negative = number->negative;
    copy->flags = BignumAllocated;
    return copy;
}

void FreeRsa_nid_no_patch(LibreSsl3Rsa* rsa) {
    if (!rsa) return;
    const int remaining = std::atomic_ref<std::int32_t>(rsa->references).fetch_add(-1) - 1;
    if (remaining > 0) return;
    if (rsa->pss || rsa->blinding || rsa->mtBlinding || rsa->extraData)
        NotImplemented_nid_no_patch("RSA_free of a key holding PSS parameters, blinding state or extra data");
    if (rsa->methodModN || rsa->methodModP || rsa->methodModQ || rsa->method)
        NotImplemented_nid_no_patch("RSA_free of a key with method data");
    for (void* component : {rsa->n, rsa->e, rsa->d, rsa->p, rsa->q, rsa->dmp1, rsa->dmq1, rsa->iqmp})
        BN_clear_free_nid_postfix(static_cast<LibreSsl3Bignum*>(component));
    Wipe(rsa, sizeof(*rsa));
    free_nid_postfix(rsa);
}

int RsaPublicEncrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding) {
    constexpr auto function = "RSA_public_encrypt";
    const LibreSsl3Bignum* modulus = nullptr;
    const LibreSsl3Bignum* exponent = nullptr;
    if (!PublicComponents(rsa, function, modulus, exponent)) return -1;
    const int size = ByteCount(*modulus);
    std::vector<unsigned char> block(static_cast<std::size_t>(size));
    if (!ApplyPadding(padding, false, block.data(), size, from, length, function)) return -1;
    const Number value = BytesNumber(block.data(), block.size(), function);
    const Number modulusNumber = ComponentNumber(*modulus, function);
    if (mbedtls_mpi_cmp_mpi(&value.value, &modulusNumber.value) >= 0) {
        QueueError(ReasonDataTooLargeForModulus);
        return -1;
    }
    Number result;
    if (!Power(value, ComponentNumber(*exponent, function), modulusNumber, result, function)) return -1;
    if (!WriteOutput(to, size, result, function)) return -1;
    return size;
}

int RsaPrivateEncrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding) {
    constexpr auto function = "RSA_private_encrypt";
    const int size = ModulusByteCount(rsa, function);
    CheckNumberSize(RequireComponent(rsa->n, function, "modulus"), "modulus");
    std::vector<unsigned char> block(static_cast<std::size_t>(size));
    if (!ApplyPadding(padding, true, block.data(), size, from, length, function)) return -1;
    const Number value = BytesNumber(block.data(), block.size(), function);
    const Number modulusNumber = ComponentNumber(RequireComponent(rsa->n, function, "modulus"), function);
    if (mbedtls_mpi_cmp_mpi(&value.value, &modulusNumber.value) >= 0) {
        QueueError(ReasonDataTooLargeForModulus);
        return -1;
    }
    Number result;
    if (!PrivatePower(*rsa, value, result, function)) return -1;
    if (!WriteOutput(to, size, result, function)) return -1;
    return size;
}

int RsaPrivateDecrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding) {
    constexpr auto function = "RSA_private_decrypt";
    if (!rsa) Reject(std::string(function) + ": null key");
    const int size = ModulusByteCount(rsa, function);
    if (length > size) {
        QueueError(ReasonDataGreaterThanModulusLength);
        return -1;
    }
    if (length > 0 && !from) Reject(std::string(function) + ": null input");
    const Number value = BytesNumber(from, static_cast<std::size_t>(length), function);
    const Number modulusNumber = ComponentNumber(RequireComponent(rsa->n, function, "modulus"), function);
    if (mbedtls_mpi_cmp_mpi(&value.value, &modulusNumber.value) >= 0) {
        QueueError(ReasonDataTooLargeForModulus);
        return -1;
    }
    Number result;
    if (!PrivatePower(*rsa, value, result, function)) return -1;
    std::vector<unsigned char> block(static_cast<std::size_t>(size));
    int decodedLength = 0;
    if (!DecodedTail(result, block, size, decodedLength, function)) return -1;
    int recovered = -1;
    if (!RecoverPadding(padding, false, to, size, block.data(), decodedLength, size, function, recovered)) return -1;
    return recovered;
}

int RsaPublicDecrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding) {
    constexpr auto function = "RSA_public_decrypt";
    const LibreSsl3Bignum* modulus = nullptr;
    const LibreSsl3Bignum* exponent = nullptr;
    if (!PublicComponents(rsa, function, modulus, exponent)) return -1;
    const int size = ByteCount(*modulus);
    if (length > size) {
        QueueError(ReasonDataGreaterThanModulusLength);
        return -1;
    }
    if (length > 0 && !from) Reject(std::string(function) + ": null input");
    const Number value = BytesNumber(from, static_cast<std::size_t>(length), function);
    const Number modulusNumber = ComponentNumber(*modulus, function);
    if (mbedtls_mpi_cmp_mpi(&value.value, &modulusNumber.value) >= 0) {
        QueueError(ReasonDataTooLargeForModulus);
        return -1;
    }
    Number result;
    if (!PublicPower(*rsa, value, result, function)) return -1;
    std::vector<unsigned char> block(static_cast<std::size_t>(size));
    int decodedLength = 0;
    if (!DecodedTail(result, block, size, decodedLength, function)) return -1;
    int recovered = -1;
    if (!RecoverPadding(padding, true, to, size, block.data(), decodedLength, size, function, recovered)) return -1;
    return recovered;
}

int RsaPaddingAddNone_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length) {
    if (length < 0 || size < 0) return 0;
    if (length > size) {
        QueueError(ReasonDataTooLargeForKeySize);
        return 0;
    }
    if (length < size) {
        QueueError(ReasonDataTooSmallForKeySize);
        return 0;
    }
    if (length > 0 && !from) Reject("RSA_padding_add_none: null input");
    if (size > 0 && !to) Reject("RSA_padding_add_none: null output");
    if (length) std::memcpy(to, from, static_cast<std::size_t>(length));
    return 1;
}

int RsaPaddingCheckNone_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength) {
    (void) modulusLength;
    if (length < 0 || size < 0) return -1;
    if (length > size) {
        QueueError(ReasonDataTooLarge);
        return -1;
    }
    if (length > 0 && !from) Reject("RSA_padding_check_none: null input");
    if (size > 0 && !to) Reject("RSA_padding_check_none: null output");
    if (size > length) std::memset(to, 0, static_cast<std::size_t>(size - length));
    if (length) std::memcpy(to + size - length, from, static_cast<std::size_t>(length));
    return size;
}

int RsaPaddingAddPkcs1Type1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length) {
    if (size < 0 || length < 0) return 0;
    if (length > size - LibreSsl3RsaPkcs1PaddingSize) {
        QueueError(ReasonDataTooLargeForKeySize);
        return 0;
    }
    if (length > 0 && !from) Reject("RSA_padding_add_PKCS1_type_1: null input");
    if (size > 0 && !to) Reject("RSA_padding_add_PKCS1_type_1: null output");
    if (size > 0) {
        to[0] = 0;
        to[1] = 1;
        std::memset(to + 2, 0xff, static_cast<std::size_t>(size - 3 - length));
        to[size - length - 1] = 0;
    }
    if (length) std::memcpy(to + size - length, from, static_cast<std::size_t>(length));
    return 1;
}

int RsaPaddingCheckPkcs1Type1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength) {
    if (size < 0 || length < 0) return -1;
    const unsigned char* data = from;
    if (modulusLength != length + 1 || length <= 0 || !data || *(data++) != 0x01) {
        QueueError(ReasonBlockTypeIsNotOne);
        return -1;
    }
    int remaining = length - 1;
    int index = 0;
    for (; index < remaining; ++index, ++data) {
        if (*data == 0xff) continue;
        if (*data) {
            QueueError(ReasonBadFixedHeaderDecrypt);
            return -1;
        }
        ++data;
        break;
    }
    if (index == remaining) {
        QueueError(ReasonNullBeforeBlockMissing);
        return -1;
    }
    if (index < 8) {
        QueueError(ReasonBadPadByteCount);
        return -1;
    }
    ++index;
    remaining -= index;
    if (remaining > size) {
        QueueError(ReasonDataTooLarge);
        return -1;
    }
    if (remaining > 0 && !to) Reject("RSA_padding_check_PKCS1_type_1: null output");
    if (remaining) std::memcpy(to, data, static_cast<std::size_t>(remaining));
    return remaining;
}

int RsaPaddingAddPkcs1Type2_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length) {
    if (size < 0 || length < 0) return 0;
    if (length > size - LibreSsl3RsaPkcs1PaddingSize) {
        QueueError(ReasonDataTooLargeForKeySize);
        return 0;
    }
    if (length > 0 && !from) Reject("RSA_padding_add_PKCS1_type_2: null input");
    if (size > 0 && !to) Reject("RSA_padding_add_PKCS1_type_2: null output");
    if (size > 0) {
        to[0] = 0;
        to[1] = 2;
        unsigned char* padding = to + 2;
        for (int index = 0; index < size - 3 - length; ++index) {
            unsigned char byte = 0;
            while (!byte) HostEntropy_nid_no_patch(&byte, 1);
            *padding++ = byte;
        }
        *padding++ = 0;
        std::memcpy(padding, from, static_cast<std::size_t>(length));
    }
    return 1;
}

int RsaPaddingCheckPkcs1Type2_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength) {
    if (size < 0 || length < 0) return -1;
    const unsigned char* data = from;
    if (modulusLength != length + 1 || length <= 0 || !data || *(data++) != 0x02) {
        QueueError(ReasonBlockTypeIsNotTwo);
        return -1;
    }
    int remaining = length - 1;
    int index = 0;
    for (; index < remaining; ++index, ++data) {
        if (*data) continue;
        ++data;
        break;
    }
    if (index == remaining) {
        QueueError(ReasonNullBeforeBlockMissing);
        return -1;
    }
    if (index < 8) {
        QueueError(ReasonBadPadByteCount);
        return -1;
    }
    ++index;
    remaining -= index;
    if (remaining > size) {
        QueueError(ReasonDataTooLarge);
        return -1;
    }
    if (remaining > 0 && !to) Reject("RSA_padding_check_PKCS1_type_2: null output");
    if (remaining) std::memcpy(to, data, static_cast<std::size_t>(remaining));
    return remaining;
}

bool RsaMaskGenerationFunction_nid_no_patch(unsigned char* mask, int length, const unsigned char* seed, int seedLength,
    const DigestMethod* digest) {
    const int size = DigestSize(digest);
    if (size < 0) return false;
    if (length <= 0 || size == 0) return true;
    if (seedLength > 0 && !seed) Reject("PKCS1_MGF1: null seed");
    for (int counter = 0; counter * size < length; ++counter) {
        const std::uint32_t bigEndianCounter = static_cast<std::uint32_t>(counter);
        const unsigned char counterBytes[] {
            static_cast<unsigned char>((bigEndianCounter >> 24) & 0xff),
            static_cast<unsigned char>((bigEndianCounter >> 16) & 0xff),
            static_cast<unsigned char>((bigEndianCounter >> 8) & 0xff),
            static_cast<unsigned char>(bigEndianCounter & 0xff),
        };
        std::vector<unsigned char> block(static_cast<std::size_t>(size));
        const std::span<const unsigned char> seedSpan(seed, static_cast<std::size_t>(seedLength));
        const std::span<const unsigned char> counterSpan(counterBytes, sizeof(counterBytes));
        if (!RunDigest(digest, {seedSpan, counterSpan}, block.data())) return false;
        const int offset = counter * size;
        const int taken = (offset + size <= length) ? size : (length - offset);
        std::memcpy(mask + offset, block.data(), static_cast<std::size_t>(taken));
    }
    return true;
}

bool RsaDigestSpans_nid_no_patch(const DigestMethod* digest, std::span<const unsigned char> first,
    std::span<const unsigned char> second, unsigned char* output) {
    return RunDigest(digest, {first, second}, output);
}

int RsaPaddingAddPkcs1OaepMgf1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    const unsigned char* label, int labelLength, const DigestMethod* digest, const DigestMethod* maskDigest) {
    const DigestMethod* messageDigest = digest ? digest : EVP_sha1_nid_postfix();
    const DigestMethod* maskMethod = maskDigest ? maskDigest : messageDigest;
    if (size < 0 || length < 0) return 0;
    const int digestLength = DigestSize(messageDigest);
    if (digestLength < 0) return 0;
    const int encodedLength = size - 1;
    if (length > encodedLength - 2 * digestLength - 1) {
        QueueError(ReasonDataTooLargeForKeySize);
        return 0;
    }
    if (encodedLength < 2 * digestLength + 1) {
        QueueError(ReasonKeySizeTooSmall);
        return 0;
    }
    if (length > 0 && !from) Reject("RSA_padding_add_PKCS1_OAEP_mgf1: null input");
    if (labelLength > 0 && !label) Reject("RSA_padding_add_PKCS1_OAEP_mgf1: null label");
    if (size > 0 && !to) Reject("RSA_padding_add_PKCS1_OAEP_mgf1: null output");
    to[0] = 0;
    unsigned char* seed = to + 1;
    unsigned char* block = to + digestLength + 1;
    const std::span<const unsigned char> labelSpan(label, static_cast<std::size_t>(labelLength > 0 ? labelLength : 0));
    const std::vector<unsigned char> hashedLabel = DigestBytes(messageDigest, labelSpan, digestLength);
    if (hashedLabel.empty()) return 0;
    std::memcpy(block, hashedLabel.data(), static_cast<std::size_t>(digestLength));
    std::memset(block + digestLength, 0, static_cast<std::size_t>(encodedLength - length - 2 * digestLength - 1));
    block[encodedLength - length - digestLength - 1] = 0x01;
    if (length) std::memcpy(block + encodedLength - length - digestLength, from, static_cast<std::size_t>(length));
    HostEntropy_nid_no_patch(seed, static_cast<std::size_t>(digestLength));
    std::vector<unsigned char> blockMask(static_cast<std::size_t>(encodedLength - digestLength));
    if (!RsaMaskGenerationFunction_nid_no_patch(blockMask.data(), encodedLength - digestLength, seed, digestLength,
            maskMethod))
        return 0;
    for (int index = 0; index < encodedLength - digestLength; ++index)
        block[static_cast<std::size_t>(index)] ^= blockMask[static_cast<std::size_t>(index)];
    std::vector<unsigned char> seedMask(static_cast<std::size_t>(digestLength));
    if (!RsaMaskGenerationFunction_nid_no_patch(seedMask.data(), digestLength, block, encodedLength - digestLength,
            maskMethod))
        return 0;
    for (int index = 0; index < digestLength; ++index)
        seed[static_cast<std::size_t>(index)] ^= seedMask[static_cast<std::size_t>(index)];
    return 1;
}

int RsaPaddingCheckPkcs1OaepMgf1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength, const unsigned char* label, int labelLength, const DigestMethod* digest,
    const DigestMethod* maskDigest) {
    const DigestMethod* messageDigest = digest ? digest : EVP_sha1_nid_postfix();
    const DigestMethod* maskMethod = maskDigest ? maskDigest : messageDigest;
    const int digestLength = DigestSize(messageDigest);
    if (digestLength <= 0) return -1;
    if (size <= 0 || length <= 0) return -1;
    if (length > 0 && !from) Reject("RSA_padding_check_PKCS1_OAEP_mgf1: null input");
    if (labelLength > 0 && !label) Reject("RSA_padding_check_PKCS1_OAEP_mgf1: null label");
    if (modulusLength < length || modulusLength < 2 * digestLength + 2) {
        QueueError(ReasonOaepDecodingError);
        return -1;
    }
    const int blockLength = modulusLength - digestLength - 1;
    std::vector<unsigned char> encoded(static_cast<std::size_t>(modulusLength));
    std::memcpy(encoded.data() + static_cast<std::size_t>(modulusLength - length), from, static_cast<std::size_t>(length));
    bool good = encoded[0] == 0;
    const unsigned char* maskedSeed = encoded.data() + 1;
    const unsigned char* maskedBlock = encoded.data() + 1 + digestLength;
    std::vector<unsigned char> seed(static_cast<std::size_t>(digestLength));
    if (!RsaMaskGenerationFunction_nid_no_patch(seed.data(), digestLength, maskedBlock, blockLength, maskMethod))
        return -1;
    for (int index = 0; index < digestLength; ++index)
        seed[static_cast<std::size_t>(index)] ^= maskedSeed[static_cast<std::size_t>(index)];
    std::vector<unsigned char> block(static_cast<std::size_t>(blockLength));
    if (!RsaMaskGenerationFunction_nid_no_patch(block.data(), blockLength, seed.data(), digestLength, maskMethod))
        return -1;
    for (int index = 0; index < blockLength; ++index)
        block[static_cast<std::size_t>(index)] ^= maskedBlock[static_cast<std::size_t>(index)];
    const std::span<const unsigned char> labelSpan(label, static_cast<std::size_t>(labelLength > 0 ? labelLength : 0));
    const std::vector<unsigned char> hashedLabel = DigestBytes(messageDigest, labelSpan, digestLength);
    if (hashedLabel.empty()) return -1;
    if (std::memcmp(block.data(), hashedLabel.data(), static_cast<std::size_t>(digestLength)) != 0) good = false;
    bool found = false;
    int oneIndex = 0;
    for (int index = digestLength; index < blockLength; ++index) {
        const unsigned char byte = block[static_cast<std::size_t>(index)];
        if (!found && byte == 1) {
            oneIndex = index;
            found = true;
        }
        if (!found && byte != 0) good = false;
    }
    if (!found) good = false;
    const int messageLength = blockLength - oneIndex - 1;
    if (size < messageLength) good = false;
    if (!good) {
        QueueError(ReasonOaepDecodingError);
        return -1;
    }
    if (messageLength > 0) {
        if (!to) Reject("RSA_padding_check_PKCS1_OAEP_mgf1: null output");
        std::memcpy(to, block.data() + oneIndex + 1, static_cast<std::size_t>(messageLength));
    }
    return messageLength;
}

int RsaSign_nid_no_patch(int type, const unsigned char* message, unsigned int messageLength, unsigned char* signature,
    unsigned int* signatureLength, const LibreSsl3Rsa* rsa) {
    constexpr auto function = "RSA_sign";
    CheckSignatureMethod(rsa, function);
    std::vector<unsigned char> digestInfo;
    const unsigned char* encoded = message;
    int encodedLength = 0;
    if (type == Md5Sha1Type) {
        if (messageLength != Md5Sha1Length) {
            QueueError(ReasonInvalidDigestLength);
            return 0;
        }
        encodedLength = Md5Sha1Length;
    } else {
        if (!EncodeDigestInfo(type, message, messageLength, digestInfo)) return 0;
        encoded = digestInfo.data();
        encodedLength = static_cast<int>(digestInfo.size());
    }
    auto result = 0;
    if (encodedLength > ModulusByteCount(rsa, function) - LibreSsl3RsaPkcs1PaddingSize) {
        QueueError(ReasonDigestTooBigForRsaKey);
    } else {
        const auto length = RsaPrivateEncrypt_nid_no_patch(encodedLength, encoded, signature, rsa, LibreSsl3RsaPaddingPkcs1);
        if (length > 0) {
            *signatureLength = static_cast<unsigned int>(length);
            result = 1;
        }
    }
    Wipe(digestInfo.data(), digestInfo.size());
    return result;
}

int RsaVerify_nid_no_patch(int type, const unsigned char* message, unsigned int messageLength,
    const unsigned char* signature, unsigned int signatureLength, const LibreSsl3Rsa* rsa) {
    constexpr auto function = "RSA_verify";
    CheckSignatureMethod(rsa, function);
    const auto modulusLength = ModulusByteCount(rsa, function);
    if (signatureLength != static_cast<unsigned int>(modulusLength)) {
        QueueError(ReasonWrongSignatureLength);
        return 0;
    }
    std::vector<unsigned char> decrypted(signatureLength);
    std::vector<unsigned char> digestInfo;
    auto result = 0;
    const auto decryptedLength = RsaPublicDecrypt_nid_no_patch(static_cast<int>(signatureLength), signature,
        decrypted.data(), rsa, LibreSsl3RsaPaddingPkcs1);
    if (decryptedLength > 0 && type == Md5Sha1Type) {
        if (decryptedLength != Md5Sha1Length) {
            QueueError(ReasonInvalidDigestLength);
        } else if (messageLength != Md5Sha1Length) {
            QueueError(ReasonInvalidMessageLength);
        } else if (CRYPTO_memcmp_nid_postfix(decrypted.data(), message, Md5Sha1Length) != 0) {
            QueueError(ReasonBadSignature);
        } else {
            result = 1;
        }
    } else if (decryptedLength > 0 && EncodeDigestInfo(type, message, messageLength, digestInfo)) {
        if (static_cast<int>(digestInfo.size()) != decryptedLength ||
            CRYPTO_memcmp_nid_postfix(digestInfo.data(), decrypted.data(), digestInfo.size()) != 0) {
            QueueError(ReasonBadSignature);
        } else {
            result = 1;
        }
    }
    Wipe(digestInfo.data(), digestInfo.size());
    Wipe(decrypted.data(), decrypted.size());
    return result;
}

int RsaPaddingAddPkcs1PssMgf1_nid_no_patch(const LibreSsl3Rsa* rsa, unsigned char* encoded,
    const unsigned char* messageHash, const DigestMethod* digest, const DigestMethod* maskDigest,
    int saltLength) {
    constexpr auto function = "RSA_padding_add_PKCS1_PSS_mgf1";
    if (!rsa) Reject(std::string(function) + ": null key");
    if (!encoded) Reject(std::string(function) + ": null encoded");
    if (!messageHash) Reject(std::string(function) + ": null messageHash");
    if (!digest) Reject(std::string(function) + ": null digest");

    const DigestMethod* maskMethod = maskDigest ? maskDigest : digest;
    const int digestLength = DigestSize(digest);
    if (digestLength < 0) return 0;

    if (saltLength == LibreSsl3RsaPssSaltLengthDigest) {
        saltLength = digestLength;
    } else if (saltLength < LibreSsl3RsaPssSaltLengthAuto) {
        QueueError(ReasonSlenCheckFailed);
        return 0;
    }

    const int modulusBits = RsaBitCount_nid_no_patch(rsa);
    const int msbBits = (modulusBits - 1) & 0x7;
    int emLen = ModulusByteCount(rsa, function);
    unsigned char* emStart = encoded;

    if (msbBits == 0) {
        *emStart++ = 0;
        emLen--;
    }

    if (saltLength == LibreSsl3RsaPssSaltLengthAuto) {
        saltLength = emLen - digestLength - 2;
    } else if (emLen < digestLength + saltLength + 2) {
        QueueError(ReasonDataTooLargeForKeySize);
        return 0;
    }

    std::vector<unsigned char> salt;
    if (saltLength > 0) {
        salt.resize(static_cast<std::size_t>(saltLength));
        HostEntropy_nid_no_patch(salt.data(), static_cast<std::size_t>(saltLength));
    }

    const int maskedDbLen = emLen - digestLength - 1;
    unsigned char* h = emStart + maskedDbLen;

    constexpr unsigned char zeroes[] = {0, 0, 0, 0, 0, 0, 0, 0};
    const std::span<const unsigned char> zeroSpan(zeroes, 8);
    const std::span<const unsigned char> messageSpan(messageHash, static_cast<std::size_t>(digestLength));
    const std::span<const unsigned char> saltSpan(salt.data(), static_cast<std::size_t>(saltLength > 0 ? saltLength : 0));

    if (!RunDigest(digest, {zeroSpan, messageSpan, saltSpan}, h)) {
        return 0;
    }

    if (!RsaMaskGenerationFunction_nid_no_patch(emStart, maskedDbLen, h, digestLength, maskMethod)) return 0;

    unsigned char* p = emStart;
    p += emLen - saltLength - digestLength - 2;
    *p++ ^= 0x1;
    if (saltLength > 0) {
        for (int index = 0; index < saltLength; ++index) *p++ ^= salt[index];
    }

    if (msbBits) emStart[0] &= 0xFF >> (8 - msbBits);

    emStart[emLen - 1] = 0xbc;

    return 1;
}

int RsaVerifyPkcs1PssMgf1_nid_no_patch(const LibreSsl3Rsa* rsa, const unsigned char* messageHash,
    const DigestMethod* digest, const DigestMethod* maskDigest, const unsigned char* encoded,
    int saltLength) {
    constexpr auto function = "RSA_verify_PKCS1_PSS_mgf1";
    if (!rsa) Reject(std::string(function) + ": null key");
    if (!messageHash) Reject(std::string(function) + ": null messageHash");
    if (!digest) Reject(std::string(function) + ": null digest");
    if (!encoded) Reject(std::string(function) + ": null encoded");

    const DigestMethod* maskMethod = maskDigest ? maskDigest : digest;
    const int digestLength = DigestSize(digest);
    if (digestLength < 0) return 0;

    if (saltLength == LibreSsl3RsaPssSaltLengthDigest) {
        saltLength = digestLength;
    } else if (saltLength < LibreSsl3RsaPssSaltLengthAuto) {
        QueueError(ReasonSlenCheckFailed);
        return 0;
    }

    const int modulusBits = RsaBitCount_nid_no_patch(rsa);
    const int msbBits = (modulusBits - 1) & 0x7;
    int emLen = ModulusByteCount(rsa, function);
    const unsigned char* emStart = encoded;

    if (emStart[0] & (0xFF << msbBits)) {
        QueueError(ReasonFirstOctetInvalid);
        return 0;
    }

    if (msbBits == 0) {
        emStart++;
        emLen--;
    }

    if (emLen < digestLength + saltLength + 2) {
        QueueError(ReasonDataTooLarge);
        return 0;
    }

    if (emStart[emLen - 1] != 0xbc) {
        QueueError(ReasonLastOctetInvalid);
        return 0;
    }

    const int maskedDbLen = emLen - digestLength - 1;
    const unsigned char* h = emStart + maskedDbLen;

    std::vector<unsigned char> db(static_cast<std::size_t>(maskedDbLen));
    std::memcpy(db.data(), emStart, static_cast<std::size_t>(maskedDbLen));

    if (!RsaMaskGenerationFunction_nid_no_patch(db.data(), maskedDbLen, h, digestLength, maskMethod))
        return 0;

    for (int index = 0; index < maskedDbLen; ++index) db[static_cast<std::size_t>(index)] ^= emStart[static_cast<std::size_t>(index)];

    if (msbBits) db[0] &= 0xFF >> (8 - msbBits);

    int index = 0;
    for (; index < maskedDbLen - 1 && db[static_cast<std::size_t>(index)] == 0; ++index);

    if (db[static_cast<std::size_t>(index)] != 0x01) {
        QueueError(ReasonSlenRecoveryFailed);
        return 0;
    }

    ++index;
    const int recoveredSaltLength = maskedDbLen - index;
    if (saltLength >= 0 && recoveredSaltLength != saltLength) {
        QueueError(ReasonSlenCheckFailed);
        return 0;
    }

    constexpr unsigned char zeroes[] = {0, 0, 0, 0, 0, 0, 0, 0};
    const std::span<const unsigned char> zeroSpan(zeroes, 8);
    const std::span<const unsigned char> messageSpan(messageHash, static_cast<std::size_t>(digestLength));
    const std::span<const unsigned char> saltSpan(db.data() + static_cast<std::size_t>(index),
        static_cast<std::size_t>(recoveredSaltLength));

    std::vector<unsigned char> expectedH(static_cast<std::size_t>(digestLength));
    if (!RunDigest(digest, {zeroSpan, messageSpan, saltSpan}, expectedH.data())) {
        return 0;
    }

    if (CRYPTO_memcmp_nid_postfix(expectedH.data(), h, static_cast<std::size_t>(digestLength)) != 0) {
        QueueError(ReasonBadSignature);
        return 0;
    }

    return 1;
}

} // namespace LibreSsl3

extern "C" {

LibreSsl3Rsa* APS5_VABI RSA_new_nid_disambig1_nid_postfix() {
    auto* rsa = static_cast<LibreSsl3Rsa*>(calloc_nid_postfix(1, sizeof(LibreSsl3Rsa)));
    if (!rsa) {
        QueueError(AllocationFailure);
        return nullptr;
    }
    rsa->references = 1;
    return rsa;
}

int APS5_VABI RSA_padding_add_PKCS1_PSS_mgf1_nid_postfix(LibreSsl3Rsa* rsa, unsigned char* encoded,
    const unsigned char* messageHash, const LibreSsl3::DigestMethod* digest,
    const LibreSsl3::DigestMethod* maskDigest, int saltLength) {
    if (!rsa || !encoded || !messageHash || !digest) {
        NotImplemented_nid_no_patch("RSA_padding_add_PKCS1_PSS_mgf1_nid_postfix");
    }
    return LibreSsl3::RsaPaddingAddPkcs1PssMgf1_nid_no_patch(rsa, encoded, messageHash, digest, maskDigest,
        saltLength);
}

int APS5_VABI RSA_check_key_nid_postfix(const LibreSsl3Rsa* rsa) {
    constexpr auto function = "RSA_check_key";
    if (!rsa) NotImplemented_nid_no_patch("RSA_check_key of a null key");
    if (!rsa->p || !rsa->q || !rsa->n || !rsa->e || !rsa->d) {
        QueueError(ReasonValueMissing);
        return 0;
    }
    const auto p = ComponentNumber(RequireComponent(rsa->p, function, "p"), function);
    const auto q = ComponentNumber(RequireComponent(rsa->q, function, "q"), function);
    const auto n = ComponentNumber(RequireComponent(rsa->n, function, "n"), function);
    const auto e = ComponentNumber(RequireComponent(rsa->e, function, "e"), function);
    const auto d = ComponentNumber(RequireComponent(rsa->d, function, "d"), function);
    if (mbedtls_mpi_cmp_int(&p.value, 0) == 0 || mbedtls_mpi_cmp_int(&q.value, 0) == 0)
        NotImplemented_nid_no_patch("RSA_check_key of a key with a zero prime factor");
    auto result = 1;
    if (IsOne(e)) {
        result = 0;
        QueueError(ReasonBadExponentValue);
    }
    if (!IsOdd(e)) {
        result = 0;
        QueueError(ReasonBadExponentValue);
    }
    for (const auto& [factor, reason] : {std::pair {&p, ReasonPNotPrime}, std::pair {&q, ReasonQNotPrime}}) {
        const auto prime = PrimalityCheck(*factor, function);
        if (prime < 0) return prime;
        if (prime == 0) {
            result = 0;
            QueueError(reason);
        }
    }
    Number product;
    if (!HandleMbedtls(mbedtls_mpi_mul_mpi(&product.value, &p.value, &q.value), function)) return -1;
    if (mbedtls_mpi_cmp_mpi(&product.value, &n.value) != 0) {
        result = 0;
        QueueError(ReasonModulusNotProductOfPrimes);
    }
    Number one;
    Number pMinusOne;
    Number qMinusOne;
    Number divisor;
    Number lcm;
    Number congruence;
    if (!HandleMbedtls(mbedtls_mpi_lset(&one.value, 1), function) || !Subtract(pMinusOne, p, one, function) ||
        !Subtract(qMinusOne, q, one, function) ||
        !HandleMbedtls(mbedtls_mpi_mul_mpi(&product.value, &pMinusOne.value, &qMinusOne.value), function) ||
        !HandleMbedtls(mbedtls_mpi_gcd(&divisor.value, &pMinusOne.value, &qMinusOne.value), function))
        return -1;
    if (mbedtls_mpi_cmp_int(&divisor.value, 0) == 0) {
        QueueBigNumberError(BigNumberReasonDivisionByZero);
        return -1;
    }
    if (!HandleMbedtls(mbedtls_mpi_div_mpi(&lcm.value, nullptr, &product.value, &divisor.value), function) ||
        !HandleMbedtls(mbedtls_mpi_mul_mpi(&product.value, &d.value, &e.value), function) ||
        !CheckedRemainder(congruence, product, lcm, function))
        return -1;
    if (!IsOne(congruence)) {
        result = 0;
        QueueError(ReasonDeNotCongruentToOne);
    }
    if (!rsa->dmp1 || !rsa->dmq1 || !rsa->iqmp) return result;
    const auto dmp1 = ComponentNumber(RequireComponent(rsa->dmp1, function, "dmp1"), function);
    const auto dmq1 = ComponentNumber(RequireComponent(rsa->dmq1, function, "dmq1"), function);
    const auto iqmp = ComponentNumber(RequireComponent(rsa->iqmp, function, "iqmp"), function);
    Number reduced;
    if (!CheckedRemainder(reduced, d, pMinusOne, function)) return -1;
    if (mbedtls_mpi_cmp_mpi(&reduced.value, &dmp1.value) != 0) {
        result = 0;
        QueueError(ReasonDmp1NotCongruentToD);
    }
    if (!CheckedRemainder(reduced, d, qMinusOne, function)) return -1;
    if (mbedtls_mpi_cmp_mpi(&reduced.value, &dmq1.value) != 0) {
        result = 0;
        QueueError(ReasonDmq1NotCongruentToD);
    }
    const auto inverse = mbedtls_mpi_inv_mod(&reduced.value, &q.value, &p.value);
    if (inverse == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) QueueBigNumberError(BigNumberReasonNoInverse);
    if (inverse == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE || !HandleMbedtls(inverse, function)) return -1;
    if (mbedtls_mpi_cmp_mpi(&reduced.value, &iqmp.value) != 0) {
        result = 0;
        QueueError(ReasonIqmpNotInverseOfQ);
    }
    return result;
}

}
