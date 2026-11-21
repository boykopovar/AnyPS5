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


static long double SinlMsun(long double x) {
    const int exponent = ExtendedExponent(x);
    if (exponent == 0) return x;
    if (exponent == 0x7fff) return (x - x) / (x - x);
    const long double magnitude = std::fabs(x);
    if (magnitude < TrigLongPio4) {
        const long double result = KernelSinl(magnitude, 0, 0);
        return std::signbit(x) ? -result : result;
    }
    long double y[2];
    const int quadrant = RemPio2l(x, y) & 3;
    if (quadrant == 0) return KernelSinl(y[0], y[1], 1);
    if (quadrant == 1) return KernelCosl(y[0], y[1]);
    if (quadrant == 2) return -KernelSinl(y[0], y[1], 1);
    return -KernelCosl(y[0], y[1]);
}

static long double CoslMsun(long double x) {
    const int exponent = ExtendedExponent(x);
    if (exponent == 0) return 1.0;
    if (exponent == 0x7fff) return (x - x) / (x - x);
    const long double magnitude = std::fabs(x);
    if (magnitude < TrigLongPio4) return KernelCosl(magnitude, 0);
    long double y[2];
    const int quadrant = RemPio2l(x, y) & 3;
    if (quadrant == 0) return KernelCosl(y[0], y[1]);
    if (quadrant == 1) return -KernelSinl(y[0], y[1], 1);
    if (quadrant == 2) return -KernelCosl(y[0], y[1]);
    return KernelSinl(y[0], y[1], 1);
}

