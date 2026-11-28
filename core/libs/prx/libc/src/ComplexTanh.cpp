// SPDX-FileCopyrightText: Copyright (c) 2011 David Schultz
// SPDX-License-Identifier: BSD-2-Clause
// Adapted from FreeBSD lib/msun/src/s_ctanh.c (release/11.4.0): https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_ctanh.c
/*-
 * Copyright (c) 2011 David Schultz
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
#include <cmath>
#include <limits>

namespace {

template <class T> struct ComplexOf;
template <> struct ComplexOf<double> { using Type = __complex__ double; };
template <> struct ComplexOf<long double> { using Type = __complex__ long double; };
template <class T> using Complex = typename ComplexOf<T>::Type;

static_assert(std::numeric_limits<double>::digits == 53);
static_assert(std::numeric_limits<long double>::digits == 53 || std::numeric_limits<long double>::digits == 64 || std::numeric_limits<long double>::digits == 113);

template <class T> constexpr T LargeMagnitude = std::numeric_limits<T>::digits == 53 ? 22 : std::numeric_limits<T>::digits == 64 ? 26 : 43;

double Tan(double x) { return tan_nid_postfix(x); }
long double Tan(long double x) { return tanl_nid_postfix(x); }
double Sin(double x) { return sin_nid_postfix(x); }
long double Sin(long double x) { return sinl_nid_postfix(x); }
double Cos(double x) { return cos_nid_postfix(x); }
long double Cos(long double x) { return cosl_nid_postfix(x); }
double Sinh(double x) { return sinh_nid_postfix(x); }
long double Sinh(long double x) { return sinhl_nid_postfix(x); }
double Exp(double x) { return exp_nid_postfix(x); }
long double Exp(long double x) { return expl_nid_postfix(x); }
double Sqrt(double x) { return sqrt_nid_postfix(x); }
long double Sqrt(long double x) { return sqrtl_nid_postfix(x); }

template <class T> Complex<T> Make(T real, T imaginary) {
    Complex<T> result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

template <class T> Complex<T> Swap(Complex<T> z) {
    return Make<T>(__imag__ z, __real__ z);
}

template <class T> Complex<T> Ctanh(Complex<T> z) {
    const T x = __real__ z;
    const T y = __imag__ z;
    if (std::isnan(x)) return Make<T>((x + 0) * (y + 0), y == 0 ? y : (x + 0) * (y + 0));
    if (std::isinf(x)) return Make<T>(std::copysign(T(1), x), std::copysign(T(0), std::isinf(y) ? y : Sin(y) * Cos(y)));
    if (!std::isfinite(y)) return Make<T>(y - y, y - y);
    if (std::fabs(x) >= LargeMagnitude<T>) {
        const T exponential = Exp(-std::fabs(x));
        return Make<T>(std::copysign(T(1), x), 4 * Sin(y) * Cos(y) * exponential * exponential);
    }
    const T t = Tan(y);
    const T beta = 1 + t * t;
    const T s = Sinh(x);
    const T rho = Sqrt(1 + s * s);
    const T denominator = 1 + beta * s * s;
    return Make<T>(beta * rho * s / denominator, t / denominator);
}

}

extern "C" {

__complex__ double APS5_VABI ctanh_nid_postfix(__complex__ double z) { return Ctanh<double>(z); }
__complex__ long double APS5_VABI ctanhl_nid_postfix(__complex__ long double z) { return Ctanh<long double>(z); }
__complex__ double APS5_VABI ctan_nid_postfix(__complex__ double z) { return Swap<double>(Ctanh<double>(Swap<double>(z))); }
__complex__ long double APS5_VABI ctanl_nid_postfix(__complex__ long double z) { return Swap<long double>(Ctanh<long double>(Swap<long double>(z))); }

}
