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


static int TrigfRemPio2(float x, double* y) {
    const double invpio2 = 6.36619772367581382433e-01, pio2_1 = 1.57079631090164184570e+00, pio2_1t = 1.58932547735281966916e-08;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix < 0x4dc90fdb) {
        volatile double rounded = x * invpio2 + 0x1.8p52;
        double fn = rounded;
        fn = fn - 0x1.8p52;
        const std::int32_t n = static_cast<std::int32_t>(fn);
        const double r = x - fn * pio2_1;
        const double w = fn * pio2_1t;
        return n;
    }
    if (ix >= 0x7f800000) {
        return 0;
    }
    const std::int32_t e0 = (ix >> 23) - 150;
    const float z = std::bit_cast<float>(ix - static_cast<std::int32_t>(static_cast<std::uint32_t>(e0) << 23));
    double tx[1] = {z}, ty[1];
    const int n = TrigfKernelRemPio2(tx, ty, e0);
    if (hx < 0) {
        return -n;
    }
    return n;
}

static float TrigfKernelSin(double x) {
    const double S1 = -0x15555554cbac77.0p-55, S2 = 0x111110896efbb2.0p-59, S3 = -0x1a00f9e2cae774.0p-65, S4 = 0x16cd878c3b46a7.0p-71;
    const double z = x * x, w = z * z, r = S3 + z * S4, s = z * x;
    return static_cast<float>((x + s * (S1 + z * S2)) + s * w * r);
}

static float TrigfKernelCos(double x) {
    const double C0 = -0x1ffffffd0c5e81.0p-54, C1 = 0x155553e1053a42.0p-57, C2 = -0x16c087e80f1e27.0p-62, C3 = 0x199342e0ee5069.0p-68;
    const double z = x * x, w = z * z, r = C2 + z * C3;
    return static_cast<float>(((1.0 + z * C0) + w * C1) + (w * z) * r);
}

float APS5_VABI sinf_nid_postfix(float x) {
    const double s1pio2 = std::bit_cast<double>(0x3FF921FB54442D18ull), s2pio2 = std::bit_cast<double>(0x400921FB54442D18ull);
    const double s3pio2 = std::bit_cast<double>(0x4012D97C7F3321D2ull), s4pio2 = std::bit_cast<double>(0x401921FB54442D18ull);
    double y;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fda) {
        if (ix < 0x39800000)
            if (static_cast<int>(x) == 0) return x;
        return TrigfKernelSin(x);
    }
    if (ix <= 0x407b53d1) {
        if (ix <= 0x4016cbe3) return hx > 0 ? TrigfKernelCos(x - s1pio2) : -TrigfKernelCos(x + s1pio2);
        return TrigfKernelSin((hx > 0 ? s2pio2 : -s2pio2) - x);
    }
    if (ix <= 0x40e231d5) {
        if (ix <= 0x40afeddf) return hx > 0 ? -TrigfKernelCos(x - s3pio2) : TrigfKernelCos(x + s3pio2);
        return TrigfKernelSin(x + (hx > 0 ? -s4pio2 : s4pio2));
    }
    if (ix >= 0x7f800000) {
        if (ix == 0x7f800000) errno = EDOM;
        return x - x;
    }
    const int n = TrigfRemPio2(x, &y);
    switch (n & 3) {
        case 0: return TrigfKernelSin(y);
        case 1: return TrigfKernelCos(y);
        case 2: return TrigfKernelSin(-y);
        default: return -TrigfKernelCos(y);
    }
}

static const double TrigfPio2[5] = {0.0, std::bit_cast<double>(0x3FF921FB54442D18ull), std::bit_cast<double>(0x400921FB54442D18ull),
                                std::bit_cast<double>(0x4012D97C7F3321D2ull), std::bit_cast<double>(0x401921FB54442D18ull)};

