// SPDX-FileCopyrightText: Copyright (c) 2013 Bruce D. Evans
// SPDX-License-Identifier: BSD-2-Clause
// Adapted from FreeBSD lib/msun/src/s_clog.c (release/11.4.0): https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_clog.c
/*-
 * Copyright (c) 2013 Bruce D. Evans
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace {

constexpr double Ln2High = 6.9314718055829871e-1;
constexpr double Ln2Low = 1.6465949582897082e-12;

__complex__ double Make(double real, double imaginary) {
    __complex__ double result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

int Exponent(double magnitude) {
    return static_cast<int>(std::bit_cast<std::uint64_t>(magnitude) >> 52) - 1023;
}

void TwoSum(double& a, double& b) {
    const double w = a + b;
    const double s = w - a;
    b = (a - (w - s)) + (b - s);
    a = w;
}

void FastTwoSum(double& a, double& b) {
    const double w = a + b;
    b = (a - w) + b;
    a = w;
}

}

extern "C" {

__complex__ double APS5_VABI clog_nid_postfix(__complex__ double z) {
    const double x = __real__ z;
    const double y = __imag__ z;
    const double v = atan2_nid_postfix(y, x);
    double ax = std::fabs(x);
    double ay = std::fabs(y);
    if (ax < ay) {
        const double t = ax;
        ax = ay;
        ay = t;
    }
    const int kx = Exponent(ax);
    const int ky = Exponent(ay);
    if (kx == DBL_MAX_EXP || ky == DBL_MAX_EXP) return Make(log_nid_postfix(hypot_nid_postfix(x, y)), v);
    if (ax == 1) {
        if (ky < (DBL_MIN_EXP - 1) / 2) return Make((ay / 2) * ay, v);
        return Make(log1p_nid_postfix(ay * ay) / 2, v);
    }
    if (kx - ky > DBL_MANT_DIG || ay == 0) return Make(log_nid_postfix(ax), v);
    if (kx >= DBL_MAX_EXP - 1) return Make(log_nid_postfix(hypot_nid_postfix(x * 0x1p-1022, y * 0x1p-1022)) + (DBL_MAX_EXP - 2) * Ln2Low + (DBL_MAX_EXP - 2) * Ln2High, v);
    if (kx >= (DBL_MAX_EXP - 1) / 2) return Make(log_nid_postfix(hypot_nid_postfix(x, y)), v);
    if (kx <= DBL_MIN_EXP - 2) return Make(log_nid_postfix(hypot_nid_postfix(x * 0x1p1023, y * 0x1p1023)) + (DBL_MIN_EXP - 2) * Ln2Low + (DBL_MIN_EXP - 2) * Ln2High, v);
    if (ky < (DBL_MIN_EXP - 1) / 2 + DBL_MANT_DIG) return Make(log_nid_postfix(hypot_nid_postfix(x, y)), v);
    double t = ax * (0x1p27 + 1);
    const double axh = (ax - t) + t;
    const double axl = ax - axh;
    const double ax2h = ax * ax;
    double ax2l = axh * axh - ax2h + 2 * axh * axl + axl * axl;
    t = ay * (0x1p27 + 1);
    const double ayh = (ay - t) + t;
    const double ayl = ay - ayh;
    const double ay2h = ay * ay;
    double ay2l = ayh * ayh - ay2h + 2 * ayh * ayl + ayl * ayl;
    double sh = ax2h;
    double sl = ay2h;
    FastTwoSum(sh, sl);
    if (sh < 0.5 || sh >= 3) return Make(log_nid_postfix(ay2l + ax2l + sl + sh) / 2, v);
    sh -= 1;
    TwoSum(sh, sl);
    TwoSum(ax2l, ay2l);
    TwoSum(sh, ax2l);
    TwoSum(sl, ay2l);
    t = ax2l + sl;
    FastTwoSum(sh, t);
    return Make(log1p_nid_postfix(ay2l + t + sh) / 2, v);
}

}
