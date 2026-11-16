#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {

constexpr int RangeError = 34;
constexpr int AccumulatorSize = 3;

template <typename T>
constexpr int Digits = std::numeric_limits<T>::digits;

template <typename T>
constexpr int WordBits = Digits<T> / 2;

template <typename T>
constexpr int LowestBit = std::numeric_limits<T>::min_exponent - Digits<T>;

class Natural {
public:
    int Bits() const {
        for (std::size_t index = limbs_.size(); index-- > 0;) {
            if (limbs_[index] != 0) return static_cast<int>(64 * index) + std::bit_width(limbs_[index]);
        }
        return 0;
    }

    bool Zero() const { return Bits() == 0; }

    std::uint64_t Bit(int index) const {
        if (index < 0) return 0;
        const auto limb = static_cast<std::size_t>(index / 64);
        return limb < limbs_.size() ? (limbs_[limb] >> (index % 64)) & 1 : 0;
    }

    void SetBit(int index) {
        Grow(static_cast<std::size_t>(index / 64) + 1);
        limbs_[static_cast<std::size_t>(index / 64)] |= std::uint64_t{1} << (index % 64);
    }

    void Add(std::uint64_t value, int shift) {
        const auto limb = static_cast<std::size_t>(shift / 64);
        const int offset = shift % 64;
        const std::uint64_t low = value << offset;
        const std::uint64_t high = offset == 0 ? 0 : value >> (64 - offset);
        Grow(limb + 3);
        std::uint64_t carry = 0;
        for (std::size_t index = limb; index < limbs_.size(); ++index) {
            const std::uint64_t part = index == limb ? low : index == limb + 1 ? high : 0;
            if (part == 0 && carry == 0 && index > limb + 1) break;
            const std::uint64_t sum = limbs_[index] + part;
            const std::uint64_t total = sum + carry;
            carry = (sum < part ? 1 : 0) + (total < sum ? 1 : 0);
            limbs_[index] = total;
            if (index + 1 == limbs_.size() && carry != 0) limbs_.push_back(0);
        }
    }

    void Subtract(const Natural& other) {
        std::uint64_t borrow = 0;
        for (std::size_t index = 0; index < limbs_.size(); ++index) {
            const std::uint64_t part = index < other.limbs_.size() ? other.limbs_[index] : 0;
            if (part == 0 && borrow == 0 && index >= other.limbs_.size()) break;
            const std::uint64_t difference = limbs_[index] - part;
            const std::uint64_t result = difference - borrow;
            borrow = (limbs_[index] < part ? 1 : 0) + (difference < borrow ? 1 : 0);
            limbs_[index] = result;
        }
    }

    void ShiftLeft(int count) {
        if (count <= 0 || Zero()) return;
        const auto whole = static_cast<std::size_t>(count / 64);
        const int bits = count % 64;
        std::vector<std::uint64_t> shifted(limbs_.size() + whole + 1, 0);
        for (std::size_t index = 0; index < limbs_.size(); ++index) {
            shifted[index + whole] |= limbs_[index] << bits;
            if (bits != 0) shifted[index + whole + 1] |= limbs_[index] >> (64 - bits);
        }
        limbs_ = std::move(shifted);
    }

    void Or(std::uint64_t bits) {
        Grow(1);
        limbs_[0] |= bits;
    }

    std::uint64_t Extract(int low, int count) const {
        std::uint64_t value = 0;
        for (int index = count; index-- > 0;) value = (value << 1) | Bit(low + index);
        return value;
    }

    void KeepBelow(int low) {
        if (low <= 0) {
            limbs_.clear();
            return;
        }
        const auto size = static_cast<std::size_t>((low + 63) / 64);
        if (limbs_.size() > size) limbs_.resize(size);
        if (low % 64 != 0 && limbs_.size() == size) limbs_[size - 1] &= (std::uint64_t{1} << (low % 64)) - 1;
    }

    friend int Compare(const Natural& left, const Natural& right) {
        const int leftBits = left.Bits();
        const int rightBits = right.Bits();
        if (leftBits != rightBits) return leftBits < rightBits ? -1 : 1;
        for (auto index = static_cast<std::size_t>((leftBits + 63) / 64); index-- > 0;) {
            if (left.limbs_[index] != right.limbs_[index]) return left.limbs_[index] < right.limbs_[index] ? -1 : 1;
        }
        return 0;
    }

private:
    void Grow(std::size_t size) {
        if (limbs_.size() < size) limbs_.resize(size, 0);
    }

