/* Derived from LibreSSL 3.4.1 (openbsd 078b3d1df8ca2011c1605cd40418c58fa0f5b143) src/lib/libcrypto/asn1/t_x509.c
 * (t_x509.c,v 1.34) and src/lib/libcrypto/asn1/asn1_par.c (asn1_par.c,v 1.28) */
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

#include <stdlib.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509.h>

const char *
ASN1_tag2str(int tag)
{
	static const char * const tag2str[] = {
		"EOC", "BOOLEAN", "INTEGER", "BIT STRING", "OCTET STRING",
		"NULL", "OBJECT", "OBJECT DESCRIPTOR", "EXTERNAL", "REAL",
		"ENUMERATED", "<ASN1 11>", "UTF8STRING", "<ASN1 13>",
		"<ASN1 14>", "<ASN1 15>", "SEQUENCE", "SET",
		"NUMERICSTRING", "PRINTABLESTRING", "T61STRING",
		"VIDEOTEXSTRING", "IA5STRING", "UTCTIME", "GENERALIZEDTIME",
		"GRAPHICSTRING", "VISIBLESTRING", "GENERALSTRING",
		"UNIVERSALSTRING", "<ASN1 29>", "BMPSTRING"
	};

	if ((tag == V_ASN1_NEG_INTEGER) || (tag == V_ASN1_NEG_ENUMERATED))
		tag &= ~0x100;

	if (tag < 0 || tag > 30)
		return "(unknown)";
	return tag2str[tag];
}

int
X509_NAME_print(BIO *bp, const X509_NAME *name, int obase)
{
	char *s, *c, *b;
	int ret = 0, l, i;

	l = 80 - 2 - obase;

	b = X509_NAME_oneline(name, NULL, 0);
	if (b == NULL)
		return 0;
	if (*b == '\0') {
		free(b);
		return 1;
	}
	s = b + 1;

	c = s;
	for (;;) {
		if (((*s == '/') &&
		    ((s[1] >= 'A') && (s[1] <= 'Z') &&
		    ((s[2] == '=') || ((s[2] >= 'A') && (s[2] <= 'Z') &&
		    (s[3] == '='))))) || (*s == '\0')) {
			i = s - c;
			if (BIO_write(bp, c, i) != i)
				goto err;
			c = s + 1;
			if (*s != '\0') {
				if (BIO_write(bp, ", ", 2) != 2)
					goto err;
			}
			l--;
		}
		if (*s == '\0')
			break;
		s++;
		l--;
	}

	ret = 1;
	if (0) {
err:
		X509error(ERR_R_BUF_LIB);
	}
	free(b);
	return (ret);
}
