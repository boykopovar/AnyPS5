/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/ocsp/ocsp_asn.c (ocsp_asn.c,v 1.9),
 * ocsp/ocsp_cl.c (ocsp_cl.c,v 1.14), asn1/asn_pack.c (asn_pack.c,v 1.16), asn1/tasn_dec.c (tasn_dec.c,v 1.34),
 * asn1/tasn_new.c (tasn_new.c,v 1.17), asn1/tasn_fre.c (tasn_fre.c,v 1.15), asn1/tasn_typ.c (tasn_typ.c,v 1.13),
 * asn1/a_bitstr.c (a_bitstr.c,v 1.24), asn1/a_type.c (a_type.c,v 1.19), asn1/x_algor.c (x_algor.c,v 1.21) and
 * asn1/x_exten.c (x_exten.c,v 1.16) */
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
/* $OpenBSD: ocsp_cl.c,v 1.14 2017/01/29 17:49:23 beck Exp $ */
/* Written by Tom Titchener <Tom_Titchener@groove.net> for the OpenSSL
 * project. */

/* History:
   This file was transfered to Richard Levitte from CertCo by Kathy
   Weinhold in mid-spring 2000 to be included in OpenSSL or released
   as a patch kit. */

/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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
/* $OpenBSD: asn_pack.c,v 1.16 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: tasn_fre.c,v 1.15 2016/12/30 16:04:34 jsing Exp $ */
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
/* $OpenBSD: tasn_typ.c,v 1.13 2015/07/24 15:09:52 jsing Exp $ */
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
/* $OpenBSD: a_bitstr.c,v 1.24 2017/01/29 17:49:22 beck Exp $ */
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
/* $OpenBSD: a_type.c,v 1.19 2016/05/04 15:00:24 tedu Exp $ */
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
/* $OpenBSD: x_algor.c,v 1.21 2015/07/24 15:09:52 jsing Exp $ */
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
/* $OpenBSD: x_exten.c,v 1.16 2015/07/24 15:09:52 jsing Exp $ */
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
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Asn1Der.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/OcspResponse.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using namespace LibreSslDer;

constexpr int NestedAsn1Error = 58;
constexpr int MallocFailure = 65;
constexpr int BooleanIsWrongLength = 106;
constexpr int DecodeError = 110;
constexpr int IllegalNull = 125;
constexpr int IllegalOptionalAny = 126;
constexpr int IllegalTaggedAny = 127;
constexpr int MissingEoc = 137;
constexpr int NoMatchingChoiceType = 143;
constexpr int NullIsWrongLength = 144;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int StringTooShort = 152;
constexpr int TypeNotConstructed = 156;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int BmpStringIsWrongLength = 214;
constexpr int UniversalStringIsWrongLength = 215;
constexpr int ExplicitLengthMismatch = 119;
constexpr int ExplicitTagNotConstructed = 120;
constexpr int FieldMissing = 121;
constexpr int BadObjectHeader = 102;
constexpr int OcspLibrary = 39;
constexpr int NotBasicResponse = 104;
constexpr int NoResponseData = 108;
constexpr int BasicResponseNid = 365;

constexpr int Any = -4;
constexpr int Other = -3;
constexpr int Undefined = -1;
constexpr int BooleanType = 1;
constexpr int IntegerType = 2;
constexpr int BitStringType = 3;
constexpr int OctetStringType = 4;
constexpr int NullType = 5;
constexpr int ObjectType = 6;
constexpr int EnumeratedType = 10;
constexpr int SequenceType = 16;
constexpr int SetType = 17;
constexpr int UniversalStringType = 28;
constexpr int BmpStringType = 30;
constexpr int GeneralizedTimeType = 24;
constexpr int BitsLeftFlag = 0x08;

constexpr unsigned Optional = 0x01;
constexpr unsigned SequenceOf = 0x04;
constexpr unsigned ImplicitTag = 0x08;
constexpr unsigned ExplicitTag = 0x10;
constexpr unsigned TagClass = 0xc0;
constexpr unsigned Context = 0x80;

enum class Kind { Primitive, Sequence, Choice, Extern };

struct Item;

struct Template {
    unsigned flags;
    int tag;
    std::size_t offset;
    const Item* item;
};

struct Item {
    Kind kind;
    int utype;
    const Template* templates;
    int count;
    std::size_t size;
    const char* name;
};

struct Asn1Type {
    int type;
    void* value;
};

