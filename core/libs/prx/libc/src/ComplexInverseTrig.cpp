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

static_assert(LDBL_MANT_DIG == 53 || LDBL_MANT_DIG == 64 || LDBL_MANT_DIG == 113);

constexpr long double HalfPi = std::numbers::pi_v<long double> / 2;
constexpr long double HalfPiLow = LDBL_MANT_DIG == 64    ? -2.50827880633416601178e-20L
                                  : LDBL_MANT_DIG == 53 ? 6.12323399573676588613e-17L
                                                        : 4.33590506506189051239852201302167598e-35L;
constexpr long double Ln2 = std::numbers::ln2_v<long double>;
constexpr long double RecipEpsilon = 1 / LDBL_EPSILON;
constexpr int Cutoff = LDBL_MANT_DIG / 2 + 1;
const long double SqrtMin = std::sqrt(LDBL_MIN);
const long double Sqrt3Epsilon = std::sqrt(3 * LDBL_EPSILON);
const long double Sqrt6Epsilon = std::sqrt(6 * LDBL_EPSILON);
const long double FourSqrtMin = 4 * SqrtMin;
const long double HalfMax = std::ldexp(1.0L, LDBL_MAX_EXP - 1);
const long double QuarterSqrtMax = std::ldexp(1.0L, LDBL_MAX_EXP / 2 - 3);
constexpr long double ACrossover = 10;
constexpr long double BCrossover = 0.6417L;
constexpr long double E = std::numbers::e_v<long double>;
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

long double HalfDifference(long double a, long double b, long double hypotenuse) {
    if (b < 0) return (hypotenuse - b) / 2;
    if (b == 0) return a / 2;
    return a * a / (hypotenuse + b) / 2;
}

struct HardWork {
    long double rx;
    bool bUsable;
    long double b;
    long double sqrtA2My2;
    long double newY;
};

HardWork DoHardWork(long double x, long double y) {
    HardWork work{};
    const long double r = hypotl_nid_postfix(x, y + 1);
    const long double s = hypotl_nid_postfix(x, y - 1);
    long double a = (r + s) / 2;
    if (a < 1) a = 1;
    if (a < ACrossover) {
        if (y == 1 && x < LDBL_EPSILON * LDBL_EPSILON / 128) {
            work.rx = sqrtl_nid_postfix(x);
        } else if (x >= LDBL_EPSILON * std::fabs(y - 1)) {
            const long double am1 = HalfDifference(x, 1 + y, r) + HalfDifference(x, 1 - y, s);
            work.rx = log1pl_nid_postfix(am1 + sqrtl_nid_postfix(am1 * (a + 1)));
        } else if (y < 1) {
            work.rx = x / sqrtl_nid_postfix((1 - y) * (1 + y));
        } else {
            work.rx = log1pl_nid_postfix((y - 1) + sqrtl_nid_postfix((y - 1) * (y + 1)));
        }
    } else {
        work.rx = logl_nid_postfix(a + sqrtl_nid_postfix(a * a - 1));
    }
    work.newY = y;
    if (y < FourSqrtMin) {
        work.bUsable = false;
        work.sqrtA2My2 = a * (2 / LDBL_EPSILON);
        work.newY = y * (2 / LDBL_EPSILON);
        return work;
    }
    work.b = y / a;
    work.bUsable = true;
    if (work.b > BCrossover) {
        work.bUsable = false;
        if (y == 1 && x < LDBL_EPSILON / 128) {
            work.sqrtA2My2 = sqrtl_nid_postfix(x) * sqrtl_nid_postfix((a + y) / 2);
        } else if (x >= LDBL_EPSILON * std::fabs(y - 1)) {
            const long double amy = HalfDifference(x, y + 1, r) + HalfDifference(x, y - 1, s);
            work.sqrtA2My2 = sqrtl_nid_postfix(amy * (a + y));
        } else if (y > 1) {
            work.sqrtA2My2 = x * (4 / LDBL_EPSILON / LDBL_EPSILON) * y / sqrtl_nid_postfix((y + 1) * (y - 1));
            work.newY = y * (4 / LDBL_EPSILON / LDBL_EPSILON);
        } else {
            work.sqrtA2My2 = sqrtl_nid_postfix((1 - y) * (1 + y));
        }
    }
    return work;
}

__complex__ long double LogForLargeValues(long double x, long double y) {
    long double ax = std::fabs(x);
    long double ay = std::fabs(y);
    if (ax < ay) {
        const long double t = ax;
        ax = ay;
        ay = t;
    }
    if (ax > HalfMax) return Make(logl_nid_postfix(hypotl_nid_postfix(x / E, y / E)) + 1, atan2l_nid_postfix(y, x));
    if (ax > QuarterSqrtMax || ay < SqrtMin) return Make(logl_nid_postfix(hypotl_nid_postfix(x, y)), atan2l_nid_postfix(y, x));
    return Make(logl_nid_postfix(ax * ax + ay * ay) / 2, atan2l_nid_postfix(y, x));
}

