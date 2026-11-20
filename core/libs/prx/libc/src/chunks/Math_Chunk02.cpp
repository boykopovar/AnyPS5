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


double APS5_VABI exp2_nid_postfix(double x) {
    static constexpr double tbl[512] = {
        0x1.6a09e667f3d5dp-1, 0x1.9880p-44, 0x1.6b052fa751744p-1, 0x1.8000p-50,
        0x1.6c012750bd9fep-1, -0x1.8780p-45, 0x1.6cfdcddd476bfp-1, 0x1.ec00p-46,
        0x1.6dfb23c651a29p-1, -0x1.8000p-50, 0x1.6ef9298593ae3p-1, -0x1.c000p-52,
        0x1.6ff7df9519386p-1, -0x1.fd80p-45, 0x1.70f7466f42da3p-1, -0x1.c880p-45,
        0x1.71f75e8ec5fc3p-1, 0x1.3c00p-46, 0x1.72f8286eacf05p-1, -0x1.8300p-44,
        0x1.73f9a48a58152p-1, -0x1.0c00p-47, 0x1.74fbd35d7ccfcp-1, 0x1.f880p-45,
        0x1.75feb564267f1p-1, 0x1.3e00p-47, 0x1.77024b1ab6d48p-1, -0x1.7d00p-45,
        0x1.780694fde5d38p-1, -0x1.d000p-50, 0x1.790b938ac1d00p-1, 0x1.3000p-49,
        0x1.7a11473eb0178p-1, -0x1.d000p-49, 0x1.7b17b0976d060p-1, 0x1.0400p-45,
        0x1.7c1ed0130c133p-1, 0x1.0000p-53, 0x1.7d26a62ff8636p-1, -0x1.6900p-45,
        0x1.7e2f336cf4e3bp-1, -0x1.2e00p-47, 0x1.7f3878491c3e8p-1, -0x1.4580p-45,
        0x1.80427543e1b4ep-1, 0x1.3000p-44, 0x1.814d2add1071ap-1, 0x1.f000p-47,
        0x1.82589994ccd7ep-1, -0x1.1c00p-45, 0x1.8364c1eb942d0p-1, 0x1.9d00p-45,
        0x1.8471a4623cab5p-1, 0x1.7100p-43, 0x1.857f4179f5bbcp-1, 0x1.2600p-45,
        0x1.868d99b4491afp-1, -0x1.2c40p-44, 0x1.879cad931a395p-1, -0x1.3000p-45,
        0x1.88ac7d98a65b8p-1, -0x1.a800p-45, 0x1.89bd0a4785800p-1, -0x1.d000p-49,
        0x1.8ace5422aa223p-1, 0x1.3280p-44, 0x1.8be05bad619fap-1, 0x1.2b40p-43,
        0x1.8cf3216b54383p-1, -0x1.ed00p-45, 0x1.8e06a5e08664cp-1, -0x1.0500p-45,
        0x1.8f1ae99157807p-1, 0x1.8280p-45, 0x1.902fed0282c0ep-1, -0x1.cb00p-46,
        0x1.9145b0b91ff96p-1, -0x1.5e00p-47, 0x1.925c353aa2ff9p-1, 0x1.5400p-48,
        0x1.93737b0cdc64ap-1, 0x1.7200p-46, 0x1.948b82b5f98aep-1, -0x1.9000p-47,
        0x1.95a44cbc852cbp-1, 0x1.5680p-45, 0x1.96bdd9a766f21p-1, -0x1.6d00p-44,
        0x1.97d829fde4e2ap-1, -0x1.1000p-47, 0x1.98f33e47a23a3p-1, 0x1.d000p-45,
        0x1.9a0f170ca0604p-1, -0x1.8a40p-44, 0x1.9b2bb4d53ff89p-1, 0x1.55c0p-44,
        0x1.9c49182a3f15bp-1, 0x1.6b80p-45, 0x1.9d674194bb8c5p-1, -0x1.c000p-49,
        0x1.9e86319e3238ep-1, 0x1.7d00p-46, 0x1.9fa5e8d07f302p-1, 0x1.6400p-46,
        0x1.a0c667b5de54dp-1, -0x1.5000p-48, 0x1.a1e7aed8eb8f6p-1, 0x1.9e00p-47,
        0x1.a309bec4a2e27p-1, 0x1.ad80p-45, 0x1.a42c980460a5dp-1, -0x1.af00p-46,
        0x1.a5503b23e259bp-1, 0x1.b600p-47, 0x1.a674a8af46213p-1, 0x1.8880p-44,
        0x1.a799e1330b3a7p-1, 0x1.1200p-46, 0x1.a8bfe53c12e8dp-1, 0x1.6c00p-47,
        0x1.a9e6b5579fcd2p-1, -0x1.9b80p-45, 0x1.ab0e521356fb8p-1, 0x1.b700p-45,
        0x1.ac36bbfd3f381p-1, 0x1.9000p-50, 0x1.ad5ff3a3c2780p-1, 0x1.4000p-49,
        0x1.ae89f995ad2a3p-1, -0x1.c900p-45, 0x1.afb4ce622f367p-1, 0x1.6500p-46,
        0x1.b0e07298db790p-1, 0x1.fd40p-45, 0x1.b20ce6c9a89a9p-1, 0x1.2700p-46,
        0x1.b33a2b84f1a4bp-1, 0x1.d470p-43, 0x1.b468415b747e7p-1, -0x1.8380p-44,
        0x1.b59728de5593ap-1, 0x1.8000p-54, 0x1.b6c6e29f1c56ap-1, 0x1.ad00p-47,
        0x1.b7f76f2fb5e50p-1, 0x1.e800p-50, 0x1.b928cf22749b2p-1, -0x1.4c00p-47,
        0x1.ba5b030a10603p-1, -0x1.d700p-47, 0x1.bb8e0b79a6f66p-1, 0x1.d900p-47,
        0x1.bcc1e904bc1ffp-1, 0x1.2a00p-47, 0x1.bdf69c3f3a16fp-1, -0x1.f780p-46,
        0x1.bf2c25bd71db8p-1, -0x1.0a00p-46, 0x1.c06286141b2e9p-1, -0x1.1400p-46,
        0x1.c199bdd8552e0p-1, 0x1.be00p-47, 0x1.c2d1cd9fa64eep-1, -0x1.9400p-47,
        0x1.c40ab5fffd02fp-1, -0x1.ed00p-47, 0x1.c544778fafd15p-1, 0x1.9660p-44,
        0x1.c67f12e57d0cbp-1, -0x1.a100p-46, 0x1.c7ba88988c1b6p-1, -0x1.8458p-42,
        0x1.c8f6d9406e733p-1, -0x1.a480p-46, 0x1.ca3405751c4dfp-1, 0x1.b000p-51,
        0x1.cb720dcef9094p-1, 0x1.1400p-47, 0x1.ccb0f2e6d1689p-1, 0x1.0200p-48,
        0x1.cdf0b555dc412p-1, 0x1.3600p-48, 0x1.cf3155b5bab3bp-1, -0x1.6900p-47,
        0x1.d072d4a0789bcp-1, 0x1.9a00p-47, 0x1.d1b532b08c8fap-1, -0x1.5e00p-46,
        0x1.d2f87080d8a85p-1, 0x1.d280p-46, 0x1.d43c8eacaa203p-1, 0x1.1a00p-47,
        0x1.d5818dcfba491p-1, 0x1.f000p-50, 0x1.d6c76e862e6a1p-1, -0x1.3a00p-47,
        0x1.d80e316c9834ep-1, -0x1.cd80p-47, 0x1.d955d71ff6090p-1, 0x1.4c00p-48,
        0x1.da9e603db32aep-1, 0x1.f900p-48, 0x1.dbe7cd63a8325p-1, 0x1.9800p-49,
        0x1.dd321f301b445p-1, -0x1.5200p-48, 0x1.de7d5641c05bfp-1, -0x1.d700p-46,
        0x1.dfc97337b9aecp-1, -0x1.6140p-46, 0x1.e11676b197d5ep-1, 0x1.b480p-47,
        0x1.e264614f5a3e7p-1, 0x1.0ce0p-43, 0x1.e3b333b16ee5cp-1, 0x1.c680p-47,
        0x1.e502ee78b3fb4p-1, -0x1.9300p-47, 0x1.e653924676d68p-1, -0x1.5000p-49,
        0x1.e7a51fbc74c44p-1, -0x1.7f80p-47, 0x1.e8f7977cdb726p-1, -0x1.3700p-48,
        0x1.ea4afa2a490e8p-1, 0x1.5d00p-49, 0x1.eb9f4867ccae4p-1, 0x1.61a0p-46,
        0x1.ecf482d8e680dp-1, 0x1.5500p-48, 0x1.ee4aaa2188514p-1, 0x1.6400p-51,
        0x1.efa1bee615a13p-1, -0x1.e800p-49, 0x1.f0f9c1cb64106p-1, -0x1.a880p-48,
        0x1.f252b376bb963p-1, -0x1.c900p-45, 0x1.f3ac948dd7275p-1, 0x1.a000p-53,
        0x1.f50765b6e4524p-1, -0x1.4f00p-48, 0x1.f6632798844fdp-1, 0x1.a800p-51,
        0x1.f7bfdad9cbe38p-1, 0x1.abc0p-48, 0x1.f91d802243c82p-1, -0x1.4600p-50,
        0x1.fa7c1819e908ep-1, -0x1.b0c0p-47, 0x1.fbdba3692d511p-1, -0x1.0e00p-51,
        0x1.fd3c22b8f7194p-1, -0x1.0de8p-46, 0x1.fe9d96b2a23eep-1, 0x1.e430p-49,
        0x1.0000000000000p+0, 0.0, 0x1.00b1afa5abcbep+0, -0x1.3400p-52,
        0x1.0163da9fb3303p+0, -0x1.2170p-46, 0x1.02168143b0282p+0, 0x1.a400p-52,
        0x1.02c9a3e77806cp+0, 0x1.f980p-49, 0x1.037d42e11bbcap+0, -0x1.7400p-51,
        0x1.04315e86e7f89p+0, 0x1.8300p-50, 0x1.04e5f72f65467p+0, -0x1.a3f0p-46,
        0x1.059b0d315855ap+0, -0x1.2840p-47, 0x1.0650a0e3c1f95p+0, 0x1.1600p-48,
        0x1.0706b29ddf71ap+0, 0x1.5240p-46, 0x1.07bd42b72a82dp+0, -0x1.9a00p-49,
        0x1.0874518759bd0p+0, 0x1.6400p-49, 0x1.092bdf66607c8p+0, -0x1.0780p-47,
        0x1.09e3ecac6f383p+0, -0x1.8000p-54, 0x1.0a9c79b1f3930p+0, 0x1.fa00p-48,
        0x1.0b5586cf988fcp+0, -0x1.ac80p-48, 0x1.0c0f145e46c8ap+0, 0x1.9c00p-50,
        0x1.0cc922b724816p+0, 0x1.5200p-47, 0x1.0d83b23395dd8p+0, -0x1.ad00p-48,
        0x1.0e3ec32d3d1f3p+0, 0x1.bac0p-46, 0x1.0efa55fdfa9a6p+0, -0x1.4e80p-47,
        0x1.0fb66affed2f0p+0, -0x1.d300p-47, 0x1.1073028d7234bp+0, 0x1.1500p-48,
        0x1.11301d0125b5bp+0, 0x1.c000p-49, 0x1.11edbab5e2af9p+0, 0x1.6bc0p-46,
        0x1.12abdc06c31d5p+0, 0x1.8400p-49, 0x1.136a814f2047dp+0, -0x1.ed00p-47,
        0x1.1429aaea92de9p+0, 0x1.8e00p-49, 0x1.14e95934f3138p+0, 0x1.b400p-49,
        0x1.15a98c8a58e71p+0, 0x1.5300p-47, 0x1.166a45471c3dfp+0, 0x1.3380p-47,
        0x1.172b83c7d5211p+0, 0x1.8d40p-45, 0x1.17ed48695bb9fp+0, -0x1.5d00p-47,
        0x1.18af9388c8d93p+0, -0x1.c880p-46, 0x1.1972658375d66p+0, 0x1.1f00p-46,
        0x1.1a35beb6fcba7p+0, 0x1.0480p-46, 0x1.1af99f81387e3p+0, -0x1.7390p-43,
        0x1.1bbe084045d54p+0, 0x1.4e40p-45, 0x1.1c82f95281c43p+0, -0x1.a200p-47,
        0x1.1d4873168b9b2p+0, 0x1.3800p-49, 0x1.1e0e75eb44031p+0, 0x1.ac00p-49,
        0x1.1ed5022fcd938p+0, 0x1.1900p-47, 0x1.1f9c18438cdf7p+0, -0x1.b780p-46,
        0x1.2063b88628d8fp+0, 0x1.d940p-45, 0x1.212be3578a81ep+0, 0x1.8000p-50,
        0x1.21f49917ddd41p+0, 0x1.b340p-45, 0x1.22bdda2791323p+0, 0x1.9f80p-46,
        0x1.2387a6e7561e7p+0, -0x1.9c80p-46, 0x1.2451ffb821427p+0, 0x1.2300p-47,
        0x1.251ce4fb2a602p+0, -0x1.3480p-46, 0x1.25e85711eceb0p+0, 0x1.2700p-46,
        0x1.26b4565e27d16p+0, 0x1.1d00p-46, 0x1.2780e341de00fp+0, 0x1.1ee0p-44,
        0x1.284dfe1f5633ep+0, -0x1.4c00p-46, 0x1.291ba7591bb30p+0, -0x1.3d80p-46,
        0x1.29e9df51fdf09p+0, 0x1.8b00p-47, 0x1.2ab8a66d10e9bp+0, -0x1.27c0p-45,
        0x1.2b87fd0dada3ap+0, 0x1.a340p-45, 0x1.2c57e39771af9p+0, -0x1.0800p-46,
        0x1.2d285a6e402d9p+0, -0x1.ed00p-47, 0x1.2df961f641579p+0, -0x1.4200p-48,
        0x1.2ecafa93e2ecfp+0, -0x1.4980p-45, 0x1.2f9d24abd8822p+0, -0x1.6300p-46,
        0x1.306fe0a31b625p+0, -0x1.2360p-44, 0x1.31432edeea50bp+0, -0x1.0df8p-40,
        0x1.32170fc4cd7b8p+0, -0x1.2480p-45, 0x1.32eb83ba8e9a2p+0, -0x1.5980p-45,
        0x1.33c08b2641766p+0, 0x1.ed00p-46, 0x1.3496266e3fa27p+0, -0x1.c000p-50,
        0x1.356c55f929f0fp+0, -0x1.0d80p-44, 0x1.36431a2de88b9p+0, 0x1.2c80p-45,
        0x1.371a7373aaa39p+0, 0x1.0600p-45, 0x1.37f26231e74fep+0, -0x1.6600p-46,
        0x1.38cae6d05d838p+0, -0x1.ae00p-47, 0x1.39a401b713ec3p+0, -0x1.4720p-43,
        0x1.3a7db34e5a020p+0, 0x1.8200p-47, 0x1.3b57fbfec6e95p+0, 0x1.e800p-44,
        0x1.3c32dc313a8f2p+0, 0x1.f800p-49, 0x1.3d0e544ede122p+0, -0x1.7a00p-46,
        0x1.3dea64c1234bbp+0, 0x1.6300p-45, 0x1.3ec70df1c4eccp+0, -0x1.8a60p-43,
        0x1.3fa4504ac7e8cp+0, -0x1.cdc0p-44, 0x1.40822c367a0bbp+0, 0x1.5b80p-45,
        0x1.4160a21f72e95p+0, 0x1.ec00p-46, 0x1.423fb27094646p+0, -0x1.3600p-46,
        0x1.431f5d950a920p+0, 0x1.3980p-45, 0x1.43ffa3f84b9ebp+0, 0x1.a000p-48,
        0x1.44e0860618919p+0, -0x1.6c00p-48, 0x1.45c2042a7d201p+0, -0x1.bc00p-47,
        0x1.46a41ed1d0016p+0, -0x1.2800p-46, 0x1.4786d668b3326p+0, 0x1.0e00p-44,
        0x1.486a2b5c13c00p+0, -0x1.d400p-45, 0x1.494e1e192af04p+0, 0x1.c200p-47,
        0x1.4a32af0d7d372p+0, -0x1.e500p-46, 0x1.4b17dea6db801p+0, 0x1.7800p-47,
        0x1.4bfdad53629e1p+0, -0x1.3800p-46, 0x1.4ce41b817c132p+0, 0x1.0800p-47,
        0x1.4dcb299fddddbp+0, 0x1.c700p-45, 0x1.4eb2d81d8ab96p+0, -0x1.ce00p-46,
        0x1.4f9b2769d2d02p+0, 0x1.9200p-46, 0x1.508417f4531c1p+0, -0x1.8c00p-47,
        0x1.516daa2cf662ap+0, -0x1.a000p-48, 0x1.5257de83f51eap+0, 0x1.a080p-43,
        0x1.5342b569d4edap+0, -0x1.6d80p-45, 0x1.542e2f4f6ac1ap+0, -0x1.2440p-44,
        0x1.551a4ca5d94dbp+0, 0x1.83c0p-43, 0x1.56070dde9116bp+0, 0x1.4b00p-45,
        0x1.56f4736b529dep+0, 0x1.15a0p-43, 0x1.57e27dbe2c40ep+0, -0x1.9e00p-45,
        0x1.58d12d497c76fp+0, -0x1.3080p-45, 0x1.59c0827ff0b4cp+0, 0x1.dec0p-43,
        0x1.5ab07dd485427p+0, -0x1.4000p-51, 0x1.5ba11fba87af4p+0, 0x1.0080p-44,
        0x1.5c9268a59460bp+0, -0x1.6c80p-45, 0x1.5d84590998e3fp+0, 0x1.69a0p-43,
        0x1.5e76f15ad20e1p+0, -0x1.b400p-46, 0x1.5f6a320dcebcap+0, 0x1.7700p-46,
        0x1.605e1b976dcb8p+0, 0x1.6f80p-45, 0x1.6152ae6cdf715p+0, 0x1.1000p-47,
        0x1.6247eb03a5531p+0, -0x1.5d00p-46, 0x1.633dd1d1929b5p+0, -0x1.2d00p-46,
        0x1.6434634ccc313p+0, -0x1.a800p-49, 0x1.652b9febc8efap+0, -0x1.8600p-45,
        0x1.6623882553397p+0, 0x1.1fe0p-40, 0x1.671c1c708328ep+0, -0x1.7200p-44,
        0x1.68155d44ca97ep+0, 0x1.6800p-49, 0x1.690f4b19e9471p+0, -0x1.9780p-45,
    };
    const double redux = 0x1.8p52 / 256;
    const double P1 = 0x1.62e42fefa39efp-1;
    const double P2 = 0x1.ebfbdff82c575p-3;
    const double P3 = 0x1.c6b08d704a0a6p-5;
    const double P4 = 0x1.3b2ab88f70400p-7;
    const double P5 = 0x1.5d88003875c74p-10;
    static volatile double huge = 0x1p1000;
    static volatile double twom1000 = 0x1p-1000;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const std::uint32_t ix = hx & 0x7fffffff;
    if (ix >= 0x40900000) {
        if (ix >= 0x7ff00000) {
            if (((ix & 0xfffff) | static_cast<std::uint32_t>(bits)) != 0 || (hx & 0x80000000) == 0) {
                return x + x;
            }
            return 0.0;
        }
        if (x >= 0x1.0p10) {
            errno = ERANGE;
            return huge * huge;
        }
        if (x <= -0x1.0ccp10) {
            errno = ERANGE;
            return twom1000 * twom1000;
        }
    } else if (ix < 0x3c900000) {
        return 1.0 + x;
    }
    double t = x + redux;
    std::uint32_t i0 = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(t));
    i0 += 256 / 2;
    const std::int32_t k = static_cast<std::int32_t>((i0 >> 8) << 20);
    i0 = (i0 & (256 - 1)) << 1;
    t -= redux;
    double z = x - t;
    t = tbl[i0];
    z -= tbl[i0 + 1];
    const double r = t + t * z * (P1 + z * (P2 + z * (P3 + z * (P4 + z * P5))));
    if (k >= -(1021 << 20)) {
        if (k == 1024 << 20) {
            return r * 2.0 * 0x1p1023;
        }
        return r * std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(0x3ff00000 + k)) << 32);
    }
    return r * std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(0x3ff00000 + k + (1000 << 20))) << 32) * twom1000;
}
double APS5_VABI ldexp_nid_postfix(double x, int exponent) {
    if (!std::isfinite(x) || x == 0.0) return x;
    const int saved_errno = errno;
    const double result = std::scalbn(x, exponent);
    errno = !std::isfinite(result) || result == 0.0 ? ERANGE : saved_errno;
    return result;
}
double APS5_VABI scalbn_nid_postfix(double x, int exponent) {
    const int saved_errno = errno;
    const double result = std::scalbn(x, exponent);
    errno = saved_errno;
    return result;
}
float APS5_VABI scalbnf_nid_postfix(float x, int exponent) {
    const int saved_errno = errno;
    const float result = std::scalbn(x, exponent);
    errno = saved_errno;
    return result;
}
long double APS5_VABI scalbnl_nid_postfix(long double x, int exponent) {
    const int saved_errno = errno;
    const long double result = std::scalbn(x, exponent);
    errno = saved_errno;
    return result;
}
float APS5_VABI significandf_nid_postfix(float x) { return x == 0.0f || !std::isfinite(x) ? x : std::scalbn(x, -std::ilogb(x)); }
double APS5_VABI significand_nid_postfix(double x) { return x == 0.0 || !std::isfinite(x) ? x : std::scalbn(x, -std::ilogb(x)); }
double APS5_VABI frexp_nid_postfix(double x, int* exponent) {
    auto bits = std::bit_cast<std::uint64_t>(x);
    auto magnitude = bits & 0x7fffffffffffffffull;
    *exponent = 0;
    if (magnitude == 0 || magnitude >= 0x7ff0000000000000ull) return x;
    int adjustment = 0;
    if (magnitude < 0x0010000000000000ull) {
        volatile double subnormal = x;
        x = subnormal * 0x1p54;
        bits = std::bit_cast<std::uint64_t>(x);
        magnitude = bits & 0x7fffffffffffffffull;
        adjustment = -54;
    }
    *exponent = static_cast<int>(magnitude >> 52) - 1022 + adjustment;
    return std::bit_cast<double>((bits & 0x800fffffffffffffull) | 0x3fe0000000000000ull);
}
float APS5_VABI frexpf_nid_postfix(float x, int* exponent) {
    auto bits = std::bit_cast<std::uint32_t>(x);
    auto magnitude = bits & 0x7fffffffu;
    *exponent = 0;
    if (magnitude == 0 || magnitude >= 0x7f800000u) return x;
    int adjustment = 0;
    if (magnitude < 0x00800000u) {
        volatile float subnormal = x;
        x = subnormal * 0x1p25f;
        bits = std::bit_cast<std::uint32_t>(x);
        magnitude = bits & 0x7fffffffu;
        adjustment = -25;
    }
    *exponent = static_cast<int>(magnitude >> 23) - 126 + adjustment;
    return std::bit_cast<float>((bits & 0x807fffffu) | 0x3f000000u);
}
// Guest long is 64-bit, including on Windows where native long is 32-bit.
std::int64_t APS5_VABI lround_nid_postfix(double x) { return NudgedToInt64<double, std::uint64_t, 52>(x, 0x1.fffffffffffffp-2); }
std::int64_t APS5_VABI lroundf_nid_postfix(float x) { return NudgedToInt64<float, std::uint32_t, 23>(x, 0x1.fffffep-2f); }
std::int64_t APS5_VABI llround_nid_postfix(double x) { return NudgedToInt64<double, std::uint64_t, 52>(x, 0x1.fffffffffffffp-2); }
std::int64_t APS5_VABI llroundf_nid_postfix(float x) { return NudgedToInt64<float, std::uint32_t, 23>(x, 0x1.fffffep-2f); }
std::int64_t APS5_VABI llroundl_nid_postfix(long double x) { return RoundToInt64(x); }
int APS5_VABI __isfinitef_nid_postfix(float x) {
    const auto exponent = std::bit_cast<std::uint32_t>(x) & 0x7f800000u;
    return exponent != 0x7f800000u ? 1 : 0;
}
int APS5_VABI __isnormal_nid_postfix(double x) {
    const auto exponent = std::bit_cast<std::uint64_t>(x) & 0x7ff0000000000000ull;
    return exponent != 0 && exponent != 0x7ff0000000000000ull ? 1 : 0;
}
int APS5_VABI __isnormalf_nid_postfix(float x) {
    const auto exponent = std::bit_cast<std::uint32_t>(x) & 0x7f800000u;
    return exponent != 0 && exponent != 0x7f800000u ? 1 : 0;
}
int APS5_VABI __isinff_nid_postfix(float x) {
    return (std::bit_cast<std::uint32_t>(x) & 0x7fffffffu) == 0x7f800000u ? 1 : 0;
}
long double APS5_VABI nearbyintl_nid_postfix(long double x) { return std::nearbyint(x); }