struct CertId {
    void* hashAlgorithm;
    Asn1String* issuerNameHash;
    Asn1String* issuerKeyHash;
    Asn1String* serialNumber;
};

struct Algorithm {
    LibreSslAsn1Object* algorithm;
    Asn1Type* parameter;
};

struct Extension {
    LibreSslAsn1Object* object;
    int critical;
    Asn1String* value;
};

static_assert(sizeof(Asn1Type) == 16 && offsetof(Asn1Type, value) == 8);
static_assert(sizeof(CertId) == 32 && offsetof(CertId, serialNumber) == 24);
static_assert(sizeof(Algorithm) == 16 && offsetof(Algorithm, parameter) == 8);
static_assert(sizeof(Extension) == 24 && offsetof(Extension, critical) == 8 && offsetof(Extension, value) == 16);
static_assert(sizeof(LibreSslOcspRevokedInfo) == 16 && sizeof(LibreSslOcspCertStatus) == 16);
static_assert(sizeof(LibreSslOcspSingleResponse) == 40 && sizeof(LibreSslOcspResponseData) == 40);
static_assert(sizeof(LibreSslOcspResponderId) == 16 && sizeof(LibreSslOcspBasicResponse) == 32);

extern const Item ObjectItem;
extern const Item OctetStringItem;
extern const Item IntegerItem;
extern const Item EnumeratedItem;
extern const Item BitStringItem;
extern const Item GeneralizedTimeItem;
extern const Item NullItem;
extern const Item BooleanItem;
extern const Item AnyItem;
extern const Item NameItem;
extern const Item CertificateItem;
extern const Item AlgorithmItem;
extern const Item ExtensionItem;
extern const Item CertIdItem;
extern const Item RevokedInfoItem;
extern const Item CertStatusItem;
extern const Item SingleResponseItem;
extern const Item ResponderIdItem;
extern const Item ResponseDataItem;
extern const Item BasicResponseItem;

constexpr Template AlgorithmTemplates[] {
    {0, 0, offsetof(Algorithm, algorithm), &ObjectItem},
    {Optional, 0, offsetof(Algorithm, parameter), &AnyItem},
};

constexpr Template ExtensionTemplates[] {
    {0, 0, offsetof(Extension, object), &ObjectItem},
    {Optional, 0, offsetof(Extension, critical), &BooleanItem},
    {0, 0, offsetof(Extension, value), &OctetStringItem},
};

constexpr Template CertIdTemplates[] {
    {0, 0, offsetof(CertId, hashAlgorithm), &AlgorithmItem},
    {0, 0, offsetof(CertId, issuerNameHash), &OctetStringItem},
    {0, 0, offsetof(CertId, issuerKeyHash), &OctetStringItem},
    {0, 0, offsetof(CertId, serialNumber), &IntegerItem},
};

constexpr Template RevokedInfoTemplates[] {
    {0, 0, offsetof(LibreSslOcspRevokedInfo, revocationTime), &GeneralizedTimeItem},
    {ExplicitTag | Context | Optional, 0, offsetof(LibreSslOcspRevokedInfo, revocationReason), &EnumeratedItem},
};

constexpr Template CertStatusTemplates[] {
    {ImplicitTag | Context, 0, offsetof(LibreSslOcspCertStatus, value), &NullItem},
    {ImplicitTag | Context, 1, offsetof(LibreSslOcspCertStatus, value), &RevokedInfoItem},
    {ImplicitTag | Context, 2, offsetof(LibreSslOcspCertStatus, value), &NullItem},
};

constexpr Template SingleResponseTemplates[] {
    {0, 0, offsetof(LibreSslOcspSingleResponse, certId), &CertIdItem},
    {0, 0, offsetof(LibreSslOcspSingleResponse, certStatus), &CertStatusItem},
    {0, 0, offsetof(LibreSslOcspSingleResponse, thisUpdate), &GeneralizedTimeItem},
    {ExplicitTag | Context | Optional, 0, offsetof(LibreSslOcspSingleResponse, nextUpdate), &GeneralizedTimeItem},
    {ExplicitTag | Context | SequenceOf | Optional, 1, offsetof(LibreSslOcspSingleResponse, singleExtensions), &ExtensionItem},
};

constexpr Template ResponderIdTemplates[] {
    {ExplicitTag | Context, 1, offsetof(LibreSslOcspResponderId, value), &NameItem},
    {ExplicitTag | Context, 2, offsetof(LibreSslOcspResponderId, value), &OctetStringItem},
};