float APS5_VABI cosf_nid_postfix(float x) {
    double y;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fda) {
        if (ix < 0x39800000)
            if (static_cast<int>(x) == 0) return 1.0f;
        return TrigfKernelCos(x);
    }
    if (ix <= 0x407b53d1) {
        if (ix > 0x4016cbe3) return -TrigfKernelCos(x + (hx > 0 ? -TrigfPio2[2] : TrigfPio2[2]));
        return hx > 0 ? TrigfKernelSin(TrigfPio2[1] - x) : TrigfKernelSin(x + TrigfPio2[1]);
    }
    if (ix <= 0x40e231d5) {
        if (ix > 0x40afeddf) return TrigfKernelCos(x + (hx > 0 ? -TrigfPio2[4] : TrigfPio2[4]));
        return hx > 0 ? TrigfKernelSin(x - TrigfPio2[3]) : TrigfKernelSin(-TrigfPio2[3] - x);
    }
    if (ix >= 0x7f800000) {
        if (ix == 0x7f800000) errno = EDOM;
        return x - x;
    }
    const int n = TrigfRemPio2(x, &y);
    switch (n & 3) {
        case 0: return TrigfKernelCos(y);
        case 1: return TrigfKernelSin(-y);
        case 2: return -TrigfKernelCos(y);
        default: return TrigfKernelSin(y);
    }
}

static float TrigfKernelTan(double x, int iy) {
    static const double T[] = {0x15554d3418c99f.0p-54, 0x1112fd38999f72.0p-55, 0x1b54c91d865afe.0p-57,
                               0x191df3908c33ce.0p-58, 0x185dadfcecf44e.0p-61, 0x1362b9bf971bcd.0p-59};
    const double z = x * x;
    double r = T[4] + z * T[5];
    const double t = T[2] + z * T[3];
    const double w = z * z;
    const double s = z * x;
    const double u = T[0] + z * T[1];
    r = (x + s * u) + (s * w) * (t + w * r);
    if (iy == 1) return static_cast<float>(r);
    return static_cast<float>(-1.0 / r);
}

float APS5_VABI tanf_nid_postfix(float x) {
    double y;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fda) {
        if (ix < 0x39800000)
            if (static_cast<int>(x) == 0) return x;
        return TrigfKernelTan(x, 1);
    }
    if (ix <= 0x407b53d1) {
        if (ix <= 0x4016cbe3) return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[1] : TrigfPio2[1]), -1);
        return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[2] : TrigfPio2[2]), 1);
    }
    if (ix <= 0x40e231d5) {
        if (ix <= 0x40afeddf) return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[3] : TrigfPio2[3]), -1);
        return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[4] : TrigfPio2[4]), 1);
    }
    if (ix >= 0x7f800000) {
        if (ix == 0x7f800000) errno = EDOM;
        return x - x;
    }
    const int n = TrigfRemPio2(x, &y);
    return TrigfKernelTan(y, 1 - ((n & 1) << 1));
}

void APS5_VABI sincosf_nid_postfix(float x, float* sinp, float* cosp) {
}

static std::int32_t HighWord(double x) {
    return static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
}

