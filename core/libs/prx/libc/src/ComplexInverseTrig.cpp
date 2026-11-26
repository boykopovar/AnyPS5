// SPDX-FileCopyrightText: Copyright (c) 2012 Stephen Montgomery-Smith <stephen@FreeBSD.ORG>
// SPDX-FileCopyrightText: Copyright (c) 2017 Mahdi Mokhtari <mmokhi@FreeBSD.org>
// SPDX-License-Identifier: BSD-2-Clause
// Adapted from FreeBSD lib/msun/src/catrig.c (release/11.4.0): https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/catrig.c
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
#include <climits>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

template <class T> struct ComplexOf;
template <> struct ComplexOf<double> { using Type = __complex__ double; };
template <> struct ComplexOf<long double> { using Type = __complex__ long double; };
template <class T> using Complex = typename ComplexOf<T>::Type;

static_assert(std::numeric_limits<double>::digits == 53);
static_assert(std::numeric_limits<long double>::digits == 53 || std::numeric_limits<long double>::digits == 64 || std::numeric_limits<long double>::digits == 113);

template <class T> constexpr T Epsilon = std::numeric_limits<T>::epsilon();
template <class T> constexpr int MaxExponent = std::numeric_limits<T>::max_exponent;
template <class T> constexpr T HalfPi = std::numbers::pi_v<T> / 2;
template <class T> constexpr T HalfPiLow = std::numeric_limits<T>::digits == 64    ? T(-2.50827880633416601178e-20L)
                                           : std::numeric_limits<T>::digits == 53 ? T(6.12323399573676588613e-17L)
                                                                                 : T(4.33590506506189051239852201302167598e-35L);
template <class T> constexpr T Ln2 = std::numbers::ln2_v<T>;
template <class T> constexpr T E = std::numbers::e_v<T>;
template <class T> constexpr T RecipEpsilon = 1 / Epsilon<T>;
template <class T> constexpr int Cutoff = std::numeric_limits<T>::digits / 2 + 1;
template <class T> const T SqrtMin = std::sqrt(std::numeric_limits<T>::min());
template <class T> const T Sqrt3Epsilon = std::sqrt(3 * Epsilon<T>);
template <class T> const T Sqrt6Epsilon = std::sqrt(6 * Epsilon<T>);
template <class T> const T FourSqrtMin = 4 * SqrtMin<T>;
template <class T> const T HalfMax = std::ldexp(T(1), MaxExponent<T> - 1);
template <class T> const T QuarterSqrtMax = std::ldexp(T(1), MaxExponent<T> / 2 - 3);
template <class T> constexpr T ACrossover = 10;
template <class T> constexpr T BCrossover = T(0.6417L);
const volatile float Tiny = 0x1p-100f;

double Hypot(double x, double y) { return hypot_nid_postfix(x, y); }
long double Hypot(long double x, long double y) { return hypotl_nid_postfix(x, y); }
double Sqrt(double x) { return sqrt_nid_postfix(x); }
long double Sqrt(long double x) { return sqrtl_nid_postfix(x); }
double Log(double x) { return log_nid_postfix(x); }
long double Log(long double x) { return logl_nid_postfix(x); }
double Log1p(double x) { return log1p_nid_postfix(x); }
long double Log1p(long double x) { return log1pl_nid_postfix(x); }
double Atan2(double y, double x) { return atan2_nid_postfix(y, x); }
long double Atan2(long double y, long double x) { return atan2l_nid_postfix(y, x); }
double Asin(double x) { return asin_nid_postfix(x); }
long double Asin(long double x) { return asinl_nid_postfix(x); }
double Acos(double x) { return acos_nid_postfix(x); }
long double Acos(long double x) { return acosl_nid_postfix(x); }
double Atanh(double x) { return atanh_nid_postfix(x); }
long double Atanh(long double x) { return atanhl_nid_postfix(x); }
double Atan(double x) { return atan_nid_postfix(x); }
long double Atan(long double x) { return atanl_nid_postfix(x); }

