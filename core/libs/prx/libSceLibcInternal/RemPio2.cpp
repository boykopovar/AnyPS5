/* Derived from FreeBSD lib/msun/src/k_rem_pio2.c (release/11.4.0) */
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
#include "prx/libc/include/general/RemPio2Table.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using RemPio2Table::Ipio2;

constexpr std::array<double, 8> PIo2 = {
    1.57079625129699707031e+00, 7.54978941586159635335e-08, 5.39030252995776476554e-15, 3.28200341580791294123e-22,
    1.27065575308067607349e-29, 1.22933308981111328932e-36, 2.73370053816464559624e-44, 2.16741683877804819444e-51,
};

constexpr std::array<std::int32_t, 4> InitJk = {3, 4, 4, 6};

constexpr std::int32_t Terms = 20;

template <typename T>
T& At(std::array<T, Terms>& values, std::int32_t index) {
    if (index < 0 || index >= Terms) throw std::out_of_range("__kernel_rem_pio2: term " + std::to_string(index) + " is outside the 20-term work arrays");
    return values[index];
}

double Ipio2At(std::int32_t index) {
    if (index < 0 || index >= static_cast<std::int32_t>(Ipio2.size())) throw std::out_of_range("__kernel_rem_pio2: e0 needs 2/pi digit chunk " + std::to_string(index) + ", past the 690-chunk table");
    return static_cast<double>(Ipio2[index]);
}

std::int32_t ToInt32(double value) {
    if (!(value > -2147483649.0 && value < 2147483648.0)) throw std::domain_error("__kernel_rem_pio2: a partial product does not fit in 32 bits");
    return static_cast<std::int32_t>(value);
}

}

