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


float APS5_VABI tanhf_nid_postfix(float x) {
    const float one = 1.0f, two = 2.0f;
    static volatile float tiny = 1.0e-30f;
    float t, z;
    const std::int32_t jx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = jx & 0x7fffffff;
    if (ix >= 0x7f800000) {
        if (jx >= 0) return one / x + one;
        return one / x - one;
    }
    if (ix < 0x41100000) {
        if (ix < 0x39800000) return x;
        if (ix >= 0x3f800000) {
            t = Expm1f(two * std::fabs(x));
            z = one - two / (t + two);
        } else {
            t = Expm1f(-two * std::fabs(x));
            z = -t / (t + two);
        }
    } else {
        z = one - tiny;
    }
    return jx >= 0 ? z : -z;
}
/*-
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
/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/invtrig.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/invtrig.c */
constexpr long double AtanCoefficients[] = {
    0xaaaaaaaaaaaaaa9fp-65L, -0xcccccccccccc62bcp-66L, 0x9249249248b81e3fp-66L, -0xe38e38e3316f3de5p-67L, 0xba2e8b8dc280726ap-67L,
    -0x9d89d5b4c6847ec4p-67L, 0x8888461d3099c677p-67L, -0xf0e8ee0f5328dc29p-68L, 0xd73ea84d24bae54ap-68L, -0xc08fa381dcd9213ap-68L,
    0xa54a26f4095f2a3ap-68L, -0xeea2d8d059ef3ad6p-69L, 0xcc82292ab894b051p-70L,
};
constexpr long double AtanHigh[] = {0xed63382b0dda7b45p-65L, 0xc90fdaa22168c235p-64L, 0xfb985e940fb4d900p-64L, 0xc90fdaa22168c235p-63L};
constexpr long double AtanLow[] = {0xdfc88bd978751a07p-130L, -0xece675d1fc8f8cbbp-130L, 0xf10f5e197793c283p-129L, -0xece675d1fc8f8cbbp-129L};
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
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_atanl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_atanl.c */
long double APS5_VABI atanl_nid_postfix(long double x) {
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    const bool negative = (parts.signExponent & 0x8000) != 0;
    if (exponent >= 0x3fff + 65) {
        if (exponent == 0x7fff && (parts.mantissa & 0x7fffffffffffffffull) != 0) return x + x;
        return negative ? SumAtCallerPrecision(-AtanHigh[3], -AtanLow[3]) : SumAtCallerPrecision(AtanHigh[3], AtanLow[3]);
    }
    const int head = (exponent << 8) | static_cast<int>((parts.mantissa >> 55) & 0xff);
    int index = -1;
    if (head < ((0x3fff - 2) << 8) + 0xc0) {
        if (1.0e300 + x > 1.0L && exponent < 0x3fff - 32) return x;
    } else {
        x = std::fabs(x);
        if (head < (0x3fff << 8) + 0x30) {
            if (head < ((0x3fff - 1) << 8) + 0x60) {
                index = 0;
                x = (2.0L * x - 1.0L) / (2.0L + x);
            } else {
                index = 1;
                x = (x - 1.0L) / (x + 1.0L);
            }
        } else if (head < ((0x3fff + 1) << 8) + 0x38) {
            index = 2;
            x = (x - 1.5L) / (1.0L + 1.5L * x);
        } else {
            index = 3;
            x = -1.0L / x;
        }
    }
    const auto& a = AtanCoefficients;
    const long double z = x * x;
    const long double w = z * z;
    const long double even = z * (a[0] + w * (a[2] + w * (a[4] + w * (a[6] + w * (a[8] + w * (a[10] + w * a[12]))))));
    const long double odd = w * (a[1] + w * (a[3] + w * (a[5] + w * (a[7] + w * (a[9] + w * a[11])))));
    if (index < 0) return x - x * (even + odd);
    const long double result = AtanHigh[index] - ((x * (even + odd) - AtanLow[index]) - x);
    return negative ? -result : result;
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
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_atan2l.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_atan2l.c */
static constexpr std::uint64_t ExtendedFractionMask = 0x7fffffffffffffffull;
static bool Atan2lOnAxis(long double yValue, const ExtendedParts& y, const ExtendedParts& x, int quadrant, long double& result) {
    static volatile long double tiny = 1.0e-300;
    const long double halfPi = AtanHigh[3];
    const long double pi = Pi;
    const int yExponent = y.signExponent & 0x7fff;
    const int xExponent = x.signExponent & 0x7fff;
    const bool yNegative = (y.signExponent & 0x8000) != 0;
    if (yExponent == 0 && (y.mantissa & ExtendedFractionMask) == 0) {
        if (quadrant < 2) result = yValue;
        else result = quadrant == 2 ? pi + tiny : -pi - tiny;
    } else if (xExponent == 0 && (x.mantissa & ExtendedFractionMask) == 0) {
        result = yNegative ? -halfPi - tiny : halfPi + tiny;
    } else if (xExponent == 0x7fff && yExponent == 0x7fff) {
        result = quadrant == 0 ? halfPi * 0.5 + tiny : quadrant == 1 ? -halfPi * 0.5 - tiny : quadrant == 2 ? 1.5 * halfPi + tiny : -1.5 * halfPi - tiny;
    } else if (xExponent == 0x7fff) {
        result = quadrant == 0 ? 0.0L : quadrant == 1 ? -0.0L : quadrant == 2 ? pi + tiny : -pi - tiny;
    } else if (yExponent == 0x7fff) {
        result = yNegative ? -halfPi - tiny : halfPi + tiny;
    } else {
        return false;
    }
    return true;
}
long double APS5_VABI atan2l_nid_postfix(long double y, long double x) {
    constexpr long double PiLow = -5.01655761266833202345e-20L;
    const ExtendedParts py = SplitExtended(y);
    const ExtendedParts px = SplitExtended(x);
    const int yExponent = py.signExponent & 0x7fff;
    const int xExponent = px.signExponent & 0x7fff;
    if ((xExponent == 0x7fff && (px.mantissa & ExtendedFractionMask) != 0) || (yExponent == 0x7fff && (py.mantissa & ExtendedFractionMask) != 0)) return x + y;
    if (px.signExponent == 0x3fff && (px.mantissa & ExtendedFractionMask) == 0) return atanl_nid_postfix(y);
    int quadrant = ((py.signExponent >> 15) & 1) | ((px.signExponent >> 14) & 2);
    long double result;
    if (Atan2lOnAxis(y, py, px, quadrant, result)) return result;
    const int k = yExponent - xExponent;
    long double z;
    if (k > 64 + 2) {
        z = SumAtCallerPrecision(AtanHigh[3], AtanLow[3]);
        quadrant &= 1;
    } else if ((px.signExponent & 0x8000) != 0 && k < -64 - 2) {
        z = 0.0L;
    } else {
        z = atanl_nid_postfix(std::fabs(y / x));
    }
    if (quadrant == 0) return z;
    if (quadrant == 1) return -z;
    if (quadrant == 2) return Pi - (z - PiLow);
    return (z - PiLow) - Pi;
}
long double APS5_VABI sinl_nid_postfix(long double x) { return SinlMsun(x); }
long double APS5_VABI cosl_nid_postfix(long double x) { return CoslMsun(x); }
long double APS5_VABI tanl_nid_postfix(long double x) { return TanlMsun(x); }
long double APS5_VABI sinhl_nid_postfix(long double x) { return SinhlMsun(x); }
long double APS5_VABI coshl_nid_postfix(long double x) { return CoshlMsun(x); }
long double APS5_VABI tanhl_nid_postfix(long double x) { return TanhlMsun(x); }
long double APS5_VABI expm1l_nid_postfix(long double x) { return Expm1lMsun(x); }
long double APS5_VABI erfl_nid_postfix(long double x) { return ErflMsun(x); }
long double APS5_VABI erfcl_nid_postfix(long double x) { return ErfclMsun(x); }
/*-
 * Copyright (c) 2005-2008 David Schultz <das@FreeBSD.ORG>
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
/* Adapted from FreeBSD release/11.4.0 lib/msun/ld80/s_exp2l.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/s_exp2l.c */
constexpr double Exp2Table[] = {
    0x1.6a09e667f3bcdp-1, -0x1.bdd3413b26480p-55, 0x1.6c012750bdabfp-1, -0x1.2895667ff0c00p-57,
    0x1.6dfb23c651a2fp-1, -0x1.bbe3a683c8800p-58, 0x1.6ff7df9519484p-1, -0x1.83c0f25860f00p-56,
    0x1.71f75e8ec5f74p-1, -0x1.16e4786887b00p-56, 0x1.73f9a48a58174p-1, -0x1.0a8d96c65d500p-55,
    0x1.75feb564267c9p-1, -0x1.0245957316e00p-55, 0x1.780694fde5d3fp-1, 0x1.866b80a021600p-55,
    0x1.7a11473eb0187p-1, -0x1.41577ee049900p-56, 0x1.7c1ed0130c132p-1, 0x1.f124cd1164e00p-55,
    0x1.7e2f336cf4e62p-1, 0x1.05d02ba157a00p-57, 0x1.80427543e1a12p-1, -0x1.27c86626d9700p-55,
    0x1.82589994cce13p-1, -0x1.d4c1dd4153300p-55, 0x1.8471a4623c7adp-1, -0x1.8d684a341ce00p-56,
    0x1.868d99b4492edp-1, -0x1.fc6f89bd4f680p-55, 0x1.88ac7d98a6699p-1, 0x1.994c2f37cb500p-55,
    0x1.8ace5422aa0dbp-1, 0x1.6e9f156864b00p-55, 0x1.8cf3216b5448cp-1, -0x1.0d55e32e9e400p-57,
    0x1.8f1ae99157736p-1, 0x1.5cc13a2e39700p-56, 0x1.9145b0b91ffc6p-1, -0x1.dd6792e582500p-55,
    0x1.93737b0cdc5e5p-1, -0x1.75fc781b58000p-58, 0x1.95a44cbc8520fp-1, -0x1.64b7c96a5f000p-57,
    0x1.97d829fde4e50p-1, -0x1.d185b7c1b8600p-55, 0x1.9a0f170ca07bap-1, -0x1.173bd91cee600p-55,
    0x1.9c49182a3f090p-1, 0x1.c7c46b071f200p-57, 0x1.9e86319e32323p-1, 0x1.824ca78e64c00p-57,
    0x1.a0c667b5de565p-1, -0x1.359495d1cd500p-55, 0x1.a309bec4a2d33p-1, 0x1.6305c7ddc3680p-55,
    0x1.a5503b23e255dp-1, -0x1.d2f6edb8d4200p-55, 0x1.a799e1330b358p-1, 0x1.bcb7ecac56400p-55,
    0x1.a9e6b5579fdbfp-1, 0x1.0fac90ef7fd00p-55, 0x1.ac36bbfd3f37ap-1, -0x1.f9234cae76d00p-56,
    0x1.ae89f995ad3adp-1, 0x1.7a1cd345dcc80p-55, 0x1.b0e07298db666p-1, -0x1.bdef54c80e400p-55,
    0x1.b33a2b84f15fbp-1, -0x1.2805e3084d800p-58, 0x1.b59728de5593ap-1, -0x1.c71dfbbba6e00p-55,
    0x1.b7f76f2fb5e47p-1, -0x1.5584f7e54ac00p-57, 0x1.ba5b030a1064ap-1, -0x1.efcd30e542900p-55,
    0x1.bcc1e904bc1d2p-1, 0x1.23dd07a2d9f00p-56, 0x1.bf2c25bd71e09p-1, -0x1.efdca3f6b9c80p-55,
    0x1.c199bdd85529cp-1, 0x1.1106589504900p-56, 0x1.c40ab5fffd07ap-1, 0x1.b4537e083c600p-55,
    0x1.c67f12e57d14bp-1, 0x1.2884dff483c80p-55, 0x1.c8f6d9406e7b5p-1, 0x1.1acbc48805c00p-57,
    0x1.cb720dcef9069p-1, 0x1.503cbd1e94a00p-57, 0x1.cdf0b555dc3fap-1, -0x1.dd83b53829d00p-56,
    0x1.d072d4a07897cp-1, -0x1.cbc3743797a80p-55, 0x1.d2f87080d89f2p-1, -0x1.d487b719d8580p-55,
    0x1.d5818dcfba487p-1, 0x1.2ed02d75b3700p-56, 0x1.d80e316c98398p-1, -0x1.11ec18bede000p-55,
    0x1.da9e603db3285p-1, 0x1.c2300696db500p-55, 0x1.dd321f301b460p-1, 0x1.2da5778f01900p-55,
    0x1.dfc97337b9b5fp-1, -0x1.1a5cd4f184b80p-55, 0x1.e264614f5a129p-1, -0x1.7b627817a1480p-55,
    0x1.e502ee78b3ff6p-1, 0x1.39e8980a9cd00p-56, 0x1.e7a51fbc74c83p-1, 0x1.2d522ca0c8e00p-55,
    0x1.ea4afa2a490dap-1, -0x1.e9c23179c2880p-55, 0x1.ecf482d8e67f1p-1, -0x1.c93f3b411ad80p-55,
    0x1.efa1bee615a27p-1, 0x1.dc7f486a4b680p-55, 0x1.f252b376bba97p-1, 0x1.3a1a5bf0d8e80p-55,
    0x1.f50765b6e4540p-1, 0x1.9d3e12dd8a180p-55, 0x1.f7bfdad9cbe14p-1, -0x1.dbb12d0063500p-55,
    0x1.fa7c1819e90d8p-1, 0x1.74853f3a59300p-56, 0x1.fd3c22b8f71f1p-1, 0x1.2eb7496657800p-58,
    0x1.0000000000000p+0, 0.0, 0x1.0163da9fb3335p+0, 0x1.b61299ab8cd80p-54,
    0x1.02c9a3e778061p+0, -0x1.19083535b0800p-56, 0x1.04315e86e7f85p+0, -0x1.0a31c1977c980p-54,
    0x1.059b0d3158574p+0, 0x1.d73e2a475b400p-55, 0x1.0706b29ddf6dep+0, -0x1.c91dfe2b13c00p-55,
    0x1.0874518759bc8p+0, 0x1.186be4bb28400p-57, 0x1.09e3ecac6f383p+0, 0x1.1487818316100p-54,
    0x1.0b5586cf9890fp+0, 0x1.8a62e4adc6100p-54, 0x1.0cc922b7247f7p+0, 0x1.01edc16e24f80p-54,
    0x1.0e3ec32d3d1a2p+0, 0x1.03a1727c58000p-59, 0x1.0fb66affed31bp+0, -0x1.b9bedc44ebc00p-57,
    0x1.11301d0125b51p+0, -0x1.6c51039449b00p-54, 0x1.12abdc06c31ccp+0, -0x1.1b514b36ca800p-58,
    0x1.1429aaea92de0p+0, -0x1.32fbf9af13680p-54, 0x1.15a98c8a58e51p+0, 0x1.2406ab9eeab00p-55,
    0x1.172b83c7d517bp+0, -0x1.19041b9d78a00p-55, 0x1.18af9388c8deap+0, -0x1.11023d1970f80p-54,
    0x1.1a35beb6fcb75p+0, 0x1.e5b4c7b496900p-55, 0x1.1bbe084045cd4p+0, -0x1.95386352ef600p-54,
    0x1.1d4873168b9aap+0, 0x1.e016e00a26400p-54, 0x1.1ed5022fcd91dp+0, -0x1.1df98027bb780p-54,
    0x1.2063b88628cd6p+0, 0x1.dc775814a8500p-55, 0x1.21f49917ddc96p+0, 0x1.2a97e9494a600p-55,
    0x1.2387a6e756238p+0, 0x1.9b07eb6c70580p-54, 0x1.251ce4fb2a63fp+0, 0x1.ac155bef4f500p-55,
    0x1.26b4565e27cddp+0, 0x1.2bd339940ea00p-55, 0x1.284dfe1f56381p+0, -0x1.a4c3a8c3f0d80p-54,
    0x1.29e9df51fdee1p+0, 0x1.612e8afad1200p-55, 0x1.2b87fd0dad990p+0, -0x1.10adcd6382000p-59,
    0x1.2d285a6e4030bp+0, 0x1.0024754db4200p-54, 0x1.2ecafa93e2f56p+0, 0x1.1ca0f45d52400p-56,
    0x1.306fe0a31b715p+0, 0x1.6f46ad2318300p-55, 0x1.32170fc4cd831p+0, 0x1.a9ce78e180400p-55,
    0x1.33c08b26416ffp+0, 0x1.3272184365980p-54, 0x1.356c55f929ff1p+0, -0x1.b5cee5c4e4600p-55,
    0x1.371a7373aa9cbp+0, -0x1.63aeabf42eb00p-54, 0x1.38cae6d05d866p+0, -0x1.e958d3c990480p-54,
    0x1.3a7db34e59ff7p+0, -0x1.5e436d661f600p-56, 0x1.3c32dc313a8e5p+0, -0x1.efff8375d2a00p-54,
    0x1.3dea64c123422p+0, 0x1.ada0911f09f00p-55, 0x1.3fa4504ac801cp+0, -0x1.7d023f956fa00p-54,
    0x1.4160a21f72e2ap+0, -0x1.ef3691c309000p-58, 0x1.431f5d950a897p+0, -0x1.1c7dde35f7a00p-55,
    0x1.44e086061892dp+0, 0x1.89b7a04ef8000p-59, 0x1.46a41ed1d0057p+0, 0x1.c944bd1648a80p-54,
    0x1.486a2b5c13cd0p+0, 0x1.3c1a3b6906200p-56, 0x1.4a32af0d7d3dep+0, 0x1.9cb62f3d1be80p-54,
    0x1.4bfdad5362a27p+0, 0x1.d4397afec4200p-56, 0x1.4dcb299fddd0dp+0, 0x1.8ecdbbc6a7800p-54,
    0x1.4f9b2769d2ca7p+0, -0x1.4b309d2595800p-54, 0x1.516daa2cf6642p+0, -0x1.f768569bd9400p-55,
    0x1.5342b569d4f82p+0, -0x1.07abe1db13d00p-55, 0x1.551a4ca5d920fp+0, -0x1.d689cefede600p-55,
    0x1.56f4736b527dap+0, 0x1.9bb2c011d9380p-54, 0x1.58d12d497c7fdp+0, 0x1.295e15b9a1e00p-55,
    0x1.5ab07dd485429p+0, 0x1.6324c05464780p-54, 0x1.5c9268a5946b7p+0, 0x1.c4b1b81698000p-60,
    0x1.5e76f15ad2148p+0, 0x1.ba6f93080e680p-54, 0x1.605e1b976dc09p+0, -0x1.3e2429b56de80p-54,
    0x1.6247eb03a5585p+0, -0x1.383c17e40b480p-54, 0x1.6434634ccc320p+0, -0x1.c483c759d8900p-55,
    0x1.6623882552225p+0, -0x1.bb60987591c00p-54, 0x1.68155d44ca973p+0, 0x1.038ae44f74000p-57,
};
long double APS5_VABI exp2l_nid_postfix(long double x) {
    static volatile long double twoToMinus10000 = 0x1p-10000L;
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    if (exponent >= 0x3fff + 14) {
        if (exponent == 0x7fff) return (parts.signExponent & 0x8000) != 0 && parts.mantissa == 0x8000000000000000ull ? 0.0L : x + x;
        if (x >= 16384) return std::numeric_limits<long double>::infinity();
        if (x <= -16446) return twoToMinus10000 * twoToMinus10000;
    } else if (exponent <= 0x3fff - 66) {
        return 1.0L + x;
    }
    constexpr double redux = 0x1.8p63 / 128;
    const long double shifted = x + redux;
    const std::uint32_t bits = static_cast<std::uint32_t>(SplitExtended(shifted).mantissa) + 64;
    const int scale = static_cast<int>(bits) >> 7;
    const std::uint32_t index = (bits & 127) << 1;
    const long double z = x - (shifted - redux);
    const long double high = Exp2Table[index];
    const long double low = Exp2Table[index + 1];
    const long double r = low + (high + low) * z * (0x1.62e42fefa39efp-1 + z * (0x1.ebfbdff82c58fp-3 + z * (0x1.c6b08d7049fap-5 +
        z * (0x1.3b2ab6fba4da5p-7 + z * (0x1.5d8804780a736p-10 + z * 0x1.430918835e33dp-13))))) + high;
    if (scale >= -16381) {
        if (scale == 16384) return r * 2.0 * 0x1p16383L;
        return r * std::ldexp(1.0L, scale);
    }
    return r * std::ldexp(1.0L, scale + 10000) * twoToMinus10000;
}
long double APS5_VABI logl_nid_postfix(long double x) { return _LLog_nid_postfix(x, 0); }
long double APS5_VABI log10l_nid_postfix(long double x) { return _LLog_nid_postfix(x, 1); }
long double APS5_VABI ldexpl_nid_postfix(long double x, int exp) { return scalbnl_nid_postfix(x, exp); }
long double APS5_VABI truncl_nid_postfix(long double x) { return std::trunc(x); }
long double APS5_VABI ceill_nid_postfix(long double x) { return std::ceil(x); }
long double APS5_VABI floorl_nid_postfix(long double x) { return std::floor(x); }
double APS5_VABI logb_nid_postfix(double x) {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull;
    if (bits == 0) {
        return -1.0 / std::fabs(x);
    }
    if (bits >= 0x7ff0000000000000ull) {
        return x * x;
    }
    if (bits < 0x0010000000000000ull) {
        const std::uint64_t scaled = std::bit_cast<std::uint64_t>(x * 0x1p54) & 0x7fffffffffffffffull;
        return static_cast<double>(static_cast<int>(scaled >> 52) - 1023 - 54);
    }
    return static_cast<double>(static_cast<int>(bits >> 52) - 1023);
}
extern "C++" {
template<typename TBits, int FractionBits>
static int BinaryExponent(TBits bits) {
    constexpr int ExponentBits = static_cast<int>(sizeof(TBits) * 8) - 1 - FractionBits;
    constexpr int Bias = (1 << (ExponentBits - 1)) - 1;
    const TBits magnitude = bits & ~(TBits{1} << (sizeof(TBits) * 8 - 1));
    const int field = static_cast<int>(magnitude >> FractionBits);
    if (magnitude == 0) return INT_MIN;
    if (field == (1 << ExponentBits) - 1) return INT_MAX;
    return field != 0 ? field - Bias : static_cast<int>(std::bit_width(magnitude)) - FractionBits - Bias;
}
}
int APS5_VABI ilogb_nid_postfix(double x) { return BinaryExponent<std::uint64_t, 52>(std::bit_cast<std::uint64_t>(x)); }
int APS5_VABI ilogbf_nid_postfix(float x) { return BinaryExponent<std::uint32_t, 23>(std::bit_cast<std::uint32_t>(x)); }
double APS5_VABI remainder_nid_postfix(double x, double y) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    SetRemainderDomainError(x, y);
    return static_cast<double>(remainder);
}
double APS5_VABI remquo_nid_postfix(double x, double y, int* quo) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    SetRemainderDomainError(x, y);
    return static_cast<double>(remainder);
}
float APS5_VABI remquof_nid_postfix(float x, float y, int* quo) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    SetRemainderDomainError(x, y);
    return static_cast<float>(remainder);
}
float APS5_VABI _FSinh_nid_postfix(float x, float y) { return ScaledHyperbolic(x, y, true); }
float APS5_VABI _FCosh_nid_postfix(float x, float y) { return ScaledHyperbolic(x, y, false); }

