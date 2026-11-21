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


float APS5_VABI __powisf2_nid_postfix(float base, int exponent) {
    const bool reciprocal = exponent < 0;
    float result = 1.0f;
    unsigned int magnitude = reciprocal ? 0u - static_cast<unsigned int>(exponent) : static_cast<unsigned int>(exponent);
    while (magnitude != 0u) {
        if ((magnitude & 1u) != 0u) result *= base;
        magnitude >>= 1u;
        if (magnitude == 0u) break;
        base *= base;
    }
    return reciprocal ? 1.0f / result : result;
}
double APS5_VABI __powidf2_nid_postfix(double base, int exponent) {
    const bool reciprocal = exponent < 0;
    double result = 1.0;
    unsigned int magnitude = reciprocal ? 0u - static_cast<unsigned int>(exponent) : static_cast<unsigned int>(exponent);
    while (magnitude != 0u) {
        if ((magnitude & 1u) != 0u) result *= base;
        magnitude >>= 1u;
        if (magnitude == 0u) break;
        base *= base;
    }
    return reciprocal ? 1.0 / result : result;
}
float APS5_VABI expf_nid_postfix(float x) { return std::exp(x); }
float APS5_VABI logf_nid_postfix(float x) {
    const float ln2_hi = static_cast<float>(6.9313812256e-01);
    const float ln2_lo = static_cast<float>(9.0580006145e-06);
    const float two25 = static_cast<float>(3.355443200e+07);
    const float Lg1 = 0xaaaaaa.0p-24f;
    const float Lg2 = 0xccce13.0p-25f;
    const float Lg3 = 0x91e9ee.0p-25f;
    const float Lg4 = 0xf89e26.0p-26f;
    static volatile float vzero = 0.0f;
    std::int32_t k = 0;
    std::int32_t ix = std::bit_cast<std::int32_t>(x);
    if (ix < 0x00800000) {
        if ((ix & 0x7fffffff) == 0) {
            errno = ERANGE;
            return -two25 / vzero;
        }
        if (ix < 0) {
            errno = EDOM;
            return (x - x) / 0.0f;
        }
        k -= 25;
        x *= two25;
        ix = std::bit_cast<std::int32_t>(x);
    }
    if (ix >= 0x7f800000) {
        return x + x;
    }
    k += (ix >> 23) - 127;
    ix &= 0x007fffff;
    std::int32_t i = (ix + (0x95f64 << 3)) & 0x800000;
    x = std::bit_cast<float>(ix | (i ^ 0x3f800000));
    k += i >> 23;
    const float f = x - 1.0f;
    const float dk = static_cast<float>(k);
    if ((0x007fffff & (0x8000 + ix)) < 0xc000) {
        if (f == 0.0f) {
            return k == 0 ? 0.0f : dk * ln2_hi + dk * ln2_lo;
        }
        const float R = f * f * (0.5f - static_cast<float>(0.33333333333333333) * f);
        return k == 0 ? f - R : dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    const float s = f / (2.0f + f);
    const float z = s * s;
    i = ix - (0x6147a << 3);
    const float w = z * z;
    const std::int32_t j = (0x6b851 << 3) - ix;
    const float t1 = w * (Lg2 + w * Lg4);
    const float t2 = z * (Lg1 + w * Lg3);
    i |= j;
    const float R = t2 + t1;
    if (i > 0) {
        const float hfsq = 0.5f * f * f;
        return k == 0 ? f - (hfsq - s * (hfsq + R)) : dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    return k == 0 ? f - s * (f - R) : dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}
/* Adapted from FreeBSD release/9.0.0 lib/msun/src/e_log2f.c: https://github.com/freebsd/freebsd-src/blob/release/9.0.0/lib/msun/src/e_log2f.c */
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
float APS5_VABI log2f_nid_postfix(float x) {
    const float ivln2hi = static_cast<float>(1.4428710938e+00);
    const float ivln2lo = static_cast<float>(-1.7605285393e-04);
    std::int32_t k;
    float special;
    if (!Log9Reduce(x, k, special)) {
        return special;
    }
    const float f = Log9Kernel(x);
    x = x - 1.0f;
    const float hi = std::bit_cast<float>(std::bit_cast<std::int32_t>(x) & static_cast<std::int32_t>(0xfffff000));
    const float lo = x - hi;
    return (x + f) * ivln2lo + (lo + f) * ivln2hi + hi * ivln2hi + k;
}
static double KernelLog(double x) {
    const double Lg1 = 6.666666666666735130e-01;
    const double Lg2 = 3.999999999940941908e-01;
    const double Lg3 = 2.857142874366239149e-01;
    const double Lg4 = 2.222219843214978396e-01;
    const double Lg5 = 1.818357216161805012e-01;
    const double Lg6 = 1.531383769920937332e-01;
    const double Lg7 = 1.479819860511658591e-01;
    std::int32_t hx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const double f = x - 1.0;
    if ((0x000fffff & (2 + hx)) < 3) {
        if (f == 0.0) {
            return 0.0;
        }
        return f * f * (0.33333333333333333 * f - 0.5);
    }
    const double s = f / (2.0 + f);
    const double z = s * s;
    hx &= 0x000fffff;
    std::int32_t i = hx - 0x6147a;
    const double w = z * z;
    const std::int32_t j = 0x6b851 - hx;
    const double t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    const double t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i |= j;
    const double R = t2 + t1;
    if (i > 0) {
        const double hfsq = 0.5 * f * f;
        return s * (hfsq + R) - hfsq;
    }
    return s * (R - f);
}

double APS5_VABI log10_nid_postfix(double x) {
    const double two54 = 1.80143985094819840000e+16;
    const double ivln10hi = 4.34294481878168880939e-01;
    const double ivln10lo = 2.50829467116452752298e-11;
    const double log10_2hi = 3.01029995663611771306e-01;
    const double log10_2lo = 3.69423907715893078616e-13;
    static volatile double zero = 0.0;
    std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(bits);
    std::int32_t k = 0;
    if (hx < 0x00100000) {
        if (((hx & 0x7fffffff) | lx) == 0) {
            return -std::numeric_limits<double>::infinity();
        }
        if (hx < 0) {
            _Feraise_nid_postfix(0x01);
            return (x - x) / zero;
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
    const std::int32_t i = (hx + 0x95f64) & 0x100000;
    x = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hx | (i ^ 0x3ff00000))) << 32) | (bits & 0xffffffffu));
    k += i >> 20;
    const double y = k;
    const double f = KernelLog(x);
    x = x - 1;
    const double hi = std::bit_cast<double>(std::bit_cast<std::uint64_t>(x) & 0xffffffff00000000ull);
    const double lo = x - hi;
    const double z = y * log10_2lo + (x + f) * ivln10lo + (lo + f) * ivln10hi + hi * ivln10hi;
    return z + y * log10_2hi;
}