    std::vector<std::uint64_t> limbs_;
};

struct Exact {
    bool negative = false;
    Natural magnitude;
    int exponent = 0;
};

struct Term {
    bool negative;
    std::uint64_t magnitude;
    int exponent;
};

using Terms = std::vector<Term>;

template <typename T>
Term Decompose(T value) {
    int exponent = 0;
    const T fraction = std::frexp(std::fabs(value), &exponent);
    return {std::signbit(value) != 0, static_cast<std::uint64_t>(std::ldexp(fraction, Digits<T>)), exponent - Digits<T>};
}

template <typename T>
void Gather(Terms& terms, const T* p, int n, bool negate = false) {
    for (int index = 0; index < n && p[index] != T(0) && std::isfinite(p[index]); ++index) {
        Term term = Decompose(p[index]);
        term.negative = term.negative != negate;
        terms.push_back(term);
    }
}

void AppendProduct(Terms& terms, const Term& left, const Term& right) {
    const bool negative = left.negative != right.negative;
    const std::uint64_t leftHalves[2] = {left.magnitude & 0xffffffffu, left.magnitude >> 32};
    const std::uint64_t rightHalves[2] = {right.magnitude & 0xffffffffu, right.magnitude >> 32};
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            const std::uint64_t product = leftHalves[i] * rightHalves[j];
            if (product != 0) terms.push_back({negative, product, left.exponent + right.exponent + 32 * (i + j)});
        }
    }
}

Exact Sum(const Terms& terms) {
    Exact result;
    if (terms.empty()) return result;
    int base = terms.front().exponent;
    for (const Term& term : terms) base = std::min(base, term.exponent);
    Natural positive;
    Natural negative;
    for (const Term& term : terms) (term.negative ? negative : positive).Add(term.magnitude, term.exponent - base);
    result.exponent = base;
    if (Compare(positive, negative) >= 0) {
        positive.Subtract(negative);
        result.magnitude = std::move(positive);
    } else {
        negative.Subtract(positive);
        result.magnitude = std::move(negative);
        result.negative = true;
    }
    return result;
}

Exact Reciprocal(const Exact& value, int precision) {
    Exact result;
    const int bits = value.magnitude.Bits();
    if (bits == 0) return result;
    Natural remainder;
    remainder.SetBit(bits - 1);
    for (int position = precision; position >= 0; --position) {
        if (Compare(remainder, value.magnitude) >= 0) {
            remainder.Subtract(value.magnitude);
            result.magnitude.SetBit(position);
        }
        remainder.ShiftLeft(1);
    }
    result.negative = value.negative;
    result.exponent = -(bits - 1 + precision) - value.exponent;
    return result;
}

Exact SquareRoot(const Exact& value, int precision) {
    Exact result;
    Natural radicand = value.magnitude;
    if (radicand.Zero()) return result;
    int shift = std::max(0, 2 * precision - radicand.Bits());
    if ((value.exponent - shift) % 2 != 0) ++shift;
    radicand.ShiftLeft(shift);
    Natural remainder;
    for (int pair = (radicand.Bits() + 1) / 2; pair-- > 0;) {
        remainder.ShiftLeft(2);
        remainder.Or((radicand.Bit(2 * pair + 1) << 1) | radicand.Bit(2 * pair));
        Natural trial = result.magnitude;
        trial.ShiftLeft(2);
        trial.Or(1);
        result.magnitude.ShiftLeft(1);
        if (Compare(remainder, trial) >= 0) {
            remainder.Subtract(trial);
            result.magnitude.Or(1);
        }
    }
    result.exponent = (value.exponent - shift) / 2;
    return result;
}

template <typename T>
T* Special(T* p, int n, T value) {
    p[0] = value;
    if (n > 1) p[1] = T(0);
    return p;
}

template <typename T>
T* Store(T* p, int n, const Exact& value) {
    int previous = 0;
    while (previous < n && p[previous] != T(0)) ++previous;
    Natural rest = value.magnitude;
    int count = 0;
    for (int top = rest.Bits() - 1; top >= 0 && count < n; top = rest.Bits() - 1) {
        const int low = std::max(top - WordBits<T> + 1, LowestBit<T> - value.exponent);
        if (low > top) break;
        T word = std::ldexp(static_cast<T>(rest.Extract(low, top - low + 1)), low + value.exponent);
        if (value.negative) word = -word;
        if (std::isinf(word)) return Special(p, n, word);
        p[count++] = word;
        rest.KeepBelow(low);
    }
    std::fill(p + count, p + std::min(n, std::max(previous, count + 1)), T(0));
    return p;
}