void RaiseInexact() {
    [[maybe_unused]] volatile float inexact = 1 + Tiny;
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
        RaiseInexact();
        return z;
    }
    const long double rx = ax == 1 && ay < LDBL_EPSILON ? (Ln2 - logl_nid_postfix(ay)) / 2 : log1pl_nid_postfix(4 * ax / SumSquares(ax - 1, ay)) / 4;
    long double ry;
    if (ax == 1) ry = atan2l_nid_postfix(2.0L, -ay) / 2;
    else if (ay < LDBL_EPSILON) ry = atan2l_nid_postfix(2 * ay, (1 - ax) * (1 + ax)) / 2;
    else ry = atan2l_nid_postfix(2 * ay, (1 - ax) * (1 + ax) - ay * ay) / 2;
    return Make(std::copysign(rx, x), std::copysign(ry, y));
}

__complex__ long double APS5_VABI casinhl_nid_postfix(__complex__ long double z) {
    const long double x = __real__ z;
    const long double y = __imag__ z;
    const long double ax = std::fabs(x);
    const long double ay = std::fabs(y);
    if (std::isnan(x) || std::isnan(y)) {
        if (std::isinf(x)) return Make(x, y + y);
        if (std::isinf(y)) return Make(y, x + x);
        if (y == 0) return Make(x + x, y);
        return Make((x + 0.0L) + (y + 0), (x + 0.0L) + (y + 0));
    }
    if (ax > RecipEpsilon || ay > RecipEpsilon) {
        const __complex__ long double w = std::signbit(x) ? LogForLargeValues(-x, -y) : LogForLargeValues(x, y);
        return Make(std::copysign(__real__ w + Ln2, x), std::copysign(__imag__ w, y));
    }
    if (x == 0 && y == 0) return z;
    RaiseInexact();
    if (ax < Sqrt6Epsilon / 4 && ay < Sqrt6Epsilon / 4) return z;
    const HardWork work = DoHardWork(ax, ay);
    const long double ry = work.bUsable ? asinl_nid_postfix(work.b) : atan2l_nid_postfix(work.newY, work.sqrtA2My2);
    return Make(std::copysign(work.rx, x), std::copysign(ry, y));
}

__complex__ long double APS5_VABI casinl_nid_postfix(__complex__ long double z) {
    const __complex__ long double result = casinhl_nid_postfix(Make(__imag__ z, __real__ z));
    return Make(__imag__ result, __real__ result);
}

__complex__ long double APS5_VABI cacosl_nid_postfix(__complex__ long double z) {
    const long double x = __real__ z;
    const long double y = __imag__ z;
    const bool sx = std::signbit(x);
    const bool sy = std::signbit(y);
    const long double ax = std::fabs(x);
    const long double ay = std::fabs(y);
    if (std::isnan(x) || std::isnan(y)) {
        if (std::isinf(x)) return Make(y + y, -HUGE_VALL);
        if (std::isinf(y)) return Make(x + x, -y);
        if (x == 0) return Make(HalfPi, y + y);
        return Make((x + 0.0L) + (y + 0), (x + 0.0L) + (y + 0));
    }
    if (ax > RecipEpsilon || ay > RecipEpsilon) {
        const __complex__ long double w = LogForLargeValues(x, y);
        const long double ry = __real__ w + Ln2;
        return Make(std::fabs(__imag__ w), sy ? ry : -ry);
    }
    if (x == 1 && y == 0) return Make(0, -y);
    RaiseInexact();
    if (ax < Sqrt6Epsilon / 4 && ay < Sqrt6Epsilon / 4) return Make(HalfPi - (x - HalfPiLow), -y);
    const HardWork work = DoHardWork(ay, ax);
    long double rx;
    if (work.bUsable) rx = acosl_nid_postfix(sx ? -work.b : work.b);
    else rx = atan2l_nid_postfix(work.sqrtA2My2, sx ? -work.newY : work.newY);
    return Make(rx, sy ? work.rx : -work.rx);
}

__complex__ long double APS5_VABI cacoshl_nid_postfix(__complex__ long double z) {
    const __complex__ long double w = cacosl_nid_postfix(z);
    const long double rx = __real__ w;
    const long double ry = __imag__ w;
    if (std::isnan(rx) && std::isnan(ry)) return Make(ry, rx);
    if (std::isnan(rx)) return Make(std::fabs(ry), rx);
    if (std::isnan(ry)) return Make(ry, ry);
    return Make(std::fabs(ry), std::copysign(rx, __imag__ z));
}

__complex__ long double APS5_VABI catanl_nid_postfix(__complex__ long double z) {
    const __complex__ long double result = catanhl_nid_postfix(Make(__imag__ z, __real__ z));
    return Make(__imag__ result, __real__ result);
}

}
