#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

constexpr int RangeError = 34;
constexpr short Finite = -1;
constexpr short ZeroCode = 0;
constexpr short InfinityCode = 1;
constexpr short NanCode = 2;
constexpr int DelayLine = 4;
constexpr int AccumulatorSize = 3;

template <typename T>
constexpr int Digits = std::numeric_limits<T>::digits;

template <typename T>
constexpr int WordBits = Digits<T> / 2;

template <typename T>
constexpr long long BigExponent = 2LL * std::numeric_limits<T>::max_exponent;

template <typename T>
short Classify(T value) {
    switch (std::fpclassify(value)) {
    case FP_ZERO:
        return ZeroCode;
    case FP_INFINITE:
        return InfinityCode;
    case FP_NAN:
        return NanCode;
    default:
        return Finite;
    }
}

template <typename T>
short Unscale(T& value, short& exponent) {
    const short code = Classify(value);
    if (code != Finite) {
        exponent = 0;
        return code;
    }
    int binary = 0;
    value = std::frexp(value, &binary);
    exponent = static_cast<short>(binary);
    return Finite;
}

template <typename T>
void KeepBits(T& value, long long bits) {
    if (bits >= Digits<T>) return;
    if (bits <= 0) {
        value = std::copysign(T(0), value);
        return;
    }
    value = std::ldexp(std::trunc(std::ldexp(value, static_cast<int>(bits))), -static_cast<int>(bits));
}

template <typename T>
void SplitHigh(T& value, short exponent) {
    KeepBits(value, WordBits<T>);
    value = std::ldexp(value, exponent);
}

template <typename T>
void CopyUp(T* p, int from, int n) {
    for (int m = from; ++m < n && (p[m - 1] = p[m]) != T(0);) {
    }
    p[n - 1] = T(0);
}

template <typename T>
T Getw(const T* p, int n) {
    if (n == 0) return T(0);
    if (n == 1 || p[0] == T(0) || p[1] == T(0)) return p[0];
    if (n == 2 || p[2] == T(0)) return p[0] + p[1];
    const T head = p[0] + p[1];
    T tail = p[2];
    if (n >= 4) tail += p[3];
    if (head - p[0] == p[1]) return head + tail;
    return p[0] + (p[1] + tail);
}

template <typename T>
T* Setw(T* p, int n, T x) {
    if (n <= 0) return p;
    T high = x;
    short exponent = 0;
    short code = Finite;
    if (n == 1 || (code = Unscale(high, exponent)) == ZeroCode) {
        p[0] = high;
        return p;
    }
    if (code > 0) {
        p[0] = high;
        p[1] = T(0);
        return p;
    }
    SplitHigh(high, exponent);
    p[0] = high;
    p[1] = x - high;
    if constexpr ((Digits<T> & 1) != 0) {
        if (n > 2 && p[1] != T(0)) {
            const T rest = p[1];
            Unscale(p[1], exponent);
            SplitHigh(p[1], exponent);
            p[2] = rest - p[1];
            if (n > 3 && p[2] != T(0)) p[3] = T(0);
            return p;
        }
    }
    if (n > 2) p[2] = T(0);
    return p;
}