constexpr Template ResponseDataTemplates[] {
    {ExplicitTag | Context | Optional, 0, offsetof(LibreSslOcspResponseData, version), &IntegerItem},
    {0, 0, offsetof(LibreSslOcspResponseData, responderId), &ResponderIdItem},
    {0, 0, offsetof(LibreSslOcspResponseData, producedAt), &GeneralizedTimeItem},
    {SequenceOf, 0, offsetof(LibreSslOcspResponseData, responses), &SingleResponseItem},
    {ExplicitTag | Context | SequenceOf | Optional, 1, offsetof(LibreSslOcspResponseData, responseExtensions), &ExtensionItem},
};

constexpr Template BasicResponseTemplates[] {
    {0, 0, offsetof(LibreSslOcspBasicResponse, tbsResponseData), &ResponseDataItem},
    {0, 0, offsetof(LibreSslOcspBasicResponse, signatureAlgorithm), &AlgorithmItem},
    {0, 0, offsetof(LibreSslOcspBasicResponse, signature), &BitStringItem},
    {ExplicitTag | Context | SequenceOf | Optional, 0, offsetof(LibreSslOcspBasicResponse, certs), &CertificateItem},
};

template<std::size_t TCount> constexpr int Count(const Template (&)[TCount]) {
    return static_cast<int>(TCount);
}

constexpr Item ObjectItem {Kind::Primitive, ObjectType, nullptr, 0, 0, "ASN1_OBJECT"};
constexpr Item OctetStringItem {Kind::Primitive, OctetStringType, nullptr, 0, 0, "ASN1_OCTET_STRING"};
constexpr Item IntegerItem {Kind::Primitive, IntegerType, nullptr, 0, 0, "ASN1_INTEGER"};
constexpr Item EnumeratedItem {Kind::Primitive, EnumeratedType, nullptr, 0, 0, "ASN1_ENUMERATED"};
constexpr Item BitStringItem {Kind::Primitive, BitStringType, nullptr, 0, 0, "ASN1_BIT_STRING"};
constexpr Item GeneralizedTimeItem {Kind::Primitive, GeneralizedTimeType, nullptr, 0, 0, "ASN1_GENERALIZEDTIME"};
constexpr Item NullItem {Kind::Primitive, NullType, nullptr, 0, 0, "ASN1_NULL"};
constexpr Item BooleanItem {Kind::Primitive, BooleanType, nullptr, 0, static_cast<std::size_t>(-1), "ASN1_BOOLEAN"};
constexpr Item AnyItem {Kind::Primitive, Any, nullptr, 0, 0, "ASN1_ANY"};
constexpr Item NameItem {Kind::Extern, 0, nullptr, 0, 0, "X509_NAME"};
constexpr Item CertificateItem {Kind::Extern, 0, nullptr, 0, 0, "X509"};
constexpr Item AlgorithmItem {Kind::Sequence, SequenceType, AlgorithmTemplates, Count(AlgorithmTemplates), sizeof(Algorithm), "X509_ALGOR"};
constexpr Item ExtensionItem {Kind::Sequence, SequenceType, ExtensionTemplates, Count(ExtensionTemplates), sizeof(Extension), "X509_EXTENSION"};
constexpr Item CertIdItem {Kind::Sequence, SequenceType, CertIdTemplates, Count(CertIdTemplates), sizeof(CertId), "OCSP_CERTID"};
constexpr Item RevokedInfoItem {Kind::Sequence, SequenceType, RevokedInfoTemplates, Count(RevokedInfoTemplates), sizeof(LibreSslOcspRevokedInfo), "OCSP_REVOKEDINFO"};
constexpr Item CertStatusItem {Kind::Choice, 0, CertStatusTemplates, Count(CertStatusTemplates), sizeof(LibreSslOcspCertStatus), "OCSP_CERTSTATUS"};
constexpr Item SingleResponseItem {Kind::Sequence, SequenceType, SingleResponseTemplates, Count(SingleResponseTemplates), sizeof(LibreSslOcspSingleResponse), "OCSP_SINGLERESP"};
constexpr Item ResponderIdItem {Kind::Choice, 0, ResponderIdTemplates, Count(ResponderIdTemplates), sizeof(LibreSslOcspResponderId), "OCSP_RESPID"};
constexpr Item ResponseDataItem {Kind::Sequence, SequenceType, ResponseDataTemplates, Count(ResponseDataTemplates), sizeof(LibreSslOcspResponseData), "OCSP_RESPDATA"};
constexpr Item BasicResponseItem {Kind::Sequence, SequenceType, BasicResponseTemplates, Count(BasicResponseTemplates), sizeof(LibreSslOcspBasicResponse), "OCSP_BASICRESP"};