struct alignas(16) LibcFloatConstant { std::uint32_t bits[4]; };
LibcFloatConstant _FInf_nid_postfix {{0x7f800000u, 0, 0, 0}};
LibcFloatConstant _FNan_nid_postfix {{0x7fc00000u, 0, 0, 0}};

short APS5_VABI _FDtest_nid_postfix(const float* value) {
    constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;
    if (value == nullptr) throw std::invalid_argument("_FDtest: null value");
    std::uint32_t bits;
    std::memcpy(&bits, value, sizeof(bits));
    const auto exponent = bits & 0x7f800000u;
    const auto fraction = bits & 0x007fffffu;
    if (exponent == 0x7f800000u) return fraction != 0 ? NotANumber : Infinite;
    if (exponent == 0) return fraction != 0 ? Denormal : Zero;
    return Finite;
}

short APS5_VABI _Dtest_nid_postfix(const double* value) {
    constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;
    if (value == nullptr) throw std::invalid_argument("_Dtest: null value");
    std::uint64_t bits;
    std::memcpy(&bits, value, sizeof(bits));
    const auto exponent = bits & 0x7ff0000000000000u;
    const auto fraction = bits & 0x000fffffffffffffu;
    if (exponent == 0x7ff0000000000000u) return fraction != 0 ? NotANumber : Infinite;
    if (exponent == 0) return fraction != 0 ? Denormal : Zero;
    return Finite;
}