template <typename T>
T* Addh(T* p, int n, T x) {
    if (n <= 0) return p;
    T scaled = x;
    short xExponent = 0;
    short code = Unscale(scaled, xExponent);
    if (code > 0) {
        if (code == NanCode || (code = Classify(p[0])) <= 0) {
            p[0] = x;
        } else if (code == NanCode || std::signbit(x) == std::signbit(p[0])) {
        } else {
            std::feraiseexcept(FE_INVALID);
            p[0] = std::numeric_limits<T>::quiet_NaN();
            if (n > 1) p[1] = T(0);
        }
        return p;
    }
    if (code == ZeroCode) return p;
    long long previous = BigExponent<T>;
    constexpr int Bits = WordBits<T>;
    for (int k = 0; k < n;) {
        T word = p[k];
        short wordExponent = 0;
        code = Unscale(word, wordExponent);
        if (code > 0) break;
        if (code == ZeroCode) {
            p[k] = x;
            if (k + 1 < n) p[k + 1] = T(0);
            break;
        }
        const long long difference = static_cast<long long>(wordExponent) - xExponent;
        if (difference <= -Bits && x != T(0)) {
            int j = k;
            while (++j < n && p[j] != T(0)) {
            }
            if (j < n - 1) {
                ++j;
            } else if (j == n) {
                --j;
            }
            for (; k < j; --j) p[j] = p[j - 1];
            p[k] = x;
            x = T(0);
        } else if (difference >= Bits && x != T(0)) {
            previous = wordExponent;
            ++k;
        } else {
            if ((p[k] += x) == T(0)) {
                CopyUp(p, k, n);
                if (p[k] == T(0)) break;
            }
            x = p[k];
            Unscale(x, xExponent);
            if (previous - Bits < xExponent) {
                KeepBits(x, xExponent - (previous - Bits));
                x = std::ldexp(x, xExponent);
                if ((p[k] -= x) == T(0)) CopyUp(p, k, n);
                if (--k == 0) {
                    previous = BigExponent<T>;
                } else {
                    scaled = p[k - 1];
                    Unscale(scaled, wordExponent);
                    previous = wordExponent;
                }
            } else if (k + 1 == n) {
                break;
            } else {
                x = p[k];
                Unscale(p[k], wordExponent);
                SplitHigh(p[k], wordExponent);
                x -= p[k];
                previous = wordExponent;
                scaled = x != T(0) ? x : p[k];
                Unscale(scaled, xExponent);
                ++k;
            }
        }
    }
    return p;
}

template <typename T>
T* Mulh(T* p, int n, T x) {
    if (n <= 0) return p;
    T line[DelayLine];
    line[0] = p[0] * x;
    const short code = Classify(line[0]);
    if (code >= 0) {
        if (code == NanCode) std::feraiseexcept(FE_INVALID);
        p[0] = line[0];
        if (code > 0 && n > 1) p[1] = T(0);
        return p;
    }
    p[0] = T(0);
    for (int j = 1, k = 0; k < n; ++k, --j) {
        for (; j < DelayLine; ++j) {
            if (k + j < n && p[k + j] != T(0)) {
                line[j] = p[k + j] * x;
                p[k + j] = T(0);
            } else {
                line[j] = T(0);
                j = 2 * DelayLine;
                break;
            }
        }
        if (line[0] == T(0)) break;
        T high = line[0];
        short exponent = 0;
        Unscale(high, exponent);
        SplitHigh(high, exponent);
        Addh(p, n, high);
        Addh(p, n, line[0] - high);
        for (int i = 1; i < j && i < DelayLine; ++i) {
            if ((line[i - 1] = line[i]) == T(0)) break;
        }
    }
    return p;
}

template <typename T>
T* Movx(T* p, int n, const T* q) {
    if (n > 0) std::memmove(p, q, static_cast<std::size_t>(n) * sizeof(T));
    return p;
}

template <typename T>
T* Setn(T* p, int n, std::int64_t x) {
    if constexpr (Digits<T> >= 27) {
        return Setw(p, n, static_cast<T>(x));
    } else {
        Setw(p, n, static_cast<T>(x / 10000));
        Mulh(p, n, static_cast<T>(10000));
        return Addh(p, n, static_cast<T>(x % 10000));
    }
}

template <typename T>
T* Addx(T* p, int n, const T* q, int m) {
    for (int k = 0; k < m && q[k] != T(0); ++k) Addh(p, n, q[k]);
    return p;
}

template <typename T>
T* Subx(T* p, int n, const T* q, int m) {
    for (int k = 0; k < m && q[k] != T(0); ++k) Addh(p, n, -q[k]);
    return p;
}

template <typename T>
T* Ldexpx(T* p, int n, int scale) {
    for (int k = 0; k < n; ++k) {
        p[k] = std::ldexp(p[k], scale);
        if (p[k] == T(0)) break;
    }
    return p;
}

