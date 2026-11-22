/* Derived from Microsoft STL stl/src/xxxprec.h and stl/src/xprec.cpp (219514876ea86491de191ceaa88632616bbc0f19) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/GuestWords.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Bits = std::numeric_limits<double>::digits;
constexpr int MaxExponent = std::numeric_limits<double>::max_exponent;
constexpr std::int64_t BigExponent = 2 * MaxExponent;
constexpr short WordBits = Bits / 2;
constexpr int DelayLine = 4;
constexpr std::size_t ReadWords = 4;
constexpr std::size_t SetWords = 4;
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

using GuestWords = ExtendedPrecisionWords::GuestWords<double>;
using ExtendedPrecisionWords::ReadWord;

std::size_t Words(int n) {
    return n > 0 ? static_cast<std::size_t>(n) : 0;
}

std::size_t AddedWords(int n) {
    return n < 0 ? 1 : Words(n);
}

void RequireUnchanged(bool unchanged, const char* function) {
    if (!unchanged) throw std::invalid_argument(std::string(function) + ": words changed while being read");
}

bool SingleFactor(const double* q, int m) {
    return m == 1 || q[0] == 0.0 || q[1] == 0.0;
}

bool SpecialInverse(double leading) {
    return 0 <= _Dtest_nid_postfix(&leading);
}

bool SpecialRoot(double leading) {
    return 0 <= _Dtest_nid_postfix(&leading) || leading < 0.0;
}

double Getw(const double* p, int n) {
    if (n == 0) return 0.0;
    if (n == 1 || p[0] == 0.0 || p[1] == 0.0) return p[0];
    if (n == 2 || p[2] == 0.0) return p[0] + p[1];
    const double p01 = p[0] + p[1];
    double p2 = p[2];
    if (4 <= n) p2 += p[3];
    if (p01 - p[0] == p[1]) return p01 + p2;
    return p[0] + (p[1] + p2);
}

double* Setw(double* p, int n, double x) {
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

double* Addh(double* p, int n, double x0) {
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

double* Mulh(double* p, int n, double x0) {
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
        Addh(p, n, y0);
        Addh(p, n, buf[0] - y0);
        while (++i < j) {
            if ((buf[i - 1] = buf[i]) == 0.0) break;
        }
    }
    return p;
}

double* Movx(double* p, int n, const double* q) {
    if (n != 0) std::memcpy(p, q, static_cast<std::size_t>(n) * sizeof(double));
    return p;
}

double* Addx(double* p, int n, const double* q, int m) {
    for (int k = 0; k < m && q[k] != 0.0; ++k) Addh(p, n, q[k]);
    return p;
}

double* Subx(double* p, int n, const double* q, int m) {
    for (int k = 0; k < m && q[k] != 0.0; ++k) Addh(p, n, -q[k]);
    return p;
}

double* Ldexpx(double* p, int n, int m) {
    for (int k = 0; k < n; ++k) {
        p[k] = ldexp_nid_postfix(p[k], m);
        if (p[k] == 0.0) break;
    }
    return p;
}

double* Mulx(double* p, int n, const double* q, int m, double* ptemp2) {
    if (n == 0 || m == 0) return p;
    if (m == 1 || q[0] == 0.0 || q[1] == 0.0) return Mulh(p, n, q[0]);
    double* px = ptemp2;
    double* pac = ptemp2 + n;
    Movx(px, n, p);
    Mulh(p, n, q[0]);
    for (int j = 1; j < m && q[j] != 0.0; ++j) {
        Movx(pac, n, px);
        Mulh(pac, n, q[j]);
        Addx(p, n, pac, n);
    }
    return p;
}

double* Invx(double* p, int n, double* ptemp4) {
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
    Movx(py, n, p);
    Mulh(py, n, -1.0);
    if (1 < n) x0 += p[1];
    Setw(p, n, 1.0 / x0);
    for (int k = 1; k < n; k <<= 1) {
        Movx(pac, n, p);
        Mulx(pac, n, py, n, ptemp2);
        Addh(pac, n, 1.0);
        Mulx(pac, n, p, n, ptemp2);
        Addx(p, n, pac, n);
    }
    return p;
}

double* Sqrtx(double* p, int n, double* ptemp4) {
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
    Setw(py, n, 1.0 / sqrt_nid_postfix(x0));
    for (int k = 2; k < n; k <<= 1) {
        Movx(pac, n, py);
        Mulh(pac, n, -0.5);
        Mulx(pac, n, p, n, ptemp2);
        Mulx(pac, n, py, n, ptemp2);
        Addh(pac, n, 1.5);
        Mulx(py, n, pac, n, ptemp2);
    }
    Mulx(p, n, py, n, ptemp2);
    return p;
}

}

extern "C" {

double APS5_VABI _Xp_getw_nid_postfix(const double* p, int n) {
    RequireCount(n, "_Xp_getw", "word");
    GuestWords words("_Xp_getw");
    const std::size_t value = words.Add(p, std::min(Words(n), ReadWords), false);
    words.Load();
    return Getw(words.Host(value), n);
}

double* APS5_VABI _Xp_setw_nid_postfix(double* p, int n, double x) {
    GuestWords words("_Xp_setw");
    const std::size_t value = words.Add(p, std::min(Words(n), SetWords), true);
    words.Load();
    Setw(words.Host(value), n, x);
    words.Store();
    return p;
}

double* APS5_VABI _Xp_setn_nid_postfix(double* p, int n, std::int64_t x) {
    return _Xp_setw_nid_postfix(p, n, static_cast<double>(x));
}

double* APS5_VABI _Xp_addh_nid_postfix(double* p, int n, double x0) {
    GuestWords words("_Xp_addh");
    const std::size_t value = words.Add(p, AddedWords(n), true);
    words.Load();
    Addh(words.Host(value), n, x0);
    words.Store();
    return p;
}

double* APS5_VABI _Xp_mulh_nid_postfix(double* p, int n, double x0) {
    GuestWords words("_Xp_mulh");
    const std::size_t value = words.Add(p, Words(n), true);
    words.Load();
    Mulh(words.Host(value), n, x0);
    words.Store();
    return p;
}

double* APS5_VABI _Xp_movx_nid_postfix(double* p, int n, const double* q) {
    RequireCount(n, "_Xp_movx", "word");
    GuestWords words("_Xp_movx");
    const std::size_t value = words.Add(p, Words(n), true);
    const std::size_t source = words.Add(q, Words(n), false);
    words.Load();
    Movx(words.Host(value), n, words.Host(source));
    words.Store();
    return p;
}

double* APS5_VABI _Xp_addx_nid_postfix(double* p, int n, const double* q, int m) {
    GuestWords words("_Xp_addx");
    const std::size_t value = words.Add(p, AddedWords(n), true);
    const std::size_t addend = words.Add(q, Words(m), false);
    words.Load();
    Addx(words.Host(value), n, words.Host(addend), m);
    words.Store();
    return p;
}

double* APS5_VABI _Xp_subx_nid_postfix(double* p, int n, const double* q, int m) {
    GuestWords words("_Xp_subx");
    const std::size_t value = words.Add(p, AddedWords(n), true);
    const std::size_t subtrahend = words.Add(q, Words(m), false);
    words.Load();
    Subx(words.Host(value), n, words.Host(subtrahend), m);
    words.Store();
    return p;
}

double* APS5_VABI _Xp_ldexpx_nid_postfix(double* p, int n, int m) {
    GuestWords words("_Xp_ldexpx");
    const std::size_t value = words.Add(p, Words(n), true);
    words.Load();
    Ldexpx(words.Host(value), n, m);
    words.Store();
    return p;
}

double* APS5_VABI _Xp_mulx_nid_postfix(double* p, int n, const double* q, int m, double* ptemp2) {
    RequireCount(n, "_Xp_mulx", "word");
    RequireCount(m, "_Xp_mulx", "multiplier word");
    if (n == 0 || m == 0) return p;
    const double head[] = {ReadWord(q, 0, "_Xp_mulx"), m == 1 ? 0.0 : ReadWord(q, 1, "_Xp_mulx")};
    const bool single = SingleFactor(head, m);
    GuestWords words("_Xp_mulx");
    const std::size_t value = words.Add(p, Words(n), true);
    const std::size_t factor = words.Add(q, Words(m), false);
    const std::size_t scratch = words.Add(ptemp2, single ? 0 : 2 * Words(n), true);
    words.Load();
    RequireUnchanged(!single || SingleFactor(words.Host(factor), m), "_Xp_mulx");
    Mulx(words.Host(value), n, words.Host(factor), m, words.Host(scratch));
    words.Store();
    return p;
}

double* APS5_VABI _Xp_invx_nid_postfix(double* p, int n, double* ptemp4) {
    RequireCount(n, "_Xp_invx", "word");
    if (n == 0) return p;
    const bool special = SpecialInverse(ReadWord(p, 0, "_Xp_invx"));
    GuestWords words("_Xp_invx");
    const std::size_t value = words.Add(p, Words(n), true);
    const std::size_t scratch = words.Add(ptemp4, special ? 0 : 4 * Words(n), true);
    words.Load();
    RequireUnchanged(!special || SpecialInverse(words.Host(value)[0]), "_Xp_invx");
    Invx(words.Host(value), n, words.Host(scratch));
    words.Store();
    return p;
}

double* APS5_VABI _Xp_sqrtx_nid_postfix(double* p, int n, double* ptemp4) {
    RequireCount(n, "_Xp_sqrtx", "word");
    if (n == 0) return p;
    const bool special = SpecialRoot(ReadWord(p, 0, "_Xp_sqrtx"));
    GuestWords words("_Xp_sqrtx");
    const std::size_t value = words.Add(p, Words(n), true);
    const std::size_t scratch = words.Add(ptemp4, special ? 0 : 4 * Words(n), true);
    words.Load();
    RequireUnchanged(!special || SpecialRoot(words.Host(value)[0]), "_Xp_sqrtx");
    Sqrtx(words.Host(value), n, words.Host(scratch));
    words.Store();
    return p;
}

}
