/* Derived from Microsoft STL stl/src/xxxprec.h and stl/src/xprec.cpp (219514876ea86491de191ceaa88632616bbc0f19) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int Bits = std::numeric_limits<double>::digits;
constexpr int MaxExponent = std::numeric_limits<double>::max_exponent;
constexpr std::int64_t BigExponent = 2 * MaxExponent;
constexpr short WordBits = Bits / 2;
constexpr int DelayLine = 4;
constexpr short InfiniteCode = 1;
constexpr short NotANumberCode = 2;
constexpr int InvalidException = 0x01;

void RequireCount(int count, const char* function, const char* what) {
    if (count < 0) throw std::invalid_argument(std::string(function) + ": negative " + what + " count " + std::to_string(count));
}

void CopyUp(double* p, int j, int n) {
    int m = j;
    while (++m < n && (p[m - 1] = p[m]) != 0.0) {
    }
    p[n - 1] = 0.0;
}

double NotANumber() {
    return std::numeric_limits<double>::quiet_NaN();
}

}

extern "C" {

double APS5_VABI _Xp_getw_nid_postfix(const double* p, int n) {
    RequireCount(n, "_Xp_getw", "word");
    if (n == 0) return 0.0;
    if (n == 1 || p[0] == 0.0 || p[1] == 0.0) return p[0];
    if (n == 2 || p[2] == 0.0) return p[0] + p[1];
    const double p01 = p[0] + p[1];
    double p2 = p[2];
    if (4 <= n) p2 += p[3];
    if (p01 - p[0] == p[1]) return p01 + p2;
    return p[0] + (p[1] + p2);
}

double* APS5_VABI _Xp_setw_nid_postfix(double* p, int n, double x) {
    double x0 = x;
    short errx = 0;
    short xexp = 0;
    if (n > 0) {
        if (n == 1 || (errx = _Dunscale_nid_postfix(&xexp, &x0)) == 0) {
            p[0] = x0;
        } else if (0 < errx) {
            p[0] = x0;
            p[1] = 0.0;
        } else {
            _Dint_nid_postfix(&x0, WordBits);
            _Dscale_nid_postfix(&x0, xexp);
            p[0] = x0;
            p[1] = x - x0;
            if ((Bits & 1) != 0 && 2 < n && p[1] != 0.0) {
                x = p[1];
                _Dunscale_nid_postfix(&xexp, &p[1]);
                _Dint_nid_postfix(&p[1], WordBits);
                _Dscale_nid_postfix(&p[1], xexp);
                p[2] = x - p[1];
                if (3 < n && p[2] != 0.0) p[3] = 0.0;
            } else if (2 < n) {
                p[2] = 0.0;
            }
        }
    }
    return p;
}

double* APS5_VABI _Xp_addh_nid_postfix(double* p, int n, double x0) {
    double xscaled = x0;
    short errx = 0;
    short xexp = 0;
    if (n == 0) return p;
    if (0 < (errx = _Dunscale_nid_postfix(&xexp, &xscaled))) {
        if (errx == NotANumberCode || (errx = _Dtest_nid_postfix(&p[0])) <= 0) {
            p[0] = x0;
        } else if (errx != NotANumberCode && std::signbit(x0) != std::signbit(p[0])) {
            _Feraise_nid_postfix(InvalidException);
            p[0] = NotANumber();
            if (1 < n) p[1] = 0.0;
        }
        return p;
    }
    if (errx == 0) return p;
    std::int64_t prevexp = BigExponent;
    int k = 0;
    while (k < n) {
        double yscaled = p[k];
        const int mybits = WordBits;
        short yexp = 0;
        if (0 < (errx = _Dunscale_nid_postfix(&yexp, &yscaled))) break;
        if (errx == 0) {
            p[k] = x0;
            if (k + 1 < n) p[k + 1] = 0.0;
            break;
        }
        const std::int64_t diff = static_cast<std::int64_t>(yexp) - xexp;
        if (diff <= -mybits && x0 != 0.0) {
            int j = k;
            do {
                ++j;
            } while (j < n && p[j] != 0.0);
            if (j < n - 1) {
                ++j;
            } else if (j == n) {
                --j;
            }
            for (; k < j; --j) p[j] = p[j - 1];
            p[k] = x0;
            x0 = 0.0;
        } else if (mybits <= diff && x0 != 0.0) {
            prevexp = yexp;
            ++k;
        } else {
            if ((p[k] += x0) == 0.0) {
                CopyUp(p, k, n);
                if (p[k] == 0.0) break;
            }
            x0 = p[k];
            _Dunscale_nid_postfix(&xexp, &x0);
            if (prevexp - mybits < xexp) {
                _Dint_nid_postfix(&x0, static_cast<short>(xexp - (prevexp - mybits)));
                _Dscale_nid_postfix(&x0, xexp);
                if ((p[k] -= x0) == 0.0) CopyUp(p, k, n);
                if (--k == 0) {
                    prevexp = BigExponent;
                } else {
                    xscaled = p[k - 1];
                    _Dunscale_nid_postfix(&yexp, &xscaled);
                    prevexp = yexp;
                }
            } else if (k + 1 == n) {
                break;
            } else {
                x0 = p[k];
                _Dunscale_nid_postfix(&yexp, &p[k]);
                _Dint_nid_postfix(&p[k], WordBits);
                _Dscale_nid_postfix(&p[k], yexp);
                x0 -= p[k];
                prevexp = yexp;
                xscaled = x0 != 0.0 ? x0 : p[k];
                _Dunscale_nid_postfix(&xexp, &xscaled);
                ++k;
            }
        }
    }
    return p;
}

double* APS5_VABI _Xp_mulh_nid_postfix(double* p, int n, double x0) {
    double buf[DelayLine];
    if (0 < n) {
        buf[0] = p[0] * x0;
        const short errx = _Dtest_nid_postfix(&buf[0]);
        if (0 <= errx) {
            if (errx == NotANumberCode) _Feraise_nid_postfix(InvalidException);
            p[0] = buf[0];
            if (0 < errx && 1 < n) p[1] = 0.0;
            return p;
        }
        p[0] = 0.0;
    }
    int j = 1;
    for (int k = 0; k < n; ++k, --j) {
        for (; j < DelayLine; ++j) {
            if (k + j < n && p[k + j] != 0.0) {
                buf[j] = p[k + j] * x0;
                p[k + j] = 0.0;
            } else {
                buf[j] = 0.0;
                j = 2 * DelayLine;
                break;
            }
        }
        if (buf[0] == 0.0) break;
        int i = 0;
        double y0 = buf[0];
        short xexp = 0;
        _Dunscale_nid_postfix(&xexp, &y0);
        _Dint_nid_postfix(&y0, WordBits);
        _Dscale_nid_postfix(&y0, xexp);
        _Xp_addh_nid_postfix(p, n, y0);
        _Xp_addh_nid_postfix(p, n, buf[0] - y0);
        while (++i < j) {
            if ((buf[i - 1] = buf[i]) == 0.0) break;
        }
    }
    return p;
}

double* APS5_VABI _Xp_setn_nid_postfix(double* p, int n, std::int64_t x) {
    return _Xp_setw_nid_postfix(p, n, static_cast<double>(x));
}

double* APS5_VABI _Xp_movx_nid_postfix(double* p, int n, const double* q) {
    RequireCount(n, "_Xp_movx", "word");
    if (n != 0) std::memcpy(p, q, static_cast<std::size_t>(n) * sizeof(double));
    return p;
}

double* APS5_VABI _Xp_addx_nid_postfix(double* p, int n, const double* q, int m) {
    for (int k = 0; k < m && q[k] != 0.0; ++k) _Xp_addh_nid_postfix(p, n, q[k]);
    return p;
}

double* APS5_VABI _Xp_subx_nid_postfix(double* p, int n, const double* q, int m) {
    for (int k = 0; k < m && q[k] != 0.0; ++k) _Xp_addh_nid_postfix(p, n, -q[k]);
    return p;
}

double* APS5_VABI _Xp_ldexpx_nid_postfix(double* p, int n, int m) {
    for (int k = 0; k < n; ++k) {
        p[k] = ldexp_nid_postfix(p[k], m);
        if (p[k] == 0.0) break;
    }
    return p;
}

double* APS5_VABI _Xp_mulx_nid_postfix(double* p, int n, const double* q, int m, double* ptemp2) {
    RequireCount(n, "_Xp_mulx", "word");
    RequireCount(m, "_Xp_mulx", "multiplier word");
    if (n == 0 || m == 0) return p;
    if (m == 1 || q[0] == 0.0 || q[1] == 0.0) return _Xp_mulh_nid_postfix(p, n, q[0]);
    double* px = ptemp2;
    double* pac = ptemp2 + n;
    _Xp_movx_nid_postfix(px, n, p);
    _Xp_mulh_nid_postfix(p, n, q[0]);
    for (int j = 1; j < m && q[j] != 0.0; ++j) {
        _Xp_movx_nid_postfix(pac, n, px);
        _Xp_mulh_nid_postfix(pac, n, q[j]);
        _Xp_addx_nid_postfix(p, n, pac, n);
    }
    return p;
}

double* APS5_VABI _Xp_invx_nid_postfix(double* p, int n, double* ptemp4) {
    RequireCount(n, "_Xp_invx", "word");
    if (n == 0) return p;
    const short errx = _Dtest_nid_postfix(&p[0]);
    if (0 <= errx) {
        if (errx == InfiniteCode) {
            p[0] = 0.0;
        } else if (errx == 0) {
            p[0] = std::numeric_limits<double>::infinity();
        }
        return p;
    }
    double* pac = ptemp4;
    double* py = ptemp4 + n;
    double* ptemp2 = py + n;
    double x0 = p[0];
    _Xp_movx_nid_postfix(py, n, p);
    _Xp_mulh_nid_postfix(py, n, -1.0);
    if (1 < n) x0 += p[1];
    _Xp_setw_nid_postfix(p, n, 1.0 / x0);
    for (int k = 1; k < n; k <<= 1) {
        _Xp_movx_nid_postfix(pac, n, p);
        _Xp_mulx_nid_postfix(pac, n, py, n, ptemp2);
        _Xp_addh_nid_postfix(pac, n, 1.0);
        _Xp_mulx_nid_postfix(pac, n, p, n, ptemp2);
        _Xp_addx_nid_postfix(p, n, pac, n);
    }
    return p;
}

double* APS5_VABI _Xp_sqrtx_nid_postfix(double* p, int n, double* ptemp4) {
    RequireCount(n, "_Xp_sqrtx", "word");
    if (n == 0) return p;
    if (0 <= _Dtest_nid_postfix(&p[0]) || p[0] < 0.0) {
        if (p[0] < 0.0) {
            _Feraise_nid_postfix(InvalidException);
            p[0] = NotANumber();
        }
        return p;
    }
    double* pac = ptemp4;
    double* py = ptemp4 + n;
    double* ptemp2 = py + n;
    double x0 = p[0];
    if (1 < n) x0 += p[1];
    _Xp_setw_nid_postfix(py, n, 1.0 / sqrt_nid_postfix(x0));
    for (int k = 2; k < n; k <<= 1) {
        _Xp_movx_nid_postfix(pac, n, py);
        _Xp_mulh_nid_postfix(pac, n, -0.5);
        _Xp_mulx_nid_postfix(pac, n, p, n, ptemp2);
        _Xp_mulx_nid_postfix(pac, n, py, n, ptemp2);
        _Xp_addh_nid_postfix(pac, n, 1.5);
        _Xp_mulx_nid_postfix(py, n, pac, n, ptemp2);
    }
    _Xp_mulx_nid_postfix(p, n, py, n, ptemp2);
    return p;
}

}
