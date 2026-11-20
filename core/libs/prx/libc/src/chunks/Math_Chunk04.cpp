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


double APS5_VABI log_nid_postfix(double x) {
    const double ln2_hi = 6.93147180369123816490e-01;
    const double ln2_lo = 1.90821492927058770002e-10;
    const double two54 = 1.80143985094819840000e+16;
    const double Lg1 = 6.666666666666735130e-01;
    const double Lg2 = 3.999999999940941908e-01;
    const double Lg3 = 2.857142874366239149e-01;
    const double Lg4 = 2.222219843214978396e-01;
    const double Lg5 = 1.818357216161805012e-01;
    const double Lg6 = 1.531383769920937332e-01;
    const double Lg7 = 1.479819860511658591e-01;
    static volatile double vzero = 0.0;
    std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(bits);
    std::int32_t k = 0;
    if (hx < 0x00100000) {
        if (((hx & 0x7fffffff) | lx) == 0) {
            errno = ERANGE;
            return -two54 / vzero;
        }
        if (hx < 0) {
            _Feraise_nid_postfix(0x01);
            return (x - x) / 0.0;
        }
        k -= 54;
        x *= two54;
        bits = std::bit_cast<std::uint64_t>(x);
        hx = static_cast<std::int32_t>(bits >> 32);
    }
    if (hx >= 0x7ff00000) {
        return x + x;
    }
    k += (hx >> 20) - 1023;
    hx &= 0x000fffff;
    std::int32_t i = (hx + 0x95f64) & 0x100000;
    x = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hx | (i ^ 0x3ff00000))) << 32) | (bits & 0xffffffffu));
    k += i >> 20;
    const double f = x - 1.0;
    if ((0x000fffff & (2 + hx)) < 3) {
        if (f == 0.0) {
            if (k == 0) {
                return 0.0;
            }
            const double dk = k;
            return dk * ln2_hi + dk * ln2_lo;
        }
        const double R = f * f * (0.5 - 0.33333333333333333 * f);
        if (k == 0) {
            return f - R;
        }
        const double dk = k;
        return dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    const double s = f / (2.0 + f);
    const double dk = k;
    const double z = s * s;
    i = hx - 0x6147a;
    const double w = z * z;
    const std::int32_t j = 0x6b851 - hx;
    const double t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    const double t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i |= j;
    const double R = t2 + t1;
    if (i > 0) {
        const double hfsq = 0.5 * f * f;
        if (k == 0) {
            return f - (hfsq - s * (hfsq + R));
        }
        return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    if (k == 0) {
        return f - s * (f - R);
    }
    return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}
double APS5_VABI log1p_nid_postfix(double x) {
    const double ln2_hi = 6.93147180369123816490e-01;
    const double ln2_lo = 1.90821492927058770002e-10;
    const double two54 = 1.80143985094819840000e+16;
    const double Lp1 = 6.666666666666735130e-01;
    const double Lp2 = 3.999999999940941908e-01;
    const double Lp3 = 2.857142874366239149e-01;
    const double Lp4 = 2.222219843214978396e-01;
    const double Lp5 = 1.818357216161805012e-01;
    const double Lp6 = 1.531383769920937332e-01;
    const double Lp7 = 1.479819860511658591e-01;
    const std::int32_t hx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ax = hx & 0x7fffffff;
    std::int32_t k = 1;
    std::int32_t hu = 0;
    double f = 0.0;
    double c = 0.0;
    if (hx < 0x3fda827a) {
        if (ax >= 0x3ff00000) {
            if (x == -1.0) {
                return -std::numeric_limits<double>::infinity();
            }
            _Feraise_nid_postfix(0x01);
            return (x - x) / (x - x);
        }
        if (ax < 0x3e200000) {
            if (two54 + x > 0.0 && ax < 0x3c900000) {
                return x;
            }
            return x - x * x * 0.5;
        }
        if (hx > 0 || hx <= static_cast<std::int32_t>(0xbfd2bec4)) {
            k = 0;
            f = x;
            hu = 1;
        }
    }
    if (hx >= 0x7ff00000) {
        return x + x;
    }
    if (k != 0) {
        double u;
        if (hx < 0x43400000) {
            u = 1.0 + x;
            hu = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(u) >> 32);
            k = (hu >> 20) - 1023;
            c = k > 0 ? 1.0 + (x - u) : x - (u - 1.0);
            c /= u;
        } else {
            u = x;
            hu = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(u) >> 32);
            k = (hu >> 20) - 1023;
            c = 0;
        }
        hu &= 0x000fffff;
        const std::uint64_t ulow = std::bit_cast<std::uint64_t>(u) & 0xffffffffull;
        if (hu < 0x6a09e) {
            u = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hu | 0x3ff00000)) << 32) | ulow);
        } else {
            k += 1;
            u = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hu | 0x3fe00000)) << 32) | ulow);
            hu = (0x00100000 - hu) >> 2;
        }
        f = u - 1.0;
    }
    const double hfsq = 0.5 * f * f;
    if (hu == 0) {
        if (f == 0.0) {
            if (k == 0) {
                return 0.0;
            }
            c += k * ln2_lo;
            return k * ln2_hi + c;
        }
        const double R = hfsq * (1.0 - 0.66666666666666666 * f);
        if (k == 0) {
            return f - R;
        }
        return k * ln2_hi - ((R - (k * ln2_lo + c)) - f);
    }
    const double s = f / (2.0 + f);
    const double z = s * s;
    const double R = z * (Lp1 + z * (Lp2 + z * (Lp3 + z * (Lp4 + z * (Lp5 + z * (Lp6 + z * Lp7))))));
    if (k == 0) {
        return f - (hfsq - s * (hfsq + R));
    }
    return k * ln2_hi - ((hfsq - (s * (hfsq + R) + (k * ln2_lo + c))) - f);
}
float APS5_VABI log1pf_nid_postfix(float x) { return std::log1p(x); }
static double Expm1(double x) {
    const double tiny = 1.0e-300;
    const double o_threshold = 7.09782712893383973096e+02;
    const double ln2_hi = 6.93147180369123816490e-01;
    const double ln2_lo = 1.90821492927058770002e-10;
    const double invln2 = 1.44269504088896338700e+00;
    const double Q1 = -3.33333333333331316428e-02;
    const double Q2 = 1.58730158725481460165e-03;
    const double Q3 = -7.93650757867487942473e-05;
    const double Q4 = 4.00821782732936239552e-06;
    const double Q5 = -2.01099218183624371326e-07;
    static volatile double huge = 1.0e+300;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const std::uint32_t xsb = hx & 0x80000000u;
    hx &= 0x7fffffff;
    if (hx >= 0x4043687a) {
        if (hx >= 0x40862e42) {
            if (hx >= 0x7ff00000) {
                if (((hx & 0xfffff) | static_cast<std::uint32_t>(bits)) != 0) return x + x;
                return xsb == 0 ? x : -1.0;
            }
            if (x > o_threshold) {
                _Feraise_nid_postfix(0x08);
                return std::numeric_limits<double>::infinity();
            }
        }
        if (xsb != 0 && x + tiny < 0.0) return tiny - 1.0;
    }
    double c = 0.0;
    std::int32_t k;
    if (hx > 0x3fd62e42) {
        double hi;
        double lo;
        if (hx < 0x3ff0a2b2) {
            if (xsb == 0) {
                hi = x - ln2_hi;
                lo = ln2_lo;
                k = 1;
            } else {
                hi = x + ln2_hi;
                lo = -ln2_lo;
                k = -1;
            }
        } else {
            k = static_cast<std::int32_t>(invln2 * x + (xsb == 0 ? 0.5 : -0.5));
            const double t = k;
            hi = x - t * ln2_hi;
            lo = t * ln2_lo;
        }
        x = hi - lo;
        c = (hi - x) - lo;
    } else if (hx < 0x3c900000) {
        const double t = huge + x;
        return x - (t - (huge + x));
    } else {
        k = 0;
    }
    const double hfx = 0.5 * x;
    const double hxs = x * hfx;
    const double r1 = 1.0 + hxs * (Q1 + hxs * (Q2 + hxs * (Q3 + hxs * (Q4 + hxs * Q5))));
    double t = 3.0 - r1 * hfx;
    double e = hxs * ((r1 - t) / (6.0 - x * t));
    if (k == 0) return x - (x * e - hxs);
    const double twopk = std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(0x3ff + k) << 20) << 32);
    e = (x * (e - c) - c);
    e -= hxs;
    if (k == -1) return 0.5 * (x - e) - 0.5;
    if (k == 1) {
        if (x < -0.25) return -2.0 * (e - (x + 0.5));
        return 1.0 + 2.0 * (x - e);
    }
    double y;
    if (k <= -2 || k > 56) {
        y = 1.0 + (x - e);
        if (k == 1024) {
            y = y * 2.0 * 0x1p1023;
        } else {
            y = y * twopk;
        }
        return y - 1.0;
    }
    if (k < 20) {
        t = std::bit_cast<double>(static_cast<std::uint64_t>(0x3ff00000u - (0x200000u >> k)) << 32);
        y = t - (e - x);
        y = y * twopk;
    } else {
        t = std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>((0x3ff - k) << 20)) << 32);
        y = x - (e + t);
        y += 1.0;
        y = y * twopk;
    }
    return y;
}