template <class T> Complex<T> Make(T real, T imaginary) {
    Complex<T> result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

template <class T> Complex<T> Swap(Complex<T> z) {
    return Make<T>(__imag__ z, __real__ z);
}

template <class T> int Exponent(T value) {
    if (value == 0) return INT_MIN / 4;
    if (!std::isfinite(value)) return INT_MAX / 4;
    return std::ilogb(value);
}

template <class T> T HalfDifference(T a, T b, T hypotenuse) {
    if (b < 0) return (hypotenuse - b) / 2;
    if (b == 0) return a / 2;
    return a * a / (hypotenuse + b) / 2;
}

template <class T> struct HardWork {
    T rx;
    bool bUsable;
    T b;
    T sqrtA2My2;
    T newY;
};

template <class T> HardWork<T> DoHardWork(T x, T y) {
    constexpr T epsilon = Epsilon<T>;
    HardWork<T> work{};
    const T r = Hypot(x, y + 1);
    const T s = Hypot(x, y - 1);
    T a = (r + s) / 2;
    if (a < 1) a = 1;
    if (a < ACrossover<T>) {
        if (y == 1 && x < epsilon * epsilon / 128) {
            work.rx = Sqrt(x);
        } else if (x >= epsilon * std::fabs(y - 1)) {
            const T am1 = HalfDifference(x, 1 + y, r) + HalfDifference(x, 1 - y, s);
            work.rx = Log1p(am1 + Sqrt(am1 * (a + 1)));
        } else if (y < 1) {
            work.rx = x / Sqrt((1 - y) * (1 + y));
        } else {
            work.rx = Log1p((y - 1) + Sqrt((y - 1) * (y + 1)));
        }
    } else {
        work.rx = Log(a + Sqrt(a * a - 1));
    }
    work.newY = y;
    if (y < FourSqrtMin<T>) {
        work.bUsable = false;
        work.sqrtA2My2 = a * (2 / epsilon);
        work.newY = y * (2 / epsilon);
        return work;
    }
    work.b = y / a;
    work.bUsable = true;
    if (work.b > BCrossover<T>) {
        work.bUsable = false;
        if (y == 1 && x < epsilon / 128) {
            work.sqrtA2My2 = Sqrt(x) * Sqrt((a + y) / 2);
        } else if (x >= epsilon * std::fabs(y - 1)) {
            const T amy = HalfDifference(x, y + 1, r) + HalfDifference(x, y - 1, s);
            work.sqrtA2My2 = Sqrt(amy * (a + y));
        } else if (y > 1) {
            work.sqrtA2My2 = x * (4 / epsilon / epsilon) * y / Sqrt((y + 1) * (y - 1));
            work.newY = y * (4 / epsilon / epsilon);
        } else {
            work.sqrtA2My2 = Sqrt((1 - y) * (1 + y));
        }
    }
    return work;
}

template <class T> Complex<T> LogForLargeValues(T x, T y) {
    T ax = std::fabs(x);
    T ay = std::fabs(y);
    if (ax < ay) {
        const T t = ax;
        ax = ay;
        ay = t;
    }
    if (ax > HalfMax<T>) return Make<T>(Log(Hypot(x / E<T>, y / E<T>)) + 1, Atan2(y, x));
    if (ax > QuarterSqrtMax<T> || ay < SqrtMin<T>) return Make<T>(Log(Hypot(x, y)), Atan2(y, x));
    return Make<T>(Log(ax * ax + ay * ay) / 2, Atan2(y, x));
}

void RaiseInexact() {
    [[maybe_unused]] volatile float inexact = 1 + Tiny;
}

template <class T> T SumSquares(T x, T y) {
    if (y < SqrtMin<T>) return x * x;
    return x * x + y * y;
}

template <class T> T RealPartReciprocal(T x, T y) {
    const int ix = Exponent(x);
    const int iy = Exponent(y);
    if (ix - iy >= Cutoff<T> || std::isinf(x)) return 1 / x;
    if (iy - ix >= Cutoff<T>) return x / y / y;
    if (ix <= MaxExponent<T> / 2 - Cutoff<T>) return x / (x * x + y * y);
    const T scale = std::ldexp(T(1), 1 - ix);
    x *= scale;
    y *= scale;
    return x / (x * x + y * y) * scale;
}

template <class T> Complex<T> Catanh(Complex<T> z) {
    const T x = __real__ z;
    const T y = __imag__ z;
    const T ax = std::fabs(x);
    const T ay = std::fabs(y);
    if (y == 0 && ax <= 1) return Make<T>(Atanh(x), y);
    if (x == 0) return Make<T>(x, Atan(y));
    if (std::isnan(x) || std::isnan(y)) {
        if (std::isinf(x)) return Make<T>(std::copysign(T(0), x), y + y);
        if (std::isinf(y)) return Make<T>(std::copysign(T(0), x), std::copysign(HalfPi<T>, y));
        return Make<T>((x + T(0)) + (y + 0), (x + T(0)) + (y + 0));
    }
    if (ax > RecipEpsilon<T> || ay > RecipEpsilon<T>) return Make<T>(RealPartReciprocal(x, y), std::copysign(HalfPi<T>, y));
    if (ax < Sqrt3Epsilon<T> / 2 && ay < Sqrt3Epsilon<T> / 2) {
        RaiseInexact();
        return z;
    }
    const T rx = ax == 1 && ay < Epsilon<T> ? (Ln2<T> - Log(ay)) / 2 : Log1p(4 * ax / SumSquares(ax - 1, ay)) / 4;
    T ry;
    if (ax == 1) ry = Atan2(T(2), -ay) / 2;
    else if (ay < Epsilon<T>) ry = Atan2(2 * ay, (1 - ax) * (1 + ax)) / 2;
    else ry = Atan2(2 * ay, (1 - ax) * (1 + ax) - ay * ay) / 2;
    return Make<T>(std::copysign(rx, x), std::copysign(ry, y));
}

template <class T> Complex<T> Casinh(Complex<T> z) {
    const T x = __real__ z;
    const T y = __imag__ z;
    const T ax = std::fabs(x);
    const T ay = std::fabs(y);
    if (std::isnan(x) || std::isnan(y)) {
        if (std::isinf(x)) return Make<T>(x, y + y);
        if (std::isinf(y)) return Make<T>(y, x + x);
        if (y == 0) return Make<T>(x + x, y);
        return Make<T>((x + T(0)) + (y + 0), (x + T(0)) + (y + 0));
    }
    if (ax > RecipEpsilon<T> || ay > RecipEpsilon<T>) {
        const Complex<T> w = std::signbit(x) ? LogForLargeValues(-x, -y) : LogForLargeValues(x, y);
        return Make<T>(std::copysign(__real__ w + Ln2<T>, x), std::copysign(__imag__ w, y));
    }
    if (x == 0 && y == 0) return z;
    RaiseInexact();
    if (ax < Sqrt6Epsilon<T> / 4 && ay < Sqrt6Epsilon<T> / 4) return z;
    const HardWork<T> work = DoHardWork(ax, ay);
    const T ry = work.bUsable ? Asin(work.b) : Atan2(work.newY, work.sqrtA2My2);
    return Make<T>(std::copysign(work.rx, x), std::copysign(ry, y));
}

template <class T> Complex<T> Cacos(Complex<T> z) {
    const T x = __real__ z;
    const T y = __imag__ z;
    const bool sx = std::signbit(x);
    const bool sy = std::signbit(y);
    const T ax = std::fabs(x);
    const T ay = std::fabs(y);
    if (std::isnan(x) || std::isnan(y)) {
        if (std::isinf(x)) return Make<T>(y + y, -std::numeric_limits<T>::infinity());
        if (std::isinf(y)) return Make<T>(x + x, -y);
        if (x == 0) return Make<T>(HalfPi<T>, y + y);
        return Make<T>((x + T(0)) + (y + 0), (x + T(0)) + (y + 0));
    }
    if (ax > RecipEpsilon<T> || ay > RecipEpsilon<T>) {
        const Complex<T> w = LogForLargeValues(x, y);
        const T ry = __real__ w + Ln2<T>;
        return Make<T>(std::fabs(__imag__ w), sy ? ry : -ry);
    }
    if (x == 1 && y == 0) return Make<T>(0, -y);
    RaiseInexact();
    if (ax < Sqrt6Epsilon<T> / 4 && ay < Sqrt6Epsilon<T> / 4) return Make<T>(HalfPi<T> - (x - HalfPiLow<T>), -y);
    const HardWork<T> work = DoHardWork(ay, ax);
    T rx;
    if (work.bUsable) rx = Acos(sx ? -work.b : work.b);
    else rx = Atan2(work.sqrtA2My2, sx ? -work.newY : work.newY);
    return Make<T>(rx, sy ? work.rx : -work.rx);
}

template <class T> Complex<T> Cacosh(Complex<T> z) {
    const Complex<T> w = Cacos<T>(z);
    const T rx = __real__ w;
    const T ry = __imag__ w;
    if (std::isnan(rx) && std::isnan(ry)) return Make<T>(ry, rx);
    if (std::isnan(rx)) return Make<T>(std::fabs(ry), rx);
    if (std::isnan(ry)) return Make<T>(ry, ry);
    return Make<T>(std::fabs(ry), std::copysign(rx, __imag__ z));
}

}

