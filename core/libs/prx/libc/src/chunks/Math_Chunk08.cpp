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


float APS5_VABI coshf_nid_postfix(float x) {
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return x * x;
    const float absolute = std::fabs(x);
    if (magnitude < 0x3eb17218u) {
        const float t = SinhfExpm1(absolute);
        const float w = 1.0f + t;
        if (magnitude < 0x39800000u) return 1.0f;
        return 1.0f + (t * t) / (w + w);
    }
    if (magnitude < 0x41100000u) {
        const float t = SinhfExp(absolute);
        return 0.5f * t + 0.5f / t;
    }
    if (magnitude < 0x42b17217u) return 0.5f * SinhfExp(absolute);
    if (magnitude <= 0x42b2d4fcu) {
        const float w = SinhfExp(0.5f * absolute);
        const float t = 0.5f * w;
        return t * w;
    }
    volatile float huge = 1.0e30f;
    return huge * huge;
}

float APS5_VABI sinhf_nid_postfix(float x) {
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return x + x;
    const float half = std::signbit(x) ? -0.5f : 0.5f;
    const float absolute = std::fabs(x);
    if (magnitude < 0x41100000u) {
        if (magnitude < 0x39800000u && 1.0e37f + x > 1.0f) return x;
        const float t = SinhfExpm1(absolute);
        if (magnitude < 0x3f800000u) return half * (2.0f * t - t * t / (t + 1.0f));
        return half * (t + t / (t + 1.0f));
    }
    if (magnitude < 0x42b17217u) return half * SinhfExp(absolute);
    if (magnitude <= 0x42b2d4fcu) {
        const float w = SinhfExp(0.5f * absolute);
        return half * w * w;
    }
    errno = ERANGE;
    return x * 1.0e37f;
}

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
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_asinh.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_asinh.c */
double APS5_VABI asinh_nid_postfix(double x) {
    const double one = 1.0;
    const double ln2 = 6.93147180559945286227e-01;
    const double huge = 1.0e300;
    const std::int32_t hx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x7ff00000) return x + x;
    if (ix < 0x3e300000 && huge + x > one) return x;
    double w;
    if (ix > 0x41b00000) {
        w = log_nid_postfix(std::fabs(x)) + ln2;
    } else if (ix > 0x40000000) {
        const double t = std::fabs(x);
        w = log_nid_postfix(2.0 * t + one / (std::sqrt(x * x + one) + t));
    } else {
        const double t = x * x;
        w = log1p_nid_postfix(std::fabs(x) + t / (one + std::sqrt(one + t)));
    }
    return hx > 0 ? w : -w;
}
float APS5_VABI asinhf_nid_postfix(float x) {
    constexpr float ln2 = 6.9314718246e-01f;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return x + x;
    if (magnitude < 0x31800000u) {
        volatile float huge = 1.0e30f;
        if (huge + x > 1.0f) return x;
    }
    const float absolute = std::fabs(x);
    float result;
    if (magnitude > 0x4d800000u) {
        result = logf_nid_postfix(absolute) + ln2;
    } else if (magnitude > 0x40000000u) {
        result = logf_nid_postfix(2.0f * absolute + 1.0f / (std::sqrt(x * x + 1.0f) + absolute));
    } else {
        const float square = x * x;
        result = AtanhfLog1p(absolute + square / (1.0f + std::sqrt(1.0f + square)));
    }
    return std::copysign(result, x);
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
 *
 */
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_acosh.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_acosh.c */
double APS5_VABI acosh_nid_postfix(double x) {
    const double one = 1.0;
    const double ln2 = 6.93147180559945286227e-01;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(bits);
    if (hx < 0x3ff00000) {
        if (!std::isnan(x) || std::signbit(x)) _Feraise_nid_postfix(0x01);
        return (x - x) / (x - x);
    }
    if (hx >= 0x41b00000) {
        if (hx >= 0x7ff00000) return x + x;
        return log_nid_postfix(x) + ln2;
    }
    if (((hx - 0x3ff00000) | lx) == 0) return 0.0;
    if (hx > 0x40000000) {
        const double t = x * x;
        return log_nid_postfix(2.0 * x - one / (x + std::sqrt(t - one)));
    }
    const double t = x - one;
    return log1p_nid_postfix(t + std::sqrt(2.0 * t + t * t));
}
float APS5_VABI acoshf_nid_postfix(float x) {
    constexpr float ln2 = 6.9314718246e-01f;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t magnitude = bits & 0x7fffffffu;
    if ((bits & 0x80000000u) != 0 || magnitude < 0x3f800000u) {
        _Feraise_nid_postfix(0x01);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (magnitude >= 0x7f800000u) return x + x;
    if (magnitude == 0x3f800000u) return 0.0f;
    if (magnitude >= 0x4d800000u) return logf_nid_postfix(x) + ln2;
    if (magnitude > 0x40000000u) {
        const float square = x * x;
        return logf_nid_postfix(2.0f * x + (-1.0f / (x + std::sqrt(square - 1.0f))));
    }
    const float t = x - 1.0f;
    return AtanhfLog1p(t + std::sqrt(2.0f * t + t * t));
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_atanh.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_atanh.c */
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
double APS5_VABI atanh_nid_postfix(double x) {
    const double huge = 1e300;
    static volatile double zero = 0.0;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    const std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(bits);
    const std::int32_t ix = hx & 0x7fffffff;
    if ((static_cast<std::uint32_t>(ix) | ((lx | (0u - lx)) >> 31)) > 0x3ff00000u) {
        _Feraise_nid_postfix(0x01);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (ix == 0x3ff00000) {
        _Feraise_nid_postfix(0x04);
        errno = ERANGE;
        return x / zero;
    }
    if (ix < 0x3e300000 && huge + x > 0.0) {
        return x;
    }
    x = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(ix)) << 32) | lx);
    double t;
    if (ix < 0x3fe00000) {
        t = x + x;
        t = 0.5 * log1p_nid_postfix(t + t * x / (1.0 - x));
    } else {
        t = 0.5 * log1p_nid_postfix((x + x) / (1.0 - x));
    }
    return hx >= 0 ? t : -t;
}

float APS5_VABI atanhf_nid_postfix(float x) {
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude > 0x3f800000u) {
        std::feraiseexcept(FE_INVALID);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (magnitude == 0x3f800000u) {
        errno = ERANGE;
        return x / 0.0f;
    }
    if (magnitude < 0x31800000u && 1.0e30f + x > 0.0f) return x;
    const float absolute = std::fabs(x);
    const float twice = absolute + absolute;
    const float t = magnitude < 0x3f000000u ? 0.5f * AtanhfLog1p(twice + twice * absolute / (1.0f - absolute))
                                            : 0.5f * AtanhfLog1p(twice / (1.0f - absolute));
    return std::signbit(x) ? -t : t;
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_sincos.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_sincos.c */
/*-
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 * 
 * s_sin.c and s_cos.c merged by Steven G. Kargl.  Descriptions of the
 * algorithms are contained in the original files.
 */
void APS5_VABI sincos_nid_postfix(double x, double* sinp, double* cosp) {
    const std::int32_t ix = HighWord(x) & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e400000 && static_cast<int>(x) == 0) {
            *sinp = x;
            *cosp = 1;
            return;
        }
        *sinp = KernelSin(x, 0, 0);
        *cosp = KernelCos(x, 0);
        return;
    }
    if (ix >= 0x7ff00000) {
        if (ix == 0x7ff00000) {
            errno = EDOM;
        }
        *sinp = x - x;
        *cosp = x - x;
        return;
    }
    double y[2];
    switch (RemPio2(x, y) & 3) {
        case 0:
            *sinp = KernelSin(y[0], y[1], 1);
            *cosp = KernelCos(y[0], y[1]);
            break;
        case 1:
            *sinp = KernelCos(y[0], y[1]);
            *cosp = -KernelSin(y[0], y[1], 1);
            break;
        case 2:
            *sinp = -KernelSin(y[0], y[1], 1);
            *cosp = -KernelCos(y[0], y[1]);
            break;
        default:
            *sinp = -KernelCos(y[0], y[1]);
            *cosp = KernelSin(y[0], y[1], 1);
    }
}