struct Tagged {
    std::int64_t length;
    int tag;
    int objectClass;
    bool indefinite;
    bool constructed;
};

int Error(int reason) {
    Fail(reason);
    return 0;
}

int CheckTagged(Tagged& header, const unsigned char*& in, std::int64_t available, int expectedTag, int expectedClass, bool optional) {
    const unsigned char* p = in;
    std::int64_t length = 0;
    int tag = 0;
    int objectClass = 0;
    const int result = ReadIdentifier(p, length, tag, objectClass, available);
    if (result & 0x80) return Error(BadObjectHeader);
    if (expectedTag >= 0 && (expectedTag != tag || expectedClass != objectClass)) {
        if (optional) return -1;
        return Error(WrongTag);
    }
    if (result & 1) length = available - (p - in);
    header = {length, tag, objectClass, (result & 1) != 0, (result & 0x20) != 0};
    in = p;
    return 1;
}

void** Field(void** value, const Template& tt) {
    return reinterpret_cast<void**>(static_cast<unsigned char*>(*value) + tt.offset);
}

int& Selector(void** value) {
    return *static_cast<int*>(*value);
}

int StackCount(const LibreSslStack* stack) {
    return stack ? stack->num : -1;
}

void FreeStack(LibreSslStack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

void ItemFree(void** value, const Item& item);

void TemplateFree(void** value, const Template& tt) {
    if (tt.flags & SequenceOf) {
        auto* stack = static_cast<LibreSslStack*>(*value);
        for (int index = 0; index < StackCount(stack); ++index) {
            void* element = stack->data[index];
            ItemFree(&element, *tt.item);
        }
        FreeStack(stack);
        *value = nullptr;
        return;
    }
    ItemFree(value, *tt.item);
}

void PrimitiveFree(void** value, const Item& item) {
    if (item.utype != BooleanType && !*value) return;
    switch (item.utype) {
    case ObjectType:
        LibreSslAsn1ObjectFree_nid_no_patch(static_cast<LibreSslAsn1Object*>(*value));
        break;
    case BooleanType:
        *reinterpret_cast<int*>(value) = static_cast<int>(item.size);
        return;
    case NullType:
        break;
    case Any:
        LibreSslAsn1TypeFree_nid_no_patch(*value);
        break;
    default:
        LibreSslAsn1StringFree_nid_no_patch(static_cast<Asn1String*>(*value));
        break;
    }
    *value = nullptr;
}

void ItemFree(void** value, const Item& item) {
    if (!value || (item.kind != Kind::Primitive && !*value)) return;
    switch (item.kind) {
    case Kind::Primitive:
        PrimitiveFree(value, item);
        break;
    case Kind::Extern:
        if (&item == &NameItem) LibreSslX509NameFree_nid_no_patch(*value);
        else NotImplemented_nid_no_patch("OCSP_BASICRESP_free: X509_free of an embedded certificate");
        *value = nullptr;
        break;
    case Kind::Choice: {
        const int index = Selector(value);
        if (index >= 0 && index < item.count) TemplateFree(Field(value, item.templates[index]), item.templates[index]);
        free_nid_postfix(*value);
        *value = nullptr;
        break;
    }
    case Kind::Sequence:
        for (int index = item.count - 1; index >= 0; --index) TemplateFree(Field(value, item.templates[index]), item.templates[index]);
        free_nid_postfix(*value);
        *value = nullptr;
        break;
    }
}

bool ItemNew(void** value, const Item& item);

void TemplateClear(void** value, const Template& tt) {
    if (!(tt.flags & SequenceOf) && tt.item->kind == Kind::Primitive && tt.item->utype == BooleanType)
        *reinterpret_cast<int*>(value) = static_cast<int>(tt.item->size);
    else
        *value = nullptr;
}

bool TemplateNew(void** value, const Template& tt) {
    if (tt.flags & Optional) {
        TemplateClear(value, tt);
        return true;
    }
    if (tt.flags & SequenceOf) {
        *value = LibreSslStackNew_nid_no_patch();
        return *value || Fail(MallocFailure);
    }
    return ItemNew(value, *tt.item);
}

bool PrimitiveNew(void** value, const Item& item) {
    switch (item.utype) {
    case ObjectType:
        *value = nullptr;
        return true;
    case BooleanType:
        *reinterpret_cast<int*>(value) = static_cast<int>(item.size);
        return true;
    case NullType:
        *value = reinterpret_cast<void*>(std::uintptr_t {1});
        return true;
    case Any: {
        auto* type = static_cast<Asn1Type*>(malloc_nid_postfix(sizeof(Asn1Type)));
        if (type) *type = {Undefined, nullptr};
        *value = type;
        break;
    }
    default:
        *value = NewString(item.utype);
        break;
    }
    return *value != nullptr;
}

bool ItemNew(void** value, const Item& item) {
    *value = nullptr;
    switch (item.kind) {
    case Kind::Primitive:
        if (!PrimitiveNew(value, item)) return Fail(MallocFailure);
        return true;
    case Kind::Extern:
        NotImplemented_nid_no_patch("OCSP_response_get1_basic: new X509 or X509_NAME");
    case Kind::Choice:
        if (!(*value = calloc_nid_postfix(1, item.size))) return Fail(MallocFailure);
        Selector(value) = -1;
        return true;
    case Kind::Sequence:
        if (!(*value = calloc_nid_postfix(1, item.size))) return Fail(MallocFailure);
        for (int index = 0; index < item.count; ++index)
            if (!TemplateNew(Field(value, item.templates[index]), item.templates[index])) return Fail(MallocFailure);
        return true;
    }
    return false;
}

bool FindEnd(const unsigned char*& in, std::int64_t length) {
    int expected = 1;
    const unsigned char* p = in;
    while (length > 0) {
        if (CheckEoc(p, length)) {
            if (--expected == 0) break;
            length -= 2;
            continue;
        }
        const unsigned char* const start = p;
        Tagged header {};
        if (!CheckTagged(header, p, length, -1, 0, false)) return Fail(NestedAsn1Error);
        if (header.indefinite) ++expected;
        else p += header.length;
        length -= p - start;
    }
    if (expected) return Fail(MissingEoc);
    in = p;
    return true;
}

bool DecodeBitString(Asn1String*& slot, const unsigned char* content, std::int64_t length) {
    if (length < 1) return Fail(StringTooShort);
    Asn1String* target = slot;
    if (!target) {
        if (!(target = NewString(BitStringType))) return Fail(MallocFailure);
    }
    const int unused = content[0];
    if (length > 1 && unused >= 32) NotImplemented_nid_no_patch("OCSP_response_get1_basic: BIT STRING with 32 or more unused bits");
    target->flags &= ~std::int64_t {BitsLeftFlag | 0x07};
    target->flags |= BitsLeftFlag | (unused & 0x07);
    unsigned char* data = nullptr;
    if (length-- > 1) {
        data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length)));
        if (!data) {
            Fail(MallocFailure);
            if (target != slot) LibreSslAsn1StringFree_nid_no_patch(target);
            return false;
        }
        std::memcpy(data, content + 1, static_cast<std::size_t>(length));
        data[length - 1] &= static_cast<unsigned char>(0xff << unused);
    }
    target->length = static_cast<int>(length);
    free_nid_postfix(target->data);
    target->data = data;
    target->type = BitStringType;
    slot = target;
    return true;
}