double APS5_VABI log2_nid_postfix(double x) {
    const double two54 = 1.80143985094819840000e+16;
    const double ivln2hi = 1.44269504072144627571e+00;
    const double ivln2lo = 1.67517131648865118353e-10;
    static volatile double zero = 0.0;
    std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::uint32_t lx = static_cast<std::uint32_t>(bits);
    std::int32_t k = 0;
    if (hx < 0x00100000) {
        if (((hx & 0x7fffffff) | lx) == 0) {
            return -std::numeric_limits<double>::infinity();
        }
        if (hx < 0) {
            _Feraise_nid_postfix(0x01);
            return (x - x) / zero;
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
    const std::int32_t i = (hx + 0x95f64) & 0x100000;
    x = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hx | (i ^ 0x3ff00000))) << 32) | (bits & 0xffffffffu));
    k += i >> 20;
    const double f = KernelLog(x);
    x = x - 1;
    const double hi = std::bit_cast<double>(std::bit_cast<std::uint64_t>(x) & 0xffffffff00000000ull);
    const double lo = x - hi;
    return (x + f) * ivln2lo + (lo + f) * ivln2hi + hi * ivln2hi + k;
}
float APS5_VABI ldexpf_nid_postfix(float x, int exp) {
    const int saved_errno = errno;
    const float result = std::scalbn(x, exp);
    errno = saved_errno;
    return result;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/s_expl.c and k_expl.h: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/s_expl.c */
/*-
 * Copyright (c) 2009-2013 Steven G. Kargl
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
 *
 * Optimized by Bruce D. Evans.
 */
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
static constexpr int ExplIntervals = 128;
static constexpr int ExplLog2Intervals = 7;
static constexpr int ExplBias = 16383;
static constexpr double ExplInvL = 1.8466496523378731e+2;
static constexpr double ExplL1 = 5.4152123484527692e-3;
static constexpr double ExplL2 = -3.2819649005320973e-13;
static constexpr double ExplA2 = 0.5;
static constexpr double ExplA3 = 1.6666666666666119e-1;
static constexpr double ExplA4 = 4.1666666666665887e-2;
static constexpr double ExplA5 = 8.3333354987869413e-3;
static constexpr double ExplA6 = 1.3888891738560272e-3;
static constexpr long double ExplOverflowThreshold = 11356.5234062941439488L;
static constexpr long double ExplUnderflowThreshold = -11399.4985314888605581L;
static constexpr long double ExplTwoPowerMinus10000 = 0x1p-10000L;
static constexpr std::uint64_t ExplUnitMantissa = 0x8000000000000000ull;
static const volatile long double ExplHuge = 0x1p10000L;
static const volatile long double ExplTiny = 0x1p-10000L;
static constexpr struct {
    double hi;
    double lo;
} ExplTable[ExplIntervals] = {
    { 0x1p+0, 0x0p+0 },
    { 0x1.0163da9fb3335p+0, 0x1.b61299ab8cdb7p-54 },
    { 0x1.02c9a3e778060p+0, 0x1.dcdef95949ef4p-53 },
    { 0x1.04315e86e7f84p+0, 0x1.7ae71f3441b49p-53 },
    { 0x1.059b0d3158574p+0, 0x1.d73e2a475b465p-55 },
    { 0x1.0706b29ddf6ddp+0, 0x1.8db880753b0f6p-53 },
    { 0x1.0874518759bc8p+0, 0x1.186be4bb284ffp-57 },
    { 0x1.09e3ecac6f383p+0, 0x1.1487818316136p-54 },
    { 0x1.0b5586cf9890fp+0, 0x1.8a62e4adc610bp-54 },
    { 0x1.0cc922b7247f7p+0, 0x1.01edc16e24f71p-54 },
    { 0x1.0e3ec32d3d1a2p+0, 0x1.03a1727c57b53p-59 },
    { 0x1.0fb66affed31ap+0, 0x1.e464123bb1428p-53 },
    { 0x1.11301d0125b50p+0, 0x1.49d77e35db263p-53 },
    { 0x1.12abdc06c31cbp+0, 0x1.f72575a649ad2p-53 },
    { 0x1.1429aaea92ddfp+0, 0x1.66820328764b1p-53 },
    { 0x1.15a98c8a58e51p+0, 0x1.2406ab9eeab0ap-55 },
    { 0x1.172b83c7d517ap+0, 0x1.b9bef918a1d63p-53 },
    { 0x1.18af9388c8de9p+0, 0x1.777ee1734784ap-53 },
    { 0x1.1a35beb6fcb75p+0, 0x1.e5b4c7b4968e4p-55 },
    { 0x1.1bbe084045cd3p+0, 0x1.3563ce56884fcp-53 },
    { 0x1.1d4873168b9aap+0, 0x1.e016e00a2643cp-54 },
    { 0x1.1ed5022fcd91cp+0, 0x1.71033fec2243ap-53 },
    { 0x1.2063b88628cd6p+0, 0x1.dc775814a8495p-55 },
    { 0x1.21f49917ddc96p+0, 0x1.2a97e9494a5eep-55 },
    { 0x1.2387a6e756238p+0, 0x1.9b07eb6c70573p-54 },
    { 0x1.251ce4fb2a63fp+0, 0x1.ac155bef4f4a4p-55 },
    { 0x1.26b4565e27cddp+0, 0x1.2bd339940e9d9p-55 },
    { 0x1.284dfe1f56380p+0, 0x1.2d9e2b9e07941p-53 },
    { 0x1.29e9df51fdee1p+0, 0x1.612e8afad1255p-55 },
    { 0x1.2b87fd0dad98fp+0, 0x1.fbbd48ca71f95p-53 },
    { 0x1.2d285a6e4030bp+0, 0x1.0024754db41d5p-54 },
    { 0x1.2ecafa93e2f56p+0, 0x1.1ca0f45d52383p-56 },
    { 0x1.306fe0a31b715p+0, 0x1.6f46ad23182e4p-55 },
    { 0x1.32170fc4cd831p+0, 0x1.a9ce78e18047cp-55 },
    { 0x1.33c08b26416ffp+0, 0x1.32721843659a6p-54 },
    { 0x1.356c55f929ff0p+0, 0x1.928c468ec6e76p-53 },
    { 0x1.371a7373aa9cap+0, 0x1.4e28aa05e8a8fp-53 },
    { 0x1.38cae6d05d865p+0, 0x1.0b53961b37da2p-53 },
    { 0x1.3a7db34e59ff6p+0, 0x1.d43792533c144p-53 },
    { 0x1.3c32dc313a8e4p+0, 0x1.08003e4516b1ep-53 },
    { 0x1.3dea64c123422p+0, 0x1.ada0911f09ebcp-55 },
    { 0x1.3fa4504ac801bp+0, 0x1.417ee03548306p-53 },
    { 0x1.4160a21f72e29p+0, 0x1.f0864b71e7b6cp-53 },
    { 0x1.431f5d950a896p+0, 0x1.b8e088728219ap-53 },
    { 0x1.44e086061892dp+0, 0x1.89b7a04ef80d0p-59 },
    { 0x1.46a41ed1d0057p+0, 0x1.c944bd1648a76p-54 },
    { 0x1.486a2b5c13cd0p+0, 0x1.3c1a3b69062f0p-56 },
    { 0x1.4a32af0d7d3dep+0, 0x1.9cb62f3d1be56p-54 },
    { 0x1.4bfdad5362a27p+0, 0x1.d4397afec42e2p-56 },
    { 0x1.4dcb299fddd0dp+0, 0x1.8ecdbbc6a7833p-54 },
    { 0x1.4f9b2769d2ca6p+0, 0x1.5a67b16d3540ep-53 },
    { 0x1.516daa2cf6641p+0, 0x1.8225ea5909b04p-53 },
    { 0x1.5342b569d4f81p+0, 0x1.be1507893b0d5p-53 },
    { 0x1.551a4ca5d920ep+0, 0x1.8a5d8c4048699p-53 },
    { 0x1.56f4736b527dap+0, 0x1.9bb2c011d93adp-54 },
    { 0x1.58d12d497c7fdp+0, 0x1.295e15b9a1de8p-55 },
    { 0x1.5ab07dd485429p+0, 0x1.6324c054647adp-54 },
    { 0x1.5c9268a5946b7p+0, 0x1.c4b1b816986a2p-60 },
    { 0x1.5e76f15ad2148p+0, 0x1.ba6f93080e65ep-54 },
    { 0x1.605e1b976dc08p+0, 0x1.60edeb25490dcp-53 },
    { 0x1.6247eb03a5584p+0, 0x1.63e1f40dfa5b5p-53 },
    { 0x1.6434634ccc31fp+0, 0x1.8edf0e2989db3p-53 },
    { 0x1.6623882552224p+0, 0x1.224fb3c5371e6p-53 },
    { 0x1.68155d44ca973p+0, 0x1.038ae44f73e65p-57 },
    { 0x1.6a09e667f3bccp+0, 0x1.21165f626cdd5p-53 },
    { 0x1.6c012750bdabep+0, 0x1.daed533001e9ep-53 },
    { 0x1.6dfb23c651a2ep+0, 0x1.e441c597c3775p-53 },
    { 0x1.6ff7df9519483p+0, 0x1.9f0fc369e7c42p-53 },
    { 0x1.71f75e8ec5f73p+0, 0x1.ba46e1e5de15ap-53 },
    { 0x1.73f9a48a58173p+0, 0x1.7ab9349cd1562p-53 },
    { 0x1.75feb564267c8p+0, 0x1.7edd354674916p-53 },
    { 0x1.780694fde5d3fp+0, 0x1.866b80a02162dp-54 },
    { 0x1.7a11473eb0186p+0, 0x1.afaa2047ed9b4p-53 },
    { 0x1.7c1ed0130c132p+0, 0x1.f124cd1164dd6p-54 },
    { 0x1.7e2f336cf4e62p+0, 0x1.05d02ba15797ep-56 },
    { 0x1.80427543e1a11p+0, 0x1.6c1bccec9346bp-53 },
    { 0x1.82589994cce12p+0, 0x1.159f115f56694p-53 },
    { 0x1.8471a4623c7acp+0, 0x1.9ca5ed72f8c81p-53 },
    { 0x1.868d99b4492ecp+0, 0x1.01c83b21584a3p-53 },
    { 0x1.88ac7d98a6699p+0, 0x1.994c2f37cb53ap-54 },
    { 0x1.8ace5422aa0dbp+0, 0x1.6e9f156864b27p-54 },
    { 0x1.8cf3216b5448bp+0, 0x1.de55439a2c38bp-53 },
    { 0x1.8f1ae99157736p+0, 0x1.5cc13a2e3976cp-55 },
    { 0x1.9145b0b91ffc5p+0, 0x1.114c368d3ed6ep-53 },
    { 0x1.93737b0cdc5e4p+0, 0x1.e8a0387e4a814p-53 },
    { 0x1.95a44cbc8520ep+0, 0x1.d36906d2b41f9p-53 },
    { 0x1.97d829fde4e4fp+0, 0x1.173d241f23d18p-53 },
    { 0x1.9a0f170ca07b9p+0, 0x1.7462137188ce7p-53 },
    { 0x1.9c49182a3f090p+0, 0x1.c7c46b071f2bep-56 },
    { 0x1.9e86319e32323p+0, 0x1.824ca78e64c6ep-56 },
    { 0x1.a0c667b5de564p+0, 0x1.6535b51719567p-53 },
    { 0x1.a309bec4a2d33p+0, 0x1.6305c7ddc36abp-54 },
    { 0x1.a5503b23e255cp+0, 0x1.1684892395f0fp-53 },
    { 0x1.a799e1330b358p+0, 0x1.bcb7ecac563c7p-54 },
    { 0x1.a9e6b5579fdbfp+0, 0x1.0fac90ef7fd31p-54 },
    { 0x1.ac36bbfd3f379p+0, 0x1.81b72cd4624ccp-53 },
    { 0x1.ae89f995ad3adp+0, 0x1.7a1cd345dcc81p-54 },
    { 0x1.b0e07298db665p+0, 0x1.2108559bf8deep-53 },
    { 0x1.b33a2b84f15fap+0, 0x1.ed7fa1cf7b290p-53 },
    { 0x1.b59728de55939p+0, 0x1.1c7102222c90ep-53 },
    { 0x1.b7f76f2fb5e46p+0, 0x1.d54f610356a79p-53 },
    { 0x1.ba5b030a10649p+0, 0x1.0819678d5eb69p-53 },
    { 0x1.bcc1e904bc1d2p+0, 0x1.23dd07a2d9e84p-55 },
    { 0x1.bf2c25bd71e08p+0, 0x1.0811ae04a31c7p-53 },
    { 0x1.c199bdd85529cp+0, 0x1.11065895048ddp-55 },
    { 0x1.c40ab5fffd07ap+0, 0x1.b4537e083c60ap-54 },
    { 0x1.c67f12e57d14bp+0, 0x1.2884dff483cadp-54 },
    { 0x1.c8f6d9406e7b5p+0, 0x1.1acbc48805c44p-56 },
    { 0x1.cb720dcef9069p+0, 0x1.503cbd1e949dbp-56 },
    { 0x1.cdf0b555dc3f9p+0, 0x1.889f12b1f58a3p-53 },
    { 0x1.d072d4a07897bp+0, 0x1.1a1e45e4342b2p-53 },
    { 0x1.d2f87080d89f1p+0, 0x1.15bc247313d44p-53 },
    { 0x1.d5818dcfba487p+0, 0x1.2ed02d75b3707p-55 },
    { 0x1.d80e316c98397p+0, 0x1.7709f3a09100cp-53 },
    { 0x1.da9e603db3285p+0, 0x1.c2300696db532p-54 },
    { 0x1.dd321f301b460p+0, 0x1.2da5778f018c3p-54 },
    { 0x1.dfc97337b9b5ep+0, 0x1.72d195873da52p-53 },
    { 0x1.e264614f5a128p+0, 0x1.424ec3f42f5b5p-53 },
    { 0x1.e502ee78b3ff6p+0, 0x1.39e8980a9cc8fp-55 },
    { 0x1.e7a51fbc74c83p+0, 0x1.2d522ca0c8de2p-54 },
    { 0x1.ea4afa2a490d9p+0, 0x1.0b1ee7431ebb6p-53 },
    { 0x1.ecf482d8e67f0p+0, 0x1.1b60625f7293ap-53 },
    { 0x1.efa1bee615a27p+0, 0x1.dc7f486a4b6b0p-54 },
    { 0x1.f252b376bba97p+0, 0x1.3a1a5bf0d8e43p-54 },
    { 0x1.f50765b6e4540p+0, 0x1.9d3e12dd8a18bp-54 },
    { 0x1.f7bfdad9cbe13p+0, 0x1.1227697fce57bp-53 },
    { 0x1.fa7c1819e90d8p+0, 0x1.74853f3a5931ep-55 },
    { 0x1.fd3c22b8f71f1p+0, 0x1.2eb74966579e7p-57 },
};

struct ExplReduced {
    long double hi;
    long double lo;
    int k;
};

static long double ExplPowerOfTwo(int exponent) { return JoinExtended(ExplUnitMantissa, static_cast<std::uint16_t>(ExplBias + exponent)); }

static ExplReduced ExplKernel(long double x) {
    const long double fn = x * ExplInvL + 0x1.8p63 - 0x1.8p63;
    const long double r = x - fn * ExplL1 - fn * ExplL2;
    const int n = static_cast<int>(fn);
    const int n2 = static_cast<int>(static_cast<unsigned>(n) % ExplIntervals);
    const long double r1 = x - fn * ExplL1;
    const long double r2 = fn * -ExplL2;
    const long double z = r * r;
    const long double q = r2 + z * ExplA2 + z * r * (ExplA3 + r * ExplA4 + z * (ExplA5 + r * ExplA6));
    const long double t = static_cast<long double>(ExplTable[n2].lo) + ExplTable[n2].hi;
    return {ExplTable[n2].hi, ExplTable[n2].lo + t * (q + r1), n >> ExplLog2Intervals};
}

static ExplReduced ExplHalfKernel(long double x) {
    ExplReduced reduced = ExplKernel(x);
    const float scale = std::bit_cast<float>(static_cast<std::uint32_t>(0x3f800000 + ((reduced.k - 1) * 0x800000)));
    reduced.hi *= scale;
    reduced.lo *= scale;
    return reduced;
}

static long double ExplHalf(long double x) {
    const ExplReduced reduced = ExplKernel(x);
    return (reduced.lo + reduced.hi) * 2 * ExplPowerOfTwo(reduced.k - 2);
}

static long double ExplMsun(long double x) {
    const std::uint16_t signExponent = SplitExtended(x).signExponent;
    const int exponent = signExponent & 0x7fff;
    if (exponent >= ExplBias + 13) {
        if (exponent == ExplBias + 16384) return (signExponent & 0x8000) != 0 ? -1 / x : x + x;
        if (x > ExplOverflowThreshold) return ExplHuge * ExplHuge;
        if (x < ExplUnderflowThreshold) return ExplTiny * ExplTiny;
    } else if (exponent < ExplBias - 75) {
        return 1 + x;
    }
    const ExplReduced reduced = ExplKernel(x);
    const long double t = reduced.hi + reduced.lo;
    if (reduced.k >= -16381) {
        if (reduced.k == 16384) return t * 2 * 0x1p16383L;
        return t * ExplPowerOfTwo(reduced.k);
    }
    return t * ExplPowerOfTwo(reduced.k + 10000) * ExplTwoPowerMinus10000;
}
static constexpr int Expm1lMantissaDigits = 64;
static constexpr double Expm1lSeriesBound = 0.1659;
static constexpr long double Expm1lB3 = 0xAAAAAAAAAAAAAAABp-66L;
static constexpr long double Expm1lB4 = 0xAAAAAAAAAAAAAAACp-68L;
static constexpr double Expm1lB5 = 8.3333333333333245e-3;
static constexpr double Expm1lB6 = 1.3888888888888861e-3;
static constexpr double Expm1lB7 = 1.9841269841532042e-4;
static constexpr double Expm1lB8 = 2.4801587302069236e-5;
static constexpr double Expm1lB9 = 2.7557316558468562e-6;
static constexpr double Expm1lB10 = 2.7557315829785151e-7;
static constexpr double Expm1lB11 = 2.5063168199779829e-8;
static constexpr double Expm1lB12 = 2.0887164654459567e-9;

static long double Expm1lSeries(long double x, int exponent) {
    if (exponent < ExplBias - 74) return x == 0 ? x : (0x1p100 * x + std::fabs(x)) * 0x1p-100;
    const long double x2 = x * x;
    const long double x4 = x2 * x2;
    const long double q = x4 * (x2 * (x4 * (x2 * Expm1lB12 + (x * Expm1lB11 + Expm1lB10)) + (x2 * (x * Expm1lB9 + Expm1lB8) + (x * Expm1lB7 + Expm1lB6))) + (x * Expm1lB5 + Expm1lB4)) + x2 * x * Expm1lB3;
    const long double xHigh = static_cast<float>(x);
    const long double xLow = x - xHigh;
    const long double halfSquareHigh = xHigh * xHigh / 2;
    const long double halfSquareLow = xLow * (x + xHigh) / 2;
    if (exponent >= ExplBias - 7) return (halfSquareHigh + xHigh) + (halfSquareLow + xLow + q);
    return x + (halfSquareLow + q + halfSquareHigh);
}

static long double Expm1lScaled(double hi, double lo, int k, long double q, long double r1) {
    const long double t = static_cast<long double>(lo) + hi;
    if (k == 0) return (hi - 1) + (lo * (r1 + 1) + t * q + hi * r1);
    if (k == -1) return ((hi - 2) + (lo * (r1 + 1) + t * q + hi * r1)) / 2;
    if (k < -7) return (hi + (lo + t * (q + r1))) * ExplPowerOfTwo(k) - 1;
    if (k > 2 * Expm1lMantissaDigits - 1) {
        const long double sum = hi + (lo + t * (q + r1));
        return (k == 16384 ? sum * 2 * 0x1p16383L : sum * ExplPowerOfTwo(k)) - 1;
    }
    const long double twoPowerMinusK = ExplPowerOfTwo(-k);
    if (k > Expm1lMantissaDigits - 1) return (hi + (lo - twoPowerMinusK + t * (q + r1))) * ExplPowerOfTwo(k);
    return ((hi - twoPowerMinusK) + (lo + t * (q + r1))) * ExplPowerOfTwo(k);
}

static long double Expm1lGeneral(long double x) {
    const long double fn = x * ExplInvL + 0x1.8p63 - 0x1.8p63;
    const int n = static_cast<int>(fn);
    const long double r1 = x - fn * ExplL1;
    const long double r2 = fn * -ExplL2;
    const long double r = r1 + r2;
    const long double z = r * r;
    const long double q = r2 + z * (ExplA2 + r * ExplA3) + z * z * (ExplA4 + r * ExplA5) + z * z * z * ExplA6;
    const auto& entry = ExplTable[static_cast<unsigned>(n) % ExplIntervals];
    return Expm1lScaled(entry.hi, entry.lo, n >> ExplLog2Intervals, q, r1);
}

static long double Expm1lMsun(long double x) {
    const std::uint16_t signExponent = SplitExtended(x).signExponent;
    const int exponent = signExponent & 0x7fff;
    if (exponent >= ExplBias + 6) {
        if (exponent == ExplBias + 16384) return (signExponent & 0x8000) != 0 ? -1 / x - 1 : x + x;
        if (x > ExplOverflowThreshold) return ExplHuge * ExplHuge;
        if ((signExponent & 0x8000) != 0) return ExplTiny + -1;
    }
    if (-Expm1lSeriesBound < x && x < Expm1lSeriesBound) return Expm1lSeries(x, exponent);
    return Expm1lGeneral(x);
}
static constexpr long double SinhlS3 = 1.66666666666666666658e-1L;
static constexpr double SinhlS5 = 8.3333333333333332e-3;
static constexpr double SinhlS7 = 1.9841269841270074e-4;
static constexpr double SinhlS9 = 2.7557319223873889e-6;
static constexpr double SinhlS11 = 2.5052108406704084e-8;
static constexpr double SinhlS13 = 1.6059042748655297e-10;
static constexpr double SinhlS15 = 7.6470006914396920e-13;
static constexpr double SinhlS17 = 2.8346142308424267e-15;
static constexpr float HyperbolicOverflowThreshold = 1.13572168e4F;
static constexpr long double SinhlHuge = 0x1p16383L;

static long double SinhlSmall(long double x) {
    const long double x2 = x * x;
    const long double x4 = x2 * x2;
    return ((SinhlS17 * x2 + SinhlS15) * x4 + (SinhlS13 * x2 + SinhlS11)) * (x2 * x * x4 * x4) + ((SinhlS9 * x2 + SinhlS7) * x2 + SinhlS5) * (x2 * x * x2) + SinhlS3 * (x2 * x) + x;
}

static long double SinhlMsun(long double x) {
    const std::int16_t signExponent = static_cast<std::int16_t>(SplitExtended(x).signExponent);
    const int exponent = signExponent & 0x7fff;
    if (exponent >= 0x7fff) return x + x;
    const double sign = signExponent < 0 ? -1 : 1;
    if (exponent < 0x4005) {
        if (exponent < ExplBias - 32 && SinhlHuge + x > 1) return x;
        if (exponent < 0x3fff) return SinhlSmall(x);
        const ExplReduced half = ExplHalfKernel(std::fabs(x));
        return sign * (half.lo - 0.25 / (half.hi + half.lo) + half.hi);
    }
    if (std::fabs(x) <= HyperbolicOverflowThreshold) return sign * ExplHalf(std::fabs(x));
    return x * SinhlHuge;
}
static constexpr long double CoshlC4 = 4.16666666666666682297e-2L;
static constexpr double CoshlC2 = 0.5;
static constexpr double CoshlC6 = 1.3888888888888616e-3;
static constexpr double CoshlC8 = 2.4801587301767953e-5;
static constexpr double CoshlC10 = 2.7557319163300398e-7;
static constexpr double CoshlC12 = 2.0876768371393075e-9;
static constexpr double CoshlC14 = 1.1469537039374480e-11;
static constexpr double CoshlC16 = 4.8473490896852041e-14;

static long double CoshlSmall(long double x) {
    const long double x2 = x * x;
    const long double x4 = x2 * x2;
    return ((CoshlC16 * x2 + CoshlC14) * x4 + (CoshlC12 * x2 + CoshlC10)) * (x4 * x4 * x2) + ((CoshlC8 * x2 + CoshlC6) * x2 + CoshlC4) * x4 + CoshlC2 * x2 + 1;
}

static long double CoshlMsun(long double x) {
    const int exponent = SplitExtended(x).signExponent & 0x7fff;
    if (exponent >= 0x7fff) return x * x;
    if (exponent < 0x3fff) {
        if (exponent < ExplBias - 32) return 1 + ExplTiny;
        return CoshlSmall(x);
    }
    if (exponent < 0x4005) {
        const ExplReduced half = ExplHalfKernel(std::fabs(x));
        return half.lo + 0.25 / (half.hi + half.lo) + half.hi;
    }
    if (std::fabs(x) <= HyperbolicOverflowThreshold) return ExplHalf(std::fabs(x));
    return ExplHuge * ExplHuge;
}
static const volatile double TanhlTiny = 1.0e-300;
static constexpr double TanhlOne = 1.0;
static constexpr long double TanhlT3 = -3.33333333333333333017e-1L;
static constexpr double TanhlT5 = 1.3333333333333314e-1;
static constexpr double TanhlT7 = -5.3968253968210485e-2;
static constexpr double TanhlT9 = 2.1869488531393817e-2;
static constexpr double TanhlT11 = -8.8632352345964591e-3;
static constexpr double TanhlT13 = 3.5921169709993771e-3;
static constexpr double TanhlT15 = -1.4555786415756001e-3;
static constexpr double TanhlT17 = 5.8645267876296793e-4;
static constexpr double TanhlT19 = -2.1121033571392224e-4;

static long double TanhlDivide(long double a, long double b, long double c, long double d, long double e, long double f) {
    long double w = a + c;
    c = (a - w) + c;
    a = w;
    b = b + c;
    w = d + f;
    f = (d - w) + f;
    d = w;
    e = e + f;
    const long double inverse = 1 / (d + e);
    long double r = (a + b) * inverse;
    const float rounded = static_cast<float>(r);
    r = rounded;
    const float high = static_cast<float>(d + e);
    e = d - high + e;
    d = high;
    return r + (a - d * r + b - e * r) * inverse;
}

}