short APS5_VABI _FDsign_nid_postfix(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<short>((bits >> 16) & 0x8000u);
}

short APS5_VABI _Dsign_nid_postfix(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<short>((bits >> 48) & 0x8000u);
}

int APS5_VABI __isnanf_nid_postfix(float x) {
    return (std::bit_cast<std::uint32_t>(x) & 0x7fffffffu) > 0x7f800000u ? 1 : 0;
}
int APS5_VABI isnanf_nid_postfix(float x) { return __isnanf_nid_postfix(x); }
int APS5_VABI isnan_nid_postfix(double x) { return __isnan_nid_postfix(x); }
int APS5_VABI isinf_nid_postfix(double x) { return __isinf_nid_postfix(x); }
int APS5_VABI __fpclassifyf_nid_postfix(float x) { return _FDtest_nid_postfix(&x); }
int APS5_VABI __signbitf_nid_postfix(float x) { return std::signbit(x) ? 0x8000 : 0; }
int APS5_VABI __signbitl_nid_postfix(long double x) {
    static_assert(sizeof(long double) == 16, "guest long double requires x87 extended precision storage");
    return std::signbit(x) ? 0x8000 : 0;
}

int APS5_VABI __isfinitel_nid_postfix(long double x) {
    static_assert(sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64 && std::numeric_limits<long double>::max_exponent == 16384 && std::endian::native == std::endian::little, "guest long double requires little-endian x87 extended precision storage");
    std::uint16_t exponent;
    std::memcpy(&exponent, reinterpret_cast<const std::uint8_t*>(&x) + 8, sizeof(exponent));
    return (exponent & 0x7fffu) != 0x7fffu;
}

