#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

constexpr short NotANumber = 2;
constexpr int GuestEdom = 33;
constexpr int GuestErange = 34;
constexpr int InvalidException = 0x01;

template<typename T>
T Sum(T left, T right) {
    if constexpr (std::is_same_v<T, double>) asm volatile("addsd %1, %0" : "+x"(left) : "x"(right));
    else asm volatile("addss %1, %0" : "+x"(left) : "x"(right));
    return left;
}

template<typename T>
T Product(T left, T right) {
    if constexpr (std::is_same_v<T, double>) asm volatile("mulsd %1, %0" : "+x"(left) : "x"(right));
    else asm volatile("mulss %1, %0" : "+x"(left) : "x"(right));
    return left;
}

template<typename T>
struct Precision;

template<>
struct Precision<double> {
    using Bits = std::uint64_t;
    static constexpr short PartBits = 26;
    static constexpr std::int64_t FirstExponent = 0x800;
    static constexpr Bits NanBits = 0x7ff8000000000000ull;
    static short Test(double* value) { return _Dtest_nid_postfix(value); }
    static short Unscale(short* exponent, double* value) { return _ZNSt8__sce_v29_DunscaleEPsPd_nid_postfix(exponent, value); }
    static void Truncate(double* value, short bits) { _ZNSt8__sce_v25_DintEPds_nid_postfix(value, bits); }
    static void Scale(double* value, std::int64_t exponent) { _ZNSt8__sce_v27_DscaleEPdl_nid_postfix(value, exponent); }
    static double Ldexp(double value, int exponent) { return ldexp_nid_postfix(value, exponent); }
};

template<>
struct Precision<float> {
    using Bits = std::uint32_t;
    static constexpr short PartBits = 12;
    static constexpr std::int64_t FirstExponent = 0x100;
    static constexpr Bits NanBits = 0x7fc00000u;
    static short Test(float* value) { return _FDtest_nid_postfix(value); }
    static short Unscale(short* exponent, float* value) { return _ZNSt8__sce_v210_FDunscaleEPsPf_nid_postfix(exponent, value); }
    static void Truncate(float* value, short bits) { _ZNSt8__sce_v26_FDintEPfs_nid_postfix(value, bits); }
    static void Scale(float* value, std::int64_t exponent) { _ZNSt8__sce_v28_FDscaleEPfl_nid_postfix(value, exponent); }
    static float Ldexp(float value, int exponent) { return ldexpf_nid_postfix(value, exponent); }
};

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
        _ZNSt8__sce_v28_FeraiseEi_nid_postfix(InvalidException);
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
        if (code == NotANumber) _ZNSt8__sce_v28_FeraiseEi_nid_postfix(InvalidException);
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
    if constexpr (std::is_same_v<T, float>) {
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

}

extern "C" {

void APS5_VABI _ZNSt8__sce_v28_FeraiseEi_nid_postfix(int exceptions) {
    if ((exceptions & 0x05) != 0) *__error_nid_postfix() = GuestEdom;
    else if ((exceptions & 0x18) != 0) *__error_nid_postfix() = GuestErange;
}

double* APS5_VABI _ZNSt8__sce_v28_Xp_setwEPdid_nid_postfix(double* p, int n, double x) { return SetWide(p, n, x); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_setwEPfif_nid_postfix(float* p, int n, float x) { return SetWide(p, n, x); }
double APS5_VABI _ZNSt8__sce_v28_Xp_getwEPKdi_nid_postfix(const double* p, int n) { return GetWide(p, n); }
float APS5_VABI _ZNSt8__sce_v29_FXp_getwEPKfi_nid_postfix(const float* p, int n) { return GetWide(p, n); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_setnEPdil_nid_postfix(double* p, int n, std::int64_t value) { return SetInteger(p, n, value); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_setnEPfil_nid_postfix(float* p, int n, std::int64_t value) { return SetInteger(p, n, value); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_movxEPdiPKd_nid_postfix(double* p, int n, const double* q) { return Move(p, n, q); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_movxEPfiPKf_nid_postfix(float* p, int n, const float* q) { return Move(p, n, q); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_addhEPdid_nid_postfix(double* p, int n, double x) { return AddHalf(p, n, x); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_addhEPfif_nid_postfix(float* p, int n, float x) { return AddHalf(p, n, x); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_addxEPdiPKdi_nid_postfix(double* p, int n, const double* q, int m) { return AddWide(p, n, q, m); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_addxEPfiPKfi_nid_postfix(float* p, int n, const float* q, int m) { return AddWide(p, n, q, m); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_mulhEPdid_nid_postfix(double* p, int n, double x) { return MultiplyHalf(p, n, x); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_mulhEPfif_nid_postfix(float* p, int n, float x) { return MultiplyHalf(p, n, x); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_mulxEPdiPKdiS0__nid_postfix(double* p, int n, const double* q, int m, double* scratch) { return MultiplyWide(p, n, q, m, scratch); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_mulxEPfiPKfiS0__nid_postfix(float* p, int n, const float* q, int m, float* scratch) { return MultiplyWide(p, n, q, m, scratch); }
double* APS5_VABI _ZNSt8__sce_v210_Xp_ldexpxEPdii_nid_postfix(double* p, int n, int exponent) { return ScaleWide(p, n, exponent); }
float* APS5_VABI _ZNSt8__sce_v211_FXp_ldexpxEPfii_nid_postfix(float* p, int n, int exponent) { return ScaleWide(p, n, exponent); }

}