extern "C" {

__complex__ double APS5_VABI catanh_nid_postfix(__complex__ double z) { return Catanh<double>(z); }
__complex__ long double APS5_VABI catanhl_nid_postfix(__complex__ long double z) { return Catanh<long double>(z); }
__complex__ double APS5_VABI catan_nid_postfix(__complex__ double z) { return Swap<double>(Catanh<double>(Swap<double>(z))); }
__complex__ long double APS5_VABI catanl_nid_postfix(__complex__ long double z) { return Swap<long double>(Catanh<long double>(Swap<long double>(z))); }
__complex__ double APS5_VABI casinh_nid_postfix(__complex__ double z) { return Casinh<double>(z); }
__complex__ long double APS5_VABI casinhl_nid_postfix(__complex__ long double z) { return Casinh<long double>(z); }
__complex__ double APS5_VABI casin_nid_postfix(__complex__ double z) { return Swap<double>(Casinh<double>(Swap<double>(z))); }
__complex__ long double APS5_VABI casinl_nid_postfix(__complex__ long double z) { return Swap<long double>(Casinh<long double>(Swap<long double>(z))); }
__complex__ double APS5_VABI cacos_nid_postfix(__complex__ double z) { return Cacos<double>(z); }
__complex__ long double APS5_VABI cacosl_nid_postfix(__complex__ long double z) { return Cacos<long double>(z); }
__complex__ double APS5_VABI cacosh_nid_postfix(__complex__ double z) { return Cacosh<double>(z); }
__complex__ long double APS5_VABI cacoshl_nid_postfix(__complex__ long double z) { return Cacosh<long double>(z); }

}