static long double TanlMsun(long double x) {
    const int exponent = ExtendedExponent(x);
    if (exponent == 0) return x;
    if (exponent == 0x7fff) return (x - x) / (x - x);
    const long double magnitude = std::fabs(x);
    if (magnitude < TrigLongPio4) {
        const long double result = KernelTanl(magnitude, 0, 0);
        return std::signbit(x) ? -result : result;
    }
    long double y[2];
    const int quadrant = RemPio2l(x, y);
    return KernelTanl(y[0], y[1], quadrant & 1);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/k_sin.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/k_sin.c */
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
static double KernelSin(double x, double y, int iy) {
    const double S1 = -1.66666666666666324348e-01;
    const double S2 = 8.33333333332248946124e-03;
    const double S3 = -1.98412698298579493134e-04;
    const double S4 = 2.75573137070700676789e-06;
    const double S5 = -2.50507602534068634195e-08;
    const double S6 = 1.58969099521155010221e-10;
    const double z = x * x;
    const double w = z * z;
    const double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    const double v = z * x;
    if (iy == 0) {
        return x + v * (S1 + z * r);
    }
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/k_cos.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/k_cos.c */
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
static double KernelCos(double x, double y) {
    const double C1 = 4.16666666666666019037e-02;
    const double C2 = -1.38888888888741095749e-03;
    const double C3 = 2.48015872894767294178e-05;
    const double C4 = -2.75573143513906633035e-07;
    const double C5 = 2.08757232129817482790e-09;
    const double C6 = -1.13596475577881948265e-11;
    const double z = x * x;
    double w = z * z;
    const double r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    const double hz = 0.5 * z;
    w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_lgamma_r.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_lgamma_r.c */
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
static double LgammaSinPi(double x) {
    constexpr double pi = 3.14159265358979311600e+00;
    double y = -x;
    volatile double vz = y + 0x1p52;
    double z = vz - 0x1p52;
    if (z == y) return 0.0;
    vz = y + 0x1p50;
    std::int32_t n = static_cast<std::int32_t>(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(static_cast<double>(vz))));
    z = vz - 0x1p50;
    if (z > y) {
        z -= 0.25;
        n--;
    }
    n &= 7;
    y = y - z + n * 0.25;
    switch (n) {
        case 0: y = KernelSin(pi * y, 0.0, 0); break;
        case 1:
        case 2: y = KernelCos(pi * (0.5 - y), 0.0); break;
        case 3:
        case 4: y = KernelSin(pi * (1.0 - y), 0.0, 0); break;
        case 5:
        case 6: y = -KernelCos(pi * (y - 1.5), 0.0); break;
        default: y = KernelSin(pi * (y - 2.0), 0.0, 0); break;
    }
    return -y;
}

static double LgammaMsun(double x) {
    constexpr double half = 5.00000000000000000000e-01, one = 1.00000000000000000000e+00, pi = 3.14159265358979311600e+00;
    constexpr double a0 = 7.72156649015328655494e-02, a1 = 3.22467033424113591611e-01, a2 = 6.73523010531292681824e-02;
    constexpr double a3 = 2.05808084325167332806e-02, a4 = 7.38555086081402883957e-03, a5 = 2.89051383673415629091e-03;
    constexpr double a6 = 1.19270763183362067845e-03, a7 = 5.10069792153511336608e-04, a8 = 2.20862790713908385557e-04;
    constexpr double a9 = 1.08011567247583939954e-04, a10 = 2.52144565451257326939e-05, a11 = 4.48640949618915160150e-05;
    constexpr double tc = 1.46163214496836224576e+00, tf = -1.21486290535849611461e-01, tt = -3.63867699703950536541e-18;
    constexpr double t0 = 4.83836122723810047042e-01, t1 = -1.47587722994593911752e-01, t2 = 6.46249402391333854778e-02;
    constexpr double t3 = -3.27885410759859649565e-02, t4 = 1.79706750811820387126e-02, t5 = -1.03142241298341437450e-02;
    constexpr double t6 = 6.10053870246291332635e-03, t7 = -3.68452016781138256760e-03, t8 = 2.25964780900612472250e-03;
    constexpr double t9 = -1.40346469989232843813e-03, t10 = 8.81081882437654011382e-04, t11 = -5.38595305356740546715e-04;
    constexpr double t12 = 3.15632070903625950361e-04, t13 = -3.12754168375120860518e-04, t14 = 3.35529192635519073543e-04;
    constexpr double u0 = -7.72156649015328655494e-02, u1 = 6.32827064025093366517e-01, u2 = 1.45492250137234768737e+00;
    constexpr double u3 = 9.77717527963372745603e-01, u4 = 2.28963728064692451092e-01, u5 = 1.33810918536787660377e-02;
    constexpr double v1 = 2.45597793713041134822e+00, v2 = 2.12848976379893395361e+00, v3 = 7.69285150456672783825e-01;
    constexpr double v4 = 1.04222645593369134254e-01, v5 = 3.21709242282423911810e-03;
    constexpr double s0 = -7.72156649015328655494e-02, s1 = 2.14982415960608852501e-01, s2 = 3.25778796408930981787e-01;
    constexpr double s3 = 1.46350472652464452805e-01, s4 = 2.66422703033638609560e-02, s5 = 1.84028451407337715652e-03;
    constexpr double s6 = 3.19475326584100867617e-05;
    constexpr double r1 = 1.39200533467621045958e+00, r2 = 7.21935547567138069525e-01, r3 = 1.71933865632803078993e-01;
    constexpr double r4 = 1.86459191715652901344e-02, r5 = 7.77942496381893596434e-04, r6 = 7.32668430744625636189e-06;
    constexpr double w0 = 4.18938533204672725052e-01, w1 = 8.33333333333329678849e-02, w2 = -2.77777777728775536470e-03;
    constexpr double w3 = 7.93650558643019558500e-04, w4 = -5.95187557450339963135e-04, w5 = 8.36339918996282139126e-04;
    constexpr double w6 = -1.63092934096575273989e-03;
    static const volatile double vzero = 0.0;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::int32_t lx = static_cast<std::int32_t>(static_cast<std::uint32_t>(bits));
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x7ff00000) return x * x;
    if (ix < 0x3c700000) {
        if ((ix | lx) == 0) return one / vzero;
        return -log_nid_postfix(std::fabs(x));
    }
    double nadj = 0.0;
    if (hx < 0) {
        if (ix >= 0x43300000) return one / vzero;
        const double t = LgammaSinPi(x);
        if (t == 0.0) return one / vzero;
        nadj = log_nid_postfix(pi / std::fabs(t * x));
        x = -x;
    }
    double r;
    if ((((ix - 0x3ff00000) | lx) == 0) || (((ix - 0x40000000) | lx) == 0)) {
        r = 0.0;
    } else if (ix < 0x40000000) {
        double y;
        int i;
        if (ix <= 0x3feccccc) {
            r = -log_nid_postfix(x);
            if (ix >= 0x3FE76944) {
                y = one - x;
                i = 0;
            } else if (ix >= 0x3FCDA661) {
                y = x - (tc - one);
                i = 1;
            } else {
                y = x;
                i = 2;
            }
        } else {
            r = 0.0;
            if (ix >= 0x3FFBB4C3) {
                y = 2.0 - x;
                i = 0;
            } else if (ix >= 0x3FF3B4C4) {
                y = x - tc;
                i = 1;
            } else {
                y = x - one;
                i = 2;
            }
        }
        if (i == 0) {
            const double z = y * y;
            const double p1 = a0 + z * (a2 + z * (a4 + z * (a6 + z * (a8 + z * a10))));
            const double p2 = z * (a1 + z * (a3 + z * (a5 + z * (a7 + z * (a9 + z * a11)))));
            const double p = y * p1 + p2;
            r += p - y / 2;
        } else if (i == 1) {
            const double z = y * y;
            const double w = z * y;
            const double p1 = t0 + w * (t3 + w * (t6 + w * (t9 + w * t12)));
            const double p2 = t1 + w * (t4 + w * (t7 + w * (t10 + w * t13)));
            const double p3 = t2 + w * (t5 + w * (t8 + w * (t11 + w * t14)));
            const double p = z * p1 - (tt - w * (p2 + y * p3));
            r += tf + p;
        } else {
            const double p1 = y * (u0 + y * (u1 + y * (u2 + y * (u3 + y * (u4 + y * u5)))));
            const double p2 = one + y * (v1 + y * (v2 + y * (v3 + y * (v4 + y * v5))));
            r += p1 / p2 - y / 2;
        }
    } else if (ix < 0x40200000) {
        const int i = static_cast<int>(x);
        const double y = x - i;
        const double p = y * (s0 + y * (s1 + y * (s2 + y * (s3 + y * (s4 + y * (s5 + y * s6))))));
        const double q = one + y * (r1 + y * (r2 + y * (r3 + y * (r4 + y * (r5 + y * r6)))));
        r = y / 2 + p / q;
        double z = one;
        switch (i) {
            case 7: z *= (y + 6); [[fallthrough]];
            case 6: z *= (y + 5); [[fallthrough]];
            case 5: z *= (y + 4); [[fallthrough]];
            case 4: z *= (y + 3); [[fallthrough]];
            case 3:
                z *= (y + 2);
                r += log_nid_postfix(z);
                break;
        }
    } else if (ix < 0x43700000) {
        const double t = log_nid_postfix(x);
        const double z = one / x;
        const double y = z * z;
        const double w = w0 + z * (w1 + y * (w2 + y * (w3 + y * (w4 + y * (w5 + y * w6)))));
        r = (x - half) * (t - one) + w;
    } else {
        r = x * (log_nid_postfix(x) - one);
    }
    if (hx < 0) r = nadj - r;
    return r;
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/k_tan.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/k_tan.c */
/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
static double KernelTan(double x, double y, int iy) {
    static constexpr double T[] = {
        3.33333333333334091986e-01, 1.33333333333201242699e-01, 5.39682539762260521377e-02, 2.18694882948595424599e-02,
        8.86323982359930005737e-03, 3.59207910759131235356e-03, 1.45620945432529025516e-03, 5.88041240820264096874e-04,
        2.46463134818469906812e-04, 7.81794442939557092300e-05, 7.14072491382608190305e-05, -1.85586374855275456654e-05,
        2.59073051863633712884e-05,
    };
    const double pio4 = 7.85398163397448278999e-01;
    const double pio4lo = 3.06161699786838301793e-17;
    const std::int32_t hx = HighWord(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x3fe59428) {
        if (hx < 0) {
            x = -x;
            y = -y;
        }
        const double z = pio4 - x;
        const double w = pio4lo - y;
        x = z + w;
        y = 0.0;
    }
    const double z = x * x;
    double w = z * z;
    double r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
    double v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
    double s = z * x;
    r = y + z * (s * (r + v) + y);
    r += T[0] * s;
    w = x + r;
    if (ix >= 0x3fe59428) {
        v = static_cast<double>(iy);
        return static_cast<double>(1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
    }
    if (iy == 1) {
        return w;
    }
    const double head = ClearLowWord(w);
    v = r - (head - x);
    const double a = -1.0 / w;
    const double t = ClearLowWord(a);
    s = 1.0 + t * head;
    return t + a * (s + t * v);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_sin.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_sin.c */
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
double APS5_VABI sin_nid_postfix(double x) {
    const std::int32_t ix = HighWord(x) & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e500000 && static_cast<int>(x) == 0) {
            return x;
        }
        return KernelSin(x, 0.0, 0);
    }
    if (ix >= 0x7ff00000) {
        if (ix == 0x7ff00000) {
            errno = EDOM;
        }
        return x - x;
    }
    double y[2];
    switch (RemPio2(x, y) & 3) {
        case 0: return KernelSin(y[0], y[1], 1);
        case 1: return KernelCos(y[0], y[1]);
        case 2: return -KernelSin(y[0], y[1], 1);
        default: return -KernelCos(y[0], y[1]);
    }
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_cos.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_cos.c */
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
double APS5_VABI cos_nid_postfix(double x) {
    const std::int32_t ix = HighWord(x) & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e46a09e && static_cast<int>(x) == 0) {
            return 1.0;
        }
        return KernelCos(x, 0.0);
    }
    if (ix >= 0x7ff00000) {
        if (ix == 0x7ff00000) {
            errno = EDOM;
        }
        return x - x;
    }
    double y[2];
    switch (RemPio2(x, y) & 3) {
        case 0: return KernelCos(y[0], y[1]);
        case 1: return -KernelSin(y[0], y[1], 1);
        case 2: return -KernelCos(y[0], y[1]);
        default: return KernelSin(y[0], y[1], 1);
    }
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_tan.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_tan.c */
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
double APS5_VABI tan_nid_postfix(double x) {
    const std::int32_t ix = HighWord(x) & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e400000 && static_cast<int>(x) == 0) {
            return x;
        }
        return KernelTan(x, 0.0, 1);
    }
    if (ix >= 0x7ff00000) {
        if (ix == 0x7ff00000) {
            errno = EDOM;
        }
        return x - x;
    }
    double y[2];
    const int n = RemPio2(x, y);
    return KernelTan(y[0], y[1], 1 - ((n & 1) << 1));
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
/* Adapted from FreeBSD release/9.0.0 lib/msun/src/e_sinh.c: https://github.com/freebsd/freebsd-src/blob/release/9.0.0/lib/msun/src/e_sinh.c */

double APS5_VABI sinh_nid_postfix(double x) {
    const double one = 1.0;
    const double shuge = 1.0e307;
    const std::int32_t jx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ix = jx & 0x7fffffff;
    if (ix >= 0x7ff00000) return x + x;
    const double h = jx < 0 ? -0.5 : 0.5;
    if (ix < 0x40360000) {
        if (ix < 0x3e300000 && shuge + x > one) return x;
        const double t = Expm1(std::fabs(x));
        if (ix < 0x3ff00000) return h * (2.0 * t - t * t / (t + one));
        return h * (t + t / (t + one));
    }
    if (ix < 0x40862e42) return h * exp_nid_postfix(std::fabs(x));
    const std::uint32_t lx = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x));
    if (ix < 0x408633ce || (ix == 0x408633ce && lx <= 0x8fb9f87du)) {
        const double w = exp_nid_postfix(0.5 * std::fabs(x));
        const double t = h * w;
        return t * w;
    }
    errno = ERANGE;
    return x * shuge;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_cosh.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_cosh.c */
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
double APS5_VABI cosh_nid_postfix(double x) {
    static volatile double huge = 1.0e+300;
    const std::uint32_t ix = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x) >> 32) & 0x7fffffffu;
    if (ix >= 0x7ff00000u) return x * x;
    const double absolute = std::fabs(x);
    if (ix < 0x3fd62e43u) {
        const double t = expm1_nid_postfix(absolute);
        const double w = 1.0 + t;
        if (ix < 0x3c800000u) return w;
        return 1.0 + (t * t) / (w + w);
    }
    if (ix < 0x40360000u) {
        const double t = exp_nid_postfix(absolute);
        return 0.5 * t + 0.5 / t;
    }
    if (ix < 0x40862e42u) return 0.5 * exp_nid_postfix(absolute);
    double result;
    const std::uint32_t lx = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x));
    if (ix < 0x408633ceu || (ix == 0x408633ceu && lx <= 0x8fb9f87du)) {
        const double w = exp_nid_postfix(0.5 * absolute);
        result = 0.5 * w * w;
    } else {
        result = huge * huge;
    }
    if (std::isinf(result)) errno = ERANGE;
    return result;
}
}