struct DinkumwareFloatCharacteristics {
    int digits;
    int mantissaDigits;
    int maxExponent10;
    int maxExponent;
    int minExponent10;
    int minExponent;
    alignas(16) std::uint16_t epsilon[8];
    alignas(16) std::uint16_t max[8];
    alignas(16) std::uint16_t min[8];
};
static_assert(sizeof(DinkumwareFloatCharacteristics) == 80 && offsetof(DinkumwareFloatCharacteristics, epsilon) == 32);

DinkumwareFloatCharacteristics _Flt_nid_postfix {6, 24, 38, 128, -37, -125,
    {0, 0x3400}, {0xffff, 0x7f7f}, {0, 0x0080}};
DinkumwareFloatCharacteristics _Dbl_nid_postfix {15, 53, 308, 1024, -307, -1021,
    {0, 0, 0, 0x3cb0}, {0xffff, 0xffff, 0xffff, 0x7fef}, {0, 0, 0, 0x0010}};
DinkumwareFloatCharacteristics _Ldbl_nid_postfix {18, 64, 4932, 16384, -4931, -16381,
    {0, 0, 0, 0x8000, 0x3fc0}, {0xffff, 0xffff, 0xffff, 0xffff, 0x7ffe}, {0, 0, 0, 0x8000, 0x0001}};