float APS5_VABI atanf_nid_postfix(float x) {
    static constexpr float atanhi[] = {static_cast<float>(4.6364760399e-01), static_cast<float>(7.8539812565e-01), static_cast<float>(9.8279368877e-01), static_cast<float>(1.5707962513e+00)};
    static constexpr float atanlo[] = {static_cast<float>(5.0121582440e-09), static_cast<float>(3.7748947079e-08), static_cast<float>(3.4473217170e-08), static_cast<float>(7.5497894159e-08)};
    static constexpr float aT[] = {static_cast<float>(3.3333328366e-01), static_cast<float>(-1.9999158382e-01), static_cast<float>(1.4253635705e-01), static_cast<float>(-1.0648017377e-01), static_cast<float>(6.1687607318e-02)};
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x4c800000) {
        if (ix > 0x7f800000) {
            return x + x;
        }
        const volatile float lo = atanlo[3];
        return hx > 0 ? atanhi[3] + lo : -atanhi[3] - lo;
    }
    int id = -1;
    if (ix < 0x3ee00000) {
        if (ix < 0x39800000) {
            return x;
        }
    } else {
        x = std::fabs(x);
        if (ix < 0x3f300000) {
            id = 0;
            x = (2.0f * x - 1.0f) / (2.0f + x);
        } else if (ix < 0x3f980000) {
            id = 1;
            x = (x - 1.0f) / (x + 1.0f);
        } else if (ix < 0x401c0000) {
            id = 2;
            x = (x - 1.5f) / (1.0f + 1.5f * x);
        } else {
            id = 3;
            x = -1.0f / x;
        }
    }
    const float z = x * x;
    const float w = z * z;
    const float s1 = z * (aT[0] + w * (aT[2] + w * aT[4]));
    const float s2 = w * (aT[1] + w * aT[3]);
    if (id < 0) {
        return x - x * (s1 + s2);
    }
    const float result = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
    return hx < 0 ? -result : result;
}