void TypeSet(Asn1Type* type, int utype) {
    if (type->value) {
        if (type->type == ObjectType) LibreSslAsn1ObjectFree_nid_no_patch(static_cast<LibreSslAsn1Object*>(type->value));
        else if (type->type != BooleanType && type->type != NullType) LibreSslAsn1StringFree_nid_no_patch(static_cast<Asn1String*>(type->value));
    }
    type->type = utype;
    type->value = nullptr;
}

bool ContentToValue(void** value, const unsigned char* content, std::int64_t length, int utype, unsigned char*& collected, const Item& item) {
    Asn1Type* type = nullptr;
    void** outer = nullptr;
    if (item.utype == Any) {
        if (!*value) {
            type = static_cast<Asn1Type*>(malloc_nid_postfix(sizeof(Asn1Type)));
            if (!type) {
                Fail(MallocFailure);
                free_nid_postfix(collected);
                collected = nullptr;
                return false;
            }
            *type = {Undefined, nullptr};
            *value = type;
        } else {
            type = static_cast<Asn1Type*>(*value);
        }
        if (utype != type->type) TypeSet(type, utype);
        outer = value;
        value = &type->value;
    }
    bool decoded = false;
    switch (utype) {
    case ObjectType:
        decoded = DecodeObject(*reinterpret_cast<LibreSslAsn1Object**>(value), content, length);
        break;
    case NullType:
        if (length) {
            Fail(NullIsWrongLength);
        } else {
            *value = reinterpret_cast<void*>(std::uintptr_t {1});
            decoded = true;
        }
        break;
    case BooleanType:
        if (length != 1) {
            Fail(BooleanIsWrongLength);
        } else {
            *reinterpret_cast<int*>(value) = content[0];
            decoded = true;
        }
        break;
    case BitStringType:
        decoded = DecodeBitString(*reinterpret_cast<Asn1String**>(value), content, length);
        break;
    case IntegerType:
    case EnumeratedType:
        decoded = DecodeInteger(*reinterpret_cast<Asn1String**>(value), content, length, utype);
        break;
    default:
        if (utype == BmpStringType && (length & 1)) {
            Fail(BmpStringIsWrongLength);
        } else if (utype == UniversalStringType && (length & 3)) {
            Fail(UniversalStringIsWrongLength);
        } else {
            decoded = DecodeOctetString(*reinterpret_cast<Asn1String**>(value), content, length, collected, utype);
            collected = nullptr;
        }
        break;
    }
    if (type && utype == NullType) type->value = nullptr;
    if (decoded) return true;
    if (type) LibreSslAsn1TypeFree_nid_no_patch(type);
    if (outer) *outer = nullptr;
    return false;
}

