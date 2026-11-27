/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pem/pem_lib.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pem/pem_pkey.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pem/pem_pk8.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_key.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_pbe.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_cipher.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/e_des.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/e_des3.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs12/p12_decr.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs12/p12_p8d.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs12/p12_p8e.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs12/p12_key.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs12/p12_utl.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/p5_pbev2.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/p5_pbe.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_int.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_type.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_object.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_bitstr.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_sig.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_algor.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_dec.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/asn1_lib.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/asn1_types.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_names.c */
/* $OpenBSD: pem_lib.c,v 1.56 2024/02/18 15:44:10 tb Exp $ */
/* $OpenBSD: pem_pkey.c,v 1.28 2023/11/19 15:46:10 tb Exp $ */
/* $OpenBSD: pem_pk8.c,v 1.14 2023/07/07 13:40:44 beck Exp $ */
/* $OpenBSD: evp_key.c,v 1.36 2024/04/09 13:52:41 beck Exp $ */
/* $OpenBSD: e_des.c,v 1.24 2024/04/09 13:52:41 beck Exp $ */
/* $OpenBSD: e_des3.c,v 1.30 2024/04/09 13:52:41 beck Exp $ */
/* $OpenBSD: a_int.c,v 1.48 2024/07/08 14:52:31 beck Exp $ */
/* $OpenBSD: a_type.c,v 1.27 2023/07/28 10:00:10 tb Exp $ */
/* $OpenBSD: a_object.c,v 1.55 2024/07/08 14:52:31 beck Exp $ */
/* $OpenBSD: a_bitstr.c,v 1.43 2024/07/08 14:52:31 beck Exp $ */
/* $OpenBSD: x_sig.c,v 1.18 2024/07/08 14:48:49 beck Exp $ */
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
/* $OpenBSD: evp_pbe.c,v 1.50 2024/04/09 13:52:41 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: evp_cipher.c,v 1.23 2024/04/10 15:00:38 beck Exp $ */
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
 * Copyright (c) 2015 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: p12_decr.c,v 1.26 2024/03/02 10:15:16 tb Exp $ */