static double ClearLowWord(double x) {
    return std::bit_cast<double>(std::bit_cast<std::uint64_t>(x) & 0xffffffff00000000ull);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_rem_pio2.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_rem_pio2.c */
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
 * Optimized by Bruce D. Evans.
 */
static int RemPio2Near(double x, double* y, std::int32_t hx, int k) {
    const double pio2_1 = 1.57079632673412561417e+00;
    const double pio2_1t = 6.07710050650619224932e-11;
    if (hx > 0) {
        const double z = x - k * pio2_1;
        y[0] = z - k * pio2_1t;
        y[1] = (z - y[0]) - k * pio2_1t;
        return k;
    }
    const double z = x + k * pio2_1;
    y[0] = z + k * pio2_1t;
    y[1] = (z - y[0]) + k * pio2_1t;
    return -k;
}

static int RemPio2(double x, double* y) {
    const double two24 = 1.67772160000000000000e+07;
    const double invpio2 = 6.36619772367581382433e-01;
    const double pio2_1 = 1.57079632673412561417e+00;
    const double pio2_1t = 6.07710050650619224932e-11;
    const double pio2_2 = 6.07710050630396597660e-11;
    const double pio2_2t = 2.02226624879595063154e-21;
    const double pio2_3 = 2.02226624871116645580e-21;
    const double pio2_3t = 8.47842766036889956997e-32;
    const std::int32_t hx = HighWord(x);
    const std::int32_t ix = hx & 0x7fffffff;
    bool medium = false;
    if (ix <= 0x400f6a7a) {
        if ((ix & 0xfffff) != 0x921fb) {
            return RemPio2Near(x, y, hx, ix <= 0x4002d97c ? 1 : 2);
        }
        medium = true;
    } else if (ix <= 0x401c463b) {
        if (ix <= 0x4015fdbc) {
            if (ix != 0x4012d97c) {
                return RemPio2Near(x, y, hx, 3);
            }
        } else if (ix != 0x401921fb) {
            return RemPio2Near(x, y, hx, 4);
        }
        medium = true;
    }
    if (medium || ix < 0x413921fb) {
        double fn = x * invpio2 + 0x1.8p52;
        fn = fn - 0x1.8p52;
        const std::int32_t n = static_cast<std::int32_t>(fn);
        double r = x - fn * pio2_1;
        double w = fn * pio2_1t;
        const std::int32_t j = ix >> 20;
        y[0] = r - w;
        std::int32_t i = j - ((HighWord(y[0]) >> 20) & 0x7ff);
        if (i > 16) {
            double t = r;
            w = fn * pio2_2;
            r = t - w;
            w = fn * pio2_2t - ((t - r) - w);
            y[0] = r - w;
            i = j - ((HighWord(y[0]) >> 20) & 0x7ff);
            if (i > 49) {
                t = r;
                w = fn * pio2_3;
                r = t - w;
                w = fn * pio2_3t - ((t - r) - w);
                y[0] = r - w;
            }
        }
        y[1] = (r - y[0]) - w;
        return n;
    }
    if (ix >= 0x7ff00000) {
        y[0] = y[1] = x - x;
        return 0;
    }
    const std::uint32_t low = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x));
    const std::int32_t e0 = (ix >> 20) - 1046;
    double z = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(ix - (e0 << 20))) << 32) | low);
    double tx[3];
    double ty[2];
    for (int i = 0; i < 2; i++) {
        tx[i] = static_cast<double>(static_cast<std::int32_t>(z));
        z = (z - tx[i]) * two24;
    }
    tx[2] = z;
    int nx = 3;
    while (tx[nx - 1] == 0.0) nx--;
    const int n = KernelRemPio2(tx, ty, e0, nx, 1);
    if (hx < 0) {
        y[0] = -ty[0];
        y[1] = -ty[1];
        return -n;
    }
    y[0] = ty[0];
    y[1] = ty[1];
    return n;
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/e_rem_pio2l.h: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/e_rem_pio2l.h */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2008 Steven G. Kargl, David Schultz, Bruce D. Evans.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * Optimized by Bruce D. Evans.
 */
static constexpr double TrigLongTwo24 = 1.67772160000000000000e+07;
static constexpr double TrigLongPio21 = 1.57079632679597125389e+00;
static constexpr double TrigLongPio22 = -1.07463465549783099519e-12;
static constexpr double TrigLongPio23 = 6.36831716351370313614e-25;
static const volatile double TrigLongInvpio2Hi = 6.3661977236758138e-01;
static const volatile double TrigLongInvpio2Lo = -3.9356538861223811e-17;
static const volatile double TrigLongPio21tHi = -1.0746346554971943e-12;
static const volatile double TrigLongPio21tLo = 8.8451028997905949e-29;
static const volatile double TrigLongPio22tHi = 6.3683171635109499e-25;
static const volatile double TrigLongPio22tLo = 2.3183081793789774e-41;
static const volatile double TrigLongPio23tHi = -2.7529965190440717e-37;
static const volatile double TrigLongPio23tLo = -4.2006647512740502e-54;

static int ExtendedExponent(long double value) {
    return SplitExtended(value).signExponent & 0x7fff;
}

