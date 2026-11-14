/* e_j1f.c -- float version of e_j1.c.
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
namespace FloatBessel {
static const volatile float huge = 1e30f;
static const float one = 1.0, invsqrtpi = 5.6418961287e-01, tpi = 6.3661974669e-01;
static const float r00 = -6.2500000000e-02, r01 = 1.4070566976e-03, r02 = -1.5995563444e-05, r03 = 4.9672799207e-08, s01 = 1.9153760746e-02,
                   s02 = 1.8594678841e-04, s03 = 1.1771846857e-06, s04 = 5.0463624390e-09, s05 = 1.2354227016e-11;

static const float zero = 0.0;

static const float U0[5] = {
    -1.9605709612e-01, 5.0443872809e-02, -1.9125689287e-03, 2.3525259166e-05, -9.1909917899e-08,
};
static const float V0[5] = {
    1.9916731864e-02, 2.0255257550e-04, 1.3560879779e-06, 6.2274145840e-09, 1.6655924903e-11,
};

static const float pr8[6] = {
    0.0000000000e+00, 1.1718750000e-01, 1.3239480972e+01, 4.1205184937e+02, 3.8747453613e+03, 7.9144794922e+03,
};
static const float ps8[5] = {
    1.1420736694e+02, 3.6509309082e+03, 3.6956207031e+04, 9.7602796875e+04, 3.0804271484e+04,
};

static const float pr5[6] = {
    1.3199052094e-11, 1.1718749255e-01, 6.8027510643e+00, 1.0830818176e+02, 5.1763616943e+02, 5.2871520996e+02,
};
static const float ps5[5] = {
    5.9280597687e+01, 9.9140142822e+02, 5.3532670898e+03, 7.8446904297e+03, 1.5040468750e+03,
};

static const float pr3[6] = {
    3.0250391081e-09, 1.1718686670e-01, 3.9329774380e+00, 3.5119403839e+01, 9.1055007935e+01, 4.8559066772e+01,
};
static const float ps3[5] = {
    3.4791309357e+01, 3.3676245117e+02, 1.0468714600e+03, 8.9081134033e+02, 1.0378793335e+02,
};

static const float pr2[6] = {
    1.0771083225e-07, 1.1717621982e-01, 2.3685150146e+00, 1.2242610931e+01, 1.7693971634e+01, 5.0735230446e+00,
};
static const float ps2[5] = {
    2.1436485291e+01, 1.2529022980e+02, 2.3227647400e+02, 1.1767937469e+02, 8.3646392822e+00,
};

static float POne(float x) {
    const float *p, *q;
    float z, r, s;
    std::int32_t ix;
    ix = std::bit_cast<std::int32_t>(x);
    ix &= 0x7fffffff;
    if (ix >= 0x41000000) {
        p = pr8;
        q = ps8;
    } else if (ix >= 0x40f71c58) {
        p = pr5;
        q = ps5;
    } else if (ix >= 0x4036db68) {
        p = pr3;
        q = ps3;
    } else {
        p = pr2;
        q = ps2;
    }
    z = one / (x * x);
    r = p[0] + z * (p[1] + z * (p[2] + z * (p[3] + z * (p[4] + z * p[5]))));
    s = one + z * (q[0] + z * (q[1] + z * (q[2] + z * (q[3] + z * q[4]))));
    return one + r / s;
}

static const float qr8[6] = {
    0.0000000000e+00, -1.0253906250e-01, -1.6271753311e+01, -7.5960174561e+02, -1.1849806641e+04, -4.8438511719e+04,
};
static const float qs8[6] = {
    1.6139537048e+02, 7.8253862305e+03, 1.3387534375e+05, 7.1965775000e+05, 6.6660125000e+05, -2.9449025000e+05,
};

static const float qr5[6] = {
    -2.0897993405e-11, -1.0253904760e-01, -8.0564479828e+00, -1.8366960144e+02, -1.3731937256e+03, -2.6124443359e+03,
};
static const float qs5[6] = {
    8.1276550293e+01, 1.9917987061e+03, 1.7468484375e+04, 4.9851425781e+04, 2.7948074219e+04, -4.7191835938e+03,
};

static const float qr3[6] = {
    -5.0783124372e-09, -1.0253783315e-01, -4.6101160049e+00, -5.7847221375e+01, -2.2824453735e+02, -2.1921012878e+02,
};
static const float qs3[6] = {
    4.7665153503e+01, 6.7386511230e+02, 3.3801528320e+03, 5.5477290039e+03, 1.9031191406e+03, -1.3520118713e+02,
};

static const float qr2[6] = {
    -1.7838172539e-07, -1.0251704603e-01, -2.7522056103e+00, -1.9663616180e+01, -4.2325313568e+01, -2.1371921539e+01,
};
static const float qs2[6] = {
    2.9533363342e+01, 2.5298155212e+02, 7.5750280762e+02, 7.3939318848e+02, 1.5594900513e+02, -4.9594988823e+00,
};

static float QOne(float x) {
    const float *p, *q;
    float s, r, z;
    std::int32_t ix;
    ix = std::bit_cast<std::int32_t>(x);
    ix &= 0x7fffffff;
    if (ix >= 0x40200000) {
        p = qr8;
        q = qs8;
    } else if (ix >= 0x40f71c58) {
        p = qr5;
        q = qs5;
    } else if (ix >= 0x4036db68) {
        p = qr3;
        q = qs3;
    } else {
        p = qr2;
        q = qs2;
    }
    z = one / (x * x);
    r = p[0] + z * (p[1] + z * (p[2] + z * (p[3] + z * (p[4] + z * p[5]))));
    s = one + z * (q[0] + z * (q[1] + z * (q[2] + z * (q[3] + z * (q[4] + z * q[5])))));
    return ((float).375 + r / s) / x;
}

} // namespace FloatBessel
} // namespace

float APS5_VABI j1f_nid_postfix(float x) {
    using namespace FloatBessel;
    float z, s, c, ss, cc, r, u, v, y;
    std::int32_t hx, ix;

    hx = std::bit_cast<std::int32_t>(x);
    ix = hx & 0x7fffffff;
    if (ix >= 0x7f800000)
        return one / x;
    y = std::fabs(x);
    if (ix >= 0x40000000) {
        s = sinf_nid_postfix(y);
        c = cosf_nid_postfix(y);
        ss = -s - c;
        cc = s - c;
        if (ix < 0x7f000000) {
            z = cosf_nid_postfix(y + y);
            if ((s * c) > zero)
                cc = z / ss;
            else
                ss = z / cc;
        }

        if (ix > 0x80000000)
            z = (invsqrtpi * cc) / sqrtf_nid_postfix(y);
        else {
            u = POne(y);
            v = QOne(y);
            z = invsqrtpi * (u * cc - v * ss) / sqrtf_nid_postfix(y);
        }
        if (hx < 0)
            return -z;
        else
            return z;
    }
    if (ix < 0x32000000) {
        if (huge + x > one)
            return (float)0.5 * x;
    }
    z = x * x;
    r = z * (r00 + z * (r01 + z * (r02 + z * r03)));
    s = one + z * (s01 + z * (s02 + z * (s03 + z * (s04 + z * s05))));
    r *= x;
    return (x * (float)0.5 + r / s);
}

float APS5_VABI y1f_nid_postfix(float x) {
    using namespace FloatBessel;
    float z, s, c, ss, cc, u, v;
    std::int32_t hx, ix;

    hx = std::bit_cast<std::int32_t>(x);
    ix = 0x7fffffff & hx;

    if (ix >= 0x7f800000)
        return one / (x + x * x);
    if (ix == 0)
        return -std::numeric_limits<float>::infinity();
    if (hx < 0)
        return std::numeric_limits<float>::quiet_NaN();

    if (ix >= 0x40000000) {
        s = sinf_nid_postfix(x);
        c = cosf_nid_postfix(x);
        ss = -s - c;
        cc = s - c;
        if (ix < 0x7f000000) {
            z = cosf_nid_postfix(x + x);
            if ((s * c) > zero)
                cc = z / ss;
            else
                ss = z / cc;
        }

        if (ix > 0x48000000)
            z = (invsqrtpi * ss) / sqrtf_nid_postfix(x);
        else {
            u = POne(x);
            v = QOne(x);
            z = invsqrtpi * (u * ss + v * cc) / sqrtf_nid_postfix(x);
        }
        return z;
    }
    if (ix <= 0x24800000) {
        return (-tpi / x);
    }
    z = x * x;
    u = U0[0] + z * (U0[1] + z * (U0[2] + z * (U0[3] + z * U0[4])));
    v = one + z * (V0[0] + z * (V0[1] + z * (V0[2] + z * (V0[3] + z * V0[4]))));
    return (x * (u / v) + tpi * (j1f_nid_postfix(x) * logf_nid_postfix(x) - one / x));
}