bool UsesCollectedBuffer(int utype) {
    switch (utype) {
    case ObjectType:
    case NullType:
    case BooleanType:
    case BitStringType:
    case IntegerType:
    case EnumeratedType:
        return false;
    default:
        return true;
    }
}

int PrimitiveDecode(void** value, const unsigned char*& in, std::int64_t available, const Item& item, int tag, int tagClass, bool optional) {
    if (!value) return Error(IllegalNull);
    int utype = item.utype;
    const unsigned char* p = in;
    if (utype == Any) {
        if (tag >= 0) return Error(IllegalTaggedAny);
        if (optional) return Error(IllegalOptionalAny);
        Tagged header {};
        if (!CheckTagged(header, p, available, -1, 0, false)) return Error(NestedAsn1Error);
        utype = header.objectClass ? Other : header.tag;
        p = in;
    }
    if (tag == -1) {
        tag = utype;
        tagClass = 0;
    }
    Tagged header {};
    const int result = CheckTagged(header, p, available, tag, tagClass, optional);
    if (!result) return Error(NestedAsn1Error);
    if (result == -1) return -1;
    std::vector<unsigned char> buffer;
    unsigned char* collected = nullptr;
    const unsigned char* content = p;
    std::int64_t length = header.length;
    if (utype == SequenceType || utype == SetType || utype == Other) {
        if (utype != Other && !header.constructed) return Error(TypeNotConstructed);
        content = in;
        if (header.indefinite) {
            if (!FindEnd(p, header.length)) return 0;
            length = p - content;
        } else {
            length = p - content + header.length;
            p += header.length;
        }
    } else if (header.constructed) {
        if (!Collect(buffer, p, header.length, header.indefinite, 0)) return 0;
        length = static_cast<std::int64_t>(buffer.size());
        content = buffer.data();
        if (UsesCollectedBuffer(utype)) {
            collected = static_cast<unsigned char*>(malloc_nid_postfix(buffer.size() + 1));
            if (!collected) return Error(MallocFailure);
            if (!buffer.empty()) std::memcpy(collected, buffer.data(), buffer.size());
            collected[buffer.size()] = 0;
            content = collected;
        }
    } else {
        p += header.length;
    }
    if (length > INT_MAX) throw std::length_error("OCSP_response_get1_basic: content longer than INT_MAX bytes");
    const bool decoded = ContentToValue(value, content, length, utype, collected, item);
    free_nid_postfix(collected);
    if (!decoded) return 0;
    in = p;
    return 1;
}

int ItemDecode(void** value, const unsigned char*& in, std::int64_t length, const Item& item, int tag, int tagClass, bool optional);

