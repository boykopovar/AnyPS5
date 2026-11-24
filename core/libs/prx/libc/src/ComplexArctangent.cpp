// SPDX-FileCopyrightText: Copyright (c) 2012 Stephen Montgomery-Smith <stephen@FreeBSD.ORG>
// SPDX-FileCopyrightText: Copyright (c) 2017 Mahdi Mokhtari <mmokhi@FreeBSD.org>
// SPDX-License-Identifier: BSD-2-Clause
// Adapted from FreeBSD lib/msun/src/catrigl.c: https://github.com/freebsd/freebsd-src/blob/0dd5a5603e7a33d976f8e6015620bbc79839c609/lib/msun/src/catrigl.c
/*-
 * Copyright (c) 2012 Stephen Montgomery-Smith <stephen@FreeBSD.ORG>
 * Copyright (c) 2017 Mahdi Mokhtari <mmokhi@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include "prx/libc/include/Api.hpp"
#include <cfloat>
#include <climits>
#include <cmath>
#include <numbers>

namespace {

constexpr long double HalfPi = std::numbers::pi_v<long double> / 2;
constexpr long double Ln2 = std::numbers::ln2_v<long double>;
constexpr long double RecipEpsilon = 1 / LDBL_EPSILON;
constexpr int Cutoff = LDBL_MANT_DIG / 2 + 1;
const long double SqrtMin = std::sqrt(LDBL_MIN);
const long double Sqrt3Epsilon = std::sqrt(3 * LDBL_EPSILON);
const volatile float Tiny = 0x1p-100f;

__complex__ long double Make(long double real, long double imaginary) {
    __complex__ long double result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

int Exponent(long double value) {
    if (value == 0) return INT_MIN / 4;
    if (!std::isfinite(value)) return INT_MAX / 4;
    return std::ilogb(value);
}

long double SumSquares(long double x, long double y) {
    if (y < SqrtMin) return x * x;
    return x * x + y * y;
}

long double RealPartReciprocal(long double x, long double y) {
    const int ix = Exponent(x);
    const int iy = Exponent(y);
    if (ix - iy >= Cutoff || std::isinf(x)) return 1 / x;
    if (iy - ix >= Cutoff) return x / y / y;
    if (ix <= LDBL_MAX_EXP / 2 - Cutoff) return x / (x * x + y * y);
    const long double scale = std::ldexp(1.0L, 1 - ix);
    x *= scale;
    y *= scale;
    return x / (x * x + y * y) * scale;
}

}

extern "C" {

__complex__ long double APS5_VABI catanhl_nid_postfix(__complex__ long double z) {
    const long double x = __real__ z;
    const long double y = __imag__ z;
    const long double ax = std::fabs(x);
    const long double ay = std::fabs(y);
    if (y == 0 && ax <= 1) return Make(atanhl_nid_postfix(x), y);
    if (x == 0) return Make(x, atanl_nid_postfix(y));
    if (std::isnan(x) || std::isnan(y)) {
        if (std::isinf(x)) return Make(std::copysign(0.0L, x), y + y);
        if (std::isinf(y)) return Make(std::copysign(0.0L, x), std::copysign(HalfPi, y));
        return Make((x + 0.0L) + (y + 0), (x + 0.0L) + (y + 0));
    }
    if (ax > RecipEpsilon || ay > RecipEpsilon) return Make(RealPartReciprocal(x, y), std::copysign(HalfPi, y));
    if (ax < Sqrt3Epsilon / 2 && ay < Sqrt3Epsilon / 2) {
        [[maybe_unused]] volatile float inexact = 1 + Tiny;
        return z;
    }
    const long double rx = ax == 1 && ay < LDBL_EPSILON ? (Ln2 - logl_nid_postfix(ay)) / 2 : log1pl_nid_postfix(4 * ax / SumSquares(ax - 1, ay)) / 4;
    long double ry;
    if (ax == 1) ry = atan2l_nid_postfix(2.0L, -ay) / 2;
    else if (ay < LDBL_EPSILON) ry = atan2l_nid_postfix(2 * ay, (1 - ax) * (1 + ax)) / 2;
    else ry = atan2l_nid_postfix(2 * ay, (1 - ax) * (1 + ax) - ay * ay) / 2;
    return Make(std::copysign(rx, x), std::copysign(ry, y));
}

__complex__ long double APS5_VABI catanl_nid_postfix(__complex__ long double z) {
    const __complex__ long double result = catanhl_nid_postfix(Make(__imag__ z, __real__ z));
    return Make(__imag__ result, __real__ result);
}

}