static std::mutex g_randLock;
static std::uint64_t g_randState = 1;

static std::uint32_t MinimalStandardNext(std::int32_t state) {
    if (state == 0) return 0x2ae1b767u;
    const auto product = static_cast<std::uint64_t>(static_cast<std::int64_t>(state)) * 16807u;
    const auto high = static_cast<std::uint32_t>(product >> 31u);
    const auto sum = (static_cast<std::uint32_t>(product) & 0x7fffffffu) + high;
    if (sum < 0x7fffffffu) return sum;
    if (sum == 0x7fffffffu) return 0;
    return static_cast<std::int32_t>(high) <= 0 ? (sum & 0x7fffffffu) - 1u : (sum & 0x7fffffffu) + 1u;
}

int APS5_VABI rand_nid_postfix() {
    std::lock_guard lock(g_randLock);
    g_randState = g_randState * 0x5851f42d4c957f2dull + 1u;
    return static_cast<int>((g_randState >> 32u) & 0x3fffffffu);
}

int APS5_VABI rand_r_nid_postfix(unsigned int* state) {
    if (state == nullptr) throw std::invalid_argument("rand_r: null state");
    return static_cast<int>(*state);
}

void APS5_VABI srand_nid_postfix(unsigned int seed) {
    std::lock_guard lock(g_randLock);
    g_randState = seed;
}