int TemplateNoExplicit(void** value, const unsigned char*& in, std::int64_t length, const Template& tt, bool optional) {
    const int tagClass = static_cast<int>(tt.flags & TagClass);
    const unsigned char* p = in;
    if (tt.flags & SequenceOf) {
        const int stackTag = (tt.flags & ImplicitTag) ? tt.tag : SequenceType;
        const int stackClass = (tt.flags & ImplicitTag) ? tagClass : 0;
        Tagged header {};
        const int result = CheckTagged(header, p, length, stackTag, stackClass, optional);
        if (!result) return Error(NestedAsn1Error);
        if (result == -1) return -1;
        length = header.length;
        bool stackEoc = header.indefinite;
        auto* stack = static_cast<LibreSslStack*>(*value);
        if (!stack) {
            *value = stack = LibreSslStackNew_nid_no_patch();
        } else {
            while (stack->num > 0) {
                void* element = stack->data[--stack->num];
                ItemFree(&element, *tt.item);
            }
        }
        if (!stack) {
            Fail(MallocFailure);
            TemplateFree(value, tt);
            return 0;
        }
        while (length > 0) {
            const unsigned char* const start = p;
            if (CheckEoc(p, length)) {
                if (!stackEoc) {
                    Fail(UnexpectedEoc);
                    TemplateFree(value, tt);
                    return 0;
                }
                length -= p - start;
                stackEoc = false;
                break;
            }
            void* element = nullptr;
            if (!ItemDecode(&element, p, length, *tt.item, -1, 0, false)) {
                Fail(NestedAsn1Error);
                TemplateFree(value, tt);
                return 0;
            }
            length -= p - start;
            if (!LibreSslStackPush_nid_no_patch(stack, element)) {
                Fail(MallocFailure);
                ItemFree(&element, *tt.item);
                TemplateFree(value, tt);
                return 0;
            }
        }
        if (stackEoc) {
            Fail(MissingEoc);
            TemplateFree(value, tt);
            return 0;
        }
    } else {
        const bool implicit = (tt.flags & ImplicitTag) != 0;
        const int result = ItemDecode(value, p, length, *tt.item, implicit ? tt.tag : -1, implicit ? tagClass : 0, optional);
        if (!result) {
            Fail(NestedAsn1Error);
            TemplateFree(value, tt);
            return 0;
        }
        if (result == -1) return -1;
    }
    in = p;
    return 1;
}

int TemplateDecode(void** value, const unsigned char*& in, std::int64_t available, const Template& tt, bool optional) {
    if (!(tt.flags & ExplicitTag)) return TemplateNoExplicit(value, in, available, tt, optional);
    const unsigned char* p = in;
    Tagged header {};
    const int result = CheckTagged(header, p, available, tt.tag, static_cast<int>(tt.flags & TagClass), optional);
    const unsigned char* const start = p;
    if (!result) return Error(NestedAsn1Error);
    if (result == -1) return -1;
    if (!header.constructed) return Error(ExplicitTagNotConstructed);
    if (!TemplateNoExplicit(value, p, header.length, tt, false)) return Error(NestedAsn1Error);
    const std::int64_t length = header.length - (p - start);
    if (header.indefinite) {
        if (!CheckEoc(p, length)) {
            Fail(MissingEoc);
            TemplateFree(value, tt);
            return 0;
        }
    } else if (length) {
        Fail(ExplicitLengthMismatch);
        TemplateFree(value, tt);
        return 0;
    }
    in = p;
    return 1;
}

int ChoiceDecode(void** value, const unsigned char*& in, std::int64_t length, const Item& item, bool optional) {
    if (*value) {
        const int index = Selector(value);
        if (index >= 0 && index < item.count) {
            TemplateFree(Field(value, item.templates[index]), item.templates[index]);
            Selector(value) = -1;
        }
    } else if (!ItemNew(value, item)) {
        Fail(NestedAsn1Error);
        ItemFree(value, item);
        return 0;
    }
    const unsigned char* p = in;
    int index = 0;
    for (; index < item.count; ++index) {
        const Template& tt = item.templates[index];
        const int result = TemplateDecode(Field(value, tt), p, length, tt, true);
        if (result == -1) continue;
        if (result > 0) break;
        Fail(NestedAsn1Error);
        ItemFree(value, item);
        return 0;
    }
    if (index == item.count) {
        if (optional) {
            ItemFree(value, item);
            return -1;
        }
        Fail(NoMatchingChoiceType);
        ItemFree(value, item);
        return 0;
    }
    Selector(value) = index;
    in = p;
    return 1;
}

