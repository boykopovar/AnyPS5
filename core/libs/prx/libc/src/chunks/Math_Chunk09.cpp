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


float APS5_VABI powf_nid_postfix(float x, float y) {
    static const float bp[] = {1.0f, 1.5f}, dp_h[] = {0.0f, static_cast<float>(5.84960938e-01)}, dp_l[] = {0.0f, static_cast<float>(1.56322085e-06)};
    const float zero = 0.0f, half = 0.5f, qrtr = 0.25f, thrd = static_cast<float>(3.33333343e-01), one = 1.0f, two = 2.0f, two24 = 16777216.0f;
    const float huge = 1.0e30f;
    static const volatile float tiny = 1.0e-30f;
    const float L1 = static_cast<float>(6.0000002384e-01), L2 = static_cast<float>(4.2857143283e-01), L3 = static_cast<float>(3.3333334327e-01);
    const float L4 = static_cast<float>(2.7272811532e-01), L5 = static_cast<float>(2.3066075146e-01), L6 = static_cast<float>(2.0697501302e-01);
    const float P1 = static_cast<float>(1.6666667163e-01), P2 = static_cast<float>(-2.7777778450e-03), P3 = static_cast<float>(6.6137559770e-05);
    const float P4 = static_cast<float>(-1.6533901999e-06), P5 = static_cast<float>(4.1381369442e-08);
    const float lg2 = static_cast<float>(6.9314718246e-01), lg2_h = static_cast<float>(6.93145752e-01), lg2_l = static_cast<float>(1.42860654e-06);
    const float ovt = static_cast<float>(4.2995665694e-08), cp = static_cast<float>(9.6179670095e-01), cp_h = static_cast<float>(9.6191406250e-01);
    const float cp_l = static_cast<float>(-1.1736857402e-04), ivln2 = static_cast<float>(1.4426950216e+00), ivln2_h = static_cast<float>(1.4426879883e+00);
    const float ivln2_l = static_cast<float>(7.0526075433e-06);
    float z, ax, z_h, z_l, p_h, p_l, y1, t1, t2, r, s, sn, t, u, v, w;
    std::int32_t i, j, k, yisint, n, is;
    const std::int32_t hx = PowfWord(x), hy = PowfWord(y);
    std::int32_t ix = hx & 0x7fffffff;
    const std::int32_t iy = hy & 0x7fffffff;
    if (iy == 0) return one;
    if (ix > 0x7f800000 || iy > 0x7f800000) {
        if ((ix > 0x7f800000 && (ix & 0x00400000) == 0) ||
            (iy > 0x7f800000 && (iy & 0x00400000) == 0)) std::feraiseexcept(FE_INVALID);
        return PowfFloat((ix > 0x7f800000 ? hx : hy) | 0x00400000);
    }
    yisint = 0;
    if (hx < 0) {
        if (iy >= 0x4b800000) yisint = 2;
        else if (iy >= 0x3f800000) {
            k = (iy >> 23) - 0x7f;
            j = iy >> (23 - k);
            if ((j << (23 - k)) == iy) yisint = 2 - (j & 1);
        }
    }
    if (iy == 0x7f800000) {
        if (ix == 0x3f800000) return y - y;
        else if (ix > 0x3f800000) return hy >= 0 ? y : zero;
        else return hy < 0 ? -y : zero;
    }
    if (iy == 0x3f800000) return hy < 0 ? one / x : x;
    if (hy == 0x40000000) return x * x;
    if (hy == 0x3f000000) {
        if (hx >= 0) return std::sqrt(x);
    }
    ax = std::fabs(x);
    if (ix == 0x7f800000 || ix == 0 || ix == 0x3f800000) {
        z = ax;
        if (hy < 0) z = one / z;
        if (hx < 0) {
            if (((ix - 0x3f800000) | yisint) == 0) {
                errno = EDOM;
                z = (z - z) / (z - z);
            }
            else if (yisint == 1) z = -z;
        }
        return z;
    }
    n = static_cast<std::int32_t>((static_cast<std::uint32_t>(hx) >> 31) - 1);
    if ((n | yisint) == 0) {
        errno = EDOM;
        return (x - x) / (x - x);
    }
    sn = one;
    if ((n | (yisint - 1)) == 0) sn = -one;
    if (iy > 0x4d000000) {
        if (ix < 0x3f7ffff8) return hy < 0 ? sn * huge * huge : sn * tiny * tiny;
        if (ix > 0x3f800007) return hy > 0 ? sn * huge * huge : sn * tiny * tiny;
        t = ax - 1;
        w = (t * t) * (half - t * (thrd - t * qrtr));
        u = ivln2_h * t;
        v = t * ivln2_l - w * ivln2;
        t1 = u + v;
        is = PowfWord(t1);
        t1 = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        t2 = v - (t1 - u);
    } else {
        float s2, s_h, s_l, t_h, t_l;
        n = 0;
        if (ix < 0x00800000) { ax *= two24; n -= 24; ix = PowfWord(ax); }
        n += (ix >> 23) - 0x7f;
        j = ix & 0x007fffff;
        ix = j | 0x3f800000;
        if (j <= 0x1cc471) k = 0;
        else if (j < 0x5db3d7) k = 1;
        else { k = 0; n += 1; ix -= 0x00800000; }
        ax = PowfFloat(ix);
        u = ax - bp[k];
        v = one / (ax + bp[k]);
        s = u * v;
        s_h = s;
        is = PowfWord(s_h);
        s_h = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        is = ((ix >> 1) & static_cast<std::int32_t>(0xfffff000)) | 0x20000000;
        t_h = PowfFloat(is + 0x00400000 + (k << 21));
        t_l = ax - (t_h - bp[k]);
        s_l = v * ((u - s_h * t_h) - s_h * t_l);
        s2 = s * s;
        r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += s_l * (s_h + s);
        s2 = s_h * s_h;
        t_h = 3 + s2 + r;
        is = PowfWord(t_h);
        t_h = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        t_l = r - ((t_h - 3) - s2);
        u = s_h * t_h;
        v = s_l * t_h + t_l * s;
        p_h = u + v;
        is = PowfWord(p_h);
        p_h = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        p_l = v - (p_h - u);
        z_h = cp_h * p_h;
        z_l = cp_l * p_h + p_l * cp + dp_l[k];
        t = static_cast<float>(n);
        t1 = (((z_h + z_l) + dp_h[k]) + t);
        is = PowfWord(t1);
        t1 = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
    }
    is = PowfWord(y);
    y1 = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
    p_l = (y - y1) * t1 + y * t2;
    p_h = y1 * t1;
    z = p_l + p_h;
    j = PowfWord(z);
    if (j > 0x43000000 || (j == 0x43000000 && p_l + ovt > z - p_h)) {
        errno = ERANGE;
        return sn * huge * huge;
    }
    if ((j & 0x7fffffff) > 0x43160000 || (static_cast<std::uint32_t>(j) == 0xc3160000u && p_l <= z - p_h)) {
        errno = ERANGE;
        return sn * tiny * tiny;
    }
    i = j & 0x7fffffff;
    k = (i >> 23) - 0x7f;
    n = 0;
    if (i > 0x3f000000) {
        n = j + (0x00800000 >> (k + 1));
        k = ((n & 0x7fffffff) >> 23) - 0x7f;
        t = PowfFloat(n & ~(0x007fffff >> k));
        n = ((n & 0x007fffff) | 0x00800000) >> (23 - k);
        if (j < 0) n = -n;
        p_h -= t;
    }
    t = p_l + p_h;
    is = PowfWord(t);
    t = PowfFloat(is & static_cast<std::int32_t>(0xffff8000));
    u = t * lg2_h;
    v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
    z = u + v;
    w = v - (z - u);
    t = z * z;
    t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    r = (z * t1) / (t1 - two) - (w + z * w);
    z = one - (r - z);
    j = PowfWord(z);
    j += static_cast<std::int32_t>(static_cast<std::uint32_t>(n) << 23);
    if ((j >> 23) <= 0) z = std::scalbn(z, n);
    else z = PowfFloat(j);
    return sn * z;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_pow.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_pow.c */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
static double SetLowWord(double x, std::uint32_t low) {
    return std::bit_cast<double>((std::bit_cast<std::uint64_t>(x) & 0xffffffff00000000ull) | low);
}

static double SetHighWord(double x, std::uint32_t high) {
    return std::bit_cast<double>((static_cast<std::uint64_t>(high) << 32) | (std::bit_cast<std::uint64_t>(x) & 0xffffffffull));
}

double APS5_VABI pow_nid_postfix(double x, double y) {
    static constexpr double bp[] = {1.0, 1.5};
    static constexpr double dp_h[] = {0.0, 5.84962487220764160156e-01};
    static constexpr double dp_l[] = {0.0, 1.35003920212974897128e-08};
    const double two53 = 9007199254740992.0;
    const double huge = 1.0e300;
    const double tiny = 1.0e-300;
    static const volatile double underflow = 1.0e-300;
    const double L1 = 5.99999999999994648725e-01;
    const double L2 = 4.28571428578550184252e-01;
    const double L3 = 3.33333329818377432918e-01;
    const double L4 = 2.72728123808534006489e-01;
    const double L5 = 2.30660745775561754067e-01;
    const double L6 = 2.06975017800338417784e-01;
    const double P1 = 1.66666666666666019037e-01;
    const double P2 = -2.77777777770155933842e-03;
    const double P3 = 6.61375632143793436117e-05;
    const double P4 = -1.65339022054652515390e-06;
    const double P5 = 4.13813679705723846039e-08;
    const double lg2 = 6.93147180559945286227e-01;
    const double lg2_h = 6.93147182464599609375e-01;
    const double lg2_l = -1.90465429995776804525e-09;
    const double ovt = 8.0085662595372944372e-0017;
    const double cp = 9.61796693925975554329e-01;
    const double cp_h = 9.61796700954437255859e-01;
    const double cp_l = -7.02846165095275826516e-09;
    const double ivln2 = 1.44269504088896338700e+00;
    const double ivln2_h = 1.44269502162933349609e+00;
    const double ivln2_l = 1.92596299112661746887e-08;
    const std::uint64_t xbits = std::bit_cast<std::uint64_t>(x);
    const std::uint64_t ybits = std::bit_cast<std::uint64_t>(y);
    const std::int32_t hx = static_cast<std::int32_t>(xbits >> 32);
    const std::int32_t hy = static_cast<std::int32_t>(ybits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(xbits);
    const std::uint32_t ly = static_cast<std::uint32_t>(ybits);
    std::int32_t ix = hx & 0x7fffffff;
    const std::int32_t iy = hy & 0x7fffffff;
    if ((iy | ly) == 0) {
        return 1.0;
    }
    if (ix > 0x7ff00000 || (ix == 0x7ff00000 && lx != 0) || iy > 0x7ff00000 || (iy == 0x7ff00000 && ly != 0)) {
        return (x + 0.0) + (y + 0.0);
    }
    int yisint = 0;
    if (hx < 0) {
        if (iy >= 0x43400000) {
            yisint = 2;
        } else if (iy >= 0x3ff00000) {
            const std::int32_t k = (iy >> 20) - 0x3ff;
            if (k > 20) {
                const std::uint32_t j = ly >> (52 - k);
                if ((j << (52 - k)) == ly) yisint = 2 - static_cast<int>(j & 1);
            } else if (ly == 0) {
                const std::int32_t j = iy >> (20 - k);
                if ((j << (20 - k)) == iy) yisint = 2 - (j & 1);
            }
        }
    }
    if (ly == 0) {
        if (iy == 0x7ff00000) {
            if (((ix - 0x3ff00000) | static_cast<std::int32_t>(lx)) == 0) {
                return y - y;
            }
            if (ix >= 0x3ff00000) {
                return hy >= 0 ? y : 0.0;
            }
            return hy < 0 ? -y : 0.0;
        }
        if (iy == 0x3ff00000) {
            return hy < 0 ? 1.0 / x : x;
        }
        if (hy == 0x40000000) {
            return x * x;
        }
        if (hy == 0x3fe00000 && hx >= 0) {
            return std::sqrt(x);
        }
    }
    double ax = std::fabs(x);
    if (lx == 0 && (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000)) {
        double z = ax;
        if (hy < 0) z = 1.0 / z;
        if (hx < 0) {
            if (((ix - 0x3ff00000) | yisint) == 0) {
                errno = EDOM;
                z = (z - z) / (z - z);
            } else if (yisint == 1) {
                z = -z;
            }
        }
        return z;
    }
    std::int32_t n = static_cast<std::int32_t>((static_cast<std::uint32_t>(hx) >> 31) - 1);
    if ((n | yisint) == 0) {
        errno = EDOM;
        return (x - x) / (x - x);
    }
    double s = 1.0;
    if ((n | (yisint - 1)) == 0) s = -1.0;
    double t1;
    double t2;
    if (iy > 0x41e00000) {
        if (iy > 0x43f00000) {
            if (ix <= 0x3fefffff) return hy < 0 ? std::numeric_limits<double>::infinity() : tiny * tiny;
            if (ix >= 0x3ff00000) return hy > 0 ? std::numeric_limits<double>::infinity() : tiny * tiny;
        }
        if (ix < 0x3fefffff) return hy < 0 ? s * huge * huge : s * underflow * underflow;
        if (ix > 0x3ff00000) return hy > 0 ? s * huge * huge : s * underflow * underflow;
        const double t = ax - 1.0;
        const double w = (t * t) * (0.5 - t * (3.3333333333333331e-01 - t * 0.25));
        const double u = ivln2_h * t;
        const double v = t * ivln2_l - w * ivln2;
        t1 = SetLowWord(u + v, 0);
        t2 = v - (t1 - u);
    } else {
        n = 0;
        volatile double scaled = ax * two53;
        if (ix < 0x00100000) {
            ax = scaled;
            n -= 53;
            ix = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(ax) >> 32);
        }
        n += (ix >> 20) - 0x3ff;
        const std::int32_t j = ix & 0x000fffff;
        ix = j | 0x3ff00000;
        int k;
        if (j <= 0x3988e) {
            k = 0;
        } else if (j < 0xbb67a) {
            k = 1;
        } else {
            k = 0;
            n += 1;
            ix -= 0x00100000;
        }
        ax = SetHighWord(ax, static_cast<std::uint32_t>(ix));
        double u = ax - bp[k];
        double v = 1.0 / (ax + bp[k]);
        const double ss = u * v;
        const double s_h = SetLowWord(ss, 0);
        double t_h = SetHighWord(0.0, static_cast<std::uint32_t>(((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18)));
        double t_l = ax - (t_h - bp[k]);
        const double s_l = v * ((u - s_h * t_h) - s_h * t_l);
        double s2 = ss * ss;
        double r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += s_l * (s_h + ss);
        s2 = s_h * s_h;
        t_h = SetLowWord(3 + s2 + r, 0);
        t_l = r - ((t_h - 3) - s2);
        u = s_h * t_h;
        v = s_l * t_h + t_l * ss;
        const double p_h = SetLowWord(u + v, 0);
        const double p_l = v - (p_h - u);
        const double z_h = cp_h * p_h;
        const double z_l = cp_l * p_h + p_l * cp + dp_l[k];
        const double t = n;
        t1 = SetLowWord(((z_h + z_l) + dp_h[k]) + t, 0);
        t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
    }
    const double y1 = SetLowWord(y, 0);
    const double p_l = (y - y1) * t1 + y * t2;
    double p_h = y1 * t1;
    double z = p_l + p_h;
    std::uint64_t zbits = std::bit_cast<std::uint64_t>(z);
    std::int32_t j = static_cast<std::int32_t>(zbits >> 32);
    const std::int32_t i0 = static_cast<std::int32_t>(static_cast<std::uint32_t>(zbits));
    if (j >= 0x40900000) {
        if (((j - 0x40900000) | i0) != 0 || p_l + ovt > z - p_h) {
            errno = ERANGE;
            return s * huge * huge;
        }
    } else if ((j & 0x7fffffff) >= 0x4090cc00) {
        if (((static_cast<std::uint32_t>(j) - 0xc090cc00u) | static_cast<std::uint32_t>(i0)) != 0 || p_l <= z - p_h) {
            errno = ERANGE;
            return s * underflow * underflow;
        }
    }
    const std::int32_t i = j & 0x7fffffff;
    std::int32_t k = (i >> 20) - 0x3ff;
    n = 0;
    if (i > 0x3fe00000) {
        n = j + (0x00100000 >> (k + 1));
        k = ((n & 0x7fffffff) >> 20) - 0x3ff;
        const double t = SetHighWord(0.0, static_cast<std::uint32_t>(n & ~(0x000fffff >> k)));
        n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
        if (j < 0) n = -n;
        p_h -= t;
    }
    double t = SetLowWord(p_l + p_h, 0);
    const double u = t * lg2_h;
    const double v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
    z = u + v;
    const double w = v - (z - u);
    t = z * z;
    const double t1b = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    const double r = (z * t1b) / (t1b - 2.0) - (w + z * w);
    z = 1.0 - (r - z);
    j = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(z) >> 32);
    j += static_cast<std::int32_t>(static_cast<std::uint32_t>(n) << 20);
    if ((j >> 20) <= 0) {
        z = std::scalbn(z, n);
    } else {
        z = SetHighWord(z, static_cast<std::uint32_t>(j));
    }
    return s * z;
}
}