template <typename T>
T* Mulx(T* p, int n, const T* q, int m, T* scratch) {
    if (n <= 0 || m == 0) return p;
    if (m <= 1 || q[0] == T(0) || q[1] == T(0)) return Mulh(p, n, q[0]);
    T* const original = scratch;
    T* const partial = scratch + n;
    Movx(original, n, p);
    Mulh(p, n, q[0]);
    for (int j = 1; j < m && q[j] != T(0); ++j) {
        Movx(partial, n, original);
        Mulh(partial, n, q[j]);
        Addx(p, n, partial, n);
    }
    return p;
}

template <typename T>
T* Invx(T* p, int n, T* scratch) {
    if (n <= 0) return p;
    const short code = Classify(p[0]);
    if (code >= 0) {
        if (code == InfinityCode) {
            p[0] = T(0);
        } else if (code == ZeroCode) {
            p[0] = std::numeric_limits<T>::infinity();
        }
        return p;
    }
    T* const accumulator = scratch;
    T* const negated = scratch + n;
    T* const product = negated + n;
    T estimate = p[0];
    Movx(negated, n, p);
    Mulh(negated, n, T(-1));
    if (n > 1) estimate += p[1];
    Setw(p, n, T(1) / estimate);
    for (int k = 1; k < n; k <<= 1) {
        Movx(accumulator, n, p);
        Mulx(accumulator, n, negated, n, product);
        Addh(accumulator, n, T(1));
        Mulx(accumulator, n, p, n, product);
        Addx(p, n, accumulator, n);
    }
    return p;
}

template <typename T>
T* Sqrtx(T* p, int n, T* scratch) {
    if (n <= 0) return p;
    if (Classify(p[0]) >= 0 || p[0] < T(0)) {
        if (p[0] < T(0)) {
            std::feraiseexcept(FE_INVALID);
            p[0] = std::numeric_limits<T>::quiet_NaN();
        }
        return p;
    }
    T* const accumulator = scratch;
    T* const root = scratch + n;
    T* const product = root + n;
    T estimate = p[0];
    if (n > 1) estimate += p[1];
    Setw(root, n, T(1) / std::sqrt(estimate));
    for (int k = 2; k < n; k <<= 1) {
        Movx(accumulator, n, root);
        Mulh(accumulator, n, T(-0.5));
        Mulx(accumulator, n, p, n, product);
        Mulx(accumulator, n, root, n, product);
        Addh(accumulator, n, T(1.5));
        Mulx(root, n, accumulator, n, product);
    }
    return Mulx(p, n, root, n, product);
}

template <typename T>
struct Tenth;

template <>
struct Tenth<float> {
    static constexpr float Fraction = 4096.0f;
    static constexpr float Words[AccumulatorSize] = {
        409.0f / Fraction,
        2457.0f / Fraction / Fraction,
        2457.0f / Fraction / Fraction / Fraction,
    };
};

template <>
struct Tenth<double> {
    static constexpr double Fraction = 67108864.0;
    static constexpr double Words[AccumulatorSize] = {
        6710886.0 / Fraction,
        26843545.0 / Fraction / Fraction,
        40265318.0 / Fraction / Fraction / Fraction,
    };
};

template <>
struct Tenth<long double> {
    static constexpr long double Fraction = 4294967296.0L;
    static constexpr long double Words[AccumulatorSize] = {
        429496729.0L / Fraction,
        2576980377.0L / Fraction / Fraction,
        2576980377.0L / Fraction / Fraction / Fraction,
    };
};

template <typename T>
T Dtento(T* value, std::int64_t power, int* error) {
    if (power == 0 || value[0] == T(0)) return Getw(value, AccumulatorSize);
    T factor[AccumulatorSize];
    std::uint64_t remaining = static_cast<std::uint64_t>(power);
    if (power > 0) {
        Setw(factor, AccumulatorSize, T(10));
    } else {
        remaining = 0 - remaining;
        Movx(factor, AccumulatorSize, Tenth<T>::Words);
    }
    for (;;) {
        T product[AccumulatorSize * 2];
        T square[AccumulatorSize];
        if ((remaining & 1) != 0) Mulx(value, AccumulatorSize, factor, AccumulatorSize, product);
        remaining >>= 1;
        if (remaining == 0) break;
        Movx(square, AccumulatorSize, factor);
        Mulx(factor, AccumulatorSize, square, AccumulatorSize, product);
    }
    const T result = Getw(value, AccumulatorSize);
    if (result == T(0) || std::isinf(result)) {
        *__error_nid_postfix() = RangeError;
        if (error != nullptr) *error |= 1;
    }
    return result;
}

} // namespace

