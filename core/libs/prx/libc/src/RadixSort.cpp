/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy and by Dan Bernstein at New York University,
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/gen/radixsort.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/radixsort.c */
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace {

using Byte = unsigned char;

struct RadixStack {
    const Byte** sa;
    int sn;
    int si;
};

constexpr int RadixThreshold = 20;
constexpr int RadixStackSize = 512;
constexpr int GuestEinval = 22;
constexpr int GuestEnomem = 12;

void SetErrno(int value) {
    *__error_nid_postfix() = value;
}

void SimpleSort(const Byte** a, int n, int b, const Byte* tr, std::uint32_t endch) {
    Byte ch;
    const Byte **ak, **ai, *s, *t;
    for (ak = a + 1; --n >= 1; ak++)
        for (ai = ak; ai > a; ai--) {
            for (s = ai[0] + b, t = ai[-1] + b; (ch = tr[*s]) != endch; s++, t++)
                if (ch != tr[*t]) break;
            if (ch >= tr[*t]) break;
            const Byte* swapped = ai[0];
            ai[0] = ai[-1];
            ai[-1] = swapped;
        }
}

void ROrderUnstable(const Byte** a, int n, int i, const Byte* tr, std::uint32_t endch);

void ROrderStable(const Byte** a, const Byte** ta, int n, int i, const Byte* tr, std::uint32_t endch);

void ROrderUnstable(const Byte** a, int n, int i, const Byte* tr, std::uint32_t endch) {
    static int count[256], nc, bmin;
    int c;
    const Byte **ak, *r;
    RadixStack s[RadixStackSize], *sp, *sp0, *sp1, temp;
    int* cp;
    int bigc;
    const Byte **an, *t, **aj, **top[256];
    sp = s;
    sp->sa = a; sp->sn = n; (sp++)->si = i;
    while (!(s >= sp)) {
        --sp;
        a = sp->sa;
        n = sp->sn;
        i = sp->si;
        if (n < RadixThreshold) {
            SimpleSort(a, n, i, tr, endch);
            continue;
        }
        an = a + n;
        if (nc == 0) {
            bmin = 255;
            for (ak = a; ak < an;) {
                c = tr[(*ak++)[i]];
                if (++count[c] == 1 && c != static_cast<int>(endch)) {
                    if (c < bmin) bmin = c;
                    nc++;
                }
            }
            if (sp + nc > s + RadixStackSize) {
                ROrderUnstable(a, n, i, tr, endch);
                continue;
            }
        }
        if (nc == 1 && count[bmin] == n) {
            sp->sa = a; sp->sn = n; (sp++)->si = i + 1;
            nc = count[bmin] = 0;
            continue;
        }
        sp0 = sp1 = sp;
        bigc = 2;
        if (endch == 0) {
            top[0] = ak = a + count[0];
        } else {
            ak = a;
            top[255] = an;
        }
        for (cp = count + bmin; nc > 0; cp++) {
            while (*cp == 0) cp++;
            if (*cp > 1) {
                if (*cp > bigc) {
                    bigc = *cp;
                    sp1 = sp;
                }
                sp->sa = ak; sp->sn = *cp; (sp++)->si = i + 1;
            }
            top[cp - count] = ak += *cp;
            nc--;
        }
        temp = *sp0;
        *sp0 = *sp1;
        *sp1 = temp;
        aj = a;
        while (aj < an) {
            r = *aj;
            for (;;) {
                ak = --top[c = tr[r[i]]];
                if (!(aj < ak)) break;
                t = *ak;
                *ak = r;
                r = t;
            }
            *aj = r;
            aj += count[c];
            count[c] = 0;
        }
    }
}

void ROrderStable(const Byte** a, const Byte** ta, int n, int i, const Byte* tr, std::uint32_t endch) {
    static int count[256], nc, bmin;
    int c;
    const Byte **ak, **ai;
    RadixStack s[RadixStackSize], *sp, *sp0, *sp1, temp;
    const Byte** top[256];
    int* cp;
    int bigc;
    sp = s;
    sp->sa = a; sp->sn = n; (sp++)->si = i;
    while (!(s >= sp)) {
        --sp;
        a = sp->sa;
        n = sp->sn;
        i = sp->si;
        if (n < RadixThreshold) {
            SimpleSort(a, n, i, tr, endch);
            continue;
        }
        if (nc == 0) {
            bmin = 255;
            for (ak = a + n; --ak >= a;) {
                c = tr[(*ak)[i]];
                if (++count[c] == 1 && c != static_cast<int>(endch)) {
                    if (c < bmin) bmin = c;
                    nc++;
                }
            }
            if (sp + nc > s + RadixStackSize) {
                ROrderStable(a, ta, n, i, tr, endch);
                continue;
            }
        }
        sp0 = sp1 = sp;
        bigc = 2;
        if (endch == 0) {
            top[0] = ak = a + count[0];
            count[0] = 0;
        } else {
            ak = a;
            top[255] = a + n;
            count[255] = 0;
        }
        for (cp = count + bmin; nc > 0; cp++) {
            while (*cp == 0) cp++;
            if ((c = *cp) > 1) {
                if (c > bigc) {
                    bigc = c;
                    sp1 = sp;
                }
                sp->sa = ak; sp->sn = c; (sp++)->si = i + 1;
            }
            top[cp - count] = ak += c;
            *cp = 0;
            nc--;
        }
        temp = *sp0;
        *sp0 = *sp1;
        *sp1 = temp;
        for (ak = ta + n, ai = a + n; ak > ta;) *--ak = *--ai;
        for (ak = ta + n; --ak >= ta;) *--top[tr[(*ak)[i]]] = *ak;
    }
}

bool SetUpTable(const Byte* tab, std::uint32_t& endch, const Byte*& tr, Byte (&tr0)[256]) {
    if (tab == nullptr) {
        tr = tr0;
        int c;
        for (c = 0; c < static_cast<int>(endch); c++) tr0[c] = static_cast<Byte>(c + 1);
        tr0[c] = 0;
        for (c++; c < 256; c++) tr0[c] = static_cast<Byte>(c);
        endch = 0;
        return true;
    }
    endch = tab[endch];
    tr = tab;
    if (endch != 0 && endch != 255) {
        SetErrno(GuestEinval);
        return false;
    }
    return true;
}

}

extern "C" int APS5_VABI radixsort_nid_postfix(const unsigned char** a, int n, const unsigned char* tab, std::uint32_t endch) {
    Byte tr0[256];
    const Byte* tr = nullptr;
    if (!SetUpTable(tab, endch, tr, tr0)) return -1;
    ROrderUnstable(a, n, 0, tr, endch);
    return 0;
}

extern "C" int APS5_VABI sradixsort_nid_postfix(const unsigned char** a, int n, const unsigned char* tab, std::uint32_t endch) {
    Byte tr0[256];
    const Byte* tr = nullptr;
    if (!SetUpTable(tab, endch, tr, tr0)) return -1;
    if (n < RadixThreshold) {
        SimpleSort(a, n, 0, tr, endch);
        return 0;
    }
    const Byte** ta = static_cast<const Byte**>(malloc_nid_postfix(static_cast<std::size_t>(n) * sizeof(a)));
    if (ta == nullptr) {
        SetErrno(GuestEnomem);
        return -1;
    }
    ROrderStable(a, ta, n, 0, tr, endch);
    free_nid_postfix(ta);
    return 0;
}