static int RemPio2lMedium(long double x, long double* y) {
    const long double invpio2 = static_cast<long double>(TrigLongInvpio2Hi) + TrigLongInvpio2Lo;
    const long double pio21t = static_cast<long double>(TrigLongPio21tHi) + TrigLongPio21tLo;
    const long double pio22t = static_cast<long double>(TrigLongPio22tHi) + TrigLongPio22tLo;
    const long double pio23t = static_cast<long double>(TrigLongPio23tHi) + TrigLongPio23tLo;
    const int exponent = ExtendedExponent(x);
    long double fn = x * invpio2 + 0x1.8p63L;
    fn = fn - 0x1.8p63L;
    const int n = static_cast<int>(fn);
    long double r = x - fn * TrigLongPio21;
    long double w = fn * pio21t;
    y[0] = r - w;
    if (exponent - ExtendedExponent(y[0]) > 22) {
        long double t = r;
        w = fn * TrigLongPio22;
        r = t - w;
        w = fn * pio22t - ((t - r) - w);
        y[0] = r - w;
        if (exponent - ExtendedExponent(y[0]) > 61) {
            t = r;
            w = fn * TrigLongPio23;
            r = t - w;
            w = fn * pio23t - ((t - r) - w);
            y[0] = r - w;
        }
    }
    y[1] = (r - y[0]) - w;
    return n;
}

static int RemPio2lLarge(long double x, long double* y) {
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    const int e0 = exponent - 0x3fff - 23;
    long double z = JoinExtended(parts.mantissa, static_cast<std::uint16_t>(exponent - e0));
    double tx[3];
    double ty[2];
    for (int i = 0; i < 2; i++) {
        tx[i] = static_cast<double>(static_cast<std::int32_t>(z));
        z = (z - tx[i]) * TrigLongTwo24;
    }
    tx[2] = static_cast<double>(z);
    int nx = 3;
    while (tx[nx - 1] == 0.0) nx--;
    const int n = KernelRemPio2(tx, ty, e0, nx, 2);
    const long double r = static_cast<long double>(ty[0]) + ty[1];
    const long double w = ty[1] - (r - ty[0]);
    if ((parts.signExponent & 0x8000) != 0) {
        y[0] = -r;
        y[1] = -w;
        return -n;
    }
    y[0] = r;
    y[1] = w;
    return n;
}