extern "C" {

double APS5_VABI _Dtento_nid_postfix(double* value, std::int64_t power, int* error) {
    return Dtento(value, power, error);
}

float APS5_VABI _FDtento_nid_postfix(float* value, std::int64_t power, int* error) {
    return Dtento(value, power, error);
}

long double APS5_VABI _LDtento_nid_postfix(long double* value, std::int64_t power, int* error) {
    return Dtento(value, power, error);
}

float* APS5_VABI _FXp_addh_nid_postfix(float* p, int n, float x) {
    return Addh(p, n, x);
}

float* APS5_VABI _FXp_addx_nid_postfix(float* p, int n, const float* q, int m) {
    return Addx(p, n, q, m);
}

float* APS5_VABI _FXp_invx_nid_postfix(float* p, int n, float* scratch) {
    return Invx(p, n, scratch);
}

float* APS5_VABI _FXp_ldexpx_nid_postfix(float* p, int n, int scale) {
    return Ldexpx(p, n, scale);
}

float* APS5_VABI _FXp_movx_nid_postfix(float* p, int n, const float* q) {
    return Movx(p, n, q);
}

float* APS5_VABI _FXp_mulh_nid_postfix(float* p, int n, float x) {
    return Mulh(p, n, x);
}

float* APS5_VABI _FXp_mulx_nid_postfix(float* p, int n, const float* q, int m, float* scratch) {
    return Mulx(p, n, q, m, scratch);
}

float* APS5_VABI _FXp_setn_nid_postfix(float* p, int n, std::int64_t x) {
    return Setn(p, n, x);
}

float* APS5_VABI _FXp_setw_nid_postfix(float* p, int n, float x) {
    return Setw(p, n, x);
}

float* APS5_VABI _FXp_sqrtx_nid_postfix(float* p, int n, float* scratch) {
    return Sqrtx(p, n, scratch);
}

float* APS5_VABI _FXp_subx_nid_postfix(float* p, int n, const float* q, int m) {
    return Subx(p, n, q, m);
}

long double* APS5_VABI _LXp_addh_nid_postfix(long double* p, int n, long double x) {
    return Addh(p, n, x);
}

long double* APS5_VABI _LXp_addx_nid_postfix(long double* p, int n, const long double* q, int m) {
    return Addx(p, n, q, m);
}

long double APS5_VABI _LXp_getw_nid_postfix(const long double* p, int n) {
    return Getw(p, n);
}

long double* APS5_VABI _LXp_invx_nid_postfix(long double* p, int n, long double* scratch) {
    return Invx(p, n, scratch);
}

long double* APS5_VABI _LXp_ldexpx_nid_postfix(long double* p, int n, int scale) {
    return Ldexpx(p, n, scale);
}

long double* APS5_VABI _LXp_movx_nid_postfix(long double* p, int n, const long double* q) {
    return Movx(p, n, q);
}

long double* APS5_VABI _LXp_mulh_nid_postfix(long double* p, int n, long double x) {
    return Mulh(p, n, x);
}

long double* APS5_VABI _LXp_mulx_nid_postfix(long double* p, int n, const long double* q, int m, long double* scratch) {
    return Mulx(p, n, q, m, scratch);
}

long double* APS5_VABI _LXp_setn_nid_postfix(long double* p, int n, std::int64_t x) {
    return Setn(p, n, x);
}

long double* APS5_VABI _LXp_setw_nid_postfix(long double* p, int n, long double x) {
    return Setw(p, n, x);
}

long double* APS5_VABI _LXp_sqrtx_nid_postfix(long double* p, int n, long double* scratch) {
    return Sqrtx(p, n, scratch);
}

long double* APS5_VABI _LXp_subx_nid_postfix(long double* p, int n, const long double* q, int m) {
    return Subx(p, n, q, m);
}

}