int APS5_VABI __fpclassifyd_nid_postfix(double value) {
    return SonyFpclassifyd(value);
}

double APS5_VABI _Sinh_nid_postfix(double x, double y) {
    return SonySinh(x, y);
}

static double CoshProduct(double x, double y) {
    if (std::isnan(x)) return x;
    if (std::isnan(y)) return y;
    if (y == 0.0 && std::isfinite(x)) return y;
    double value = std::fabs(x);
    const bool exponential = value >= 18.738;
    _Exp_nid_postfix(&value, exponential ? y : 1.0, -1);
    return exponential ? value : y * (value + 0.25 / value);
}

double APS5_VABI _Cosh_nid_postfix(double x, double y) {
    const double result = CoshProduct(x, y);
    if (std::isnan(result) && !std::isnan(x) && !std::isnan(y)) errno = EDOM;
    else if (std::isinf(result) && std::isfinite(x) && std::isfinite(y)) errno = ERANGE;
    return result;
}

long double APS5_VABI _LCosh_nid_postfix(long double x, long double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return y * std::cosh(x);
    if (y == 0.0L) return std::copysign(0.0L, y);
    constexpr long double Ln2High = 0x1.62e42fefa0000p-1L;
    constexpr long double Ln2Low = 0x1.cf79abc9e3b398p-40L;
    constexpr long double InverseLn2 = 0x1.71547652b82fe178p+0L;
    const long double magnitude = std::abs(x);
    if (magnitude <= (std::numeric_limits<long double>::max_exponent - 1) * (Ln2High + Ln2Low))
        return y * std::cosh(x);
    int scaleExponent;
    const long double scale = std::frexp(y, &scaleExponent);
    constexpr int MaximumExponentSpan = std::numeric_limits<long double>::max_exponent -
        std::numeric_limits<long double>::min_exponent + std::numeric_limits<long double>::digits + 4;
    if (magnitude > MaximumExponentSpan * (Ln2High + Ln2Low))
        return std::scalbn(scale, INT_MAX);
    const int exponent = static_cast<int>(magnitude * InverseLn2);
    const long double negatedExponent = -exponent;
    const long double reduced = std::fma(negatedExponent, Ln2Low, std::fma(negatedExponent, Ln2High, magnitude));
    return std::scalbn(scale * (0.5L * std::exp(reduced)), scaleExponent + exponent);
}

