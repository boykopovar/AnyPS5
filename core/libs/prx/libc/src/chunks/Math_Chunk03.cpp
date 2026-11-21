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


double APS5_VABI copysign_nid_postfix(double x, double y) {
    return std::copysign(x, y);
}
float APS5_VABI copysignf_nid_postfix(float x, float y) {
    return std::copysign(x, y);
}
int APS5_VABI abs_nid_postfix(int x) { return x < 0 ? static_cast<int>(0u - static_cast<unsigned>(x)) : x; }
extern "C++" {
template<typename TFloat, typename TBits, int FractionBits>
static TFloat RoundToward(TFloat x, bool up, TFloat huge) {
    constexpr int ExponentBits = static_cast<int>(sizeof(TBits) * 8) - 1 - FractionBits;
    constexpr int Bias = (1 << (ExponentBits - 1)) - 1;
    constexpr TBits Sign = TBits{1} << (sizeof(TBits) * 8 - 1);
    TBits bits = std::bit_cast<TBits>(x);
    const int exponent = static_cast<int>((bits & ~Sign) >> FractionBits) - Bias;
    if (exponent >= FractionBits) return x;
    const bool away = ((bits & Sign) != 0) != up;
    const TBits mask = exponent < 0 ? ~Sign : (TBits{1} << (FractionBits - exponent)) - 1;
    if ((bits & mask) == 0) return x;
    [[maybe_unused]] const volatile TFloat inexact = huge + x;
    if (exponent < 0) return std::bit_cast<TFloat>((bits & Sign) | (away ? static_cast<TBits>(Bias) << FractionBits : TBits{0}));
    if (away) bits += mask + 1;
    return std::bit_cast<TFloat>(bits & ~mask);
}
}
double APS5_VABI trunc_nid_postfix(double x) { return std::isnan(x) ? x + x : std::trunc(x); }
double APS5_VABI ceil_nid_postfix(double x) { return std::isnan(x) ? x + x : RoundToward<double, std::uint64_t, 52>(x, true, 1.0e300); }
float APS5_VABI truncf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::trunc(x); }
float APS5_VABI ceilf_nid_postfix(float x) { return std::isnan(x) ? x + x : RoundToward<float, std::uint32_t, 23>(x, true, 1.0e30f); }
double APS5_VABI floor_nid_postfix(double x) { return std::isnan(x) ? x + x : RoundToward<double, std::uint64_t, 52>(x, false, 1.0e300); }
float APS5_VABI floorf_nid_postfix(float x) { return std::isnan(x) ? x + x : RoundToward<float, std::uint32_t, 23>(x, false, 1.0e30f); }
double APS5_VABI fma_nid_postfix(double x, double y, double z) { return std::fma(x, y, z); }
float APS5_VABI fmaf_nid_postfix(float x, float y, float z) { return std::fma(x, y, z); }
double APS5_VABI fmin_nid_postfix(double x, double y) { return GuestFmin(x, y); }
float APS5_VABI fminf_nid_postfix(float x, float y) { return GuestFmin(x, y); }
long double APS5_VABI fminl_nid_postfix(long double x, long double y) { return GuestFmin(x, y); }
double APS5_VABI fmax_nid_postfix(double x, double y) { return GuestFmax(x, y); }
float APS5_VABI fmaxf_nid_postfix(float x, float y) { return GuestFmax(x, y); }
long double APS5_VABI fmaxl_nid_postfix(long double x, long double y) { return GuestFmax(x, y); }
double APS5_VABI fdim_nid_postfix(double x, double y) {
    if ((std::bit_cast<std::uint64_t>(x) << 1) > 0xffe0000000000000ull) return x;
    if ((std::bit_cast<std::uint64_t>(y) << 1) > 0xffe0000000000000ull) return y;
    const volatile double difference = x - y;
    return y < x ? difference : 0.0;
}
float APS5_VABI fdimf_nid_postfix(float x, float y) {
    if ((std::bit_cast<std::uint32_t>(x) << 1) > 0xff000000u) return x;
    if ((std::bit_cast<std::uint32_t>(y) << 1) > 0xff000000u) return y;
    const volatile float difference = x - y;
    return y < x ? difference : 0.0f;
}
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
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_acosl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_acosl.c */
long double APS5_VABI acosl_nid_postfix(long double x) {
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    const bool negative = (parts.signExponent & 0x8000) != 0;
    if (exponent >= 0x3fff) {
        if (exponent == 0x3fff && (parts.mantissa & 0x7fffffffffffffffull) == 0) return negative ? SumAtCallerPrecision(Pi, 2.0L * HalfPiLow) : 0.0L;
        std::feraiseexcept(FE_INVALID);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (exponent < 0x3ffe) {
        if (exponent < 0x3fff - 65) return SumAtCallerPrecision(HalfPiHigh, HalfPiLow);
        const long double z = x * x;
        const long double r = AsinNumerator(z) / AsinDenominator(z);
        return HalfPiHigh - (x - (HalfPiLow - x * r));
    }
    if (negative) {
        const long double z = (1.0L + x) * 0.5L;
        const long double s = std::sqrt(z);
        const long double r = AsinNumerator(z) / AsinDenominator(z);
        return Pi - 2.0L * (s + (r * s - HalfPiLow));
    }
    const long double z = (1.0L - x) * 0.5L;
    const long double s = std::sqrt(z);
    const long double high = ClearLowMantissaWord(s);
    const long double correction = (z - high * high) / (s + high);
    const long double r = AsinNumerator(z) / AsinDenominator(z);
    return 2.0L * (high + (r * s + correction));
}
long double APS5_VABI acoshl_nid_postfix(long double x) {
    constexpr long double Huge = 0x1p67L;
    constexpr long double Ln2 = 0xb.17217f7d1cf79acp-4L;
    const short kind = _LDtest_nid_postfix(&x);
    if (kind == 2 || (kind == 1 && !std::signbit(x))) return x;
    if (x < 1.0L) {
        _Feraise_nid_postfix(0x01);
        return std::numeric_limits<long double>::quiet_NaN();
    }
    if (x == 1.0L) return 0.0L;
    if (!(x < Huge)) return _LLog_nid_postfix(x, 0) + Ln2;
    const long double t = -1.0L + x;
    return log1pl_nid_postfix(sqrtl_nid_postfix(x * t + t) + t);
}
long double APS5_VABI asinhl_nid_postfix(long double x) {
    constexpr long double Tiny = 0x1p-66L;
    constexpr long double Huge = 0x1p67L;
    constexpr long double Ln2 = 0xb.17217f7d1cf79acp-4L;
    const short kind = _LDtest_nid_postfix(&x);
    if (kind == 0 || kind == 1 || kind == 2) return x;
    const long double a = x < 0.0L ? -x : x;
    long double result;
    if (a < Tiny) result = a;
    else if (!(a < Huge)) result = _LLog_nid_postfix(a, 0) + Ln2;
    else result = log1pl_nid_postfix(a / (sqrtl_nid_postfix(a * a + 1.0L) + 1.0L) * a + a);
    return x < 0.0L ? -result : result;
}
long double APS5_VABI atanhl_nid_postfix(long double x) {
    constexpr long double Tiny = 0x1p-33L;
    const short kind = _LDtest_nid_postfix(&x);
    if (kind == 0 || kind == 2) return x;
    const long double a = x < 0.0L ? -x : x;
    if (a > 1.0L) {
        _Feraise_nid_postfix(0x01);
        return std::numeric_limits<long double>::quiet_NaN();
    }
    if (a == 1.0L) {
        _Feraise_nid_postfix(0x04);
        return x < 0.0L ? -std::numeric_limits<long double>::infinity() : std::numeric_limits<long double>::infinity();
    }
    if (-Tiny < a && a < Tiny) return x;
    const long double q = a / (1.0L - a);
    const long double result = log1pl_nid_postfix(q + q) * 0.5L;
    return x < 0.0L ? -result : result;
}
long double APS5_VABI cbrtl_nid_postfix(long double x) {
    constexpr std::uint32_t B1 = 709958130;
    constexpr int Bias = 0x3fff;
    ExtendedParts parts = SplitExtended(x);
    const std::uint16_t sign = parts.signExponent & 0x8000;
    int k = parts.signExponent & 0x7fff;
    if (k == 0x7fff) return x + x;
    if (k == 0) {
        if (parts.mantissa == 0) return x;
        parts = SplitExtended(x * 0x1p514L);
        k = (parts.signExponent & 0x7fff) - (Bias + 514);
    } else {
        k -= Bias;
    }
    long double reduced = JoinExtended(parts.mantissa, Bias);
    switch (k % 3) {
    case 1:
    case -2:
        reduced = 2 * reduced;
        --k;
        break;
    case 2:
    case -1:
        reduced = 4 * reduced;
        k -= 2;
        break;
    }
    const long double scale = JoinExtended(0x8000000000000000ull, static_cast<std::uint16_t>(sign | (Bias + k / 3)));
    const float estimate = std::bit_cast<float>((std::bit_cast<std::uint32_t>(static_cast<float>(reduced)) & 0x7fffffffu) / 3 + B1);
    const double dx = static_cast<double>(reduced);
    double dt = estimate;
    double dr = dt * dt * dt;
    dt = dt * (dx + dx + dr) / (dx + dr + dr);
    dr = dt * dt * dt;
    dt = dt * (dx + dx + dr) / (dx + dr + dr);
    const volatile double shift = 0x1p32;
    const volatile double away = 0x1p-31;
    long double t = (static_cast<long double>(shift) + away + dt) - 0x1p32L;
    long double r = reduced / (t * t);
    r = (r - t) / ((t + t) + r);
    t = t + t * r;
    return t * scale;
}
long double APS5_VABI log1pl_nid_postfix(long double x) {
    constexpr long double Tiny = 0x1p-66L;
    const short kind = _LDtest_nid_postfix(&x);
    if (kind == 0 || kind == 2 || (kind == 1 && !std::signbit(x))) return x;
    if (x < -1.0L) {
        _Feraise_nid_postfix(0x01);
        return std::numeric_limits<long double>::quiet_NaN();
    }
    if (x == -1.0L) {
        _Feraise_nid_postfix(0x04);
        return -std::numeric_limits<long double>::infinity();
    }
    if (-Tiny <= x && x < Tiny) return x;
    const long double u = 1.0L + x;
    const long double correction = ((u + -1.0L) - x) * (-1.0L / u);
    return _LLog_nid_postfix(u, 0) + correction;
}
long double APS5_VABI log2l_nid_postfix(long double x) { return _LLog_nid_postfix(x, -1); }
long double APS5_VABI lgammal_nid_postfix(long double x) { return std::lgamma(x); }
long double APS5_VABI tgammal_nid_postfix(long double x) { return std::tgamma(x); }
float APS5_VABI tgammaf_nid_postfix(float x) { return GuestTgammaf(x); }
double APS5_VABI tgamma_nid_postfix(double x) {
    const int savedErrno = errno;
    const double result = std::tgamma(x);
    errno = std::isfinite(x) && x < 0.0 && std::trunc(x) == x ? EDOM : savedErrno;
    if (result != 0.0 || !std::isfinite(x) || x >= 0.0 || std::trunc(x) == x) return result;
    const bool negative = std::fmod(std::floor(-x), 2.0) == 0.0;
    return std::copysign(0.0, negative ? -1.0 : 1.0);
}
long double APS5_VABI hypotl_nid_postfix(long double x, long double y) {
    constexpr int Scale = 0x2058;
    constexpr std::uint64_t IntegerBit = 0x8000000000000000ull;
    int ha = SplitExtended(x).signExponent & 0x7fff;
    int hb = SplitExtended(y).signExponent & 0x7fff;
    long double a = x;
    long double b = y;
    if (hb > ha) {
        a = y;
        b = x;
        const int exponent = ha;
        ha = hb;
        hb = exponent;
    }
    a = std::fabs(a);
    b = std::fabs(b);
    if (ha - hb > 71) return a + b;
    int k = 0;
    if (ha > 0x5ff3) {
        if (ha == 0x7fff) {
            const volatile long double difference = std::fabs(x + 0.0L) - std::fabs(y + 0.0L);
            long double w = difference;
            if (SplitExtended(a).mantissa == IntegerBit) w = a;
            if (hb == 0x7fff && SplitExtended(b).mantissa == IntegerBit) w = b;
            return w;
        }
        ha -= Scale;
        hb -= Scale;
        k += Scale;
        a = JoinExtended(SplitExtended(a).mantissa, static_cast<std::uint16_t>(ha));
        b = JoinExtended(SplitExtended(b).mantissa, static_cast<std::uint16_t>(hb));
    }
    if (hb < 0x200b) {
        if (hb <= 0) {
            if (SplitExtended(b).mantissa == 0) return a;
            const volatile std::uint64_t scaleMantissa = 0x4000000000000000ull;
            const long double t1 = JoinExtended(scaleMantissa, 0x7fff);
            return t1 + t1;
        } else {
            ha += Scale;
            hb += Scale;
            k -= Scale;
            a = JoinExtended(SplitExtended(a).mantissa, static_cast<std::uint16_t>(ha));
            b = JoinExtended(SplitExtended(b).mantissa, static_cast<std::uint16_t>(hb));
        }
    }
    long double w = a - b;
    if (w > b) {
        const long double t1 = ClearLowMantissaWord(a);
        const long double t2 = a - t1;
        w = std::sqrt(t1 * t1 - (b * (-b) - t2 * (a + t1)));
    } else {
        a = a + a;
        const long double y1 = ClearLowMantissaWord(b);
        const long double y2 = b - y1;
        const long double t1 = ClearLowMantissaWord(a);
        const long double t2 = a - t1;
        w = std::sqrt(t1 * y1 - (w * (-w) - (t1 * y2 + t2 * b)));
    }
    if (k == 0) return w;
    return JoinExtended(IntegerBit, static_cast<std::uint16_t>(0x3fff + k)) * w;
}
float APS5_VABI hypot3f_nid_postfix(float x, float y, float z) { return Hypot3(x, y, z, hypotf_nid_postfix); }
double APS5_VABI hypot3_nid_postfix(double x, double y, double z) { return Hypot3(x, y, z, hypot_nid_postfix); }
long double APS5_VABI hypot3l_nid_postfix(long double x, long double y, long double z) { return Hypot3(x, y, z, hypotl_nid_postfix); }

long double APS5_VABI asinl_nid_postfix(long double x) {
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    if (exponent >= 0x3fff) {
        if (exponent == 0x3fff && (parts.mantissa & 0x7fffffffffffffffull) == 0) return x * HalfPiHigh + x * HalfPiLow;
        std::feraiseexcept(FE_INVALID);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (exponent < 0x3ffe) {
        if (exponent < 0x3fff - 32 && 1.0e300L + x > 1.0L) return x;
        const long double t = x * x;
        return x + x * (AsinNumerator(t) / AsinDenominator(t));
    }
    const long double t = (1.0L - std::fabs(x)) * 0.5L;
    const long double p = AsinNumerator(t);
    const long double q = AsinDenominator(t);
    const long double s = std::sqrt(t);
    long double result;
    if ((parts.mantissa >> 32) >= 0xf3333333u) {
        result = HalfPiHigh - (2.0L * (s + s * (p / q)) - HalfPiLow);
    } else {
        const long double w = ClearLowMantissaWord(s);
        const long double c = (t - w * w) / (s + w);
        const long double r = p / q;
        result = QuarterPiHigh - ((2.0L * s * r - (HalfPiLow - 2.0L * c)) - (QuarterPiHigh - 2.0L * w));
    }
    return (parts.signExponent & 0x8000) != 0 ? -result : result;
}

long double APS5_VABI frexpl_nid_postfix(long double x, int* exponent) {
    if (!std::isfinite(x)) return x;
    return std::frexp(x, exponent);
}
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_exp.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_exp.c */
double APS5_VABI exp_nid_postfix(double x) {
    static constexpr double halF[2] = {0.5, -0.5};
    static constexpr double ln2HI[2] = {6.93147180369123816490e-01, -6.93147180369123816490e-01};
    static constexpr double ln2LO[2] = {1.90821492927058770002e-10, -1.90821492927058770002e-10};
    static volatile double huge = 1.0e+300;
    static volatile double twom1000 = 9.33263618503218878990e-302;
    const double invln2 = 1.44269504088896338700e+00;
    const double P1 = 1.66666666666666019037e-01;
    const double P2 = -2.77777777770155933842e-03;
    const double P3 = 6.61375632143793436117e-05;
    const double P4 = -1.65339022054652515390e-06;
    const double P5 = 4.13813679705723846039e-08;
    double hi = 0.0;
    double lo = 0.0;
    std::int32_t k = 0;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const std::int32_t xsb = (hx >> 31) & 1;
    hx &= 0x7fffffff;
    if (hx >= 0x40862e42) {
        if (hx >= 0x7ff00000) {
            if (((hx & 0xfffff) | static_cast<std::uint32_t>(bits)) != 0) {
                return x + x;
            }
            return xsb == 0 ? x : 0.0;
        }
        if (x > 7.09782712893383973096e+02) {
            errno = ERANGE;
            return huge * huge;
        }
        if (x < -7.45133219101941108420e+02) {
            errno = ERANGE;
            return twom1000 * twom1000;
        }
    }
    if (x == 1.0) {
        return std::bit_cast<double>(std::uint64_t{0x4005bf0a8b145769});
    }
    if (hx > 0x3fd62e42) {
        if (hx < 0x3ff0a2b2) {
            hi = x - ln2HI[xsb];
            lo = ln2LO[xsb];
            k = 1 - xsb - xsb;
        } else {
            k = static_cast<std::int32_t>(invln2 * x + halF[xsb]);
            const double t = k;
            hi = x - t * ln2HI[0];
            lo = t * ln2LO[0];
        }
        x = hi - lo;
    } else if (hx < 0x3e300000) {
        return 1.0 + x;
    }
    const double t = x * x;
    const double twopk = std::bit_cast<double>(static_cast<std::uint64_t>(0x3ff + (k >= -1021 ? k : k + 1000)) << 52);
    const double c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    if (k == 0) {
        return 1.0 - ((x * c) / (c - 2.0) - x);
    }
    const double y = 1.0 - ((lo - (x * c) / (2.0 - c)) - hi);
    if (k >= -1021) {
        if (k == 1024) {
            return y * 2.0 * 0x1p1023;
        }
        return y * twopk;
    }
    return y * twopk * twom1000;
}
double APS5_VABI atan_nid_postfix(double x) {
    static constexpr double atanhi[] = {4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01, 1.57079632679489655800e+00};
    static constexpr double atanlo[] = {2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17, 6.12323399573676603587e-17};
    static constexpr double aT[] = {
        3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01, -1.11111104054623557880e-01,
        9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02, -5.83357013379057348645e-02,
        4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02,
    };
    static volatile double atanloInfinity = 6.12323399573676603587e-17;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::int32_t ix = hx & 0x7fffffff;
    int id;
    if (ix >= 0x44100000) {
        if (ix > 0x7ff00000 || (ix == 0x7ff00000 && static_cast<std::uint32_t>(bits) != 0)) {
            return x + x;
        }
        return hx > 0 ? atanhi[3] + atanloInfinity : -atanhi[3] - atanloInfinity;
    }
    if (ix < 0x3fdc0000) {
        if (ix < 0x3e400000) {
            return x;
        }
        id = -1;
    } else {
        x = std::fabs(x);
        if (ix < 0x3ff30000) {
            if (ix < 0x3fe60000) {
                id = 0;
                x = (2.0 * x - 1.0) / (2.0 + x);
            } else {
                id = 1;
                x = (x - 1.0) / (x + 1.0);
            }
        } else if (ix < 0x40038000) {
            id = 2;
            x = (x - 1.5) / (1.0 + 1.5 * x);
        } else {
            id = 3;
            x = -1.0 / x;
        }
    }
    double z = x * x;
    const double w = z * z;
    const double s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    const double s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) {
        return x - x * (s1 + s2);
    }
    z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
    return hx < 0 ? -z : z;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_log.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_log.c */
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
}
