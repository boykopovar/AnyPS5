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


static long double TanhlSmall(long double x) {
    const long double x2 = x * x;
    const long double x4 = x2 * x2;
    return ((TanhlT19 * x2 + TanhlT17) * x4 + (TanhlT15 * x2 + TanhlT13)) * (x2 * x * x2 * x4 * x4) + ((TanhlT11 * x2 + TanhlT9) * x4 + (TanhlT7 * x2 + TanhlT5)) * (x2 * x * x2) + TanhlT3 * (x2 * x) + x;
}

static long double TanhlMagnitude(long double x, int exponent) {
    if (exponent >= 0x4004 && std::fabs(x) >= 40) return TanhlOne - TanhlTiny;
    const ExplReduced half = ExplHalfKernel(2 * std::fabs(x));
    if (exponent < 0x4001 && std::fabs(x) < 1.5) return TanhlDivide(half.hi, half.lo, -0.5, half.hi, half.lo, 0.5);
    return TanhlOne - TanhlOne / (half.lo + 0.5 + half.hi);
}

static long double TanhlMsun(long double x) {
    const std::int16_t signExponent = static_cast<std::int16_t>(SplitExtended(x).signExponent);
    const int exponent = signExponent & 0x7fff;
    if (exponent >= 0x7fff) return signExponent >= 0 ? TanhlOne / x + TanhlOne : TanhlOne / x - TanhlOne;
    if (exponent < ExplBias - 32) return x == 0 ? x : (0x1p200 * x - x) * 0x1p-200;
    if (exponent < 0x3ffd) return TanhlSmall(x);
    const double sign = signExponent < 0 ? -1 : 1;
    return sign * TanhlMagnitude(x, exponent);
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/s_erfl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/s_erfl.c */
/* Converted to long double by Steven G. Kargl. */
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
static constexpr long double ErflPp[] = {
    0x8375d410a6db446cp-66L, -0xa46c7d09ec3d0cecp-65L, -0x9b31e66325576f86p-68L,
    -0x804ac72c9a0b97ddp-70L, -0x9f42bcbc3d5a601dp-75L, -0x9ec4ad6193470693p-79L,
};
static constexpr long double ErflQq[] = {
    1.0L, 0xdb4b8eb713188d6bp-65L, 0xa5750835b2459bd1p-67L,
    0x8b85d6bd6a90b51cp-70L, 0x87332f82cff4ff96p-74L, 0x83466cb6bf9dca00p-79L,
    0xf5bf98c2f996bf63p-87L,
};
static constexpr long double ErflPa[] = {
    0xe8211158da02c692p-90L, 0xd488f89f36988618p-65L, -0xece74f8c63fa3942p-67L,
    0xc8d31e020727c006p-67L, 0x985d5d5fafb0551fp-68L, -0xa5b6c4854d2f5452p-71L,
    0x85c8d58fe3993a47p-71L, 0xddbfbc23677b35cfp-76L,
};
static constexpr long double ErflQa[] = {
    1.0L, 0xb8a977896f5eff3fp-64L, 0x9fcd662c3d4eac86p-64L,
    0x9d0b618eac67ba07p-65L, 0x881a4293f6d6c92dp-66L, 0xbab144f07dea45bfp-68L,
    0xa6c34ba438bdc900p-70L, 0x8fa866dc20717a91p-72L,
};
static constexpr long double ErflRa[] = {
    -0xa1a091e0fb4f335ap-70L, -0xc2b0d045ae37df6bp-64L, -0xf2cec3ee7da636c5p-60L,
    -0x813cc205395adc7dp-56L, -0x8737c8b7b4062c2fp-54L, -0x8ffe5383c08d4943p-53L,
    -0x983573e64d5015a9p-53L, -0x92a794e763a6d4dbp-54L, -0xd5ad1fae77c3d9a3p-57L,
    -0x934c1a247807bb9cp-61L,
};
static constexpr long double ErflSa[] = {
    1.0L, 0xd342f90012bb1189p-59L, 0x839be13d9d5da883p-55L,
    0x9f8cba6d1ae1b24bp-53L, 0xcaa83f403713e33ep-52L, 0x8796aff2f3c47968p-51L,
    0xb6ef97f9c753157bp-52L, 0xe02aee5f83773d1cp-54L, 0xc82b83855b88e07ep-57L,
    0x92f030aefadf28adp-62L,
};
static constexpr long double ErflRb[] = {
    -0xa1a091cf43abcd26p-70L, -0xd19d2df1cbb8da0ap-64L, -0x9a4dd1383e5daf5bp-59L,
    -0xbff0ae9fc0751de6p-56L, -0xdde08465310b472bp-54L, -0xe796e1d38c8c70a9p-53L,
    -0xbaf655a76e0ab3b5p-53L, -0x95d21e3e75503c21p-55L,
};
static constexpr long double ErflSb[] = {
    1.0L, 0x814487ed823c8cbdp-58L, 0xbe4bfbb1301304bep-55L,
    0x809c4ade46b927c7p-52L, 0xa55284359f3395a8p-51L, 0xbcfa72da9b820874p-51L,
    0x9d09a35988934631p-52L, 0xd675bbe542c159fap-56L,
};
static constexpr long double ErflRc[] = {
    -0xa1a091cf437a17adp-70L, -0xbe79c5a978122b00p-64L, -0xdb26f9bbe31a2794p-60L,
    -0xb5f69a38f5747ac8p-57L, -0xd79676d970d0a21ap-56L, -0xfe528153c45ec97cp-57L,
};
static constexpr long double ErflSc[] = {
    1.0L, 0xc5e8cd46d5604a96p-59L, 0xc5f0f5a5484520ebp-56L,
    0x964e3c7b34db9170p-54L, 0x99be1b89faa0596ap-54L, 0xf80dfcbf37ffc5eap-57L,
};
static constexpr long double ErflEfx = 0x8375d410a6db446cp-66L;
static constexpr long double ErflEfx8 = 0x8375d410a6db446cp-63L;
static constexpr long double ErflErx = 0xd7bb3d0000000000p-64L;

static long double ErflPolynomial(std::span<const long double> coefficients, long double z) {
    long double result = coefficients.back();
    for (std::size_t index = coefficients.size() - 1; index-- > 0;) result = coefficients[index] + z * result;
    return result;
}

static long double ErflSmallRatio(long double z) { return ErflPolynomial(ErflPp, z) / ErflPolynomial(ErflQq, z); }

static long double ErflNearOneRatio(long double s) { return ErflPolynomial(ErflPa, s) / ErflPolynomial(ErflQa, s); }

static long double ErflTail(long double ax) {
    const long double s = 1 / (ax * ax);
    long double ratio;
    if (ax < 2.85715) ratio = ErflPolynomial(ErflRa, s) / ErflPolynomial(ErflSa, s);
    else if (ax < 7) ratio = ErflPolynomial(ErflRb, s) / ErflPolynomial(ErflSb, s);
    else ratio = ErflPolynomial(ErflRc, s) / ErflPolynomial(ErflSc, s);
    const long double z = static_cast<float>(ax);
    return ExplMsun(-z * z - 0.5625) * ExplMsun((z - ax) * (z + ax) + ratio);
}

static long double ErflMsun(long double x) {
    const std::uint16_t signExponent = SplitExtended(x).signExponent;
    if ((signExponent & 0x7fff) == 0x7fff) return (1 - ((signExponent >> 15) << 1)) + 1 / x;
    const long double ax = std::fabs(x);
    if (ax < 0.84375) {
        if (ax >= 0x1p-34L) return x + x * ErflSmallRatio(x * x);
        if (ax < 0x1p-16373L) return (8 * x + ErflEfx8 * x) / 8;
        return x + ErflEfx * x;
    }
    if (ax < 1.25) {
        const long double ratio = ErflNearOneRatio(ax - 1);
        return x >= 0 ? ErflErx + ratio : -ErflErx - ratio;
    }
    if (ax >= 7) return x >= 0 ? 1 - ExplTiny : ExplTiny - 1;
    const long double r = ErflTail(ax);
    return x >= 0 ? 1 - r / ax : r / ax - 1;
}

static long double ErfclSmall(long double x, long double ax) {
    if (ax < 0x1p-34L) return 1 - x;
    const long double y = ErflSmallRatio(x * x);
    if (ax < 0.25L) return 1 - (x + x * y);
    return 0.5 - (x * y + (x - 0.5));
}

static long double ErfclMsun(long double x) {
    const std::uint16_t signExponent = SplitExtended(x).signExponent;
    if ((signExponent & 0x7fff) == 0x7fff) return ((signExponent >> 15) << 1) + 1 / x;
    const long double ax = std::fabs(x);
    if (ax < 0.84375L) return ErfclSmall(x, ax);
    if (ax < 1.25L) {
        const long double ratio = ErflNearOneRatio(ax - 1);
        return x >= 0 ? (1 - ErflErx) - ratio : 1 + (ErflErx + ratio);
    }
    if (ax >= 108) return x > 0 ? ExplTiny * ExplTiny : 2 - ExplTiny;
    if (x < -7) return 2 - ExplTiny;
    const long double r = ErflTail(ax);
    return x > 0 ? r / ax : 2 - r / ax;
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_fmod.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_fmod.c */
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
double APS5_VABI fmod_nid_postfix(double x, double y) {
    const std::uint64_t ix = std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull;
    const std::uint64_t iy = std::bit_cast<std::uint64_t>(y) & 0x7fffffffffffffffull;
    if (iy == 0 || ix >= 0x7ff0000000000000ull || iy > 0x7ff0000000000000ull) {
        _Feraise_nid_postfix(0x01);
        if (iy > 0x7ff0000000000000ull) {
            return y + y;
        }
        if (ix > 0x7ff0000000000000ull) {
            return x + x;
        }
        return (x * y) / (x * y);
    }
    const int saved_errno = errno;
    const double result = std::fmod(x, y);
    errno = saved_errno;
    return result;
}
extern "C++" {
template<typename TFloat, typename TBits, int FractionBits>
static TFloat RoundHalfAway(TFloat x) {
    constexpr int ExponentBits = static_cast<int>(sizeof(TBits) * 8) - 1 - FractionBits;
    constexpr int Bias = (1 << (ExponentBits - 1)) - 1;
    constexpr TBits Sign = TBits{1} << (sizeof(TBits) * 8 - 1);
    const TBits bits = std::bit_cast<TBits>(x);
    const int exponent = static_cast<int>((bits & ~Sign) >> FractionBits);
    if (exponent == (1 << ExponentBits) - 1) return x;
    const bool negative = !(x >= 0);
    const TBits magnitudeBits = negative ? bits & ~Sign : bits;
    TBits wholeBits = magnitudeBits;
    if (exponent < Bias) wholeBits &= Sign;
    else if (exponent < Bias + FractionBits) wholeBits &= ~((TBits{1} << (Bias + FractionBits - exponent)) - 1);
    const TFloat whole = std::bit_cast<TFloat>(wholeBits);
    const volatile TFloat above = whole + 1;
    const TFloat rounded = whole - std::bit_cast<TFloat>(magnitudeBits) > static_cast<TFloat>(-0.5) ? whole : above;
    return negative ? -rounded : rounded;
}
}
float APS5_VABI roundf_nid_postfix(float x) { return RoundHalfAway<float, std::uint32_t, 23>(x); }
double APS5_VABI round_nid_postfix(double x) { return RoundHalfAway<double, std::uint64_t, 52>(x); }
float APS5_VABI cbrtf_nid_postfix(float x) {
    const std::uint32_t B1 = 709958130;
    const std::uint32_t B2 = 642849266;
    std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t sign = hx & 0x80000000u;
    hx ^= sign;
    if (hx >= 0x7f800000u) {
        return x + x;
    }
    float t;
    if (hx < 0x00800000u) {
        if (hx == 0) {
            return x;
        }
        t = 0x1p24f * x;
        const std::uint32_t high = std::bit_cast<std::uint32_t>(t);
        t = std::bit_cast<float>(sign | ((high & 0x7fffffffu) / 3 + B2));
    } else {
        t = std::bit_cast<float>(sign | (hx / 3 + B1));
    }
    double T = t;
    double r = T * T * T;
    T = T * (static_cast<double>(x) + x + r) / (x + r + r);
    r = T * T * T;
    T = T * (static_cast<double>(x) + x + r) / (x + r + r);
    return static_cast<float>(T);
}
static long double PartialRemainder(long double x, long double y, std::uint16_t* status) {
    long double remainder;
    asm("1:\n\tfprem1\n\tfnstsw %%ax\n\ttestw $0x400, %%ax\n\tjnz 1b" : "=t"(remainder), "=a"(*status) : "0"(x), "u"(y));
    return remainder;
}

static void SetRemainderDomainError(long double x, long double y) {
    if (!std::isnan(x) && !std::isnan(y) && (y == 0 || std::isinf(x))) {
        errno = EDOM;
    }
}

static int RemquoQuotient(std::uint16_t status, bool negative) {
    const int quotient = ((status >> 8) & 1) << 2 | ((status >> 14) & 1) << 1 | ((status >> 9) & 1);
    return negative ? -quotient : quotient;
}

float APS5_VABI remainderf_nid_postfix(float x, float y) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    SetRemainderDomainError(x, y);
    return static_cast<float>(remainder);
}
float APS5_VABI nextafterf_nid_postfix(float x, float y) {
    const std::uint32_t ux = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t uy = std::bit_cast<std::uint32_t>(y);
    if ((ux & 0x7fffffffu) > 0x7f800000u || (uy & 0x7fffffffu) > 0x7f800000u) {
        volatile float sum = x + y;
        (void)sum;
        return std::bit_cast<float>(((ux & 0x7fffffffu) > 0x7f800000u ? ux : uy) | 0x00400000u);
    }
    if (x == y) {
        return y;
    }
    if ((ux & 0x7fffffffu) == 0) {
        const volatile float smallest = std::bit_cast<float>((uy & 0x80000000u) | 1u);
        volatile float product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    const bool towardZero = ((ux ^ uy) & 0x80000000u) != 0 || ux > uy;
    const std::uint32_t next = towardZero ? ux - 1 : ux + 1;
    const std::uint32_t exponent = next & 0x7f800000u;
    if (exponent == 0x7f800000u) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) {
        volatile float product = x * x;
        if (product != x) _Feraise_nid_postfix(0x10);
    }
    return std::bit_cast<float>(next);
}
double APS5_VABI nextafter_nid_postfix(double x, double y) {
    const std::uint64_t ux = std::bit_cast<std::uint64_t>(x);
    const std::uint64_t uy = std::bit_cast<std::uint64_t>(y);
    if ((ux & 0x7fffffffffffffffull) > 0x7ff0000000000000ull || (uy & 0x7fffffffffffffffull) > 0x7ff0000000000000ull) {
        volatile double sum = x + y;
        (void)sum;
        return std::bit_cast<double>(((ux & 0x7fffffffffffffffull) > 0x7ff0000000000000ull ? ux : uy) | 0x0008000000000000ull);
    }
    if (x == y) {
        return y;
    }
    if ((ux & 0x7fffffffffffffffull) == 0) {
        const volatile double smallest = std::bit_cast<double>((uy & 0x8000000000000000ull) | 1u);
        volatile double product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    const bool towardZero = ((ux ^ uy) & 0x8000000000000000ull) != 0 || ux > uy;
    const std::uint64_t next = towardZero ? ux - 1 : ux + 1;
    const std::uint64_t exponent = next & 0x7ff0000000000000ull;
    if (exponent == 0x7ff0000000000000ull) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) {
        volatile double product = x * x;
        if (product != x) _Feraise_nid_postfix(0x10);
    }
    return std::bit_cast<double>(next);
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_nextafterl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_nextafterl.c */
long double APS5_VABI nextafterl_nid_postfix(long double x, long double y) {
    const ExtendedParts ux = SplitExtended(x);
    const ExtendedParts uy = SplitExtended(y);
    if (((ux.signExponent & 0x7fff) == 0x7fff && (ux.mantissa << 1) != 0) || ((uy.signExponent & 0x7fff) == 0x7fff && (uy.mantissa << 1) != 0)) {
        return x + y;
    }
    if (x == y) {
        return y;
    }
    if (x == 0.0L) {
        const volatile long double smallest = JoinExtended(1, static_cast<std::uint16_t>(uy.signExponent & 0x8000));
        volatile long double product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    auto high = static_cast<std::uint32_t>(ux.mantissa >> 32);
    auto low = static_cast<std::uint32_t>(ux.mantissa);
    std::uint16_t exponent = ux.signExponent & 0x7fff;
    const std::uint32_t integer = high & 0x80000000u;
    if ((x > 0.0L) != (x < y)) {
        if (low == 0) {
            if ((high & 0x7fffffffu) == 0) exponent = static_cast<std::uint16_t>((exponent - 1) & 0x7fff);
            high = (high - 1) | integer;
        }
        --low;
    } else {
        ++low;
        if (low == 0) {
            high = (high + 1) | integer;
            if ((high & 0x7fffffffu) == 0) exponent = static_cast<std::uint16_t>((exponent + 1) & 0x7fff);
        }
    }
    if (exponent == 0x7fff) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) high &= 0x7fffffffu;
    const long double next = JoinExtended(std::uint64_t{high} << 32 | low, static_cast<std::uint16_t>(exponent | (ux.signExponent & 0x8000)));
    if (exponent == 0) {
        volatile long double product = next * next;
        if (product != next) _Feraise_nid_postfix(0x10);
    }
    return next;
}
long double APS5_VABI nexttowardl_nid_postfix(long double x, long double y) {
    return nextafterl_nid_postfix(x, y);
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_nexttoward.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_nexttoward.c */
double APS5_VABI nexttoward_nid_postfix(double x, long double y) {
    const std::uint64_t ux = std::bit_cast<std::uint64_t>(x);
    const ExtendedParts uy = SplitExtended(y);
    if ((ux & 0x7fffffffffffffffull) > 0x7ff0000000000000ull || ((uy.signExponent & 0x7fff) == 0x7fff && (uy.mantissa << 1) != 0)) {
        return static_cast<double>(x + y);
    }
    if (x == y) {
        return static_cast<double>(y);
    }
    if (x == 0.0) {
        const volatile double smallest = std::bit_cast<double>(static_cast<std::uint64_t>(uy.signExponent & 0x8000u) << 48 | 1u);
        volatile double product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    const std::uint64_t next = (static_cast<std::int32_t>(ux >> 32) > 0) != (x < y) ? ux - 1 : ux + 1;
    const std::uint64_t exponent = next & 0x7ff0000000000000ull;
    if (exponent == 0x7ff0000000000000ull) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) {
        volatile double product = x * x;
        if (product != x) _Feraise_nid_postfix(0x10);
    }
    return std::bit_cast<double>(next);
}
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_nexttowardf.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_nexttowardf.c */
float APS5_VABI nexttowardf_nid_postfix(float x, long double y) {
    const std::uint32_t ux = std::bit_cast<std::uint32_t>(x);
    const ExtendedParts uy = SplitExtended(y);
    if ((ux & 0x7fffffffu) > 0x7f800000u || ((uy.signExponent & 0x7fff) == 0x7fff && (uy.mantissa << 1) != 0)) {
        return static_cast<float>(x + y);
    }
    if (x == y) {
        return static_cast<float>(y);
    }
    if ((ux & 0x7fffffffu) == 0) {
        const volatile float smallest = std::bit_cast<float>(static_cast<std::uint32_t>(uy.signExponent & 0x8000u) << 16 | 1u);
        volatile float product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    const std::uint32_t next = (static_cast<std::int32_t>(ux) >= 0) != (x < y) ? ux - 1 : ux + 1;
    const std::uint32_t exponent = next & 0x7f800000u;
    if (exponent == 0x7f800000u) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) {
        volatile float product = x * x;
        if (product != x) _Feraise_nid_postfix(0x10);
    }
    return std::bit_cast<float>(next);
}
int APS5_VABI __isfinite_nid_postfix(double x) {
    const auto exponent = std::bit_cast<std::uint64_t>(x) & 0x7ff0000000000000ull;
    return exponent != 0x7ff0000000000000ull ? 1 : 0;
}
int APS5_VABI __isnan_nid_postfix(double x) {
    return (std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull ? 1 : 0;
}
int APS5_VABI __isinf_nid_postfix(double x) {
    return (std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull) == 0x7ff0000000000000ull ? 1 : 0;
}
int APS5_VABI __signbit_nid_postfix(double x) { return std::signbit(x) ? 0x8000 : 0; }
float APS5_VABI nanf_nid_postfix(const char* tagp) {
    (void)tagp;
    return std::bit_cast<float>(0x7fc00000u);
}
double APS5_VABI nan_nid_postfix(const char* tagp) {
    (void)tagp;
    return std::bit_cast<double>(0x7ff8000000000000ull);
}

double APS5_VABI modf_nid_postfix(double x, double* integral) {
    const auto bits = std::bit_cast<std::uint64_t>(x);
    const auto exponent = (bits >> 52u) & 0x7ffull;
    const auto zero = std::bit_cast<double>(bits & 0x8000000000000000ull);
    if (exponent < 1023u) {
        *integral = zero;
        return x;
    }
    if (exponent >= 1075u) {
        *integral = x;
        return exponent == 0x7ffu && (bits & 0xfffffffffffffull) != 0 ? x : zero;
    }
    const auto mask = (std::uint64_t{1} << (1075u - exponent)) - 1u;
    *integral = std::bit_cast<double>(bits & ~mask);
    return (bits & mask) == 0 ? zero : x - *integral;
}
float APS5_VABI modff_nid_postfix(float x, float* integral) {
    const auto bits = std::bit_cast<std::uint32_t>(x);
    const auto exponent = (bits >> 23u) & 0xffu;
    const auto zero = std::bit_cast<float>(bits & 0x80000000u);
    if (exponent < 127u) {
        *integral = zero;
        return x;
    }
    if (exponent >= 150u) {
        *integral = x;
        if (exponent == 0xffu && (bits & 0x7fffffu) != 0) {
            GuestCpu::RaiseInvalidForSignalingNan(x);
            return x;
        }
        return zero;
    }
    const auto mask = (std::uint32_t{1} << (150u - exponent)) - 1u;
    *integral = std::bit_cast<float>(bits & ~mask);
    return (bits & mask) == 0 ? zero : x - *integral;
}
double APS5_VABI tanh_nid_postfix(double x) {
    const double huge = 1.0e300;
    const std::int32_t jx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ix = jx & 0x7fffffff;
    if (ix >= 0x7ff00000) {
        return jx >= 0 ? 1.0 / x + 1.0 : 1.0 / x - 1.0;
    }
    double z;
    if (ix < 0x40360000) {
        if (ix < 0x3e300000 && huge + x > 1.0) {
            return x;
        }
        if (ix >= 0x3ff00000) {
            const double t = Expm1(2.0 * std::fabs(x));
            z = 1.0 - 2.0 / (t + 2.0);
        } else {
            const double t = Expm1(-2.0 * std::fabs(x));
            z = -t / (t + 2.0);
        }
    } else {
        z = 1.0;
    }
    return jx >= 0 ? z : -z;
}

}