template <typename T>
int Precision(int n) {
    return (n + 1) * WordBits<T>;
}

template <typename T>
T Leading(T value) {
    int exponent = 0;
    std::frexp(value, &exponent);
    return std::ldexp(std::trunc(std::ldexp(value, WordBits<T> - exponent)), exponent - WordBits<T>);
}

template <typename T>
T Getw(const T* p, int n) {
    if (n == 0) return T(0);
    const T first = p[0];
    if (n == 1 || first == T(0) || p[1] == T(0)) return first;
    const T second = p[1];
    if (n == 2 || p[2] == T(0)) return first + second;
    const T low = n >= 4 ? p[2] + p[3] : p[2];
    const T high = first + second;
    return high - first == second ? high + low : first + (second + low);
}

template <typename T>
T* Setw(T* p, int n, T x) {
    if (n <= 0) return p;
    if (!std::isfinite(x)) return Special(p, n, x);
    int count = 0;
    for (T rest = x; rest != T(0) && count < n;) {
        const T word = count + 1 == n ? rest : Leading(rest);
        p[count++] = word;
        rest -= word;
    }
    if (count < n) p[count] = x == T(0) ? x : T(0);
    return p;
}

template <typename T>
T* Addh(T* p, int n, T x) {
    if (n <= 0 || x == T(0)) return p;
    if (!std::isfinite(x)) {
        if (std::isnan(x) || std::isfinite(p[0])) return Special(p, n, x);
        if (std::isinf(p[0]) && std::signbit(p[0]) != std::signbit(x)) {
            std::feraiseexcept(FE_INVALID);
            return Special(p, n, std::numeric_limits<T>::quiet_NaN());
        }
        return p;
    }
    if (!std::isfinite(p[0])) return p;
    Terms terms;
    Gather(terms, p, n);
    terms.push_back(Decompose(x));
    return Store(p, n, Sum(terms));
}

template <typename T>
T* Accumulate(T* p, int n, const T* q, int m, bool negate) {
    if (n <= 0) return p;
    int count = 0;
    bool special = !std::isfinite(p[0]);
    for (; count < m && q[count] != T(0); ++count) special = special || !std::isfinite(q[count]);
    if (special) {
        for (int index = 0; index < count; ++index) Addh(p, n, negate ? -q[index] : q[index]);
        return p;
    }
    if (count == 0) return p;
    Terms terms;
    Gather(terms, p, n);
    Gather(terms, q, count, negate);
    return Store(p, n, Sum(terms));
}

template <typename T>
T* Mulh(T* p, int n, T x) {
    if (n <= 0) return p;
    const T leading = p[0] * x;
    if (leading == T(0)) {
        p[0] = leading;
        return p;
    }
    if (!std::isfinite(leading)) {
        if (std::isnan(leading)) std::feraiseexcept(FE_INVALID);
        return Special(p, n, leading);
    }
    Terms words;
    Gather(words, p, n);
    const Term factor = Decompose(x);
    Terms terms;
    for (const Term& word : words) AppendProduct(terms, word, factor);
    return Store(p, n, Sum(terms));
}

template <typename T>
T* Mulx(T* p, int n, const T* q, int m) {
    if (n <= 0 || m == 0) return p;
    if (m < 2 || q[0] == T(0) || q[1] == T(0)) return Mulh(p, n, q[0]);
    const T leading = p[0] * q[0];
    if (leading == T(0) || !std::isfinite(leading)) return Mulh(p, n, q[0]);
    Terms left;
    Terms right;
    Gather(left, p, n);
    Gather(right, q, m);
    Terms terms;
    for (const Term& a : left) {
        for (const Term& b : right) AppendProduct(terms, a, b);
    }
    return Store(p, n, Sum(terms));
}

template <typename T>
T* Movx(T* p, int n, const T* q) {
    if (n > 0) std::memmove(p, q, static_cast<std::size_t>(n) * sizeof(T));
    return p;
}

template <typename T>
T* Setn(T* p, int n, std::int64_t x) {
    if constexpr (Digits<T> >= 64) {
        return Setw(p, n, static_cast<T>(x));
    } else {
        Setw(p, n, static_cast<T>(x / 10000));
        Mulh(p, n, static_cast<T>(10000));
        return Addh(p, n, static_cast<T>(x % 10000));
    }
}

template <typename T>
T* Ldexpx(T* p, int n, int scale) {
    for (int index = 0; index < n; ++index) {
        p[index] = std::ldexp(p[index], scale);
        if (p[index] == T(0)) break;
    }
    return p;
}

