#pragma once

#include "prx/libc/include/Api.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace LibcDetail {

inline constexpr short NotANumber = 2;
inline constexpr int InvalidException = 0x01;

inline constexpr long double LogPolynomial[] = {
    0x87f45b3375e0c9edp-66L, 0x87a58c55129af2f8p-66L, 0x9d90cfd7547e4ed2p-66L, 0xba2e6ba9488d3d0cp-66L,
    0xe38e393a60e2888bp-66L, 0x924924920646cc89p-65L, 0xcccccccccd00c2ebp-65L, 0xaaaaaaaaaaaaa334p-64L,
};

template<typename T>
T Sum(T left, T right) {
    if constexpr (std::is_same_v<T, long double>) return left + right;
    else if constexpr (std::is_same_v<T, double>) asm volatile("addsd %1, %0" : "+x"(left) : "x"(right));
    else asm volatile("addss %1, %0" : "+x"(left) : "x"(right));
    return left;
}

template<typename T>
T Product(T left, T right) {
    if constexpr (std::is_same_v<T, long double>) return left * right;
    else if constexpr (std::is_same_v<T, double>) asm volatile("mulsd %1, %0" : "+x"(left) : "x"(right));
    else asm volatile("mulss %1, %0" : "+x"(left) : "x"(right));
    return left;
}

template<typename T>
struct Precision;

template<typename T>
void HighPart(T* value, short bits) {
    short exponent = 0;
    Precision<T>::Unscale(&exponent, value);
    Precision<T>::Truncate(value, bits);
    Precision<T>::Scale(value, exponent);
}

template<typename T>
void CloseUp(T* p, std::int64_t from, std::int64_t count) {
    for (std::int64_t index = from + 1; index < count;) {
        const T value = p[index];
        p[index - 1] = value;
        ++index;
        if (value == 0) break;
    }
    p[count - 1] = 0;
}

template<typename T>
T* AddHalf(T* p, int n, T x) {
    using P = Precision<T>;
    T pending = x;
    T scaled = x;
    short pendingExponent = 0;
    if (n == 0) return p;
    short code = P::Unscale(&pendingExponent, &scaled);
    if (code > 0) {
        if (code == NotANumber) {
            p[0] = pending;
            return p;
        }
        const short current = P::Test(p);
        if (current <= 0) {
            p[0] = pending;
            return p;
        }
        if (current == NotANumber || std::signbit(pending) == std::signbit(p[0])) return p;
        P::Raise(InvalidException);
        p[0] = std::bit_cast<T>(P::NanBits);
        if (n >= 2) p[1] = 0;
        return p;
    }
    if (code == 0 || n <= 0) return p;
    const std::int64_t count = n;
    int k = 0;
    std::int64_t previous = P::FirstExponent;
    do {
        T term = p[k];
        short termExponent = 0;
        code = P::Unscale(&termExponent, &term);
        if (code > 0) return p;
        T* slot = p + k;
        if (code == 0) {
            *slot = pending;
            if (++k < n) p[k] = 0;
            return p;
        }
        const std::int64_t difference = std::int64_t{termExponent} - pendingExponent;
        if (difference <= -P::PartBits && pending != 0) {
            int end = n > k + 1 ? n : k + 1;
            int last = end - 1;
            for (std::int64_t index = std::int64_t{k} + 1; index < count; ++index) {
                if (p[index] == 0) {
                    end = static_cast<int>(index);
                    last = end - 1;
                    break;
                }
            }
            int target = end != n ? end : last;
            if (end < n - 1) target = end + 1;
            for (std::int64_t index = target; index > k; --index) p[index] = p[index - 1];
            *slot = pending;
            pending = 0;
        } else if (difference >= P::PartBits && pending != 0) {
            previous = termExponent;
            ++k;
        } else {
            *slot = Sum(pending, *slot);
            if (*slot == 0) {
                CloseUp(p, k, count);
                if (*slot == 0) return p;
            }
            pending = *slot;
            P::Unscale(&pendingExponent, &pending);
            if (previous - P::PartBits < pendingExponent) {
                P::Truncate(&pending, static_cast<short>(pendingExponent - (previous - P::PartBits)));
                P::Scale(&pending, pendingExponent);
                *slot = *slot - pending;
                if (*slot == 0) CloseUp(p, k, count);
                if (--k == 0) {
                    previous = P::FirstExponent;
                } else {
                    scaled = p[k - 1];
                    short exponent = 0;
                    P::Unscale(&exponent, &scaled);
                    previous = exponent;
                }
            } else {
                if (++k == n) return p;
                pending = *slot;
                short exponent = 0;
                P::Unscale(&exponent, slot);
                P::Truncate(slot, P::PartBits);
                P::Scale(slot, exponent);
                pending = pending - *slot;
                scaled = pending != 0 ? pending : *slot;
                P::Unscale(&pendingExponent, &scaled);
                previous = exponent;
            }
        }
    } while (k < n);
    return p;
}