static int RemPio2l(long double x, long double* y) {
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    if (exponent < 0x3fff + 25 || (exponent == 0x3fff + 25 && (parts.mantissa >> 32) < 0xc90fdaa2u)) return RemPio2lMedium(x, y);
    return RemPio2lLarge(x, y);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/k_sinl.c and k_cosl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/k_sinl.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2008 Steven G. Kargl, David Schultz, Bruce D. Evans.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
static const volatile double TrigLongS1Hi = -0.16666666666666666;
static const volatile double TrigLongS1Lo = -9.2563760475949941e-18;
static constexpr double TrigLongS2 = 0.0083333333333333332;
static constexpr double TrigLongS3 = -0.00019841269841269427;
static constexpr double TrigLongS4 = 0.0000027557319223597490;
static constexpr double TrigLongS5 = -0.000000025052108218074604;
static constexpr double TrigLongS6 = 1.6059006598854211e-10;
static constexpr double TrigLongS7 = -7.6429779983024564e-13;
static constexpr double TrigLongS8 = 2.6174587166648325e-15;

static long double KernelSinl(long double x, long double y, int iy) {
    const long double s1 = static_cast<long double>(TrigLongS1Hi) + TrigLongS1Lo;
    const long double z = x * x;
    const long double v = z * x;
    const long double r = TrigLongS2 + z * (TrigLongS3 + z * (TrigLongS4 + z * (TrigLongS5 + z * (TrigLongS6 + z * (TrigLongS7 + z * TrigLongS8)))));
    if (iy == 0) return x + v * (s1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * s1);
}

static const volatile double TrigLongC1Hi = 0.041666666666666664;
static const volatile double TrigLongC1Lo = 2.2598839032744733e-18;
static constexpr double TrigLongC2 = -0.0013888888888888874;
static constexpr double TrigLongC3 = 0.000024801587301571716;
static constexpr double TrigLongC4 = -0.00000027557319215507120;
static constexpr double TrigLongC5 = 0.0000000020876754400407278;
static constexpr double TrigLongC6 = -1.1470297442401303e-11;
static constexpr double TrigLongC7 = 4.7383039476436467e-14;

static long double KernelCosl(long double x, long double y) {
    const long double c1 = static_cast<long double>(TrigLongC1Hi) + TrigLongC1Lo;
    const long double z = x * x;
    const long double r = z * (c1 + z * (TrigLongC2 + z * (TrigLongC3 + z * (TrigLongC4 + z * (TrigLongC5 + z * (TrigLongC6 + z * TrigLongC7))))));
    const long double hz = 0.5 * z;
    const long double w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/k_tanl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/k_tanl.c */
/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 * Copyright (c) 2008 Steven G. Kargl, David Schultz, Bruce D. Evans.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
static const volatile double TrigLongT3Hi = 0.33333333333333331;
static const volatile double TrigLongT3Lo = 1.8350121769317163e-17;
static const volatile double TrigLongT5Hi = 0.13333333333333336;
static const volatile double TrigLongT5Lo = 1.3051083651294260e-17;
static const volatile double TrigLongT7Hi = 0.053968253968250494;
static const volatile double TrigLongT7Lo = 3.1509625637859973e-18;
static const volatile double TrigLongPio4Hi = 0.78539816339744828;
static const volatile double TrigLongPio4Lo = 3.0628711372715500e-17;
static const volatile double TrigLongPio4LoHi = -1.2541394031670831e-20;
static const volatile double TrigLongPio4LoLo = 6.1493048227390915e-37;
static constexpr double TrigLongT9 = 0.021869488536312216;
static constexpr double TrigLongT11 = 0.0088632355256619590;
static constexpr double TrigLongT13 = 0.0035921281113786528;
static constexpr double TrigLongT15 = 0.0014558334756312418;
static constexpr double TrigLongT17 = 0.00059003538700862256;
static constexpr double TrigLongT19 = 0.00023907843576635544;
static constexpr double TrigLongT21 = 0.000097154625656538905;
static constexpr double TrigLongT23 = 0.000038440165747303162;
static constexpr double TrigLongT25 = 0.000018082171885432524;
static constexpr double TrigLongT27 = 0.0000024196006108814377;
static constexpr double TrigLongT29 = 0.0000078293456938132840;
static constexpr double TrigLongT31 = -0.0000032609076735050182;
static constexpr double TrigLongT33 = 0.0000023261313142559411;

static long double KernelTanl(long double x, long double y, int iy) {
    const long double t3 = static_cast<long double>(TrigLongT3Hi) + TrigLongT3Lo;
    const long double t5 = static_cast<long double>(TrigLongT5Hi) + TrigLongT5Lo;
    const long double t7 = static_cast<long double>(TrigLongT7Hi) + TrigLongT7Lo;
    const long double pio4 = static_cast<long double>(TrigLongPio4Hi) + TrigLongPio4Lo;
    const long double pio4lo = static_cast<long double>(TrigLongPio4LoHi) + TrigLongPio4LoLo;
    iy = iy == 1 ? -1 : 1;
    const long double osign = x >= 0 ? 1.0 : -1.0;
    const bool folded = std::fabs(x) >= 0.67434;
    if (folded) {
        if (x < 0) {
            x = -x;
            y = -y;
        }
        const long double high = pio4 - x;
        const long double low = pio4lo - y;
        x = high + low;
        y = 0.0;
    }
    long double z = x * x;
    long double w = z * z;
    long double r = t5 + w * (TrigLongT9 + w * (TrigLongT13 + w * (TrigLongT17 + w * (TrigLongT21 + w * (TrigLongT25 + w * (TrigLongT29 + w * TrigLongT33))))));
    long double v = z * (t7 + w * (TrigLongT11 + w * (TrigLongT15 + w * (TrigLongT19 + w * (TrigLongT23 + w * (TrigLongT27 + w * TrigLongT31))))));
    long double s = z * x;
    r = y + z * (s * (r + v) + y);
    r += t3 * s;
    w = x + r;
    if (folded) {
        v = static_cast<long double>(iy);
        return osign * (v - 2.0 * (x - (w * w / (w + v) - r)));
    }
    if (iy == 1) return w;
    z = w;
    z = z + 0x1p32 - 0x1p32;
    v = r - (z - x);
    long double t = -1.0 / w;
    const long double a = t;
    t = t + 0x1p32 - 0x1p32;
    s = 1.0 + t * z;
    return t + a * (s + t * v);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_sinl.c, s_cosl.c and s_tanl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_sinl.c */
/*-
 * Copyright (c) 2007 Steven G. Kargl
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
static constexpr double TrigLongPio4 = 0.78539816339744830962;

}