template <typename T>
T* Invx(T* p, int n) {
    if (n <= 0 || std::isnan(p[0])) return p;
    if (std::isinf(p[0])) {
        p[0] = T(0);
        return p;
    }
    Terms terms;
    Gather(terms, p, n);
    const Exact value = Sum(terms);
    if (value.magnitude.Zero()) return Special(p, n, std::numeric_limits<T>::infinity());
    return Store(p, n, Reciprocal(value, Precision<T>(n)));
}

template <typename T>
T* Sqrtx(T* p, int n) {
    if (n <= 0 || std::isnan(p[0]) || p[0] == T(0) || p[0] == std::numeric_limits<T>::infinity()) return p;
    Terms terms;
    Gather(terms, p, n);
    const Exact value = Sum(terms);
    if (p[0] < T(0) || value.negative) {
        std::feraiseexcept(FE_INVALID);
        return Special(p, n, std::numeric_limits<T>::quiet_NaN());
    }
    return Store(p, n, SquareRoot(value, Precision<T>(n)));
}

template <typename T>
constexpr std::array<T, AccumulatorSize> TenthWords() {
    std::array<T, AccumulatorSize> words{};
    std::uint64_t remainder = 1;
    T scale = T(1);
    for (T& word : words) {
        remainder <<= WordBits<T>;
        scale /= static_cast<T>(std::uint64_t{1} << WordBits<T>);
        word = static_cast<T>(remainder / 10) * scale;
        remainder %= 10;
    }
    return words;
}

template <typename T>
constexpr std::array<T, AccumulatorSize> Tenth = TenthWords<T>();

template <typename T>
T Dtento(T* value, std::int64_t power, int* error) {
    if (power == 0 || value[0] == T(0)) return Getw(value, AccumulatorSize);
    const T ten[1] = {T(10)};
    const T* const factor = power > 0 ? ten : Tenth<T>.data();
    const int words = power > 0 ? 1 : AccumulatorSize;
    std::uint64_t steps = static_cast<std::uint64_t>(power);
    if (power < 0) steps = 0 - steps;
    for (; steps > 0 && value[0] != T(0) && std::isfinite(value[0]); --steps) Mulx(value, AccumulatorSize, factor, words);
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
    return Accumulate(p, n, q, m, false);
}

float* APS5_VABI _FXp_invx_nid_postfix(float* p, int n, float*) {
    return Invx(p, n);
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

float* APS5_VABI _FXp_mulx_nid_postfix(float* p, int n, const float* q, int m, float*) {
    return Mulx(p, n, q, m);
}

float* APS5_VABI _FXp_setn_nid_postfix(float* p, int n, std::int64_t x) {
    return Setn(p, n, x);
}

float* APS5_VABI _FXp_setw_nid_postfix(float* p, int n, float x) {
    return Setw(p, n, x);
}

float* APS5_VABI _FXp_sqrtx_nid_postfix(float* p, int n, float*) {
    return Sqrtx(p, n);
}

float* APS5_VABI _FXp_subx_nid_postfix(float* p, int n, const float* q, int m) {
    return Accumulate(p, n, q, m, true);
}

long double* APS5_VABI _LXp_addh_nid_postfix(long double* p, int n, long double x) {
    return Addh(p, n, x);
}

long double* APS5_VABI _LXp_addx_nid_postfix(long double* p, int n, const long double* q, int m) {
    return Accumulate(p, n, q, m, false);
}

long double APS5_VABI _LXp_getw_nid_postfix(const long double* p, int n) {
    return Getw(p, n);
}

long double* APS5_VABI _LXp_invx_nid_postfix(long double* p, int n, long double*) {
    return Invx(p, n);
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

long double* APS5_VABI _LXp_mulx_nid_postfix(long double* p, int n, const long double* q, int m, long double*) {
    return Mulx(p, n, q, m);
}

long double* APS5_VABI _LXp_setn_nid_postfix(long double* p, int n, std::int64_t x) {
    return Setn(p, n, x);
}

long double* APS5_VABI _LXp_setw_nid_postfix(long double* p, int n, long double x) {
    return Setw(p, n, x);
}

long double* APS5_VABI _LXp_sqrtx_nid_postfix(long double* p, int n, long double*) {
    return Sqrtx(p, n);
}

long double* APS5_VABI _LXp_subx_nid_postfix(long double* p, int n, const long double* q, int m) {
    return Accumulate(p, n, q, m, true);
}

}