template<typename T>
T* MultiplyHalf(T* p, int n, T x) {
    using P = Precision<T>;
    if (n <= 0) return p;
    std::array<T, 8> parts{};
    parts[0] = Product(x, p[0]);
    const short code = P::Test(&parts[0]);
    if (code >= 0) {
        if (code == NotANumber) P::Raise(InvalidException);
        p[0] = parts[0];
        if (code != 0 && n != 1) p[1] = 0;
        return p;
    }
    int filled = 1;
    p[0] = 0;
    for (std::int64_t row = 0; row < n; ++row) {
        T* terms = p + row;
        if (filled <= 3) {
            int index = filled;
            for (; index < 4; ++index) {
                if (row + index >= n || terms[index] == 0) break;
                parts[index] = Product(x, terms[index]);
                terms[index] = 0;
            }
            if (index == 4) {
                filled = 4;
            } else {
                parts[index] = 0;
                filled = 8;
            }
        }
        if (parts[0] == 0) return p;
        T high = parts[0];
        HighPart(&high, P::PartBits);
        AddHalf(p, n, high);
        AddHalf(p, n, parts[0] - high);
        --filled;
        for (int index = 0; index != filled;) {
            const T value = parts[index + 1];
            parts[index] = value;
            ++index;
            if (value == 0) break;
        }
    }
    return p;
}

template<typename T>
T* SetWide(T* p, int n, T x) {
    using P = Precision<T>;
    T value = x;
    if (n <= 0) return p;
    if (n == 1) {
        p[0] = x;
        return p;
    }
    short exponent = 0;
    const short code = P::Unscale(&exponent, &value);
    if (code == 0) {
        p[0] = value;
        return p;
    }
    if (code > 0) {
        p[0] = value;
        p[1] = 0;
        return p;
    }
    P::Truncate(&value, P::PartBits);
    P::Scale(&value, exponent);
    p[0] = value;
    const T rest = x - value;
    p[1] = rest;
    if (n < 3) return p;
    if constexpr (!std::is_same_v<T, double>) {
        p[2] = 0;
    } else {
        if (rest == 0) {
            p[2] = 0;
            return p;
        }
        HighPart(&p[1], P::PartBits);
        p[2] = rest - p[1];
        if (n != 3 && !(p[2] == 0)) p[3] = 0;
    }
    return p;
}

template<typename T>
T GetWide(const T* p, int n) {
    if (n == 0) return 0;
    if (n == 1) return p[0];
    const T first = p[0];
    if (first == 0) return first;
    const T second = p[1];
    if (second == 0) return first;
    if (n == 2) return Sum(first, second);
    T third = p[2];
    if (third == 0) return Sum(first, second);
    const T sum = Sum(first, second);
    if (n >= 4) third = Sum(third, p[3]);
    const T carried = sum - first;
    const T rounded = Sum(sum, third);
    const T tail = Sum(second, third);
    const T exact = Sum(first, tail);
    return carried == second ? rounded : exact;
}

