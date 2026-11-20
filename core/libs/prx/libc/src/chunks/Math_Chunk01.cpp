extern "C" {
/* Derived from FreeBSD lib/msun/src/e_powf.c (release/9.0.0); lib/msun/src/s_expm1.c (release/11.4.0);
 * lib/msun/src/s_log1p.c (release/11.4.0); lib/msun/src/e_atan2f.c (release/11.4.0);
 * lib/msun/src/s_expm1f.c (59bc2f095289); lib/msun/src/s_log1pf.c (59bc2f095289);
 * lib/msun/src/e_expf.c (59bc2f095289); lib/msun/src/e_atanhf.c (59bc2f095289); lib/msun/src/e_sinhf.c
 * (5aa554c7e5b6); lib/msun/src/e_coshf.c (5aa554c7e5b6); lib/msun/src/e_acoshf.c (5aa554c7e5b6);
 * lib/msun/src/s_atanf.c (release/11.4.0); lib/msun/src/e_logf.c (release/11.4.0);
 * lib/msun/src/e_acosf.c (release/11.4.0); lib/msun/src/e_asinf.c (release/11.4.0);
 * lib/msun/src/s_tanh.c (release/11.4.0); lib/msun/src/s_logb.c (release/11.4.0);
 * lib/msun/src/s_logbf.c (release/11.4.0); lib/msun/src/e_fmodf.c (release/11.4.0);
 * lib/msun/src/s_cbrtf.c (release/11.4.0); lib/msun/src/k_sinf.c (release/11.4.0);
 * lib/msun/src/k_cosf.c (release/11.4.0); lib/msun/src/e_rem_pio2f.c (release/11.4.0);
 * lib/msun/src/s_atan.c (release/11.4.0); lib/msun/src/s_sinf.c (release/11.4.0);
 * lib/msun/src/s_cosf.c (release/11.4.0); lib/msun/src/s_tanf.c (release/11.4.0);
 * lib/msun/src/s_tanhf.c (release/11.4.0) */
/* Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Optimized by Bruce D. Evans. */
/* e_rem_pio2f.c, k_cosf.c and s_cbrtf.c:
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Debugged and optimized by Bruce D. Evans. */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* Derived from FreeBSD lib/msun/src/s_cbrt.c (release/11.4.0) */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * Optimized by Bruce D. Evans.
 */
/* Derived from FreeBSD lib/msun/src/k_tanf.c (release/11.4.0) */
/* Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Optimized by Bruce D. Evans. */
/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* Derived from FreeBSD lib/msun/src/k_rem_pio2.c (release/11.4.0); lib/msun/src/e_sinh.c
 * (release/11.4.0); lib/msun/src/e_asinl.c (9f0432041b8f) */
/* Converted to long double by David Schultz <das@FreeBSD.ORG>. */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* Derived from FreeBSD lib/msun/src/e_atan2.c (release/11.4.0) */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */
/* Derived from FreeBSD lib/msun/src/e_log10.c (release/11.4.0); lib/msun/src/e_log2.c
 * (release/11.4.0); lib/msun/src/k_log.h (release/11.4.0) */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */
/* Derived from FreeBSD lib/msun/src/s_exp2f.c (release/11.4.0) */
/*-
 * Copyright (c) 2005 David Schultz <das@FreeBSD.ORG>
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
/* Derived from FreeBSD lib/msun/ld80/invtrig.c (9f0432041b8f) */
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 David Schultz <das@FreeBSD.ORG>
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
/* Derived from FreeBSD lib/msun/src/s_cbrtl.c (release/11.4.0) */
/*-
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2009-2011, Bruce D. Evans, Steven G. Kargl, David Schultz.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * The argument reduction and testing for exceptional cases was
 * written by Steven G. Kargl with input from Bruce D. Evans
 * and David A. Schultz.
 */
/* Derived from FreeBSD lib/msun/src/e_hypotl.c (release/11.4.0) */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
#include "prx/libc/include/Api.hpp"
#include <mutex>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestCpu.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/general/RemPio2Table.hpp"

namespace {

template<typename TFloat>
std::int64_t RoundToInt64(TFloat value) {
    const TFloat rounded = std::round(value);
    if (!(rounded >= static_cast<TFloat>(INT64_MIN) && rounded < -static_cast<TFloat>(INT64_MIN))) {
        std::feraiseexcept(FE_INVALID);
        return INT64_MIN;
    }
    return static_cast<std::int64_t>(rounded);
}

template<typename TFloat, typename TBits, int FractionBits>
std::int64_t NudgedToInt64(TFloat x, TFloat nudge) {
    constexpr int ExponentBits = static_cast<int>(sizeof(TBits) * 8) - 1 - FractionBits;
    constexpr int Bias = (1 << (ExponentBits - 1)) - 1;
    constexpr TBits Sign = TBits{1} << (sizeof(TBits) * 8 - 1);
    const volatile TFloat sum = x < 0 ? x - nudge : x + nudge;
    TBits bits = std::bit_cast<TBits>(static_cast<TFloat>(sum));
    const int exponent = static_cast<int>((bits & ~Sign) >> FractionBits);
    if (exponent < Bias) bits &= Sign;
    else if (exponent < Bias + FractionBits) bits &= ~((TBits{1} << (Bias + FractionBits - exponent)) - 1);
    const TFloat whole = std::bit_cast<TFloat>(bits);
    if (!(whole >= static_cast<TFloat>(INT64_MIN) && whole < -static_cast<TFloat>(INT64_MIN))) {
        std::feraiseexcept(FE_INVALID);
        return INT64_MIN;
    }
    return static_cast<std::int64_t>(whole);
}

float SinhfExpm1(float x) {
    constexpr float Ln2High = 6.9313812256e-01f, Ln2Low = 9.0580006145e-06f, InverseLn2 = 1.4426950216e+00f;
    constexpr float Q1 = -3.3333212137e-2f, Q2 = 1.5807170421e-3f;
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x);
    int k = 0;
    float c = 0.0f;
    if (magnitude > 0x3eb17218u) {
        float high;
        float low;
        if (magnitude < 0x3f851592u) {
            high = x - Ln2High;
            low = Ln2Low;
            k = 1;
        } else {
            k = static_cast<int>(InverseLn2 * x + 0.5f);
            const float t = static_cast<float>(k);
            high = x - t * Ln2High;
            low = t * Ln2Low;
        }
        x = high - low;
        c = (high - x) - low;
    }
    const float halfX = 0.5f * x;
    const float halfSquare = x * halfX;
    const float r1 = 1.0f + halfSquare * (Q1 + halfSquare * Q2);
    const float t = 3.0f - r1 * halfX;
    float e = halfSquare * ((r1 - t) / (6.0f - x * t));
    if (k == 0) return x - (x * e - halfSquare);
    e = x * (e - c) - c;
    e -= halfSquare;
    if (k == 1) {
        if (x < -0.25f) return -2.0f * (e - (x + 0.5f));
        return 1.0f + 2.0f * (x - e);
    }
    const float twoPowK = std::bit_cast<float>(static_cast<std::uint32_t>(0x7f + k) << 23);
    const float oneMinusTwoPowMinusK = std::bit_cast<float>(0x3f800000u - (0x1000000u >> k));
    return (oneMinusTwoPowMinusK - (e - x)) * twoPowK;
}

volatile float AcosfPio2Low = 7.5497894159e-08f;

float AcosfRational(float z) {
    constexpr float P0 = 1.6666586697e-01f, P1 = -4.2743422091e-02f, P2 = -8.6563630030e-03f, Q1 = -7.0662963390e-01f;
    return z * (P0 + z * (P1 + z * P2)) / (1.0f + z * Q1);
}

float SinhfExp(float x) {
    constexpr float Ln2High = 6.9314575195e-01f, Ln2Low = 1.4286067653e-06f, InverseLn2 = 1.4426950216e+00f;
    constexpr float P1 = 1.6666625440e-1f, P2 = -2.7667332906e-3f;
    const int k = static_cast<int>(InverseLn2 * x + 0.5f);
    const float kFloat = static_cast<float>(k);
    const float high = x - kFloat * Ln2High;
    const float low = kFloat * Ln2Low;
    x = high - low;
    const float t = x * x;
    const float c = x - t * (P1 + t * P2);
    const float y = 1.0f - ((low - (x * c) / (2.0f - c)) - high);
    if (k == 128) return y * 2.0f * 0x1p127f;
    return y * std::bit_cast<float>(static_cast<std::uint32_t>(0x7f + k) << 23);
}

float GuestTgammaf(float x) {
    if (std::isgreater(static_cast<double>(x), 171.63)) {
        static volatile double zero = 0.0;
        return static_cast<float>(static_cast<double>(x) / zero);
    }
    if (std::isfinite(x) && std::isless(static_cast<double>(x), -190.0) && std::trunc(x) != x) {
        const double interval = std::floor(-static_cast<double>(x));
        return std::fmod(interval, 2.0) == 0.0 ? -0.0f : 0.0f;
    }
    const int savedErrno = errno;
    const float result = static_cast<float>(std::tgamma(static_cast<double>(x)));
    errno = std::isfinite(x) && x < 0.0f && std::trunc(x) == x ? EDOM : savedErrno;
    if (std::isnan(result) && !std::isnan(x)) return std::bit_cast<float>(0xffc00000u);
    if (result != 0.0f || !std::isfinite(x) || x >= 0.0f || std::trunc(x) == x) return result;
    const double interval = std::floor(-static_cast<double>(x));
    const bool negative = std::fmod(interval, 2.0) == 0.0;
    return std::copysign(0.0f, negative ? -1.0f : 1.0f);
}

float AtanhfLog1p(float x) {
    constexpr float Ln2High = 6.9313812256e-01f, Ln2Low = 9.0580006145e-06f;
    constexpr float Lp1 = 6.6666668653e-01f, Lp2 = 4.0000000596e-01f, Lp3 = 2.8571429849e-01f, Lp4 = 2.2222198546e-01f;
    constexpr float Lp5 = 1.8183572590e-01f, Lp6 = 1.5313838422e-01f, Lp7 = 1.4798198640e-01f;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    if (bits < 0x38000000u) return bits < 0x33800000u ? x : x - x * x * 0.5f;
    int k = 0;
    float f = x;
    float c = 0.0f;
    std::uint32_t mantissa = 1;
    if (bits >= 0x3ed413d0u) {
        const float u = 1.0f + x;
        const std::uint32_t uBits = std::bit_cast<std::uint32_t>(u);
        k = static_cast<int>(uBits >> 23) - 127;
        c = (k > 0 ? 1.0f - (u - x) : x - (u - 1.0f)) / u;
        mantissa = uBits & 0x007fffffu;
        if (mantissa < 0x3504f4u) {
            f = std::bit_cast<float>(mantissa | 0x3f800000u) - 1.0f;
        } else {
            ++k;
            f = std::bit_cast<float>(mantissa | 0x3f000000u) - 1.0f;
            mantissa = (0x00800000u - mantissa) >> 2;
        }
    }
    const float halfSquare = 0.5f * f * f;
    const float kFloat = static_cast<float>(k);
    if (mantissa == 0) {
        if (f == 0.0f) return kFloat * Ln2High + (c + kFloat * Ln2Low);
        const float r = halfSquare * (1.0f - 0.66666666666666666f * f);
        return kFloat * Ln2High - ((r - (kFloat * Ln2Low + c)) - f);
    }
    const float s = f / (2.0f + f);
    const float z = s * s;
    const float r = z * (Lp1 + z * (Lp2 + z * (Lp3 + z * (Lp4 + z * (Lp5 + z * (Lp6 + z * Lp7))))));
    if (k == 0) return f - (halfSquare - s * (halfSquare + r));
    return kFloat * Ln2High - ((halfSquare - (s * (halfSquare + r) + (kFloat * Ln2Low + c))) - f);
}

static_assert(std::numeric_limits<long double>::digits == 64, "guest long double is the x87 80-bit format");

struct ExtendedParts {
    std::uint64_t mantissa;
    std::uint16_t signExponent;
};

template <typename TValue, typename THypot>
TValue Hypot3(TValue x, TValue y, TValue z, THypot hypot) {
    if (std::isnan(x)) return x;
    if (std::isnan(y)) return y;
    if (std::isnan(z)) return z;
    if (std::isinf(x) || std::isinf(y) || std::isinf(z)) return std::numeric_limits<TValue>::infinity();
    if (x == 0) return hypot(y, z);
    if (y == 0) return hypot(x, z);
    if (z == 0) return hypot(x, y);
    x = std::fabs(x);
    y = std::fabs(y);
    z = std::fabs(z);
    if (z >= x && z >= y) return hypot(z, hypot(x, y));
    if (y >= x && y >= z) return hypot(y, hypot(x, z));
    return hypot(x, hypot(y, z));
}

ExtendedParts SplitExtended(long double value) {
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(value));
    ExtendedParts parts;
    std::memcpy(&parts.mantissa, bytes, sizeof(parts.mantissa));
    std::memcpy(&parts.signExponent, bytes + sizeof(parts.mantissa), sizeof(parts.signExponent));
    return parts;
}