float APS5_VABI atan2f_nid_postfix(float y, float x) {
    static volatile float tiny = 1.0e-30f;
    static volatile float pi_lo = static_cast<float>(-8.7422776573e-08);
    const float pi_o_4 = static_cast<float>(7.8539818525e-01);
    const float pi_o_2 = static_cast<float>(1.5707963705e+00);
    const float pi = static_cast<float>(3.1415927410e+00);
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    const std::int32_t hy = std::bit_cast<std::int32_t>(y);
    const std::int32_t iy = hy & 0x7fffffff;
    if (iy > 0x7f800000) {
        return y + y;
    }
    if (ix > 0x7f800000) {
        return x + x;
    }
    if (hx == 0x3f800000) {
        return atanf_nid_postfix(y);
    }
    int m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
    if (iy == 0) {
        if (m < 2) {
            return y;
        }
        return m == 2 ? pi + tiny : -pi - tiny;
    }
    if (ix == 0) {
        return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    }
    if (ix == 0x7f800000) {
        if (iy == 0x7f800000) {
            switch (m) {
                case 0: return pi_o_4 + tiny;
                case 1: return -pi_o_4 - tiny;
                case 2: return 3.0f * pi_o_4 + tiny;
                default: return -3.0f * pi_o_4 - tiny;
            }
        }
        switch (m) {
            case 0: return 0.0f;
            case 1: return -0.0f;
            case 2: return pi + tiny;
            default: return -pi - tiny;
        }
    }
    if (iy == 0x7f800000) {
        return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    }
    const std::int32_t k = (iy - ix) >> 23;
    float z;
    if (k > 26) {
        z = pi_o_2 + 0.5f * pi_lo;
        m &= 1;
    } else if (k < -26 && hx < 0) {
        z = 0.0f;
    } else {
        z = atanf_nid_postfix(std::fabs(y / x));
    }
    switch (m) {
        case 0: return z;
        case 1: return -z;
        case 2: return pi - (z - pi_lo);
        default: return (z - pi_lo) - pi;
    }
}
double APS5_VABI atan2_nid_postfix(double y, double x) {
    static volatile double tiny = 1.0e-300;
    static volatile double pi_lo = 1.2246467991473531772E-16;
    const double pi_o_4 = 7.8539816339744827900E-01;
    const double pi_o_2 = 1.5707963267948965580E+00;
    const double pi = 3.1415926535897931160E+00;
    const std::uint64_t xbits = std::bit_cast<std::uint64_t>(x);
    const std::uint64_t ybits = std::bit_cast<std::uint64_t>(y);
    const std::int32_t hx = static_cast<std::int32_t>(xbits >> 32);
    const std::int32_t hy = static_cast<std::int32_t>(ybits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(xbits);
    const std::uint32_t ly = static_cast<std::uint32_t>(ybits);
    const std::int32_t ix = hx & 0x7fffffff;
    const std::int32_t iy = hy & 0x7fffffff;
    if ((static_cast<std::uint32_t>(ix) | ((lx | (0u - lx)) >> 31)) > 0x7ff00000u ||
        (static_cast<std::uint32_t>(iy) | ((ly | (0u - ly)) >> 31)) > 0x7ff00000u) {
        volatile double sum = x + y;
        (void)sum;
        return std::bit_cast<double>(((ybits & 0x7fffffffffffffffull) > 0x7ff0000000000000ull ? ybits : xbits) | 0x0008000000000000ull);
    }
    if (hx == 0x3ff00000 && lx == 0) {
        return atan_nid_postfix(y);
    }
    int m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
    if ((static_cast<std::uint32_t>(iy) | ly) == 0) {
        if (m < 2) {
            return y;
        }
        return m == 2 ? pi + tiny : -pi - tiny;
    }
    if ((static_cast<std::uint32_t>(ix) | lx) == 0) {
        return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    }
    if (ix == 0x7ff00000) {
        if (iy == 0x7ff00000) {
            switch (m) {
                case 0: return pi_o_4 + tiny;
                case 1: return -pi_o_4 - tiny;
                case 2: return 3.0 * pi_o_4 + tiny;
                default: return -3.0 * pi_o_4 - tiny;
            }
        }
        switch (m) {
            case 0: return 0.0;
            case 1: return -0.0;
            case 2: return pi + tiny;
            default: return -pi - tiny;
        }
    }
    if (iy == 0x7ff00000) {
        return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    }
    const std::int32_t k = (iy - ix) >> 20;
    double z;
    if (k > 60) {
        z = pi_o_2 + 0.5 * pi_lo;
        m &= 1;
    } else if (hx < 0 && k < -60) {
        z = 0.0;
    } else {
        z = atan_nid_postfix(std::fabs(y / x));
    }
    switch (m) {
        case 0: return z;
        case 1: return -z;
        case 2: return pi - (z - pi_lo);
        default: return (z - pi_lo) - pi;
    }
}
static inline std::int32_t PowfWord(float f) { return std::bit_cast<std::int32_t>(f); }
static inline float PowfFloat(std::int32_t w) { return std::bit_cast<float>(w); }

}