template<typename T>
T* SetInteger(T* p, int n, std::int64_t value) {
    if constexpr (std::is_same_v<T, float>) {
        const std::int64_t high = value / 10000;
        const std::int64_t low = value - high * 10000;
        SetWide(p, n, static_cast<float>(high));
        MultiplyHalf(p, n, 10000.0f);
        AddHalf(p, n, static_cast<float>(low));
        return p;
    } else {
        return SetWide(p, n, static_cast<T>(value));
    }
}

template<typename T>
T* Move(T* p, int n, const T* q) {
    std::memcpy(p, q, static_cast<std::size_t>(static_cast<std::int64_t>(n)) * sizeof(T));
    return p;
}

template<typename T>
T* AddWide(T* p, int n, const T* q, int m) {
    for (int index = 0; index < m; ++index) {
        if (q[index] == 0) break;
        AddHalf(p, n, q[index]);
    }
    return p;
}

template<typename T>
T* MultiplyWide(T* p, int n, const T* q, int m, T* scratch) {
    if (n == 0 || m == 0) return p;
    if (q[0] == 0 || q[1] == 0) {
        MultiplyHalf(p, n, q[0]);
        return p;
    }
    const auto bytes = static_cast<std::size_t>(static_cast<std::int64_t>(n)) * sizeof(T);
    std::memcpy(scratch, p, bytes);
    MultiplyHalf(p, n, q[0]);
    T* product = scratch + n;
    for (int index = 1; index < m; ++index) {
        if (q[index] == 0) break;
        std::memcpy(product, scratch, bytes);
        MultiplyHalf(product, n, q[index]);
        for (int term = 0; term < n; ++term) {
            if (product[term] == 0) break;
            AddHalf(p, n, product[term]);
        }
    }
    return p;
}

template<typename T>
T* ScaleWide(T* p, int n, int exponent) {
    for (int index = 0; index < n; ++index) {
        p[index] = Precision<T>::Ldexp(p[index], exponent);
        if (p[index] == 0) break;
    }
    return p;
}


struct ExtendedWords {
    std::uint64_t mantissa;
    std::uint16_t signExponent;
};

struct ExtendedStep {
    short code;
    bool stored;
};

inline constexpr short FiniteCode = -1;
inline constexpr short ZeroCode = 0;
inline constexpr short InfiniteCode = 1;

inline ExtendedWords WordsOf(const long double& value) {
    ExtendedWords words;
    std::memcpy(&words.mantissa, &value, sizeof(words.mantissa));
    std::memcpy(&words.signExponent, reinterpret_cast<const unsigned char*>(&value) + 8, sizeof(words.signExponent));
    return words;
}

inline void StoreWords(long double& value, const ExtendedWords& words) {
    std::memcpy(&value, &words.mantissa, sizeof(words.mantissa));
    std::memcpy(reinterpret_cast<unsigned char*>(&value) + 8, &words.signExponent, sizeof(words.signExponent));
}

inline short SpecialCode(const ExtendedWords& words) {
    return (words.mantissa & 0x7fffffffffffffffull) != 0 ? NotANumber : InfiniteCode;
}

inline short TestWords(const ExtendedWords& words) {
    if ((words.signExponent & 0x7fff) == 0x7fff) return SpecialCode(words);
    if ((words.mantissa >> 63) != 0) return FiniteCode;
    return (words.signExponent & 0x7fff) != 0 || words.mantissa != 0 ? short{-2} : ZeroCode;
}

inline ExtendedStep TruncateExtended(long double& value, short scale) {
    ExtendedWords words = WordsOf(value);
    const int field = words.signExponent & 0x7fff;
    if (field == 0x7fff) return {SpecialCode(words), false};
    if (words.mantissa == 0) return {ZeroCode, false};
    const short drop = static_cast<short>(0x3ffe + 64 - field - scale);
    if (drop <= 0) return {ZeroCode, false};
    if (drop >= 64) {
        StoreWords(value, {0, static_cast<std::uint16_t>(words.signExponent & 0x8000)});
        return {FiniteCode, true};
    }
    const std::uint64_t mask = (std::uint64_t{1} << drop) - 1;
    const std::uint64_t lost = words.mantissa & mask;
    words.mantissa &= ~mask;
    StoreWords(value, words);
    return {lost != 0 ? FiniteCode : ZeroCode, true};
}