int SequenceDecode(void** value, const unsigned char*& in, std::int64_t length, const Item& item, int tag, int tagClass, bool optional) {
    const unsigned char* p = in;
    if (tag == -1) {
        tag = SequenceType;
        tagClass = 0;
    }
    Tagged header {};
    const int result = CheckTagged(header, p, length, tag, tagClass, optional);
    if (!result) {
        Fail(NestedAsn1Error);
        ItemFree(value, item);
        return 0;
    }
    if (result == -1) return -1;
    bool eoc = header.indefinite;
    const bool noLength = eoc;
    length = header.length;
    if (!header.constructed) {
        Fail(SequenceNotConstructed);
        ItemFree(value, item);
        return 0;
    }
    if (!*value && !ItemNew(value, item)) {
        Fail(NestedAsn1Error);
        ItemFree(value, item);
        return 0;
    }
    int index = 0;
    for (; index < item.count; ++index) {
        const Template& tt = item.templates[index];
        void** field = Field(value, tt);
        if (!length) break;
        const unsigned char* const start = p;
        if (CheckEoc(p, length)) {
            if (!eoc) {
                Fail(UnexpectedEoc);
                ItemFree(value, item);
                return 0;
            }
            length -= p - start;
            eoc = false;
            break;
        }
        const bool fieldOptional = index != item.count - 1 && (tt.flags & Optional);
        const int decoded = TemplateDecode(field, p, length, tt, fieldOptional);
        if (!decoded) {
            ItemFree(value, item);
            return 0;
        }
        if (decoded == -1) {
            TemplateFree(field, tt);
            continue;
        }
        length -= p - start;
    }
    if ((eoc && !CheckEoc(p, length)) || (!noLength && length)) {
        Fail(eoc ? MissingEoc : SequenceLengthMismatch);
        ItemFree(value, item);
        return 0;
    }
    for (; index < item.count; ++index) {
        const Template& tt = item.templates[index];
        if (!(tt.flags & Optional)) {
            Fail(FieldMissing);
            ItemFree(value, item);
            return 0;
        }
        TemplateFree(Field(value, tt), tt);
    }
    in = p;
    return 1;
}

int ItemDecode(void** value, const unsigned char*& in, std::int64_t length, const Item& item, int tag, int tagClass, bool optional) {
    if (!value) return 0;
    switch (item.kind) {
    case Kind::Primitive:
        return PrimitiveDecode(value, in, length, item, tag, tagClass, optional);
    case Kind::Extern:
        NotImplemented_nid_no_patch(&item == &NameItem ? "OCSP_response_get1_basic: X509_NAME responder ID"
            : "OCSP_response_get1_basic: embedded X509 certificate");
    case Kind::Choice:
        return ChoiceDecode(value, in, length, item, optional);
    case Kind::Sequence:
        return SequenceDecode(value, in, length, item, tag, tagClass, optional);
    }
    return 0;
}

}

extern "C" {

LibreSslOcspBasicResponse* APS5_VABI OCSP_response_get1_basic_nid_postfix(LibreSslOcspResponse* response) {
    if (!response) throw std::invalid_argument(std::string(__func__) + ": null response");
    const auto* bytes = static_cast<const LibreSslOcspResponseBytes*>(response->responseBytes);
    if (!bytes) {
        LibreSslPutError_nid_no_patch(OcspLibrary, 0xfff, NoResponseData);
        return nullptr;
    }
    if (LibreSslObjectNid_nid_no_patch(bytes->responseType) != BasicResponseNid) {
        LibreSslPutError_nid_no_patch(OcspLibrary, 0xfff, NotBasicResponse);
        return nullptr;
    }
    const Asn1String* octets = bytes->response;
    if (!octets) throw std::invalid_argument(std::string(__func__) + ": null response octets");
    if (!octets->data && octets->length) throw std::invalid_argument(std::string(__func__) + ": null response data");
    const unsigned char* p = octets->data;
    void* decoded = nullptr;
    if (ItemDecode(&decoded, p, octets->length, BasicResponseItem, -1, 0, false) > 0) return static_cast<LibreSslOcspBasicResponse*>(decoded);
    Fail(DecodeError);
    return nullptr;
}

void APS5_VABI OCSP_BASICRESP_free_nid_postfix(LibreSslOcspBasicResponse* response) {
    void* value = response;
    ItemFree(&value, BasicResponseItem);
}

}
