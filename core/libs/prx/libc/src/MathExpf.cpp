/* Adapted from FreeBSD msun e_expf.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/e_expf.c */
/* Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com. */
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
#include <cstdint>
#include <limits>

extern "C" {

float APS5_VABI expf_nid_postfix(float x) {
    static constexpr float halF[2] = {0.5f, -0.5f};
    static constexpr float ln2HI[2] = {0x1.62e4000000000p-1f, -0x1.62e4000000000p-1f};
    static constexpr float ln2LO[2] = {0x1.7f7d1c0000000p-20f, -0x1.7f7d1c0000000p-20f};
    static volatile float huge = 0x1.93e5940000000p+99f;
    static volatile float twom100 = 0x1.0000000000000p-100f;
    const float invln2 = 0x1.7154760000000p+0f;
    const float P1 = 0x1.55551e0000000p-3f;
    const float P2 = -0x1.6aa42a0000000p-9f;
    float hi = 0.0f;
    float lo = 0.0f;
    std::int32_t k = 0;
    std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::int32_t xsb = (hx >> 31) & 1;
    hx &= 0x7fffffff;
    if (hx >= 0x42b17218) {
        if (hx > 0x7f800000) {
            return x + x;
        }
        if (hx == 0x7f800000) {
            return xsb == 0 ? x : 0.0f;
        }
        if (x > 0x1.62e3000000000p+6f) {
            _Feraise_nid_postfix(0x08);
            return std::numeric_limits<float>::infinity();
        }
        if (x < -0x1.9fe36a0000000p+6f) {
            _Feraise_nid_postfix(0x10);
            return twom100 * twom100;
        }
    }
    if (x == 1.0f) return 0x1.5bf0a8p+1f;
    if (hx > 0x3eb17218) {
        if (hx < 0x3f851592) {
            hi = x - ln2HI[xsb];
            lo = ln2LO[xsb];
            k = 1 - xsb - xsb;
        } else {
            k = static_cast<std::int32_t>(invln2 * x + halF[xsb]);
            const float t = static_cast<float>(k);
            hi = x - t * ln2HI[0];
            lo = t * ln2LO[0];
        }
        x = hi - lo;
    } else {
        const volatile float trigger = huge + x;
        if (trigger > 1.0f && hx < 0x39000000) return 1.0f + x;
    }
    const float t = x * x;
    const float twopk = std::bit_cast<float>(static_cast<std::uint32_t>(0x7f + (k >= -125 ? k : k + 100)) << 23);
    const float c = x - t * (P1 + t * P2);
    if (k == 0) {
        return 1.0f + (x - (x * c) / (c - 2.0f));
    }
    const float y = 1.0f + (hi - (lo - (x * c) / (2.0f - c)));
    if (k >= -125) {
        if (k == 128) {
            return (y + y) * 0x1p127f;
        }
        return y * twopk;
    }
    return y * twopk * twom100;
}

}