extern "C" {

int APS5_VABI __kernel_rem_pio2_nid_postfix(double* x, double* y, int e0, int nx, int prec) {
    if (x == nullptr || y == nullptr) throw std::invalid_argument("__kernel_rem_pio2: null x or y");
    if (prec < 0 || prec > 3) throw std::invalid_argument("__kernel_rem_pio2: precision " + std::to_string(prec) + " is not 0 to 3");
    if (nx < 1 || nx > Terms - InitJk[prec]) throw std::invalid_argument("__kernel_rem_pio2: " + std::to_string(nx) + " input chunks do not fit the 20-term work arrays");
    if (e0 < std::numeric_limits<std::int32_t>::min() + 24 * (Terms + 3)) throw std::invalid_argument("__kernel_rem_pio2: exponent " + std::to_string(e0) + " overflows the reduction's exponent arithmetic");
    const double zero = 0.0, one = 1.0, two24 = 1.67772160000000000000e+07, twon24 = 5.96046447753906250000e-08;
    std::int32_t jz, jx, jv, jp, jk, carry, n, i, j, k, m, q0, ih;
    std::array<std::int32_t, Terms> iq{};
    std::array<double, Terms> f{}, fq{}, q{};
    double z, fw;
    jk = InitJk[prec];
    jp = jk;
    jx = nx - 1;
    jv = (e0 - 3) / 24;
    if (jv < 0) jv = 0;
    if (jv + jk >= static_cast<std::int32_t>(Ipio2.size())) throw std::out_of_range("__kernel_rem_pio2: e0 " + std::to_string(e0) + " needs 2/pi digits past the 690-chunk table");
    q0 = e0 - 24 * (jv + 1);
    j = jv - jx;
    m = jx + jk;
    for (i = 0; i <= m; i++, j++) At(f, i) = j < 0 ? zero : Ipio2At(j);
    for (i = 0; i <= jk; i++) {
        for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * At(f, jx + i - j);
        At(q, i) = fw;
    }
    jz = jk;
recompute:
    for (i = 0, j = jz, z = At(q, jz); j > 0; i++, j--) {
        fw = static_cast<double>(ToInt32(twon24 * z));
        At(iq, i) = ToInt32(z - two24 * fw);
        z = At(q, j - 1) + fw;
    }
    z = std::scalbn(z, q0);
    z -= 8.0 * std::floor(z * 0.125);
    n = ToInt32(z);
    z -= static_cast<double>(n);
    ih = 0;
    if (q0 > 0) {
        i = At(iq, jz - 1) >> (24 - q0);
        n += i;
        At(iq, jz - 1) -= i << (24 - q0);
        ih = At(iq, jz - 1) >> (23 - q0);
    } else if (q0 == 0) {
        ih = At(iq, jz - 1) >> 23;
    } else if (z >= 0.5) {
        ih = 2;
    }
    if (ih > 0) {
        n += 1;
        carry = 0;
        for (i = 0; i < jz; i++) {
            j = At(iq, i);
            if (carry == 0) {
                if (j != 0) {
                    carry = 1;
                    At(iq, i) = 0x1000000 - j;
                }
            } else {
                At(iq, i) = 0xffffff - j;
            }
        }
        if (q0 > 0) {
            switch (q0) {
                case 1: At(iq, jz - 1) &= 0x7fffff; break;
                case 2: At(iq, jz - 1) &= 0x3fffff; break;
            }
        }
        if (ih == 2) {
            z = one - z;
            if (carry != 0) z -= std::scalbn(one, q0);
        }
    }
    if (z == zero) {
        j = 0;
        for (i = jz - 1; i >= jk; i--) j |= At(iq, i);
        if (j == 0) {
            for (k = 1; At(iq, jk - k) == 0; k++) {
            }
            for (i = jz + 1; i <= jz + k; i++) {
                At(f, jx + i) = Ipio2At(jv + i);
                for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * At(f, jx + i - j);
                At(q, i) = fw;
            }
            jz += k;
            goto recompute;
        }
    }
    if (z == 0.0) {
        jz -= 1;
        q0 -= 24;
        while (At(iq, jz) == 0) {
            jz--;
            q0 -= 24;
        }
    } else {
        z = std::scalbn(z, -q0);
        if (z >= two24) {
            fw = static_cast<double>(ToInt32(twon24 * z));
            At(iq, jz) = ToInt32(z - two24 * fw);
            jz += 1;
            q0 += 24;
            At(iq, jz) = ToInt32(fw);
        } else {
            At(iq, jz) = ToInt32(z);
        }
    }
    fw = std::scalbn(one, q0);
    for (i = jz; i >= 0; i--) {
        At(q, i) = fw * static_cast<double>(At(iq, i));
        fw *= twon24;
    }
    for (i = jz; i >= 0; i--) {
        for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) fw += PIo2[k] * At(q, i + k);
        At(fq, jz - i) = fw;
    }
    switch (prec) {
        case 0:
            fw = 0.0;
            for (i = jz; i >= 0; i--) fw += At(fq, i);
            y[0] = ih == 0 ? fw : -fw;
            break;
        case 1:
        case 2:
            fw = 0.0;
            for (i = jz; i >= 0; i--) fw += At(fq, i);
            y[0] = ih == 0 ? fw : -fw;
            fw = At(fq, 0) - fw;
            for (i = 1; i <= jz; i++) fw += At(fq, i);
            y[1] = ih == 0 ? fw : -fw;
            break;
        case 3:
            if (jz < 1) throw std::domain_error("__kernel_rem_pio2: a one-term quad result would read an unset term");
            for (i = jz; i > 0; i--) {
                fw = At(fq, i - 1) + At(fq, i);
                At(fq, i) += At(fq, i - 1) - fw;
                At(fq, i - 1) = fw;
            }
            for (i = jz; i > 1; i--) {
                fw = At(fq, i - 1) + At(fq, i);
                At(fq, i) += At(fq, i - 1) - fw;
                At(fq, i - 1) = fw;
            }
            for (fw = 0.0, i = jz; i >= 2; i--) fw += At(fq, i);
            if (ih == 0) {
                y[0] = At(fq, 0);
                y[1] = At(fq, 1);
                y[2] = fw;
            } else {
                y[0] = -At(fq, 0);
                y[1] = -At(fq, 1);
                y[2] = -fw;
            }
            break;
    }
    return n & 7;
}

}