long double APS5_VABI _LSinh_nid_postfix(long double x, long double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return y * std::sinh(x);
    if (y == 0.0L) {
        const bool negative = std::signbit(x) != std::signbit(y);
        return std::copysign(0.0L, negative ? -1.0L : 1.0L);
    }
    constexpr long double Ln2High = 0x1.62e42fefa0000p-1L;
    constexpr long double Ln2Low = 0x1.cf79abc9e3b398p-40L;
    constexpr long double InverseLn2 = 0x1.71547652b82fe178p+0L;
    const long double magnitude = std::abs(x);
    if (magnitude <= (std::numeric_limits<long double>::max_exponent - 1) * (Ln2High + Ln2Low))
        return y * std::sinh(x);
    int scaleExponent;
    const long double scale = std::frexp(y, &scaleExponent);
    constexpr int MaximumExponentSpan = std::numeric_limits<long double>::max_exponent -
        std::numeric_limits<long double>::min_exponent + std::numeric_limits<long double>::digits + 4;
    const long double sign = std::copysign(1.0L, x);
    if (magnitude > MaximumExponentSpan * (Ln2High + Ln2Low))
        return sign * std::scalbn(scale, INT_MAX);
    const int exponent = static_cast<int>(magnitude * InverseLn2);
    const long double negatedExponent = -exponent;
    const long double reduced = std::fma(negatedExponent, Ln2Low, std::fma(negatedExponent, Ln2High, magnitude));
    return sign * std::scalbn(scale * (0.5L * std::exp(reduced)), scaleExponent + exponent);
}

int APS5_VABI _Fltrounds_nid_postfix(void) {
    const std::uint16_t control = GuestCpu::X87Control();
    constexpr std::array<int, 4> Rounding{1, 3, 2, 0};
    return Rounding[(control >> 10u) & 3u];
}

static_assert(sizeof(GuestFloatingPointEnvironment) == 32 && alignof(GuestFloatingPointEnvironment) == 4 && offsetof(GuestFloatingPointEnvironment, mxcsr) == 28);

int APS5_VABI feupdateenv_nid_postfix(const GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("feupdateenv: null environment");
    GuestFloatingPointEnvironment saved{};
    if (!GuestCopyIn_nid_no_patch(&saved, environment, sizeof(saved))) throw std::invalid_argument("feupdateenv: unreadable environment");
    if ((saved.mxcsr & ~GuestCpu::MxcsrMask()) != 0) throw std::invalid_argument("feupdateenv: unsupported MXCSR bits");
    const auto pending = (GuestCpu::X87Status() | GuestCpu::Mxcsr()) & 0x3f;
    std::array<std::uint32_t, 7> x87;
    std::memcpy(x87.data(), &saved, sizeof(x87));
    GuestCpu::SetX87Environment(x87);
    GuestCpu::SetMxcsr(saved.mxcsr);
    GuestCpu::SetX87Status(static_cast<std::uint16_t>(GuestCpu::X87Status() | pending));
    GuestCpu::SetMxcsr(GuestCpu::Mxcsr() | pending);
    GuestCpu::X87Wait();
    return 0;
}

int APS5_VABI fegetenv_nid_postfix(GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("fegetenv: null environment");
    const auto x87 = GuestCpu::X87Environment();
    GuestFloatingPointEnvironment current{};
    std::memcpy(&current, x87.data(), sizeof(x87));
    current.mxcsr = GuestCpu::Mxcsr();
    if (!GuestCopyOut_nid_no_patch(environment, &current, sizeof(current))) throw std::invalid_argument("fegetenv: unwritable environment");
    return 0;
}

int APS5_VABI feholdexcept_nid_postfix(GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("feholdexcept: null environment");
    const auto csr = GuestCpu::Mxcsr();
    const auto x87 = GuestCpu::X87Environment();
    GuestFloatingPointEnvironment held{};
    std::memcpy(&held, x87.data(), sizeof(x87));
    held.mxcsr = csr;
    if (!GuestCopyOut_nid_no_patch(environment, &held, sizeof(held))) throw std::invalid_argument("feholdexcept: unwritable environment");
    GuestCpu::X87ClearExceptions();
    GuestCpu::SetX87Control(static_cast<std::uint16_t>(x87[0] | 0x3fu));
    GuestCpu::SetMxcsr((csr & ~0x3fu) | 0x1f80u);
    return 0;
}