long double JoinExtended(std::uint64_t mantissa, std::uint16_t signExponent) {
    unsigned char bytes[sizeof(long double)] = {};
    std::memcpy(bytes, &mantissa, sizeof(mantissa));
    std::memcpy(bytes + sizeof(mantissa), &signExponent, sizeof(signExponent));
    long double value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

long double ClearLowMantissaWord(long double value) {
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(value));
    std::memset(bytes, 0, sizeof(std::uint32_t));
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

long double SumAtCallerPrecision(long double high, long double low) {
    volatile long double sum = high;
    return sum + low;
}

constexpr long double AsinNumeratorCoefficients[] = {
    1.66666666666666666631e-01L, -4.16313987993683104320e-01L, 3.69068046323246813704e-01L, -1.36213932016738603108e-01L,
    1.78324189708471965733e-02L, -2.19216428382605211588e-04L, -7.10526623669075243183e-06L,
};
constexpr long double AsinDenominatorCoefficients[] = {
    -2.94788392796209867269e+00L, 3.27309890266528636716e+00L, -1.68285799854822427013e+00L, 3.90699412641738801874e-01L,
    -3.14365703596053263322e-02L,
};
constexpr long double HalfPiHigh = 1.57079632679489661926e+00L;
constexpr long double HalfPiLow = -2.50827880633416601173e-20L;
constexpr long double QuarterPiHigh = 7.85398163397448309628e-01L;
const volatile long double Pi = 3.14159265358979323846264338327950280e+00L;

long double AsinNumerator(long double t) {
    const auto& p = AsinNumeratorCoefficients;
    return t * (p[0] + t * (p[1] + t * (p[2] + t * (p[3] + t * (p[4] + t * (p[5] + t * p[6]))))));
}

long double AsinDenominator(long double t) {
    const auto& q = AsinDenominatorCoefficients;
    return 1.0L + t * (q[0] + t * (q[1] + t * (q[2] + t * (q[3] + t * q[4]))));
}

template<typename T>
bool NanWithoutException(T value) {
    if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return (bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    } else if constexpr (sizeof(T) == sizeof(std::uint64_t)) {
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return (bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    } else {
        static_assert(std::numeric_limits<T>::digits == 64);
        std::uint64_t significand;
        std::uint16_t signAndExponent;
        std::memcpy(&significand, &value, sizeof(significand));
        std::memcpy(&signAndExponent, reinterpret_cast<const unsigned char*>(&value) + sizeof(significand), sizeof(signAndExponent));
        return (signAndExponent & 0x7fff) == 0x7fff && (significand & UINT64_C(0x7fffffffffffffff)) != 0;
    }
}

template<typename T>
T GuestFmin(T x, T y) {
    if (NanWithoutException(x))
        return y;
    if (NanWithoutException(y))
        return x;
    if (std::signbit(x) != std::signbit(y))
        return std::signbit(x) ? x : y;
    return x < y ? x : y;
}

template<typename T>
T GuestFmax(T x, T y) {
    if (NanWithoutException(x))
        return y;
    if (NanWithoutException(y))
        return x;
    if (std::signbit(x) != std::signbit(y))
        return std::signbit(x) ? y : x;
    return x < y ? y : x;
}

float GuestBesselSecondKindZero() {
    return -std::numeric_limits<float>::infinity();
}

float ScaledSine(float x, float y) {
    const float magnitude = std::fabs(x);
    float result;
    if (magnitude < 0x1p-12f) {
        result = y == 1.0f ? magnitude : magnitude * y;
    } else if (magnitude < 0x1.193ea8p-1f) {
        const float z = magnitude * magnitude;
        const float denominator = (z * 0x1.e256c4p-10f + -0x1.f401b8p-5f) * z + 1.0f;
        result = (magnitude + magnitude * z * 0x1.b0a9d8p-4f) / denominator * y;
    } else if (magnitude < 4.0f) {
        const float rising = expm1f_nid_postfix(magnitude);
        result = (expm1f_nid_postfix(-magnitude) * (rising * 0.5f) + rising) * y;
    } else if (magnitude < 7.7f) {
        const float rising = expm1f_nid_postfix(magnitude);
        result = (expm1f_nid_postfix(-magnitude) * 0.5f + 1.0f) * rising * y;
    } else {
        result = magnitude;
        _FExp_nid_postfix(&result, y, -1);
    }
    return x < 0.0f ? -result : result;
}

float ScaledHyperbolic(float x, float y, bool sine) {
    const auto xBits = std::bit_cast<std::uint32_t>(x);
    const auto xMagnitude = xBits & 0x7fffffffu;
    const auto yMagnitude = std::bit_cast<std::uint32_t>(y) & 0x7fffffffu;
    constexpr std::uint32_t infinity = 0x7f800000u;
    if (xMagnitude > infinity) return x;
    if (yMagnitude > infinity) return y;
    if ((xMagnitude == infinity && yMagnitude == 0) || (sine && xMagnitude == 0 && yMagnitude == infinity)) {
        errno = EDOM;
        std::feraiseexcept(FE_INVALID);
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (xMagnitude == infinity) return sine ? x * y : std::copysign(std::numeric_limits<float>::infinity(), y);
    if (xMagnitude == 0 || yMagnitude == 0 || yMagnitude == infinity) return sine ? x * y : y;
    if (sine) return ScaledSine(x, y);
    const float magnitude = std::bit_cast<float>(xMagnitude);
    const bool exponential = magnitude >= 8.675f;
    float result = magnitude;
    _FExp_nid_postfix(&result, exponential ? y : 1.0f, -1);
    return exponential ? result : y * (result + 0.25f / result);
}

}

namespace {

std::lldiv_t wrappingLldiv(long long numerator, long long denominator) {
    if (denominator == -1) return {static_cast<long long>(0ull - static_cast<unsigned long long>(numerator)), 0};
    return std::lldiv(numerator, denominator);
}

}


std::lldiv_t APS5_VABI lldiv_nid_postfix(long long numerator, long long denominator) {
    return wrappingLldiv(numerator, denominator);
}

std::lldiv_t APS5_VABI ldiv_nid_postfix(std::int64_t numerator, std::int64_t denominator) {
    return wrappingLldiv(numerator, denominator);
}

std::lldiv_t APS5_VABI imaxdiv_nid_postfix(std::int64_t numerator, std::int64_t denominator) {
    return wrappingLldiv(numerator, denominator);
}

std::int64_t APS5_VABI labs_nid_postfix(std::int64_t x) {
    return x < 0 ? static_cast<std::int64_t>(0u - static_cast<std::uint64_t>(x)) : x;
}

std::int64_t APS5_VABI llabs_nid_postfix(std::int64_t x) {
    return labs_nid_postfix(x);
}

std::int64_t APS5_VABI imaxabs_nid_postfix(std::int64_t x) {
    return labs_nid_postfix(x);
}

float APS5_VABI fmodf_nid_postfix(float x, float y) {
    const std::uint32_t ix = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    const std::uint32_t iy = std::bit_cast<std::uint32_t>(y) & 0x7fffffffu;
    if (iy == 0 || ix >= 0x7f800000u || iy > 0x7f800000u) {
        _Feraise_nid_postfix(0x01);
        if (iy > 0x7f800000u) {
            return y + y;
        }
        if (ix > 0x7f800000u) {
            return x + x;
        }
        return (x * y) / (x * y);
    }
    const int saved_errno = errno;
    const float result = std::fmod(x, y);
    errno = saved_errno;
    return result;
}
static float AsinfRational(float t) {
    const float p = t * (1.6666586697e-01f + t * (-4.2743422091e-02f + t * -8.6563630030e-03f));
    const float q = 1.0f + t * -7.0662963390e-01f;
    return p / q;
}

float APS5_VABI asinf_nid_postfix(float x) {
    const double pio2 = 1.570796326794896558e+00;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x3f800000) {
        if (ix == 0x3f800000) {
            return static_cast<float>(x * pio2);
        }
        _Feraise_nid_postfix(0x01);
        return (x - x) / (x - x);
    }
    if (ix < 0x3f000000) {
        if (ix < 0x39800000) {
            return x;
        }
        return x + x * AsinfRational(x * x);
    }
    const float t = (1.0f - std::fabs(x)) * 0.5f;
    const double s = std::sqrt(static_cast<double>(t));
    const float result = static_cast<float>(pio2 - 2.0 * (s + s * AsinfRational(t)));
    return hx > 0 ? result : -result;
}
float APS5_VABI acosf_nid_postfix(float x) {
    if (std::isnan(x)) {
        _Feraise_nid_postfix(0x01);
        return std::bit_cast<float>(std::bit_cast<std::uint32_t>(x) | 0x00400000u);
    }
    if (std::fabs(x) > 1.0f) {
        _Feraise_nid_postfix(0x01);
        return std::bit_cast<float>(0xffc00000u);
    }
    constexpr float Pi = 3.1415925026e+00f, Pio2High = 1.5707962513e+00f;
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude == 0x3f800000u) return std::signbit(x) ? Pi + 2.0f * AcosfPio2Low : 0.0f;
    if (magnitude < 0x3f000000u) {
        if (magnitude <= 0x32800000u) return Pio2High + AcosfPio2Low;
        return Pio2High - (x - (AcosfPio2Low - x * AcosfRational(x * x)));
    }
    if (std::signbit(x)) {
        const float z = (1.0f + x) * 0.5f;
        const float s = std::sqrt(z);
        return Pi - 2.0f * (s + (AcosfRational(z) * s - AcosfPio2Low));
    }
    const float z = (1.0f - x) * 0.5f;
    const float s = std::sqrt(z);
    const float high = std::bit_cast<float>(std::bit_cast<std::uint32_t>(s) & 0xfffff000u);
    const float correction = (z - high * high) / (s + high);
    return 2.0f * (high + (AcosfRational(z) * s + correction));
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_hypotf.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_hypotf.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
float APS5_VABI hypotf_nid_postfix(float x, float y) {
    const auto word = [](float value) { return static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)); };
    const auto fromWord = [](std::int32_t value) { return std::bit_cast<float>(static_cast<std::uint32_t>(value)); };
    std::int32_t ha = word(x) & 0x7fffffff;
    std::int32_t hb = word(y) & 0x7fffffff;
    float a = x;
    float b = y;
    if (hb > ha) {
        std::swap(a, b);
        std::swap(ha, hb);
    }
    a = std::fabs(a);
    b = std::fabs(b);
    if (ha - hb > 0xf000000) {
        return a + b;
    }
    int k = 0;
    if (ha > 0x58800000) {
        if (ha >= 0x7f800000) {
            float w = std::fabs(x + 0.0f) - std::fabs(y + 0.0f);
            if (ha == 0x7f800000) {
                w = a;
            }
            if (hb == 0x7f800000) {
                w = b;
            }
            return w;
        }
        ha -= 0x22000000;
        hb -= 0x22000000;
        k += 68;
        a = fromWord(ha);
        b = fromWord(hb);
    }
    if (hb < 0x26800000) {
        if (hb <= 0x007fffff) {
            if (hb == 0) {
                return a;
            }
            b *= 0x1p126f;
            a *= 0x1p126f;
            k -= 126;
        } else {
            ha += 0x22000000;
            hb += 0x22000000;
            k -= 68;
            a = fromWord(ha);
            b = fromWord(hb);
        }
    }
    float w = a - b;
    if (w > b) {
        const float t1 = fromWord(ha & static_cast<std::int32_t>(0xfffff000));
        const float t2 = a - t1;
        w = std::sqrt(t1 * t1 - (b * (-b) - t2 * (a + t1)));
    } else {
        a = a + a;
        const float y1 = fromWord(hb & static_cast<std::int32_t>(0xfffff000));
        const float y2 = b - y1;
        const float t1 = fromWord((ha + 0x00800000) & static_cast<std::int32_t>(0xfffff000));
        const float t2 = a - t1;
        w = std::sqrt(t1 * y1 - (w * (-w) - (t1 * y2 + t2 * b)));
    }
    if (k != 0) {
        return fromWord((127 + k) << 23) * w;
    }
    return w;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_hypot.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_hypot.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */
double APS5_VABI hypot_nid_postfix(double x, double y) {
    const auto highWord = [](double value) { return static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(value) >> 32); };
    const auto lowWord = [](double value) { return static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(value)); };
    const auto withHighWord = [](double value, std::int32_t high) {
        return std::bit_cast<double>((std::bit_cast<std::uint64_t>(value) & 0xffffffffull) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(high)) << 32));
    };
    std::int32_t ha = highWord(x) & 0x7fffffff;
    std::int32_t hb = highWord(y) & 0x7fffffff;
    double a = x;
    double b = y;
    if (hb > ha) {
        std::swap(a, b);
        std::swap(ha, hb);
    }
    a = std::fabs(a);
    b = std::fabs(b);
    if (ha - hb > 0x3c00000) {
        return a + b;
    }
    int k = 0;
    if (ha > 0x5f300000) {
        if (ha >= 0x7ff00000) {
            double w = std::fabs(x + 0.0) - std::fabs(y + 0.0);
            if (((static_cast<std::uint32_t>(ha) & 0xfffff) | lowWord(a)) == 0) {
                w = a;
            }
            if (((static_cast<std::uint32_t>(hb) ^ 0x7ff00000) | lowWord(b)) == 0) {
                w = b;
            }
            return w;
        }
        ha -= 0x25800000;
        hb -= 0x25800000;
        k += 600;
        a = withHighWord(a, ha);
        b = withHighWord(b, hb);
    }
    if (hb < 0x20b00000) {
        if (hb <= 0x000fffff) {
            if ((static_cast<std::uint32_t>(hb) | lowWord(b)) == 0) {
                return a;
            }
            b *= 0x1p1022;
            a *= 0x1p1022;
            k -= 1022;
        } else {
            ha += 0x25800000;
            hb += 0x25800000;
            k -= 600;
            a = withHighWord(a, ha);
            b = withHighWord(b, hb);
        }
    }
    double w = a - b;
    if (w > b) {
        const double t1 = withHighWord(0.0, ha);
        const double t2 = a - t1;
        w = std::sqrt(t1 * t1 - (b * (-b) - t2 * (a + t1)));
    } else {
        a = a + a;
        const double y1 = withHighWord(0.0, hb);
        const double y2 = b - y1;
        const double t1 = withHighWord(0.0, ha + 0x00100000);
        const double t2 = a - t1;
        w = std::sqrt(t1 * y1 - (w * (-w) - (t1 * y2 + t2 * b)));
    }
    if (k != 0) {
        return withHighWord(0.0, (1023 + k) << 20) * w;
    }
    return w;
}
/* Adapted from FreeBSD release/9.0.0 lib/msun/src/e_log10f.c: https://github.com/freebsd/freebsd-src/blob/release/9.0.0/lib/msun/src/e_log10f.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
static bool Log9Reduce(float& x, std::int32_t& k, float& special) {
    static volatile float zero = 0.0f;
    const float two25 = static_cast<float>(3.3554432000e+07);
    k = 0;
    std::int32_t hx = std::bit_cast<std::int32_t>(x);
    if (hx < 0x00800000) {
        if ((hx & 0x7fffffff) == 0) {
            special = -std::numeric_limits<float>::infinity();
            return false;
        }
        if (hx < 0) {
            _Feraise_nid_postfix(0x01);
            special = (x - x) / zero;
            return false;
        }
        k -= 25;
        x *= two25;
        hx = std::bit_cast<std::int32_t>(x);
    }
    if (hx >= 0x7f800000) {
        special = x + x;
        return false;
    }
    k += (hx >> 23) - 127;
    hx &= 0x007fffff;
    const std::int32_t i = (hx + 0x4afb0d) & 0x800000;
    x = std::bit_cast<float>(hx | (i ^ 0x3f800000));
    k += i >> 23;
    return true;
}

/* Adapted from FreeBSD release/9.0.0 lib/msun/src/k_logf.h: https://github.com/freebsd/freebsd-src/blob/release/9.0.0/lib/msun/src/k_logf.h */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
static float Log9Kernel(float x) {
    const float Lg1 = 0xaaaaaa.0p-24f;
    const float Lg2 = 0xccce13.0p-25f;
    const float Lg3 = 0x91e9ee.0p-25f;
    const float Lg4 = 0xf89e26.0p-26f;
    std::int32_t ix = std::bit_cast<std::int32_t>(x);
    const float f = x - 1.0f;
    if ((0x007fffff & (0x8000 + ix)) < 0xc000) {
        if (f == 0.0f) {
            return 0.0f;
        }
        return f * f * (static_cast<float>(0.33333333333333333) * f - 0.5f);
    }
    const float s = f / (2.0f + f);
    const float z = s * s;
    ix &= 0x007fffff;
    std::int32_t i = ix - (0x6147a << 3);
    const float w = z * z;
    const std::int32_t j = (0x6b851 << 3) - ix;
    const float t1 = w * (Lg2 + w * Lg4);
    const float t2 = z * (Lg1 + w * Lg3);
    i |= j;
    const float R = t2 + t1;
    if (i > 0) {
        const float hfsq = 0.5f * f * f;
        return s * (hfsq + R) - hfsq;
    }
    return s * (R - f);
}