inline ExtendedStep ScaleExtendedBits(long double& value, std::int64_t exponent) {
    const ExtendedWords words = WordsOf(value);
    if ((words.signExponent & 0x7fff) == 0x7fff) return {SpecialCode(words), false};
    if (words.mantissa == 0) {
        StoreWords(value, {0, static_cast<std::uint16_t>(words.signExponent & 0x8000)});
        return {ZeroCode, true};
    }
    const std::uint16_t sign = words.signExponent & 0x8000;
    const int field = words.signExponent & 0x7fff;
    const int shift = std::countl_zero(words.mantissa);
    const std::uint64_t mantissa = words.mantissa << shift;
    const std::int64_t normalized = (field != 0 ? field : 1) - shift;
    ExtendedWords scaled{mantissa, sign};
    short code = FiniteCode;
    if (exponent > 0 && 0x7fff - normalized <= exponent) {
        scaled = {0x8000000000000000ull, static_cast<std::uint16_t>(sign | 0x7fff)};
        code = InfiniteCode;
    } else if (-normalized < exponent) {
        scaled.signExponent = static_cast<std::uint16_t>(sign | static_cast<std::uint16_t>(normalized + exponent));
    } else if (normalized + exponent < -63) {
        scaled = {0, 0};
        code = ZeroCode;
    } else {
        const int dropped = static_cast<int>(1 - normalized - exponent);
        const std::uint64_t lost = dropped == 64 ? mantissa : mantissa & ((std::uint64_t{1} << dropped) - 1);
        scaled.mantissa = dropped == 64 ? 0 : mantissa >> dropped;
        if (lost != 0) {
            const std::uint64_t half = std::uint64_t{1} << (dropped - 1);
            const int rounding = _Fltrounds_nid_postfix();
            const bool up = rounding == 3 ? sign != 0 : rounding == 2 ? sign == 0 : rounding != 0 && (lost > half || (lost == half && (scaled.mantissa & 1) != 0));
            if (up) ++scaled.mantissa;
        }
        if (scaled.mantissa == 0x8000000000000000ull) scaled.signExponent = static_cast<std::uint16_t>(sign | 1);
        if (scaled.mantissa == 0) code = ZeroCode;
    }
    StoreWords(value, scaled);
    return {code, true};
}

inline ExtendedStep UnscaleExtended(short& exponent, long double& value) {
    const ExtendedWords words = WordsOf(value);
    exponent = 0;
    if ((words.signExponent & 0x7fff) == 0x7fff) return {SpecialCode(words), false};
    if (words.mantissa == 0) return {ZeroCode, false};
    const int field = words.signExponent & 0x7fff;
    const int shift = std::countl_zero(words.mantissa);
    exponent = static_cast<short>((field != 0 ? field - 0x3ffe : -16381) - shift);
    StoreWords(value, {words.mantissa << shift, static_cast<std::uint16_t>((words.signExponent & 0x8000) | 0x3ffe)});
    return {FiniteCode, true};
}

template<>
struct Precision<long double> {
    static constexpr short PartBits = 32;
    static constexpr std::int64_t FirstExponent = 0x8000;
    static constexpr long double NanBits = std::numeric_limits<long double>::quiet_NaN();
    static short Test(long double* value) { return TestWords(WordsOf(*value)); }
    static short Unscale(short* exponent, long double* value) { return UnscaleExtended(*exponent, *value).code; }
    static void Truncate(long double* value, short bits) { TruncateExtended(*value, bits); }
    static void Scale(long double* value, std::int64_t exponent) { ScaleExtendedBits(*value, exponent); }
    static long double Ldexp(long double value, int exponent) { return ldexpl_nid_postfix(value, exponent); }
    static void Raise(int exceptions) { _Feraise_nid_postfix(exceptions); }
};

}