double APS5_VABI cbrt_nid_postfix(double x) {
    const std::uint32_t B1 = 715094163;
    const std::uint32_t B2 = 696219795;
    const double P0 = 1.87595182427177009643;
    const double P1 = -1.88497979543377169875;
    const double P2 = 1.621429720105354466140;
    const double P3 = -0.758397934778766047437;
    const double P4 = 0.145996192886612446982;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const std::uint32_t low = static_cast<std::uint32_t>(bits);
    const std::uint32_t sign = hx & 0x80000000u;
    hx ^= sign;
    if (hx >= 0x7ff00000) {
        return x + x;
    }
    double t;
    if (hx < 0x00100000) {
        if ((hx | low) == 0) {
            return x;
        }
        t = std::bit_cast<double>(std::uint64_t{0x4350000000000000}) * x;
        const std::uint32_t high = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(t) >> 32);
        t = std::bit_cast<double>(static_cast<std::uint64_t>(sign | ((high & 0x7fffffff) / 3 + B2)) << 32);
    } else {
        t = std::bit_cast<double>(static_cast<std::uint64_t>(sign | (hx / 3 + B1)) << 32);
    }
    double r = (t * t) * (t / x);
    t = t * ((P0 + r * (P1 + r * P2)) + ((r * r) * r) * (P3 + r * P4));
    t = std::bit_cast<double>((std::bit_cast<std::uint64_t>(t) + 0x80000000u) & 0xffffffffc0000000ull);
    const double s = t * t;
    r = x / s;
    const double w = t + t;
    r = (r - t) / (w + r);
    return t + t * r;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_asin.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_asin.c */
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
static double AsinRational(double t) {
    const double pS0 = 1.66666666666666657415e-01;
    const double pS1 = -3.25565818622400915405e-01;
    const double pS2 = 2.01212532134862925881e-01;
    const double pS3 = -4.00555345006794114027e-02;
    const double pS4 = 7.91534994289814532176e-04;
    const double pS5 = 3.47933107596021167570e-05;
    const double qS1 = -2.40339491173441421878e+00;
    const double qS2 = 2.02094576023350569471e+00;
    const double qS3 = -6.88283971605453293030e-01;
    const double qS4 = 7.70381505559019352791e-02;
    const double p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
    const double q = 1.0 + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
    return p / q;
}

