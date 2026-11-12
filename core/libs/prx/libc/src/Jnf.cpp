/* e_jnf.c -- float version of e_jn.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
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

#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

#include "prx/libc/include/general/VabiMacros.hpp"

namespace {
std::int32_t WrappedAdd(std::int32_t a, std::int32_t b) { return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
static const float two = 2.0000000000e+00, one = 1.0000000000e+00;

static const float zero = 0.0000000000e+00;

} // namespace

float APS5_VABI jnf_nid_postfix(int n, float x) {
    std::int32_t i, hx, ix, sgn;
    float a, b, temp, di;
    float z, w;

    hx = std::bit_cast<std::int32_t>(x);
    ix = 0x7fffffff & hx;

    if (ix > 0x7f800000)
        return x + x;
    if (n < 0) {
        n = std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(n));
        x = -x;
        hx = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(hx) ^ 0x80000000u);
    }
    if (n == 0)
        return (j0f_nid_postfix(x));
    if (n == 1)
        return (j1f_nid_postfix(x));
    sgn = (n & 1) & (hx >> 31);
    x = std::fabs(x);
    if (ix == 0 || ix >= 0x7f800000)
        b = zero;
    else if ((float)n <= x) {

        a = j0f_nid_postfix(x);
        b = j1f_nid_postfix(x);
        for (i = 1; i < n; i++) {
            temp = b;
            b = b * ((float)WrappedAdd(i, i) / x) - a;
            a = temp;
        }
    } else {
        if (ix < 0x30800000) {

            if (n > 33)
                b = zero;
            else {
                temp = x * (float)0.5;
                b = temp;
                for (a = one, i = 2; i <= n; i++) {
                    a *= (float)i;
                    b *= temp;
                }
                b = b / a;
            }
        } else {

            float t, v;
            float q0, q1, h, tmp;
            std::int32_t k, m;
            w = (WrappedAdd(n, n)) / (float)x;
            h = (float)2.0 / (float)x;
            q0 = w;
            z = w + h;
            q1 = w * z - (float)1.0;
            k = 1;
            while (q1 < (float)1.0e9) {
                k = WrappedAdd(k, 1);
                z += h;
                tmp = z * q1 - q0;
                q0 = q1;
                q1 = tmp;
            }
            m = WrappedAdd(n, n);
            for (t = zero, i = WrappedAdd(WrappedAdd(n, k), WrappedAdd(n, k)); i >= m; i = WrappedAdd(i, -2))
                t = one / (i / x - t);
            a = t;
            b = one;

            tmp = n;
            v = two / x;
            tmp = tmp * logf_nid_postfix(std::fabs(v * tmp));
            if (tmp < (float)8.8721679688e+01) {
                for (i = WrappedAdd(n, -1), di = (float)WrappedAdd(i, i); i > 0; i--) {
                    temp = b;
                    b *= di;
                    b = b / x - a;
                    a = temp;
                    di -= two;
                }
            } else {
                for (i = WrappedAdd(n, -1), di = (float)WrappedAdd(i, i); i > 0; i--) {
                    temp = b;
                    b *= di;
                    b = b / x - a;
                    a = temp;
                    di -= two;

                    if (b > (float)1e10) {
                        a /= b;
                        t /= b;
                        b = one;
                    }
                }
            }
            z = j0f_nid_postfix(x);
            w = j1f_nid_postfix(x);
            if (std::fabs(z) >= std::fabs(w))
                b = (t * z / b);
            else
                b = (t * w / a);
        }
    }
    if (sgn == 1)
        return -b;
    else
        return b;
}

float APS5_VABI ynf_nid_postfix(int n, float x) {
    std::int32_t i, hx, ix, ib;
    std::int32_t sign;
    float a, b, temp;

    hx = std::bit_cast<std::int32_t>(x);
    ix = 0x7fffffff & hx;

    if (ix > 0x7f800000)
        return x + x;
    if (ix == 0)
        return -std::numeric_limits<float>::infinity();
    if (hx < 0)
        return ix == 0x7f800000 ? zero / zero : std::numeric_limits<float>::quiet_NaN();
    sign = 1;
    if (n < 0) {
        n = std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(n));
        sign = 1 - ((n & 1) << 1);
    }
    if (n == 0)
        return (y0f_nid_postfix(x));
    if (n == 1)
        return (sign * y1f_nid_postfix(x));
    if (ix == 0x7f800000)
        return zero;

    a = y0f_nid_postfix(x);
    b = y1f_nid_postfix(x);

    ib = std::bit_cast<std::int32_t>(b);
    for (i = 1; i < n && ib != 0xff800000; i++) {
        temp = b;
        b = ((float)WrappedAdd(i, i) / x) * b - a;
        ib = std::bit_cast<std::int32_t>(b);
        a = temp;
    }
    if (sign > 0)
        return b;
    else
        return -b;
}