float APS5_VABI log10f_nid_postfix(float x) {
    const float ivln10hi = static_cast<float>(4.3432617188e-01);
    const float ivln10lo = static_cast<float>(-3.1689971365e-05);
    const float log10_2hi = static_cast<float>(3.0102920532e-01);
    const float log10_2lo = static_cast<float>(7.9034151668e-07);
    std::int32_t k;
    float special;
    if (!Log9Reduce(x, k, special)) {
        return special;
    }
    const float y = static_cast<float>(k);
    const float f = Log9Kernel(x);
    x = x - 1.0f;
    const float hi = std::bit_cast<float>(std::bit_cast<std::int32_t>(x) & static_cast<std::int32_t>(0xfffff000));
    const float lo = x - hi;
    const float z = y * log10_2lo + (x + f) * ivln10lo + (lo + f) * ivln10hi + hi * ivln10hi;
    return z + y * log10_2hi;
}
float APS5_VABI logbf_nid_postfix(float x) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (bits == 0) {
        return -1.0f / std::fabs(x);
    }
    if (bits >= 0x7f800000u) {
        return x * x;
    }
    if (bits < 0x00800000u) {
        const std::uint32_t scaled = std::bit_cast<std::uint32_t>(x * 0x1p25f) & 0x7fffffffu;
        return static_cast<float>(static_cast<int>(scaled >> 23) - 127 - 25);
    }
    return static_cast<float>(static_cast<int>(bits >> 23) - 127);
}
float APS5_VABI exp2f_nid_postfix(float x) {
    static constexpr double exp2ft[16] = {
        0x1.6a09e667f3bcdp-1, 0x1.7a11473eb0187p-1, 0x1.8ace5422aa0dbp-1, 0x1.9c49182a3f090p-1,
        0x1.ae89f995ad3adp-1, 0x1.c199bdd85529cp-1, 0x1.d5818dcfba487p-1, 0x1.ea4afa2a490dap-1,
        0x1.0000000000000p+0, 0x1.0b5586cf9890fp+0, 0x1.172b83c7d517bp+0, 0x1.2387a6e756238p+0,
        0x1.306fe0a31b715p+0, 0x1.3dea64c123422p+0, 0x1.4bfdad5362a27p+0, 0x1.5ab07dd485429p+0,
    };
    static volatile float huge = 0x1p100f;
    static volatile float twom100 = 0x1p-100f;
    const float redux = 0x1.8p23f / 16;
    const float P1 = 0x1.62e430p-1f;
    const float P2 = 0x1.ebfbe0p-3f;
    const float P3 = 0x1.c6b348p-5f;
    const float P4 = 0x1.3b2c9cp-7f;
    const std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t ix = hx & 0x7fffffff;
    if (ix >= 0x43000000) {
        if (ix >= 0x7f800000) {
            return (ix & 0x7fffff) != 0 || (hx & 0x80000000) == 0 ? x + x : 0.0f;
        }
        if (x >= 0x1.0p7f) {
            errno = ERANGE;
            return huge * huge;
        }
        if (x <= -0x1.2cp7f) {
            errno = ERANGE;
            return twom100 * twom100;
        }
    } else if (ix <= 0x33000000) {
        return 1.0f + x;
    }
    volatile float rounded = x + redux;
    float t = rounded;
    std::uint32_t i0 = std::bit_cast<std::uint32_t>(t) + 8;
    const std::int32_t k = static_cast<std::int32_t>(i0 >> 4) << 20;
    i0 &= 15;
    t -= redux;
    const double z = x - t;
    const double twopk = std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(0x3ff00000 + k)) << 32);
    const double u = exp2ft[i0] * z;
    const double tv = exp2ft[i0] + u * (P1 + z * P2) + u * (z * z) * (P3 + z * P4);
    return static_cast<float>(tv * twopk);
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_exp2.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_exp2.c */
/*-
 * Copyright (c) 2005 David Schultz <das@FreeBSD.ORG>
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
}
