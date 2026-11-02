/* Derived from FreeBSD lib/msun/src/k_cos.c, lib/msun/src/k_sin.c, lib/msun/src/k_cosf.c and
 * lib/msun/src/k_sinf.c (release/11.4.0) */
/* k_cosf.c and k_sinf.c:
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Debugged and optimized by Bruce D. Evans. */
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
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

double APS5_VABI __kernel_cos_nid_postfix(double x, double y) {
    const double one = 1.0, C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03, C3 = 2.48015872894767294178e-05,
                 C4 = -2.75573143513906633035e-07, C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
    const double z = x * x;
    double w = z * z;
    const double r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    const double hz = 0.5 * z;
    w = one - hz;
    return w + (((one - w) - hz) + (z * r - x * y));
}

double APS5_VABI __kernel_sin_nid_postfix(double x, double y, int iy) {
    const double half = 5.00000000000000000000e-01, S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
                 S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06, S5 = -2.50507602534068634195e-08,
                 S6 = 1.58969099521155010221e-10;
    const double z = x * x, w = z * z;
    const double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    const double v = z * x;
    if (iy == 0) return x + v * (S1 + z * r);
    return x - ((z * (half * y - v * r) - y) - v * S1);
}

float APS5_VABI __kernel_cosdf_nid_postfix(double x) {
    const double one = 1.0, C0 = -0x1ffffffd0c5e81.0p-54, C1 = 0x155553e1053a42.0p-57, C2 = -0x16c087e80f1e27.0p-62,
                 C3 = 0x199342e0ee5069.0p-68;
    const double z = x * x, w = z * z, r = C2 + z * C3;
    return static_cast<float>(((one + z * C0) + w * C1) + (w * z) * r);
}

float APS5_VABI __kernel_sindf_nid_postfix(double x) {
    const double S1 = -0x15555554cbac77.0p-55, S2 = 0x111110896efbb2.0p-59, S3 = -0x1a00f9e2cae774.0p-65,
                 S4 = 0x16cd878c3b46a7.0p-71;
    const double z = x * x, w = z * z, r = S3 + z * S4, s = z * x;
    return static_cast<float>((x + s * (S1 + z * S2)) + s * w * r);
}

}