/* $OpenBSD: p12_key.c,v 1.35 2024/03/02 10:15:16 tb Exp $ */
/* $OpenBSD: p12_utl.c,v 1.21 2023/02/16 08:38:17 tb Exp $ */
/* $OpenBSD: p5_pbe.c,v 1.28 2024/07/08 14:48:49 beck Exp $ */
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
/* $OpenBSD: p12_p8d.c,v 1.12 2024/03/02 10:15:16 tb Exp $ */
/* $OpenBSD: p12_p8e.c,v 1.13 2024/03/02 10:15:16 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: p5_pbev2.c,v 1.35 2024/03/26 07:03:10 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999-2004.
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
/* $OpenBSD: x_algor.c,v 1.41 2024/07/08 14:48:49 beck Exp $ */
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
/*	$OpenBSD: evp_names.c,v 1.18 2024/08/31 10:38:49 tb Exp $ */
/*
 * Copyright (c) 2023 Theo Buehler <tb@openbsd.org>
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

#include "prx/libSceLibreSsl3/include/PemEncryption.hpp"
#include "prx/libSceLibreSsl3/src/Asn1Reader.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <mbedtls/des.h>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace LibreSsl3Asn1;

namespace {

using namespace std::string_view_literals;

constexpr int EvpLibrary = 6;
constexpr int PemLibrary = 9;
constexpr int Pkcs12Library = 35;

constexpr int ErrREvpLib = 6;
constexpr int ErrRMallocFailure = 65;

constexpr int PemBadDecrypt = 101;
constexpr int PemBadIvChars = 103;
constexpr int PemBadPasswordRead = 104;
constexpr int PemUnsupportedEncryption = 114;

constexpr int EvpBadDecrypt = 100;
constexpr int EvpIvTooLarge = 102;
constexpr int EvpUnsupportedCipher = 107;
constexpr int EvpWrongFinalBlockLength = 109;
constexpr int EvpDecodeError = 114;
constexpr int EvpKeygenFailure = 120;
constexpr int EvpUnknownPbeAlgorithm = 121;
constexpr int EvpCipherParameterError = 122;
constexpr int EvpUnsupportedKeylength = 123;
constexpr int EvpUnsupportedKeyDerivationFunction = 124;
constexpr int EvpUnsupportedPrf = 125;
constexpr int EvpUnsupportedSaltType = 126;
constexpr int EvpNoCipherSet = 131;
constexpr int EvpUnsupportedNumberOfRounds = 135;
constexpr int EvpBadKeyLength = 137;
constexpr int EvpUnknownDigest = 161;

constexpr int Pkcs12DecodeError = 101;
constexpr int Pkcs12EncodeError = 102;
constexpr int Pkcs12EncryptError = 103;
constexpr int Pkcs12IvGenError = 106;
constexpr int Pkcs12KeyGenError = 107;
constexpr int Pkcs12AlgorCipherInitError = 115;
constexpr int Pkcs12CipherFinalError = 116;
constexpr int Pkcs12PbeCryptError = 117;

constexpr std::size_t MaxDigestSize = 64;
constexpr std::size_t MaxKeyLength = 64;
constexpr std::size_t MaxIvLength = 16;
constexpr std::size_t Pkcs5SaltLength = 8;
constexpr int Pkcs5DefaultIterations = 2048;
constexpr std::size_t DesBlockSize = 8;
constexpr std::size_t MaxDesKeyLength = 24;
constexpr std::size_t Pkcs5IvEnd = 16;
constexpr int Pkcs12KeyId = 1;
constexpr int Pkcs12IvId = 2;

constexpr unsigned char TagInteger = 0x02;
constexpr unsigned char TagOctetString = 0x04;
constexpr unsigned char TagObject = 0x06;
constexpr unsigned char TagSequence = 0x30;

constexpr std::string_view Aes128CbcOid = "\x60\x86\x48\x01\x65\x03\x04\x01\x02"sv;
constexpr std::string_view Aes192CbcOid = "\x60\x86\x48\x01\x65\x03\x04\x01\x16"sv;
constexpr std::string_view Aes256CbcOid = "\x60\x86\x48\x01\x65\x03\x04\x01\x2a"sv;
constexpr std::string_view DesCbcOid = "\x2b\x0e\x03\x02\x07"sv;
constexpr std::string_view DesEde3CbcOid = "\x2a\x86\x48\x86\xf7\x0d\x03\x07"sv;
constexpr std::string_view Pbes2Oid = "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x0d"sv;
constexpr std::string_view PbeMd2DesOid = "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x01"sv;
constexpr std::string_view PbeMd5DesOid = "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x03"sv;
constexpr std::string_view PbeMd2Rc2Oid = "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x04"sv;
constexpr std::string_view PbeSha1DesOid = "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x0a"sv;
constexpr std::string_view PbeSha1TripleDesOid = "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x03"sv;
constexpr std::string_view PbeSha1TwoKeyDesOid = "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x04"sv;
constexpr std::string_view Pbkdf2Oid = "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x0c"sv;
constexpr std::string_view HmacWithMd5Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x06"sv;
constexpr std::string_view HmacWithSha1Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x07"sv;
constexpr std::string_view HmacWithSha224Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x08"sv;
constexpr std::string_view HmacWithSha256Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x09"sv;
constexpr std::string_view HmacWithSha384Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x0a"sv;
constexpr std::string_view HmacWithSha512Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x0b"sv;
constexpr std::string_view HmacWithSha512_224Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x0c"sv;
constexpr std::string_view HmacWithSha512_256Oid = "\x2a\x86\x48\x86\xf7\x0d\x02\x0d"sv;
constexpr std::string_view HmacSha3_224Oid = "\x60\x86\x48\x01\x65\x03\x04\x02\x0d"sv;
constexpr std::string_view HmacSha3_256Oid = "\x60\x86\x48\x01\x65\x03\x04\x02\x0e"sv;
constexpr std::string_view HmacSha3_384Oid = "\x60\x86\x48\x01\x65\x03\x04\x02\x0f"sv;
constexpr std::string_view HmacSha3_512Oid = "\x60\x86\x48\x01\x65\x03\x04\x02\x10"sv;

struct UnavailableCipherOid {
    std::string_view oid;
    std::string_view name;
};

struct UnavailablePbeOid {
    std::string_view oid;
    std::string_view name;
};

constexpr std::array<std::string_view, 86> UnavailableCipherNames{
    "AES-128-CFB"sv, "AES-128-CFB1"sv, "AES-128-CFB8"sv, "AES-128-CTR"sv, "AES-128-ECB"sv, "AES-128-OFB"sv,
    "AES-128-XTS"sv, "AES-192-CFB"sv, "AES-192-CFB1"sv, "AES-192-CFB8"sv, "AES-192-CTR"sv, "AES-192-ECB"sv,
    "AES-192-OFB"sv, "AES-256-CFB"sv, "AES-256-CFB1"sv, "AES-256-CFB8"sv, "AES-256-CTR"sv, "AES-256-ECB"sv,
    "AES-256-OFB"sv, "AES-256-XTS"sv, "BF"sv, "BF-CBC"sv, "BF-CFB"sv, "BF-ECB"sv, "BF-OFB"sv, "CAMELLIA-128-CBC"sv,
    "CAMELLIA-128-CFB"sv, "CAMELLIA-128-CFB1"sv, "CAMELLIA-128-CFB8"sv, "CAMELLIA-128-ECB"sv, "CAMELLIA-128-OFB"sv,
    "CAMELLIA-192-CBC"sv, "CAMELLIA-192-CFB"sv, "CAMELLIA-192-CFB1"sv, "CAMELLIA-192-CFB8"sv, "CAMELLIA-192-ECB"sv,
    "CAMELLIA-192-OFB"sv, "CAMELLIA-256-CBC"sv, "CAMELLIA-256-CFB"sv, "CAMELLIA-256-CFB1"sv, "CAMELLIA-256-CFB8"sv,
    "CAMELLIA-256-ECB"sv, "CAMELLIA-256-OFB"sv, "CAMELLIA128"sv, "CAMELLIA192"sv, "CAMELLIA256"sv, "CAST"sv,
    "CAST5-CBC"sv, "CAST5-CFB"sv, "CAST5-ECB"sv, "CAST5-OFB"sv, "DES-CFB"sv, "DES-CFB1"sv, "DES-CFB8"sv, "DES-ECB"sv,
    "DES-EDE"sv, "DES-EDE-CFB"sv, "DES-EDE-OFB"sv, "DES-EDE3"sv, "DES-EDE3-CFB"sv, "DES-EDE3-CFB1"sv,
    "DES-EDE3-CFB8"sv, "DES-EDE3-OFB"sv, "DES-OFB"sv, "DESX"sv, "DESX-CBC"sv, "IDEA"sv, "IDEA-CBC"sv, "IDEA-CFB"sv,
    "IDEA-ECB"sv, "IDEA-OFB"sv, "RC2"sv,
    "RC2-40-CBC"sv, "RC2-64-CBC"sv, "RC2-CBC"sv, "RC2-CFB"sv, "RC2-ECB"sv, "RC2-OFB"sv, "RC4"sv, "RC4-40"sv,
    "SM4"sv, "SM4-CBC"sv, "SM4-CFB"sv, "SM4-CTR"sv, "SM4-ECB"sv, "SM4-OFB"sv,
};

constexpr std::array<UnavailableCipherOid, 44> UnavailableCipherOids{{
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x04"sv, "AES-128-CFB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x01"sv, "AES-128-ECB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x03"sv, "AES-128-OFB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x18"sv, "AES-192-CFB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x15"sv, "AES-192-ECB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x17"sv, "AES-192-OFB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x2c"sv, "AES-256-CFB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x29"sv, "AES-256-ECB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x2b"sv, "AES-256-OFB"sv},
    {"\x2b\x06\x01\x04\x01\x97\x55\x01\x02"sv, "BF-CBC"sv},
    {"\x2a\x83\x08\x8c\x9a\x4b\x3d\x01\x01\x01\x02"sv, "CAMELLIA-128-CBC"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x04"sv, "CAMELLIA-128-CFB"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x01"sv, "CAMELLIA-128-ECB"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x03"sv, "CAMELLIA-128-OFB"sv},
    {"\x2a\x83\x08\x8c\x9a\x4b\x3d\x01\x01\x01\x03"sv, "CAMELLIA-192-CBC"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x18"sv, "CAMELLIA-192-CFB"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x15"sv, "CAMELLIA-192-ECB"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x17"sv, "CAMELLIA-192-OFB"sv},
    {"\x2a\x83\x08\x8c\x9a\x4b\x3d\x01\x01\x01\x04"sv, "CAMELLIA-256-CBC"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x2c"sv, "CAMELLIA-256-CFB"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x29"sv, "CAMELLIA-256-ECB"sv},
    {"\x03\xa2\x31\x05\x03\x01\x09\x2b"sv, "CAMELLIA-256-OFB"sv},
    {"\x2a\x86\x48\x86\xf6\x7d\x07\x42\x0a"sv, "CAST5-CBC"sv},
    {"\x2b\x0e\x03\x02\x09"sv, "DES-CFB"sv},
    {"\x2b\x0e\x03\x02\x06"sv, "DES-ECB"sv},
    {"\x2b\x0e\x03\x02\x11"sv, "DES-EDE"sv},
    {"\x2b\x0e\x03\x02\x08"sv, "DES-OFB"sv},
    {"\x2b\x06\x01\x04\x01\x81\x3c\x07\x01\x01\x02"sv, "IDEA-CBC"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x03\x02"sv, "RC2-CBC"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x03\x04"sv, "RC4"sv},
    {"\x2a\x81\x1c\xcf\x55\x01\x68\x02"sv, "SM4-CBC"sv},
    {"\x2a\x81\x1c\xcf\x55\x01\x68\x04"sv, "SM4-CFB"sv},
    {"\x2a\x81\x1c\xcf\x55\x01\x68\x07"sv, "SM4-CTR"sv},
    {"\x2a\x81\x1c\xcf\x55\x01\x68\x01"sv, "SM4-ECB"sv},
    {"\x2a\x81\x1c\xcf\x55\x01\x68\x03"sv, "SM4-OFB"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x07"sv, "id-aes128-CCM"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x06"sv, "id-aes128-GCM"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x05"sv, "id-aes128-wrap"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x1b"sv, "id-aes192-CCM"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x1a"sv, "id-aes192-GCM"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x19"sv, "id-aes192-wrap"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x2f"sv, "id-aes256-CCM"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x2e"sv, "id-aes256-GCM"sv},
    {"\x60\x86\x48\x01\x65\x03\x04\x01\x2d"sv, "id-aes256-wrap"sv},
}};

constexpr std::array<UnavailablePbeOid, 6> UnavailablePbeOids{{
    {"\x2a\x86\x48\x86\xf7\x0d\x01\x05\x0b"sv, "PBE-SHA1-RC2-64"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x01"sv, "PBE-SHA1-RC4-128"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x02"sv, "PBE-SHA1-RC4-40"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x05"sv, "PBE-SHA1-RC2-128"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x06"sv, "PBE-SHA1-RC2-40"sv},
    {"\x2a\x86\x48\x86\xf7\x0d\x01\x05\x06"sv, "PBE-MD5-RC2-64"sv},
}};

struct DesPbeScheme {
    std::string_view oid;
    LibreSsl3PemDes cipher;
    bool sha1;
    bool pkcs12;
};

constexpr std::array<DesPbeScheme, 4> DesPbeSchemes{{
    {PbeMd5DesOid, LibreSsl3PemDes::Single, false, false},
    {PbeSha1DesOid, LibreSsl3PemDes::Single, true, false},
    {PbeSha1TripleDesOid, LibreSsl3PemDes::ThreeKey, true, true},
    {PbeSha1TwoKeyDesOid, LibreSsl3PemDes::TwoKey, true, true},
}};

void ClearBytes(void* data, std::size_t length) {
    auto* const bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = 0;
}

struct ClearVectorOnExit {
    std::vector<unsigned char>& bytes;
    ~ClearVectorOnExit() { ClearBytes(bytes.data(), bytes.size()); }
};

template<std::size_t Size>
struct ClearArrayOnExit {
    std::array<unsigned char, Size>& bytes;
    ~ClearArrayOnExit() { ClearBytes(bytes.data(), bytes.size()); }
};

struct ClearPasswordOnExit {
    std::array<char, LibreSsl3PemBufferSize>& bytes;
    ~ClearPasswordOnExit() { ClearBytes(bytes.data(), bytes.size()); }
};

using CipherContext = std::unique_ptr<LibreSslEvpCipherCtx, decltype(&EVP_CIPHER_CTX_free_nid_postfix)>;

CipherContext NewCipherContext() {
    CipherContext context(EVP_CIPHER_CTX_new_nid_postfix(), EVP_CIPHER_CTX_free_nid_postfix);
    if (context.get() == nullptr) throw std::bad_alloc();
    return context;
}

const LibreSslEvpCipher* Aes128Cbc() {
    return reinterpret_cast<const LibreSslEvpCipher*>(EVP_aes_128_cbc_nid_postfix());
}

const LibreSslEvpCipher* Aes192Cbc() {
    return reinterpret_cast<const LibreSslEvpCipher*>(EVP_aes_192_cbc_nid_postfix());
}

const LibreSslEvpCipher* Aes256Cbc() {
    return reinterpret_cast<const LibreSslEvpCipher*>(EVP_aes_256_cbc_nid_postfix());
}

[[noreturn]] void MissingPrimitive(const char* caller, std::string_view what) {
    const std::string message = std::string(caller) + ": " + std::string(what);
    NotImplemented_nid_no_patch(message.c_str());
}

struct CipherChoice {
    const LibreSslEvpCipher* evp = nullptr;
    LibreSsl3PemDes des = LibreSsl3PemDes::None;
};

bool IsSet(const CipherChoice& cipher) {
    return cipher.evp != nullptr || cipher.des != LibreSsl3PemDes::None;
}

int DesKeyLength(LibreSsl3PemDes des) {
    if (des == LibreSsl3PemDes::Single) return 8;
    if (des == LibreSsl3PemDes::TwoKey) return 16;
    return 24;
}

int KeyLength(const CipherChoice& cipher) {
    return cipher.des != LibreSsl3PemDes::None ? DesKeyLength(cipher.des) : cipher.evp->keyLength;
}

int IvLength(const CipherChoice& cipher) {
    return cipher.des != LibreSsl3PemDes::None ? static_cast<int>(DesBlockSize) : cipher.evp->ivLength;
}

CipherChoice CipherByName(std::string_view name) {
    if (name == "AES-128-CBC"sv || name == "AES128"sv) return {Aes128Cbc(), LibreSsl3PemDes::None};
    if (name == "AES-192-CBC"sv || name == "AES192"sv) return {Aes192Cbc(), LibreSsl3PemDes::None};
    if (name == "AES-256-CBC"sv || name == "AES256"sv) return {Aes256Cbc(), LibreSsl3PemDes::None};
    if (name == "DES-CBC"sv || name == "DES"sv) return {nullptr, LibreSsl3PemDes::Single};
    if (name == "DES-EDE-CBC"sv) return {nullptr, LibreSsl3PemDes::TwoKey};
    if (name == "DES-EDE3-CBC"sv || name == "DES3"sv) return {nullptr, LibreSsl3PemDes::ThreeKey};
    for (const std::string_view known : UnavailableCipherNames) {
        if (name == known) MissingPrimitive("PEM_get_EVP_CIPHER_INFO", name);
    }
    return {};
}

CipherChoice CipherByOid(std::string_view oid) {
    if (oid == Aes128CbcOid) return {Aes128Cbc(), LibreSsl3PemDes::None};
    if (oid == Aes192CbcOid) return {Aes192Cbc(), LibreSsl3PemDes::None};
    if (oid == Aes256CbcOid) return {Aes256Cbc(), LibreSsl3PemDes::None};
    if (oid == DesCbcOid) return {nullptr, LibreSsl3PemDes::Single};
    if (oid == DesEde3CbcOid) return {nullptr, LibreSsl3PemDes::ThreeKey};
    for (const UnavailableCipherOid& known : UnavailableCipherOids) {
        if (oid == known.oid) MissingPrimitive("PKCS5_v2_PBE_keyivgen", known.name);
    }
    return {};
}

const DesPbeScheme* FindDesPbeScheme(std::string_view oid) {
    for (const DesPbeScheme& scheme : DesPbeSchemes) {
        if (oid == scheme.oid) return &scheme;
    }
    return nullptr;
}

std::string_view OidOfCipherObject(const void* cipher) {
    if (cipher == static_cast<const void*>(Aes128Cbc())) return Aes128CbcOid;
    if (cipher == static_cast<const void*>(Aes192Cbc())) return Aes192CbcOid;
    if (cipher == static_cast<const void*>(Aes256Cbc())) return Aes256CbcOid;
    MissingPrimitive("PKCS5_pbe2_set",
        "a cipher object other than EVP_aes_128_cbc, EVP_aes_192_cbc and EVP_aes_256_cbc");
}

const EvpMd* DigestByPrfOid(std::string_view oid) {
    if (oid == HmacWithMd5Oid) return EVP_md5_nid_postfix();
    if (oid == HmacWithSha1Oid) return reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix());
    if (oid == HmacWithSha224Oid) return reinterpret_cast<const EvpMd*>(EVP_sha224_nid_postfix());
    if (oid == HmacWithSha256Oid) return EVP_sha256_nid_postfix();
    if (oid == HmacWithSha384Oid) return EVP_sha384_nid_postfix();
    if (oid == HmacWithSha512Oid) return EVP_sha512_nid_postfix();
    if (oid == HmacWithSha512_256Oid) return EVP_sha512_256_nid_postfix();
    if (oid == HmacWithSha512_224Oid) MissingPrimitive("PKCS5_v2_PBKDF2_keyivgen", "EVP_sha512_224");
    if (oid == HmacSha3_224Oid) MissingPrimitive("PKCS5_v2_PBKDF2_keyivgen", "EVP_sha3_224");
    if (oid == HmacSha3_256Oid) MissingPrimitive("PKCS5_v2_PBKDF2_keyivgen", "EVP_sha3_256");
    if (oid == HmacSha3_384Oid) MissingPrimitive("PKCS5_v2_PBKDF2_keyivgen", "EVP_sha3_384");
    if (oid == HmacSha3_512Oid) MissingPrimitive("PKCS5_v2_PBKDF2_keyivgen", "EVP_sha3_512");
    return nullptr;
}

bool LoadIv(const char*& from, unsigned char* to, int count) {
    for (int index = 0; index < count; ++index) to[index] = 0;
    const int digits = count * 2;
    const char* cursor = from;
    for (int index = 0; index < digits; ++index) {
        int value = 0;
        if (*cursor >= '0' && *cursor <= '9') {
            value = *cursor - '0';
        } else if (*cursor >= 'A' && *cursor <= 'F') {
            value = *cursor - 'A' + 10;
        } else if (*cursor >= 'a' && *cursor <= 'f') {
            value = *cursor - 'a' + 10;
        } else {
            PutError(PemLibrary, PemBadIvChars);
            return false;
        }
        ++cursor;
        to[index / 2] = static_cast<unsigned char>(to[index / 2] | (value << ((index & 1) != 0 ? 0 : 4)));
    }
    from = cursor;
    return true;
}

void RequireDigestFits(std::uint32_t digestSize, std::size_t capacity) {
    if (digestSize > capacity) throw std::length_error("EVP_BytesToKey: digest longer than EVP_MAX_MD_SIZE");
}

bool DeriveKeyBlocks(EvpMdContext* context, const EvpMd* digest, const unsigned char* salt, const unsigned char* data,
    int dataLength, int count, int keyRemaining, int ivRemaining, unsigned char* key, unsigned char* iv,
    std::array<unsigned char, MaxDigestSize>& block) {
    std::uint32_t digestSize = 0;
    bool addDigest = false;
    for (;;) {
        if (!EVP_DigestInit_ex_nid_postfix(context, digest, nullptr)) return false;
        if (addDigest && !EVP_DigestUpdate_nid_postfix(context, block.data(), digestSize)) return false;
        addDigest = true;
        if (!EVP_DigestUpdate_nid_postfix(context, data, static_cast<std::size_t>(dataLength))) return false;
        if (salt != nullptr && !EVP_DigestUpdate_nid_postfix(context, salt, Pkcs5SaltLength)) return false;
        if (!EVP_DigestFinal_ex_nid_postfix(context, block.data(), &digestSize)) return false;
        RequireDigestFits(digestSize, block.size());
        for (unsigned round = 1; round < static_cast<unsigned>(count); ++round) {
            if (!EVP_DigestInit_ex_nid_postfix(context, digest, nullptr)) return false;
            if (!EVP_DigestUpdate_nid_postfix(context, block.data(), digestSize)) return false;
            if (!EVP_DigestFinal_ex_nid_postfix(context, block.data(), &digestSize)) return false;
            RequireDigestFits(digestSize, block.size());
        }
        std::uint32_t used = 0;
        while (keyRemaining != 0 && used != digestSize) {
            if (key != nullptr) *key++ = block[used];
            --keyRemaining;
            ++used;
        }
        while (ivRemaining != 0 && used != digestSize) {
            if (iv != nullptr) *iv++ = block[used];
            --ivRemaining;
            ++used;
        }
        if (keyRemaining == 0 && ivRemaining == 0) return true;
    }
}

int BytesToKey(int keyLength, int ivLength, const EvpMd* digest, const unsigned char* salt, const unsigned char* data,
    int dataLength, int count, unsigned char* key, unsigned char* iv) {
    if (static_cast<std::size_t>(keyLength) > MaxKeyLength) {
        PutError(EvpLibrary, EvpBadKeyLength);
        return 0;
    }
    if (static_cast<std::size_t>(ivLength) > MaxIvLength) {
        PutError(EvpLibrary, EvpIvTooLarge);
        return 0;
    }
    if (data == nullptr) return keyLength;
    EvpMdContext* context = EVP_MD_CTX_create_nid_postfix();
    if (context == nullptr) return 0;
    std::array<unsigned char, MaxDigestSize> block{};
    const ClearArrayOnExit<MaxDigestSize> clearBlock{block};
    bool derived = false;
    try {
        derived = DeriveKeyBlocks(context, digest, salt, data, dataLength, count, keyLength, ivLength, key, iv, block);
    } catch (...) {
        EVP_MD_CTX_destroy_nid_postfix(context);
        throw;
    }
    EVP_MD_CTX_destroy_nid_postfix(context);
    return derived ? keyLength : 0;
}

bool DesCbcDecrypt(LibreSsl3PemDes des, const unsigned char* key, const unsigned char* iv, const unsigned char* input,
    std::size_t length, std::vector<unsigned char>& plain) {
    if (length == 0 || (length % DesBlockSize) != 0) {
        PutError(EvpLibrary, EvpWrongFinalBlockLength);
        return false;
    }
    std::vector<unsigned char> blocks(length, 0);
    const ClearVectorOnExit clearBlocks{blocks};
    std::array<unsigned char, DesBlockSize> chain{};
    std::memcpy(chain.data(), iv, chain.size());
    int status = 0;
    if (des == LibreSsl3PemDes::Single) {
        mbedtls_des_context context;
        mbedtls_des_init(&context);
        status = mbedtls_des_setkey_dec(&context, key);
        if (status == 0)
            status = mbedtls_des_crypt_cbc(&context, MBEDTLS_DES_DECRYPT, length, chain.data(), input, blocks.data());
        mbedtls_des_free(&context);
    } else {
        mbedtls_des3_context context;
        mbedtls_des3_init(&context);
        status = des == LibreSsl3PemDes::TwoKey ? mbedtls_des3_set2key_dec(&context, key)
                                               : mbedtls_des3_set3key_dec(&context, key);
        if (status == 0)
            status = mbedtls_des3_crypt_cbc(&context, MBEDTLS_DES_DECRYPT, length, chain.data(), input, blocks.data());
        mbedtls_des3_free(&context);
    }
    if (status != 0) throw std::runtime_error("DES CBC: mbedtls failed");
    const std::size_t pad = blocks[length - 1];
    if (pad == 0 || pad > DesBlockSize) {
        PutError(EvpLibrary, EvpBadDecrypt);
        return false;
    }
    const std::size_t plainLength = length - pad;
    for (std::size_t index = plainLength; index < length; ++index) {
        if (blocks[index] != pad) {
            PutError(EvpLibrary, EvpBadDecrypt);
            return false;
        }
    }
    plain.assign(blocks.begin(), blocks.begin() + static_cast<std::ptrdiff_t>(plainLength));
    return true;
}

struct MdContextHolder {
    EvpMdContext* context = EVP_MD_CTX_create_nid_postfix();
    MdContextHolder() = default;
    MdContextHolder(const MdContextHolder&) = delete;
    MdContextHolder& operator=(const MdContextHolder&) = delete;
    ~MdContextHolder() {
        if (context != nullptr) EVP_MD_CTX_destroy_nid_postfix(context);
    }
};

using DigestPart = std::pair<const unsigned char*, std::size_t>;

bool DigestParts(EvpMdContext* context, const EvpMd* digest, std::initializer_list<DigestPart> parts,
    unsigned char* out, std::uint32_t& size) {
    if (!EVP_DigestInit_ex_nid_postfix(context, digest, nullptr)) return false;
    for (const DigestPart& part : parts) {
        if (part.second != 0 && !EVP_DigestUpdate_nid_postfix(context, part.first, part.second)) return false;
    }
    if (!EVP_DigestFinal_ex_nid_postfix(context, out, &size)) return false;
    RequireDigestFits(size, MaxDigestSize);
    return true;
}

struct Algorithm {
    std::string_view oid;
    bool hasParameter = false;
    Element parameter;
};

bool ReadAlgorithmSequence(Reader& reader, Algorithm& algorithm) {
    Reader inner{nullptr, 0};
    if (!EnterSequence(reader, inner)) return false;
    bool present = false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    Element object;
    if (ReadField(inner, TypeObject, false, object) != 1) return false;
    algorithm.oid = std::string_view(reinterpret_cast<const char*>(object.content), object.length);
    algorithm.hasParameter = false;
    if (!AtField(inner, present)) return false;
    if (present) {
        if (ReadField(inner, TypeAny, false, algorithm.parameter) != 1) return false;
        algorithm.hasParameter = true;
    }
    return SequenceComplete(inner);
}

bool ReadAlgorithm(Reader& reader, Algorithm& algorithm) {
    if (ReadAlgorithmSequence(reader, algorithm)) return true;
    PutError(Asn1Library, Asn1NestedError);
    return false;
}

struct EncryptedKey {
    Algorithm algorithm;
    const unsigned char* data = nullptr;
    std::size_t length = 0;
};

bool ReadEncryptedKey(const unsigned char* der, std::size_t length, EncryptedKey& key) {
    Reader reader{der, length};
    Reader inner{nullptr, 0};
    if (!EnterSequence(reader, inner)) return false;
    bool present = false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    if (!ReadAlgorithm(inner, key.algorithm)) return false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    Element digest;
    if (ReadField(inner, TypeOctetString, false, digest) != 1) return false;
    if (!SequenceComplete(inner)) return false;
    key.data = digest.content;
    key.length = digest.length;
    return true;
}

struct Pbkdf2Parameters {
    Element salt;
    Integer iterations;
    bool hasKeyLength = false;
    Integer keyLength;
    bool hasPrf = false;
    Algorithm prf;
};

bool ReadPbkdf2Parameters(const Element& parameter, Pbkdf2Parameters& parameters) {
    Reader reader{parameter.encoded, parameter.encodedLength};
    Reader inner{nullptr, 0};
    if (!EnterSequence(reader, inner)) return false;
    bool present = false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    if (ReadField(inner, TypeAny, false, parameters.salt) != 1) return false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    Element iterations;
    if (ReadField(inner, TypeInteger, false, iterations) != 1) return false;
    StoreInteger(iterations, parameters.iterations);
    parameters.hasKeyLength = false;
    parameters.hasPrf = false;
    if (!AtField(inner, present)) return false;
    if (present) {
        Element keyLength;
        const int found = ReadField(inner, TypeInteger, true, keyLength);
        if (found == 0) return false;
        if (found == 1) {
            StoreInteger(keyLength, parameters.keyLength);
            parameters.hasKeyLength = true;
        }
        if (!AtField(inner, present)) return false;
        if (present) {
            if (!ReadAlgorithm(inner, parameters.prf)) return false;
            parameters.hasPrf = true;
        }
    }
    return SequenceComplete(inner);
}

bool ReadPbes2Parameters(const Element& parameter, Algorithm& keyFunction, Algorithm& encryption) {
    Reader reader{parameter.encoded, parameter.encodedLength};
    Reader inner{nullptr, 0};
    if (!EnterSequence(reader, inner)) return false;
    bool present = false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    if (!ReadAlgorithm(inner, keyFunction)) return false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    if (!ReadAlgorithm(inner, encryption)) return false;
    return SequenceComplete(inner);
}

struct PbeParameters {
    Element salt;
    Integer iterations;
};

bool ReadPbeParameters(const Element& parameter, PbeParameters& parameters) {
    Reader reader{parameter.encoded, parameter.encodedLength};
    Reader inner{nullptr, 0};
    if (!EnterSequence(reader, inner)) return false;
    bool present = false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    if (ReadField(inner, TypeOctetString, false, parameters.salt) != 1) return false;
    if (!AtField(inner, present)) return false;
    if (!present) return MissingField();
    Element iterations;
    if (ReadField(inner, TypeInteger, false, iterations) != 1) return false;
    StoreInteger(iterations, parameters.iterations);
    return SequenceComplete(inner);
}

struct PbeContext {
    CipherContext evp = NewCipherContext();
    LibreSsl3PemDes des = LibreSsl3PemDes::None;
    std::array<unsigned char, MaxDesKeyLength> desKey{};
    std::array<unsigned char, DesBlockSize> desOiv{};
    std::array<unsigned char, DesBlockSize> desIv{};
    PbeContext() = default;
    PbeContext(const PbeContext&) = delete;
    PbeContext& operator=(const PbeContext&) = delete;
    ~PbeContext() { ClearBytes(desKey.data(), desKey.size()); }
};

void SelectDes(PbeContext& context, LibreSsl3PemDes des) {
    context.des = des;
    context.desIv = context.desOiv;
}

void SetDesKey(PbeContext& context, const unsigned char* key, const unsigned char* iv) {
    if (iv != nullptr) std::memcpy(context.desOiv.data(), iv, context.desOiv.size());
    context.desIv = context.desOiv;
    std::memcpy(context.desKey.data(), key, static_cast<std::size_t>(DesKeyLength(context.des)));
}

bool Pbkdf2KeyIvGen(PbeContext& context, const char* password, int passwordLength, const Algorithm& keyFunction,
    int encrypt) {
    if (context.des == LibreSsl3PemDes::None && context.evp.get()->cipher == nullptr) {
        PutError(EvpLibrary, EvpNoCipherSet);
        return false;
    }
    const int keyLength =
        context.des != LibreSsl3PemDes::None ? DesKeyLength(context.des) : context.evp.get()->keyLen;
    std::array<unsigned char, MaxKeyLength> key{};
    const ClearArrayOnExit<MaxKeyLength> clearKey{key};
    if (static_cast<unsigned>(keyLength) > key.size()) {
        PutError(EvpLibrary, EvpBadKeyLength);
        return false;
    }
    if (!keyFunction.hasParameter || keyFunction.parameter.type != TypeSequence) {
        PutError(EvpLibrary, EvpDecodeError);
        return false;
    }
    Pbkdf2Parameters parameters;
    if (!ReadPbkdf2Parameters(keyFunction.parameter, parameters)) {
        PutError(EvpLibrary, EvpDecodeError);
        return false;
    }
    if (parameters.hasKeyLength && IntegerValue(parameters.keyLength) != static_cast<std::int64_t>(keyLength)) {
        PutError(EvpLibrary, EvpUnsupportedKeylength);
        return false;
    }
    const EvpMd* digest = DigestByPrfOid(parameters.hasPrf ? parameters.prf.oid : HmacWithSha1Oid);
    if (digest == nullptr) {
        PutError(EvpLibrary, EvpUnsupportedPrf);
        return false;
    }
    if (parameters.salt.type != TypeOctetString) {
        PutError(EvpLibrary, EvpUnsupportedSaltType);
        return false;
    }
    const int iterations = static_cast<int>(IntegerValue(parameters.iterations));
    if (iterations <= 0) {
        PutError(EvpLibrary, EvpUnsupportedNumberOfRounds);
        return false;
    }
    if (!PKCS5_PBKDF2_HMAC_nid_postfix(password, passwordLength, parameters.salt.content,
            static_cast<int>(parameters.salt.length), iterations, digest, keyLength, key.data()))
        return false;
    if (context.des != LibreSsl3PemDes::None) {
        SetDesKey(context, key.data(), nullptr);
        return true;
    }
    return EVP_CipherInit_ex_nid_postfix(context.evp.get(), nullptr, nullptr, key.data(), nullptr, encrypt) != 0;
}

bool DesIvFromParameter(PbeContext& context, const Element& parameter) {
    if (parameter.type != TypeOctetString) {
        PutError(Asn1Library, Asn1DataIsWrong);
        PutError(EvpLibrary, EvpCipherParameterError);
        return false;
    }
    const std::size_t copied = parameter.length < DesBlockSize ? parameter.length : DesBlockSize;
    if (copied != 0) std::memcpy(context.desOiv.data(), parameter.content, copied);
    if (parameter.length != DesBlockSize) {
        PutError(EvpLibrary, EvpCipherParameterError);
        return false;
    }
    context.desIv = context.desOiv;
    return true;
}

bool Pbes2KeyIvGen(PbeContext& context, const char* password, int passwordLength, const Algorithm& algorithm,
    int encrypt) {
    if (!algorithm.hasParameter || algorithm.parameter.type != TypeSequence) {
        PutError(EvpLibrary, EvpDecodeError);
        return false;
    }
    Algorithm keyFunction;
    Algorithm encryption;
    if (!ReadPbes2Parameters(algorithm.parameter, keyFunction, encryption)) {
        PutError(EvpLibrary, EvpDecodeError);
        return false;
    }
    if (keyFunction.oid != Pbkdf2Oid) {
        PutError(EvpLibrary, EvpUnsupportedKeyDerivationFunction);
        return false;
    }
    const CipherChoice choice = CipherByOid(encryption.oid);
    if (!IsSet(choice)) {
        PutError(EvpLibrary, EvpUnsupportedCipher);
        return false;
    }
    if (choice.des != LibreSsl3PemDes::None) {
        SelectDes(context, choice.des);
        if (encryption.hasParameter && !DesIvFromParameter(context, encryption.parameter)) return false;
        return Pbkdf2KeyIvGen(context, password, passwordLength, keyFunction, encrypt);
    }
    const LibreSslEvpCipher* const cipher = choice.evp;
    LibreSslEvpCipherCtx* const evp = context.evp.get();
    if (!EVP_CipherInit_ex_nid_postfix(evp, cipher, nullptr, nullptr, nullptr, encrypt)) return false;
    if (encryption.hasParameter) {
        const int ivLength = cipher->ivLength;
        if (ivLength < 0 || static_cast<std::size_t>(ivLength) > sizeof(evp->oiv)
            || static_cast<std::size_t>(ivLength) > sizeof(evp->iv)) {
            PutError(EvpLibrary, EvpIvTooLarge);
        } else {
            if (encryption.parameter.type != TypeOctetString) {
                PutError(Asn1Library, Asn1DataIsWrong);
                PutError(EvpLibrary, EvpCipherParameterError);
                return false;
            }
            const std::size_t wanted = static_cast<std::size_t>(ivLength);
            const std::size_t copied = encryption.parameter.length < wanted ? encryption.parameter.length : wanted;
            if (copied != 0) std::memcpy(evp->oiv, encryption.parameter.content, copied);
            if (encryption.parameter.length != wanted) {
                PutError(EvpLibrary, EvpCipherParameterError);
                return false;
            }
            if (wanted != 0) std::memcpy(evp->iv, evp->oiv, wanted);
        }
    }
    return Pbkdf2KeyIvGen(context, password, passwordLength, keyFunction, encrypt);
}

bool Pkcs5KeyIvGen(PbeContext& context, const char* password, int passwordLength, const Algorithm& algorithm,
    LibreSsl3PemDes cipher, const EvpMd* digest) {
    if (!algorithm.hasParameter || algorithm.parameter.type != TypeSequence) {
        PutError(EvpLibrary, EvpDecodeError);
        return false;
    }
    PbeParameters parameters;
    if (!ReadPbeParameters(algorithm.parameter, parameters)) {
        PutError(EvpLibrary, EvpDecodeError);
        return false;
    }
    const int iterations = static_cast<int>(IntegerValue(parameters.iterations));
    if (iterations <= 0) {
        PutError(EvpLibrary, EvpUnsupportedNumberOfRounds);
        return false;
    }
    if (password == nullptr) {
        passwordLength = 0;
    } else if (passwordLength == -1) {
        passwordLength = static_cast<int>(std::strlen(password));
    }
    if (passwordLength < 0) throw std::invalid_argument("PKCS5_PBE_keyivgen: negative password length");
    const MdContextHolder holder;
    if (holder.context == nullptr) return false;
    std::array<unsigned char, MaxDigestSize> block{};
    const ClearArrayOnExit<MaxDigestSize> clearBlock{block};
    std::uint32_t size = 0;
    if (!DigestParts(holder.context, digest,
            {{reinterpret_cast<const unsigned char*>(password), static_cast<std::size_t>(passwordLength)},
                {parameters.salt.content, parameters.salt.length}},
            block.data(), size))
        return false;
    for (int round = 1; round < iterations; ++round) {
        const std::size_t previous = size;
        if (!DigestParts(holder.context, digest, {{block.data(), previous}}, block.data(), size)) return false;
    }
    SelectDes(context, cipher);
    SetDesKey(context, block.data(), block.data() + (Pkcs5IvEnd - DesBlockSize));
    return true;
}

bool Pkcs12KeyGen(const char* password, int passwordLength, const unsigned char* salt, std::size_t saltLength, int id,
    int iterations, std::size_t count, unsigned char* out, const EvpMd* digest) {
    std::vector<unsigned char> unicode;
    const ClearVectorOnExit clearUnicode{unicode};
    if (password != nullptr) {
        const std::size_t characters =
            passwordLength < 0 ? std::strlen(password) : static_cast<std::size_t>(passwordLength);
        unicode.assign((characters + 1) * 2, 0);
        for (std::size_t index = 0; index < characters; ++index)
            unicode[index * 2 + 1] = static_cast<unsigned char>(password[index]);
    }
    const MdContextHolder holder;
    const int blockSize = EVP_MD_block_size_nid_postfix(digest);
    const int digestSize = EVP_MD_size_nid_postfix(digest);
    if (holder.context == nullptr || blockSize <= 0 || digestSize <= 0) {
        PutError(Pkcs12Library, ErrRMallocFailure);
        return false;
    }
    const std::size_t v = static_cast<std::size_t>(blockSize);
    const std::size_t u = static_cast<std::size_t>(digestSize);
    RequireDigestFits(static_cast<std::uint32_t>(digestSize), MaxDigestSize);
    const std::size_t saltBlocks = v * ((saltLength + v - 1) / v);
    const std::size_t passwordBlocks = unicode.empty() ? 0 : v * ((unicode.size() + v - 1) / v);
    std::vector<unsigned char> input(saltBlocks + passwordBlocks, 0);
    const ClearVectorOnExit clearInput{input};
    for (std::size_t index = 0; index < saltBlocks; ++index) input[index] = salt[index % saltLength];
    for (std::size_t index = 0; index < passwordBlocks; ++index)
        input[saltBlocks + index] = unicode[index % unicode.size()];
    const std::vector<unsigned char> diversifier(v, static_cast<unsigned char>(id));
    std::vector<unsigned char> filler(v, 0);
    const ClearVectorOnExit clearFiller{filler};
    std::array<unsigned char, MaxDigestSize> block{};
    const ClearArrayOnExit<MaxDigestSize> clearBlock{block};
    for (;;) {
        std::uint32_t size = 0;
        bool hashed = DigestParts(holder.context, digest, {{diversifier.data(), v}, {input.data(), input.size()}},
            block.data(), size);
        for (int round = 1; hashed && round < iterations; ++round)
            hashed = DigestParts(holder.context, digest, {{block.data(), u}}, block.data(), size);
        if (!hashed) {
            PutError(Pkcs12Library, ErrRMallocFailure);
            return false;
        }
        const std::size_t copied = count < u ? count : u;
        std::memcpy(out, block.data(), copied);
        if (u >= count) return true;
        count -= u;
        out += u;
        for (std::size_t index = 0; index < v; ++index) filler[index] = block[index % u];
        for (std::size_t start = 0; start < input.size(); start += v) {
            unsigned carry = 1;
            for (std::size_t index = v; index-- > 0;) {
                carry += static_cast<unsigned>(input[start + index]) + filler[index];
                input[start + index] = static_cast<unsigned char>(carry);
                carry >>= 8;
            }
        }
    }
}

bool Pkcs12KeyIvGen(PbeContext& context, const char* password, int passwordLength, const Algorithm& algorithm,
    LibreSsl3PemDes cipher, const EvpMd* digest) {
    if (!algorithm.hasParameter || algorithm.parameter.type != TypeSequence) {
        PutError(Pkcs12Library, Pkcs12DecodeError);
        return false;
    }
    PbeParameters parameters;
    if (!ReadPbeParameters(algorithm.parameter, parameters)) {
        PutError(Pkcs12Library, Pkcs12DecodeError);
        return false;
    }
    const int iterations = static_cast<int>(IntegerValue(parameters.iterations));
    if (iterations <= 0) {
        PutError(Pkcs12Library, Pkcs12DecodeError);
        return false;
    }
    std::array<unsigned char, MaxDesKeyLength> key{};
    const ClearArrayOnExit<MaxDesKeyLength> clearKey{key};
    std::array<unsigned char, DesBlockSize> iv{};
    if (!Pkcs12KeyGen(password, passwordLength, parameters.salt.content, parameters.salt.length, Pkcs12KeyId,
            iterations, static_cast<std::size_t>(DesKeyLength(cipher)), key.data(), digest)) {
        PutError(Pkcs12Library, Pkcs12KeyGenError);
        return false;
    }
    if (!Pkcs12KeyGen(password, passwordLength, parameters.salt.content, parameters.salt.length, Pkcs12IvId,
            iterations, iv.size(), iv.data(), digest)) {
        PutError(Pkcs12Library, Pkcs12IvGenError);
        return false;
    }
    SelectDes(context, cipher);
    SetDesKey(context, key.data(), iv.data());
    return true;
}

std::string_view UnavailablePbeName(std::string_view oid) {
    for (const UnavailablePbeOid& known : UnavailablePbeOids) {
        if (oid == known.oid) return known.name;
    }
    return {};
}

bool PbeCipherInit(const Algorithm& algorithm, const char* password, int passwordLength, PbeContext& context,
    int encrypt) {
    bool generated = false;
    if (algorithm.oid == Pbes2Oid) {
        generated = Pbes2KeyIvGen(context, password, passwordLength, algorithm, encrypt);
    } else if (algorithm.oid == Pbkdf2Oid) {
        generated = Pbkdf2KeyIvGen(context, password, passwordLength, algorithm, encrypt);
    } else if (const DesPbeScheme* const scheme = FindDesPbeScheme(algorithm.oid); scheme != nullptr) {
        const EvpMd* const digest =
            scheme->sha1 ? reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix()) : EVP_md5_nid_postfix();
        generated = scheme->pkcs12
            ? Pkcs12KeyIvGen(context, password, passwordLength, algorithm, scheme->cipher, digest)
            : Pkcs5KeyIvGen(context, password, passwordLength, algorithm, scheme->cipher, digest);
    } else if (algorithm.oid == PbeMd2DesOid || algorithm.oid == PbeMd2Rc2Oid) {
        PutError(EvpLibrary, EvpUnknownDigest);
        return false;
    } else {
        const std::string_view unavailable = UnavailablePbeName(algorithm.oid);
        if (!unavailable.empty()) MissingPrimitive("EVP_PBE_CipherInit", unavailable);
        PutError(EvpLibrary, EvpUnknownPbeAlgorithm);
        return false;
    }
    if (!generated) {
        PutError(EvpLibrary, EvpKeygenFailure);
        return false;
    }
    return true;
}

bool PbeCrypt(const Algorithm& algorithm, const char* password, int passwordLength, const unsigned char* input,
    int inputLength, std::vector<unsigned char>& output, int encrypt) {
    PbeContext context;
    if (!PbeCipherInit(algorithm, password, passwordLength, context, encrypt)) {
        PutError(Pkcs12Library, Pkcs12AlgorCipherInitError);
        return false;
    }
    if (context.des != LibreSsl3PemDes::None) {
        if (encrypt != 0) NotImplemented_nid_no_patch("PKCS12_pbe_crypt: DES encryption");
        if (inputLength < 0) throw std::invalid_argument("PKCS12_pbe_crypt: input length outside 0 to INT_MAX");
        if (!DesCbcDecrypt(context.des, context.desKey.data(), context.desIv.data(), input,
                static_cast<std::size_t>(inputLength), output)) {
            PutError(Pkcs12Library, Pkcs12CipherFinalError);
            return false;
        }
        return true;
    }
    LibreSslEvpCipherCtx* const evp = context.evp.get();
    const int blockSize = evp->cipher->blockSize;
    if (blockSize < 0 || inputLength < 0 || inputLength > std::numeric_limits<int>::max() - blockSize)
        throw std::invalid_argument("PKCS12_pbe_crypt: input length outside 0 to INT_MAX");
    output.assign(static_cast<std::size_t>(inputLength) + static_cast<std::size_t>(blockSize) + 1, 0);
    int written = 0;
    if (!EVP_CipherUpdate_nid_disambig1_nid_postfix(evp, output.data(), &written, input, inputLength)) {
        PutError(Pkcs12Library, ErrREvpLib);
        return false;
    }
    const int updated = written;
    if (!EVP_CipherFinal_ex_nid_postfix(evp, output.data() + updated, &written)) {
        PutError(Pkcs12Library, Pkcs12CipherFinalError);
        return false;
    }
    const std::size_t total = static_cast<std::size_t>(updated) + static_cast<std::size_t>(written);
    if (total > output.size()) throw std::length_error("PKCS12_pbe_crypt: cipher wrote past its buffer");
    ClearBytes(output.data() + total, output.size() - total);
    output.resize(total);
    return true;
}

LibreSsl3Pkcs8PrivKeyInfo* DecryptToPkcs8(const EncryptedKey& key, const char* password, int passwordLength) {
    std::vector<unsigned char> plain;
    const ClearVectorOnExit clearPlain{plain};
    if (!PbeCrypt(key.algorithm, password, passwordLength, key.data, static_cast<int>(key.length), plain, 0)) {
        PutError(Pkcs12Library, Pkcs12PbeCryptError);
        return nullptr;
    }
    const unsigned char empty = 0;
    const unsigned char* cursor = plain.empty() ? &empty : plain.data();
    LibreSsl3Pkcs8PrivKeyInfo* info =
        d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(plain.size()));
    if (info == nullptr) PutError(Pkcs12Library, Pkcs12DecodeError);
    return info;
}

void AppendLength(std::vector<unsigned char>& out, std::size_t length) {
    if (length < 0x80) {
        out.push_back(static_cast<unsigned char>(length));
        return;
    }
    std::array<unsigned char, sizeof(std::size_t)> bytes{};
    std::size_t count = 0;
    for (std::size_t value = length; value != 0; value >>= 8) bytes[count++] = static_cast<unsigned char>(value & 0xff);
    out.push_back(static_cast<unsigned char>(0x80 | count));
    while (count != 0) out.push_back(bytes[--count]);
}

void AppendElement(std::vector<unsigned char>& out, unsigned char tag, const unsigned char* content,
    std::size_t length) {
    out.push_back(tag);
    AppendLength(out, length);
    if (length != 0) out.insert(out.end(), content, content + length);
}

void AppendElement(std::vector<unsigned char>& out, unsigned char tag, const std::vector<unsigned char>& content) {
    AppendElement(out, tag, content.data(), content.size());
}

void AppendElement(std::vector<unsigned char>& out, unsigned char tag, std::string_view content) {
    AppendElement(out, tag, reinterpret_cast<const unsigned char*>(content.data()), content.size());
}

std::vector<unsigned char> Pbes2Algorithm(std::string_view cipherOid, const unsigned char* iv, std::size_t ivLength,
    const unsigned char* salt, std::size_t saltLength, int iterations) {
    std::vector<unsigned char> kdfBody;
    AppendElement(kdfBody, TagOctetString, salt, saltLength);
    std::vector<unsigned char> count;
    for (unsigned value = static_cast<unsigned>(iterations); value != 0; value >>= 8)
        count.insert(count.begin(), static_cast<unsigned char>(value & 0xff));
    if (count.empty() || (count.front() & 0x80u) != 0) count.insert(count.begin(), 0);
    AppendElement(kdfBody, TagInteger, count);
    std::vector<unsigned char> keyFunctionBody;
    AppendElement(keyFunctionBody, TagObject, Pbkdf2Oid);
    AppendElement(keyFunctionBody, TagSequence, kdfBody);
    std::vector<unsigned char> encryptionBody;
    AppendElement(encryptionBody, TagObject, cipherOid);
    AppendElement(encryptionBody, TagOctetString, iv, ivLength);
    std::vector<unsigned char> schemeBody;
    AppendElement(schemeBody, TagSequence, keyFunctionBody);
    AppendElement(schemeBody, TagSequence, encryptionBody);
    std::vector<unsigned char> algorithmBody;
    AppendElement(algorithmBody, TagObject, Pbes2Oid);
    AppendElement(algorithmBody, TagSequence, schemeBody);
    std::vector<unsigned char> algorithm;
    AppendElement(algorithm, TagSequence, algorithmBody);
    return algorithm;
}

}

bool PemCipherInfoFromDekInfo_nid_no_patch(const char* dekInfo, LibreSsl3PemCipherInfo& info) {
    const char* header = dekInfo;
    for (;;) {
        const char character = *header;
        if (!((character >= 'A' && character <= 'Z') || character == '-' || (character >= '0' && character <= '9')))
            break;
        ++header;
    }
    const std::string_view name(dekInfo, static_cast<std::size_t>(header - dekInfo));
    const bool terminated = *header == '\0';
    const CipherChoice cipher = CipherByName(name);
    info.cipher = cipher.evp;
    info.des = cipher.des;
    if (!IsSet(cipher)) {
        PutError(PemLibrary, PemUnsupportedEncryption);
        return false;
    }
    if (terminated) throw std::invalid_argument("PEM_get_EVP_CIPHER_INFO: DEK-Info ends at the cipher name");
    ++header;
    const int ivLength = IvLength(cipher);
    if (ivLength < 0 || static_cast<std::size_t>(ivLength) > sizeof(info.iv))
        throw std::invalid_argument("PEM_get_EVP_CIPHER_INFO: IV length outside 0 to 16");
    return LoadIv(header, info.iv, ivLength);
}

int PemReadPassword_nid_no_patch(char* buffer, int writeFlag, LibreSslPemPasswordCallback passwordCallback,
    void* userData) {
    int length = 0;
    if (passwordCallback != nullptr) {
        length = passwordCallback(buffer, LibreSsl3PemBufferSize, writeFlag, userData);
    } else {
        if (userData == nullptr) NotImplemented_nid_no_patch("PEM_def_callback: pass phrase prompt on the terminal");
        const char* const phrase = static_cast<const char*>(userData);
        while (length < LibreSsl3PemBufferSize) {
            char character = 0;
            const int savedErrno = errno;
            const bool readable = GuestCopyIn_nid_no_patch(&character, phrase + length, 1);
            errno = savedErrno;
            if (!readable) throw std::invalid_argument("PEM_def_callback: unreadable pass phrase");
            if (character == '\0') break;
            buffer[length] = character;
            ++length;
        }
    }
    if (length > LibreSsl3PemBufferSize)
        throw std::invalid_argument("PEM password callback returned more than PEM_BUFSIZE bytes");
    return length;
}

bool PemDoHeader_nid_no_patch(const LibreSsl3PemCipherInfo& info, std::vector<unsigned char>& data, int& length,
    LibreSslPemPasswordCallback passwordCallback, void* userData) {
    const CipherChoice cipher{info.cipher, info.des};
    if (!IsSet(cipher)) return true;
    if (length < 0 || static_cast<std::size_t>(length) > data.size())
        throw std::invalid_argument("PEM_do_header: length outside the record");
    std::array<char, LibreSsl3PemBufferSize> password{};
    const ClearPasswordOnExit clearPassword{password};
    const int passwordLength = PemReadPassword_nid_no_patch(password.data(), 0, passwordCallback, userData);
    if (passwordLength <= 0) {
        PutError(PemLibrary, PemBadPasswordRead);
        return false;
    }
    std::array<unsigned char, MaxKeyLength> key{};
    const ClearArrayOnExit<MaxKeyLength> clearKey{key};
    const auto* const phrase = reinterpret_cast<const unsigned char*>(password.data());
    if (!BytesToKey(KeyLength(cipher), IvLength(cipher), EVP_md5_nid_postfix(), info.iv, phrase, passwordLength, 1,
            key.data(), nullptr))
        return false;
    if (cipher.des != LibreSsl3PemDes::None) {
        std::vector<unsigned char> decrypted;
        const ClearVectorOnExit clearDecrypted{decrypted};
        if (!DesCbcDecrypt(cipher.des, key.data(), info.iv, data.data(), static_cast<std::size_t>(length), decrypted)) {
            PutError(PemLibrary, PemBadDecrypt);
            return false;
        }
        ClearBytes(data.data(), data.size());
        if (!decrypted.empty()) std::memcpy(data.data(), decrypted.data(), decrypted.size());
        length = static_cast<int>(decrypted.size());
        return true;
    }
    const int blockSize = info.cipher->blockSize;
    if (blockSize < 0 || length > std::numeric_limits<int>::max() - blockSize)
        throw std::invalid_argument("PEM_do_header: record longer than INT_MAX");
    std::vector<unsigned char> plain(static_cast<std::size_t>(length) + static_cast<std::size_t>(blockSize) + 1, 0);
    const ClearVectorOnExit clearPlain{plain};
    const CipherContext context = NewCipherContext();
    int updated = 0;
    int finished = 0;
    int ok = EVP_DecryptInit_ex_nid_postfix(context.get(), info.cipher, nullptr, key.data(), info.iv);
    if (ok)
        ok = EVP_DecryptUpdate_nid_disambig1_nid_postfix(context.get(), plain.data(), &updated, data.data(), length);
    if (ok) ok = EVP_DecryptFinal_ex_nid_postfix(context.get(), plain.data() + updated, &finished);
    if (!ok) {
        PutError(PemLibrary, PemBadDecrypt);
        return false;
    }
    const std::size_t total = static_cast<std::size_t>(updated) + static_cast<std::size_t>(finished);
    if (total > plain.size()) throw std::length_error("PEM_do_header: cipher wrote past its buffer");
    ClearBytes(data.data(), data.size());
    if (total > data.size()) data.resize(total);
    if (total != 0) std::memcpy(data.data(), plain.data(), total);
    length = static_cast<int>(total);
    return true;
}

LibreSsl3Pkcs8PrivKeyInfo* PemDecryptPkcs8_nid_no_patch(const unsigned char* der, int length,
    LibreSslPemPasswordCallback passwordCallback, void* userData, bool& asn1Failure) {
    asn1Failure = true;
    EncryptedKey key;
    if (der == nullptr || length < 0 || !ReadEncryptedKey(der, static_cast<std::size_t>(length), key)) return nullptr;
    std::array<char, LibreSsl3PemBufferSize> password{};
    const ClearPasswordOnExit clearPassword{password};
    const int passwordLength = PemReadPassword_nid_no_patch(password.data(), 0, passwordCallback, userData);
    if (passwordLength <= 0) {
        PutError(PemLibrary, PemBadPasswordRead);
        asn1Failure = false;
        return nullptr;
    }
    return DecryptToPkcs8(key, password.data(), passwordLength);
}

bool PemEncryptPkcs8_nid_no_patch(const LibreSsl3EvpCipher* cipher, const unsigned char* plain, int plainLength,
    const char* password, int passwordLength, std::vector<unsigned char>& der) {
    if (cipher == nullptr) throw std::invalid_argument("PKCS8_encrypt: null cipher");
    const std::string_view cipherOid = OidOfCipherObject(cipher);
    const int ivLength = cipher->ivLength;
    std::array<unsigned char, MaxIvLength> iv{};
    if (ivLength < 0 || static_cast<std::size_t>(ivLength) > iv.size())
        throw std::invalid_argument("PKCS5_pbe2_set: IV length outside 0 to 16");
    if (ivLength > 0) RAND_bytes_nid_postfix(iv.data(), ivLength);
    std::array<unsigned char, Pkcs5SaltLength> salt{};
    RAND_bytes_nid_postfix(salt.data(), static_cast<int>(salt.size()));
    const std::vector<unsigned char> algorithmDer = Pbes2Algorithm(
        cipherOid, iv.data(), static_cast<std::size_t>(ivLength), salt.data(), salt.size(), Pkcs5DefaultIterations);
    Reader reader{algorithmDer.data(), algorithmDer.size()};
    Algorithm algorithm;
    if (!ReadAlgorithm(reader, algorithm))
        throw std::logic_error("PKCS5_pbe2_set: generated algorithm does not decode");
    if (plain == nullptr || plainLength <= 0) {
        PutError(Pkcs12Library, Pkcs12EncodeError);
        PutError(Pkcs12Library, Pkcs12EncryptError);
        return false;
    }
    std::vector<unsigned char> encrypted;
    if (!PbeCrypt(algorithm, password, passwordLength, plain, plainLength, encrypted, 1)) {
        PutError(Pkcs12Library, Pkcs12EncryptError);
        PutError(Pkcs12Library, Pkcs12EncryptError);
        return false;
    }
    std::vector<unsigned char> body = algorithmDer;
    AppendElement(body, TagOctetString, encrypted);
    der.clear();
    AppendElement(der, TagSequence, body);
    return true;
}