double APS5_VABI asin_nid_postfix(double x) {
    const double pio2_hi = 1.57079632679489655800e+00;
    const double pio2_lo = 6.12323399573676603587e-17;
    const double pio4_hi = 7.85398163397448278999e-01;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x3ff00000) {
        if (((ix - 0x3ff00000) | static_cast<std::uint32_t>(bits)) == 0) {
            return x * pio2_hi + x * pio2_lo;
        }
        _Feraise_nid_postfix(0x01);
        return (x - x) / (x - x);
    }
    if (ix < 0x3fe00000) {
        if (ix < 0x3e500000) {
            return x;
        }
        return x + x * AsinRational(x * x);
    }
    const double t = (1.0 - std::fabs(x)) * 0.5;
    const double s = std::sqrt(t);
    double result;
    if (ix >= 0x3fef3333) {
        result = pio2_hi - (2.0 * (s + s * AsinRational(t)) - pio2_lo);
    } else {
        const double w = std::bit_cast<double>(std::bit_cast<std::uint64_t>(s) & 0xffffffff00000000ull);
        const double c = (t - w * w) / (s + w);
        const double p = 2.0 * s * AsinRational(t) - (pio2_lo - 2.0 * c);
        const double q = pio4_hi - 2.0 * w;
        result = pio4_hi - (p - q);
    }
    return hx > 0 ? result : -result;
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_acos.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_acos.c */
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
double APS5_VABI acos_nid_postfix(double x) {
    const double pi = 3.14159265358979311600e+00;
    const double pio2_hi = 1.57079632679489655800e+00;
    static volatile double pio2_lo = 6.12323399573676603587e-17;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x3ff00000) {
        if (((ix - 0x3ff00000) | static_cast<std::uint32_t>(bits)) == 0) {
            return hx > 0 ? 0.0 : pi + 2.0 * pio2_lo;
        }
        _Feraise_nid_postfix(0x01);
        return (x - x) / (x - x);
    }
    if (ix < 0x3fe00000) {
        if (ix <= 0x3c600000) {
            return pio2_hi + pio2_lo;
        }
        return pio2_hi - (x - (pio2_lo - x * AsinRational(x * x)));
    }
    if (hx < 0) {
        const double z = (1.0 + x) * 0.5;
        const double s = std::sqrt(z);
        const double w = AsinRational(z) * s - pio2_lo;
        return pi - 2.0 * (s + w);
    }
    const double z = (1.0 - x) * 0.5;
    const double s = std::sqrt(z);
    const double df = std::bit_cast<double>(std::bit_cast<std::uint64_t>(s) & 0xffffffff00000000ull);
    const double c = (z - df * df) / (s + df);
    const double w = AsinRational(z) * s + c;
    return 2.0 * (df + w);
}
double APS5_VABI sqrt_nid_postfix(double x) { return std::sqrt(x); }
float APS5_VABI sqrtf_nid_postfix(float x) { return std::sqrt(x); }
double APS5_VABI fabs_nid_postfix(double x) { return std::fabs(x); }
float APS5_VABI fabsf_nid_postfix(float x) { return std::fabs(x); }
long double APS5_VABI fabsl_nid_postfix(long double x) { return std::fabs(x); }
}