double APS5_VABI expm1_nid_postfix(double x) {
    const double result = Expm1(x);
    if (std::isinf(result) && std::isfinite(x)) {
        errno = ERANGE;
    }
    return result;
}
static float Expm1f(float x) {
    const float one = 1.0f, tiny = 1.0e-30f, o_threshold = static_cast<float>(8.8721679688e+01);
    const float ln2_hi = static_cast<float>(6.9313812256e-01), ln2_lo = static_cast<float>(9.0580006145e-06), invln2 = static_cast<float>(1.4426950216e+00);
    const float Q1 = static_cast<float>(-3.3333212137e-2), Q2 = static_cast<float>(1.5807170421e-3);
    static volatile float huge = 1.0e+30f;
    float y, hi, lo, c = 0.0f, t, e, hxs, hfx, r1, twopk;
    std::int32_t k;
    std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t xsb = hx & 0x80000000u;
    hx &= 0x7fffffff;
    if (hx >= 0x4195b844) {
        if (hx >= 0x42b17218) {
            if (hx > 0x7f800000) return x + x;
            if (hx == 0x7f800000) return xsb == 0 ? x : -1.0f;
            if (x > o_threshold) return huge * huge;
        }
        if (xsb != 0) {
            if (x + tiny < 0.0f) return tiny - one;
        }
    }
    if (hx > 0x3eb17218) {
        if (hx < 0x3F851592) {
            if (xsb == 0) { hi = x - ln2_hi; lo = ln2_lo; k = 1; }
            else { hi = x + ln2_hi; lo = -ln2_lo; k = -1; }
        } else {
            k = static_cast<std::int32_t>(invln2 * x + (xsb == 0 ? 0.5f : -0.5f));
            t = static_cast<float>(k);
            hi = x - t * ln2_hi;
            lo = t * ln2_lo;
        }
        volatile float rounded = hi - lo;
        x = rounded;
        c = (hi - x) - lo;
    } else if (hx < 0x33000000) {
        t = huge + x;
        return x - (t - (huge + x));
    } else k = 0;
    hfx = 0.5f * x;
    hxs = x * hfx;
    r1 = one + hxs * (Q1 + hxs * Q2);
    t = 3.0f - r1 * hfx;
    e = hxs * ((r1 - t) / (6.0f - x * t));
    if (k == 0) return x - (x * e - hxs);
    twopk = std::bit_cast<float>(static_cast<std::uint32_t>(0x7f + k) << 23);
    e = (x * (e - c) - c);
    e -= hxs;
    if (k == -1) return 0.5f * (x - e) - 0.5f;
    if (k == 1) {
        if (x < -0.25f) return -2.0f * (e - (x + 0.5f));
        return one + 2.0f * (x - e);
    }
    if (k <= -2 || k > 56) {
        y = one - (e - x);
        if (k == 128) y = y * 2.0f * 0x1p127f;
        else y = y * twopk;
        return y - one;
    }
    t = one;
    if (k < 23) {
        t = std::bit_cast<float>(static_cast<std::uint32_t>(0x3f800000 - (0x1000000 >> k)));
        y = t - (e - x);
        y = y * twopk;
    } else {
        t = std::bit_cast<float>(static_cast<std::uint32_t>((0x7f - k) << 23));
        y = x - (e + t);
        y += one;
        y = y * twopk;
    }
    return y;
}

float APS5_VABI expm1f_nid_postfix(float x) {
    const float result = Expm1f(x);
    if (std::isinf(result) && std::isfinite(x)) {
        errno = ERANGE;
    }
    return result;
}
float APS5_VABI erff_nid_postfix(float x) { return std::erf(x); }
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_erf.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_erf.c */
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
static double ErfSmallRatio(double z) {
    const double pp0 = 1.28379167095512558561e-01;
    const double pp1 = -3.25042107247001499370e-01;
    const double pp2 = -2.84817495755985104766e-02;
    const double pp3 = -5.77027029648944159157e-03;
    const double pp4 = -2.37630166566501626084e-05;
    const double qq1 = 3.97917223959155352819e-01;
    const double qq2 = 6.50222499887672944485e-02;
    const double qq3 = 5.08130628187576562776e-03;
    const double qq4 = 1.32494738004321644526e-04;
    const double qq5 = -3.96022827877536812320e-06;
    const double r = pp0 + z * (pp1 + z * (pp2 + z * (pp3 + z * pp4)));
    const double s = 1.0 + z * (qq1 + z * (qq2 + z * (qq3 + z * (qq4 + z * qq5))));
    return r / s;
}

}