const GuestFloatingPointEnvironment __fe_dfl_env_nid_postfix {0xffff037fu, 0xffff0000u, 0xffffffffu, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff}, 0x9fc0u};

union SonyDconst {
    unsigned short _Word[8];
    float _Float;
    double _Double;
    long double _Long_double;
};

static_assert(std::endian::native == std::endian::little);
SonyDconst _Inf_nid_postfix {{0, 0, 0, 0x7ff0, 0, 0, 0, 0}};
SonyDconst _Denorm_nid_postfix {{1, 0, 0, 0, 0, 0, 0, 0}};
SonyDconst _Hugeval_nid_postfix {{0, 0, 0, 0x7ff0, 0, 0, 0, 0}};
SonyDconst _Nan_nid_postfix {{0, 0, 0, 0x7ff8, 0, 0, 0, 0}};
SonyDconst _Snan_nid_postfix {{1, 0, 0, 0x7ff0, 0, 0, 0, 0}};
LibcFloatConstant _FDenorm_nid_postfix {{0x00000001u, 0, 0, 0}};
LibcFloatConstant _FSnan_nid_postfix {{0x7f800001u, 0, 0, 0}};
SonyDconst _LDenorm_nid_postfix {{1, 0, 0, 0, 0, 0, 0, 0}};
SonyDconst _LInf_nid_postfix {{0, 0, 0, 0x8000, 0x7fff, 0, 0, 0}};
SonyDconst _LNan_nid_postfix {{0, 0, 0, 0xc000, 0x7fff, 0, 0, 0}};
SonyDconst _LSnan_nid_postfix {{1, 0, 0, 0x8000, 0x7fff, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v27_DenormE_nid_postfix {{1, 0, 0, 0, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v24_EpsE_nid_postfix {{0, 0, 0, 0x3c90, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v28_HugevalE_nid_postfix {{0, 0, 0, 0x7ff0, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v24_InfE_nid_postfix {{0, 0, 0, 0x7ff0, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v24_NanE_nid_postfix {{0, 0, 0, 0x7ff8, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v26_RtepsE_nid_postfix {{0, 0, 0, 0x3e40, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v25_SnanE_nid_postfix {{1, 0, 0, 0x7ff0, 0, 0, 0, 0}};
double _ZNSt8__sce_v25_XbigE_nid_postfix = std::bit_cast<double>(std::uint64_t{0x4032bced916872b0});
LibcFloatConstant _ZNSt8__sce_v28_FDenormE_nid_postfix {{0x00000001u, 0, 0, 0}};
LibcFloatConstant _ZNSt8__sce_v25_FEpsE_nid_postfix {{0x33000000u, 0, 0, 0}};
LibcFloatConstant _ZNSt8__sce_v25_FInfE_nid_postfix {{0x7f800000u, 0, 0, 0}};
LibcFloatConstant _ZNSt8__sce_v25_FNanE_nid_postfix {{0x7fc00000u, 0, 0, 0}};
LibcFloatConstant _ZNSt8__sce_v27_FRtepsE_nid_postfix {{0x39800000u, 0, 0, 0}};
LibcFloatConstant _ZNSt8__sce_v26_FSnanE_nid_postfix {{0x7f800001u, 0, 0, 0}};
float _ZNSt8__sce_v26_FXbigE_nid_postfix = std::bit_cast<float>(std::uint32_t{0x410acccd});
SonyDconst _ZNSt8__sce_v28_LDenormE_nid_postfix {{1, 0, 0, 0, 0, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v25_LEpsE_nid_postfix {{0, 0, 0, 0x8000, 0x3fbd, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v25_LInfE_nid_postfix {{0, 0, 0, 0x8000, 0x7fff, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v25_LNanE_nid_postfix {{0, 0, 0, 0xc000, 0x7fff, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v27_LRtepsE_nid_postfix {{0, 0, 0, 0x8000, 0x3fde, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v26_LSnanE_nid_postfix {{1, 0, 0, 0x8000, 0x7fff, 0, 0, 0}};
SonyDconst _ZNSt8__sce_v26_LXbigE_nid_postfix {{0, 0, 0, 0xb000, 0x4003, 0, 0, 0}};

int APS5_VABI fesetenv_nid_postfix(const GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("fesetenv: null environment");
    GuestFloatingPointEnvironment requested{};
    if (!GuestCopyIn_nid_no_patch(&requested, environment, sizeof(requested))) throw std::invalid_argument("fesetenv: unreadable environment");
    if ((requested.mxcsr & ~GuestCpu::MxcsrMask()) != 0) throw std::invalid_argument("fesetenv: unsupported MXCSR bits");
    std::array<std::uint32_t, 7> x87;
    std::memcpy(x87.data(), &requested, sizeof(x87));
    GuestCpu::SetX87Environment(x87);
    GuestCpu::SetMxcsr(requested.mxcsr);
    return 0;
}
}
