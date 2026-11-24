// SPDX-FileCopyrightText: Copyright (c) 2011 David Schultz
// SPDX-License-Identifier: BSD-2-Clause
// Adapted from FreeBSD lib/msun/src/s_ctanh.c (release/11.4.0): https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_ctanh.c
/*-
 * Copyright (c) 2011 David Schultz
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

#include "prx/libc/include/Api.hpp"
#include <cmath>

namespace {

constexpr long double LargeMagnitude = 26.0L;

__complex__ long double Make(long double real, long double imaginary) {
    __complex__ long double result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

}

extern "C" {

__complex__ long double APS5_VABI ctanhl_nid_postfix(__complex__ long double z) {
    const long double x = __real__ z;
    const long double y = __imag__ z;
    if (std::isnan(x)) return Make((x + 0) * (y + 0), y == 0 ? y : (x + 0) * (y + 0));
    if (std::isinf(x)) return Make(std::copysign(1.0L, x), std::copysign(0.0L, std::isinf(y) ? y : std::sin(y) * std::cos(y)));
    if (!std::isfinite(y)) return Make(y - y, y - y);
    if (std::fabs(x) >= LargeMagnitude) {
        const long double exponential = std::exp(-std::fabs(x));
        return Make(std::copysign(1.0L, x), 4 * std::sin(y) * std::cos(y) * exponential * exponential);
    }
    const long double t = std::tan(y);
    const long double beta = 1 + t * t;
    const long double s = std::sinh(x);
    const long double rho = std::sqrt(1 + s * s);
    const long double denominator = 1 + beta * s * s;
    return Make(beta * rho * s / denominator, t / denominator);
}

__complex__ long double APS5_VABI ctanl_nid_postfix(__complex__ long double z) {
    const __complex__ long double result = ctanhl_nid_postfix(Make(__imag__ z, __real__ z));
    return Make(__imag__ result, __real__ result);
}

}
