#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>

namespace {

constexpr int RangeError = 34;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal extended precision: %s\n", message);
    std::abort();
}

template <typename T>
bool Same(T left, T right) {
    if (std::isnan(left) && std::isnan(right)) return true;
    return left == right && std::signbit(left) == std::signbit(right);
}

template <typename T>
bool WithinUlp(T value, T expected) {
    return value == expected || std::nextafter(value, expected) == expected;
}

template <typename T>
long double Total(const T* p, int n) {
    long double sum = 0;
    for (int i = n - 1; i >= 0; --i) sum += p[i];
    return sum;
}

template <typename T>
int Words(const T* p, int n) {
    int count = 0;
    while (count < n && p[count] != T(0)) ++count;
    return count;
}

template <typename T>
int SignificantBits(T value) {
    if (value == T(0) || !std::isfinite(value)) return 0;
    int exponent = 0;
    T fraction = std::frexp(std::fabs(value), &exponent);
    int bits = 0;
    while (fraction != T(0)) {
        fraction = fraction * T(2);
        const T whole = std::trunc(fraction);
        fraction -= whole;
        ++bits;
    }
    return bits;
}

void CheckFloatSetw() {
    std::mt19937 random(1);
    std::uniform_int_distribution<std::uint32_t> bits;
    for (int i = 0; i < 20000; ++i) {
        float x = 0;
        const std::uint32_t pattern = bits(random);
        std::memcpy(&x, &pattern, sizeof(x));
        if (!std::isfinite(x)) continue;
        float p[4] = {7, 7, 7, 7};
        Require(_FXp_setw_nid_postfix(p, 4, x) == p, "setw returns its array");
        if (x == 0.0f) {
            Require(Same(p[0], x) && p[1] == 7.0f, "setw stores a zero in one word");
            continue;
        }
        const int used = Words(p, 4);
        Require(used >= 1 && used <= 2 && Total(p, used) == x, "setw splits a float into at most two exact words");
        Require(p[used] == 0.0f && p[used + 1] == 7.0f, "setw ends a split float with one zero");
        Require(SignificantBits(p[0]) <= 12, "the leading word keeps half the significand");
        Require(Same(_FXp_getw_nid_postfix(p, 4), x), "a split float reads back unchanged");
    }
    float p[3] = {5, 5, 5};
    _FXp_setw_nid_postfix(p, 1, 1.0f / 3.0f);
    Require(p[0] == 1.0f / 3.0f && p[1] == 5.0f, "setw into one word stores the value whole");
    _FXp_setw_nid_postfix(p, 3, std::numeric_limits<float>::infinity());
    Require(std::isinf(p[0]) && p[1] == 0.0f && p[2] == 5.0f, "setw stores infinity with one zero after it");
    _FXp_setw_nid_postfix(p, 0, 2.0f);
    Require(std::isinf(p[0]), "setw into no words writes nothing");
    const float subnormal = std::numeric_limits<float>::denorm_min() * 77.0f;
    _FXp_setw_nid_postfix(p, 3, subnormal);
    Require(p[0] + p[1] == subnormal, "setw splits a subnormal exactly");
}

void CheckFloatSetn() {
    const std::int64_t values[] = {0, 1, -1, 9999, 10000, 123456789, -123456789, 2147483647, -2147483648LL};
    for (const std::int64_t value : values) {
        float p[6] = {};
        _FXp_setn_nid_postfix(p, 6, value);
        Require(Total(p, 6) == static_cast<long double>(value), "setn holds a 32-bit integer exactly");
    }
    float p[6] = {};
    _FXp_setn_nid_postfix(p, 6, 1234567890123LL);
    Require(Total(p, 6) == static_cast<long double>(static_cast<float>(123456789LL)) * 10000.0L + 123.0L,
            "setn of a wide integer rounds the quotient by 10000 to float first");
}

void CheckFloatAddh() {
    std::mt19937 random(2);
    std::uniform_int_distribution<int> mantissa(-(1 << 11), 1 << 11);
    std::uniform_int_distribution<int> scale(-30, -10);
    for (int round = 0; round < 2000; ++round) {
        float p[10] = {};
        long double exact = 0;
        for (int i = 0; i < 30; ++i) {
            const float x = std::ldexp(static_cast<float>(mantissa(random)), scale(random));
            exact += x;
            Require(_FXp_addh_nid_postfix(p, 10, x) == p, "addh returns its array");
        }
        Require(Total(p, 10) == exact, "addh keeps the exact sum of half-precision floats");
        const int used = Words(p, 10);
        for (int i = 1; i < used; ++i) {
            Require(std::fabs(p[i]) < std::fabs(p[i - 1]), "addh keeps words in decreasing magnitude");
        }
    }
    float p[4] = {};
    _FXp_setw_nid_postfix(p, 4, 1.0f);
    _FXp_addh_nid_postfix(p, 4, -1.0f);
    Require(p[0] == 0.0f, "addh cancels to zero");
    _FXp_addh_nid_postfix(p, 4, 0.0f);
    Require(p[0] == 0.0f, "adding zero changes nothing");
    _FXp_setw_nid_postfix(p, 4, 3.0f);
    _FXp_addh_nid_postfix(p, 4, std::numeric_limits<float>::infinity());
    Require(std::isinf(p[0]) && p[0] > 0, "adding infinity to a finite value stores infinity");
    _FXp_addh_nid_postfix(p, 4, std::numeric_limits<float>::infinity());
    Require(std::isinf(p[0]) && p[0] > 0, "adding a like-signed infinity keeps it");
    std::feclearexcept(FE_ALL_EXCEPT);
    _FXp_addh_nid_postfix(p, 4, -std::numeric_limits<float>::infinity());
    Require(std::isnan(p[0]) && p[1] == 0.0f, "infinity minus infinity is not a number");
    Require(std::fetestexcept(FE_INVALID) != 0, "infinity minus infinity raises invalid");
    _FXp_addh_nid_postfix(p, 4, 1.0f);
    Require(std::isnan(p[0]), "a value that is not a number absorbs finite additions");
    _FXp_setw_nid_postfix(p, 4, 1.0f);
    _FXp_addh_nid_postfix(p, 4, std::numeric_limits<float>::quiet_NaN());
    Require(std::isnan(p[0]), "adding a value that is not a number stores it");
    float full[2] = {};
    _FXp_setw_nid_postfix(full, 2, 1.0f);
    _FXp_addh_nid_postfix(full, 2, std::ldexp(1.0f, -40));
    _FXp_addh_nid_postfix(full, 2, std::ldexp(1.0f, -60));
    Require(full[0] == 1.0f && full[1] == std::ldexp(1.0f, -40), "a full array keeps its largest words");
}

void CheckFloatMulh() {
    std::mt19937 random(3);
    std::uniform_real_distribution<float> unit(-4.0f, 4.0f);
    std::uniform_int_distribution<int> half(-(1 << 11), 1 << 11);
    for (int round = 0; round < 5000; ++round) {
        const float x = unit(random);
        const int yHalf = half(random);
        const float y = std::ldexp(static_cast<float>(yHalf != 0 ? yHalf : 1), -9);
        float p[6] = {};
        _FXp_setw_nid_postfix(p, 6, x);
        Require(_FXp_mulh_nid_postfix(p, 6, y) == p, "mulh returns its array");
        Require(Total(p, 6) == static_cast<long double>(x) * y, "mulh by a half-precision value keeps the exact product");
        const int zHalf = half(random);
        const float z = std::ldexp(static_cast<float>(zHalf != 0 ? zHalf : 1), -5);
        _FXp_mulh_nid_postfix(p, 6, z);
        Require(Total(p, 6) == static_cast<long double>(x) * y * z, "a second mulh stays exact");
    }
    float p[4] = {};
    _FXp_setw_nid_postfix(p, 4, 2.5f);
    _FXp_mulh_nid_postfix(p, 4, 0.0f);
    Require(p[0] == 0.0f && _FXp_getw_nid_postfix(p, 4) == 0.0f, "multiplying by zero clears the leading word");
    _FXp_setw_nid_postfix(p, 4, 2.5f);
    _FXp_mulh_nid_postfix(p, 4, std::numeric_limits<float>::infinity());
    Require(std::isinf(p[0]) && p[1] == 0.0f, "multiplying by infinity stores infinity and a zero");
    std::feclearexcept(FE_ALL_EXCEPT);
    _FXp_mulh_nid_postfix(p, 4, 0.0f);
    Require(std::isnan(p[0]) && std::fetestexcept(FE_INVALID) != 0, "infinity times zero is invalid");
    _FXp_setw_nid_postfix(p, 4, std::numeric_limits<float>::max());
    _FXp_mulh_nid_postfix(p, 4, 4.0f);
    Require(std::isinf(p[0]), "an overflowing product becomes infinity");
    _FXp_setw_nid_postfix(p, 4, 0x1.8p-140f);
    _FXp_mulh_nid_postfix(p, 4, 0x1p-8f);
    Require(p[0] == 0x1.8p-148f && p[1] == 0.0f, "a subnormal product keeps its exact bits");
    _FXp_setw_nid_postfix(p, 4, 0x1.8p-140f);
    _FXp_mulh_nid_postfix(p, 4, 0x1p-10f);
    Require(p[0] == 0.0f, "bits below the smallest subnormal are dropped");
}

void CheckFloatMulx() {
    std::mt19937 random(4);
    std::uniform_real_distribution<float> unit(0.5f, 2.0f);
    for (int round = 0; round < 3000; ++round) {
        float a[6] = {};
        float b[6] = {};
        float scratch[12];
        const float a0 = unit(random);
        const float a1 = std::ldexp(unit(random), -20);
        const float b0 = unit(random);
        const float b1 = std::ldexp(unit(random), -20);
        float low[3] = {};
        _FXp_setw_nid_postfix(a, 6, a0);
        _FXp_setw_nid_postfix(low, 3, a1);
        _FXp_addx_nid_postfix(a, 6, low, 3);
        _FXp_setw_nid_postfix(b, 6, b0);
        _FXp_setw_nid_postfix(low, 3, b1);
        _FXp_addx_nid_postfix(b, 6, low, 3);
        Require(Total(a, 6) == static_cast<long double>(a0) + a1, "addx of split words is exact");
        const long double left = Total(a, 6);
        const long double right = Total(b, 6);
        Require(_FXp_mulx_nid_postfix(a, 6, b, 6, scratch) == a, "mulx returns its first array");
        const long double expected = left * right;
        Require(std::fabs(Total(a, 6) - expected) <= std::fabs(expected) * 0x1p-60L, "mulx keeps the product to six words");
    }
    float a[4] = {};
    float one[1] = {3.0f};
    float scratch[8];
    _FXp_setw_nid_postfix(a, 4, 1.5f);
    _FXp_mulx_nid_postfix(a, 4, one, 1, scratch);
    Require(Total(a, 4) == 4.5L, "mulx by a one-word value is a mulh");
    _FXp_mulx_nid_postfix(a, 0, one, 1, scratch);
    _FXp_mulx_nid_postfix(a, 4, one, 0, scratch);
    Require(Total(a, 4) == 4.5L, "mulx with an empty side changes nothing");
    _FXp_mulx_nid_postfix(a, 4, one, -1, scratch);
    Require(Total(a, 4) == 13.5L, "mulx with a negative count multiplies by the first word");
}

void CheckFloatMoveScaleAddSub() {
    float p[5] = {};
    float q[5] = {9, 9, 9, 9, 9};
    float low[3] = {};
    _FXp_setw_nid_postfix(p, 5, 1.0f / 7.0f);
    _FXp_setw_nid_postfix(low, 3, std::ldexp(1.0f / 3.0f, -30));
    _FXp_addx_nid_postfix(p, 5, low, 3);
    Require(Words(p, 5) == 4, "a float and a far smaller float take four words");
    Require(_FXp_movx_nid_postfix(q, 5, p) == q, "movx returns its target");
    Require(std::memcmp(p, q, sizeof(p)) == 0, "movx copies every word");
    _FXp_movx_nid_postfix(q, 0, nullptr);
    const long double before = Total(p, 5);
    Require(_FXp_ldexpx_nid_postfix(p, 5, 7) == p, "ldexpx returns its array");
    Require(Total(p, 5) == before * 128.0L, "ldexpx scales every word");
    _FXp_ldexpx_nid_postfix(p, 5, -7);
    Require(std::memcmp(p, q, sizeof(p)) == 0, "ldexpx scales back exactly");
    _FXp_ldexpx_nid_postfix(p, 5, -400);
    Require(p[0] == 0.0f && p[1] == q[1], "ldexpx stops at the first word that underflows");
    float sum[8] = {};
    _FXp_setw_nid_postfix(sum, 8, 2.0f);
    Require(_FXp_addx_nid_postfix(sum, 8, q, 5) == sum, "addx returns its array");
    Require(Total(sum, 8) == 2.0L + Total(q, 5), "addx adds every word");
    Require(_FXp_subx_nid_postfix(sum, 8, q, 5) == sum, "subx returns its array");
    Require(Total(sum, 8) == 2.0L, "subx takes the words away again");
    _FXp_subx_nid_postfix(sum, 8, sum, 8);
    Require(sum[0] == 0.0f, "an array minus itself is zero");
    float stop[3] = {1.0f, 0.0f, 5.0f};
    float target[4] = {};
    _FXp_addx_nid_postfix(target, 4, stop, 3);
    Require(Total(target, 4) == 1.0L, "addx stops at the first zero word");
}

void CheckFloatInvx() {
    std::mt19937 random(5);
    std::uniform_real_distribution<float> unit(-1000.0f, 1000.0f);
    for (int round = 0; round < 5000; ++round) {
        const float x = unit(random);
        if (x == 0.0f) continue;
        float p[4] = {};
        float scratch[16];
        _FXp_setw_nid_postfix(p, 4, x);
        Require(_FXp_invx_nid_postfix(p, 4, scratch) == p, "invx returns its array");
        Require(std::fabs(Total(p, 4) * x - 1.0L) < 0x1p-40L, "invx doubles the precision of the reciprocal");
        Require(WithinUlp(_FXp_getw_nid_postfix(p, 4), 1.0f / x), "invx reads back within an ulp of the rounded reciprocal");
    }
    float p[3] = {};
    float scratch[12];
    _FXp_setw_nid_postfix(p, 3, std::numeric_limits<float>::infinity());
    _FXp_invx_nid_postfix(p, 3, scratch);
    Require(p[0] == 0.0f, "the reciprocal of infinity is zero");
    _FXp_invx_nid_postfix(p, 3, scratch);
    Require(std::isinf(p[0]) && p[0] > 0, "the reciprocal of zero is infinity");
    p[0] = std::numeric_limits<float>::quiet_NaN();
    _FXp_invx_nid_postfix(p, 3, scratch);
    Require(std::isnan(p[0]), "the reciprocal of a value that is not a number stays one");
    float single[1] = {4.0f};
    _FXp_invx_nid_postfix(single, 1, scratch);
    Require(single[0] == 0.25f, "invx of one word keeps an exact reciprocal");
    single[0] = 3.0f;
    _FXp_invx_nid_postfix(single, 1, scratch);
    Require(single[0] == 0x1.554p-2f, "invx of one word keeps the leading twelve bits of the reciprocal");
}

void CheckFloatSqrtx() {
    std::mt19937 random(6);
    std::uniform_real_distribution<float> unit(0.0f, 1000.0f);
    for (int round = 0; round < 5000; ++round) {
        const float x = unit(random);
        if (x == 0.0f) continue;
        float p[4] = {};
        float scratch[16];
        _FXp_setw_nid_postfix(p, 4, x);
        Require(_FXp_sqrtx_nid_postfix(p, 4, scratch) == p, "sqrtx returns its array");
        const long double root = Total(p, 4);
        Require(std::fabs(root * root / x - 1.0L) < 0x1p-36L, "sqrtx refines the square root");
        Require(WithinUlp(_FXp_getw_nid_postfix(p, 4), std::sqrt(x)), "sqrtx reads back within an ulp of the rounded square root");
    }
    float p[3] = {};
    float scratch[12];
    _FXp_setw_nid_postfix(p, 3, -2.0f);
    std::feclearexcept(FE_ALL_EXCEPT);
    _FXp_sqrtx_nid_postfix(p, 3, scratch);
    Require(std::isnan(p[0]) && std::fetestexcept(FE_INVALID) != 0, "the square root of a negative value is invalid");
    _FXp_setw_nid_postfix(p, 3, -0.0f);
    _FXp_sqrtx_nid_postfix(p, 3, scratch);
    Require(Same(p[0], -0.0f), "the square root of negative zero is negative zero");
    _FXp_setw_nid_postfix(p, 3, std::numeric_limits<float>::infinity());
    _FXp_sqrtx_nid_postfix(p, 3, scratch);
    Require(std::isinf(p[0]) && p[0] > 0, "the square root of infinity is infinity");
    _FXp_setw_nid_postfix(p, 3, -std::numeric_limits<float>::infinity());
    std::feclearexcept(FE_ALL_EXCEPT);
    _FXp_sqrtx_nid_postfix(p, 3, scratch);
    Require(std::isnan(p[0]) && std::fetestexcept(FE_INVALID) != 0, "the square root of minus infinity is invalid");
}

float FloatDtento(float x, std::int64_t power, int* error) {
    float p[3] = {};
    _FXp_setw_nid_postfix(p, 3, x);
    return _FDtento_nid_postfix(p, power, error);
}

void CheckDtento() {
    int error = 0;
    *__error_nid_postfix() = 0;
    Require(FloatDtento(1.5f, 3, &error) == 1500.0f, "_FDtento scales up by a power of ten");
    Require(FloatDtento(7.0f, 0, &error) == 7.0f, "_FDtento by ten to the zero reads the value back");
    Require(FloatDtento(0.0f, 30, &error) == 0.0f, "_FDtento leaves zero alone");
    Require(FloatDtento(1.0f, 10, &error) == 1e10f, "_FDtento gives ten to the tenth");
    Require(FloatDtento(1.0f, -1, &error) == 0.1f, "_FDtento gives one tenth");
    Require(FloatDtento(3.0f, -5, &error) == 3e-5f, "_FDtento scales down by a power of ten");
    Require(error == 0 && *__error_nid_postfix() == 0, "_FDtento in range reports no error");
    Require(std::isinf(FloatDtento(1.0f, 39, &error)), "_FDtento overflows to infinity");
    Require(error == 1 && *__error_nid_postfix() == RangeError, "_FDtento reports overflow as a range error");
    error = 4;
    *__error_nid_postfix() = 0;
    Require(FloatDtento(1.0f, -60, &error) == 0.0f, "_FDtento underflows to zero");
    Require(error == 5 && *__error_nid_postfix() == RangeError, "_FDtento sets the low error bit on underflow");
    *__error_nid_postfix() = 0;
    Require(FloatDtento(1.0f, -60, nullptr) == 0.0f, "_FDtento accepts a null error pointer");
    Require(*__error_nid_postfix() == RangeError, "_FDtento without an error pointer still sets errno");
    *__error_nid_postfix() = 0;
    Require(FloatDtento(1.0f, std::numeric_limits<std::int64_t>::min(), &error) == 0.0f,
            "_FDtento by the most negative power underflows");

    error = 0;
    *__error_nid_postfix() = 0;
    double d[3] = {1.5, 0, 0};
    Require(_Dtento_nid_postfix(d, 3, &error) == 1500.0, "_Dtento scales up by a power of ten");
    for (int power = -22; power <= 22; ++power) {
        double v[3] = {1.0, 0, 0};
        const double expected = power >= 0 ? std::pow(10.0, power) : 1.0 / std::pow(10.0, -power);
        Require(_Dtento_nid_postfix(v, power, &error) == expected, "_Dtento gives the rounded power of ten");
    }
    Require(error == 0 && *__error_nid_postfix() == 0, "_Dtento in range reports no error");
    double big[3] = {1.0, 0, 0};
    Require(std::isinf(_Dtento_nid_postfix(big, 309, &error)) && error == 1, "_Dtento overflows to infinity");

    error = 0;
    *__error_nid_postfix() = 0;
    long double l[3] = {1.5L, 0, 0};
    Require(_LDtento_nid_postfix(l, 3, &error) == 1500.0L, "_LDtento scales up by a power of ten");
    for (int power = 0; power <= 27; ++power) {
        long double v[3] = {1.0L, 0, 0};
        long double expected = 1.0L;
        for (int i = 0; i < power; ++i) expected *= 10.0L;
        Require(_LDtento_nid_postfix(v, power, &error) == expected, "_LDtento gives exact powers of ten");
    }
    long double tenth[3] = {1.0L, 0, 0};
    Require(_LDtento_nid_postfix(tenth, -1, &error) == 0.1L, "_LDtento gives one tenth");
    Require(error == 0 && *__error_nid_postfix() == 0, "_LDtento in range reports no error");
    float floatTenth[3] = {1.0f, 0, 0};
    _FDtento_nid_postfix(floatTenth, -1, &error);
    Require(floatTenth[0] == 0x1.998p-4f && floatTenth[1] == 0x1.998p-16f && floatTenth[2] == 0x1.99p-28f,
            "_FDtento leaves one tenth in twelve-bit words in its argument");
    double doubleTenth[3] = {1.0, 0, 0};
    _Dtento_nid_postfix(doubleTenth, -1, &error);
    Require(doubleTenth[0] == 0x1.9999998p-4 && doubleTenth[1] == 0x1.9999998p-32 && doubleTenth[2] == 0x1.99998p-60,
            "_Dtento leaves one tenth in 26-bit words in its argument");
    long double longTenth[3] = {1.0L, 0, 0};
    _LDtento_nid_postfix(longTenth, -1, &error);
    Require(longTenth[0] == 0xc.cccccccp-7L && longTenth[1] == 0xc.cccccccp-39L && longTenth[2] == 0xc.cccccc8p-71L,
            "_LDtento leaves one tenth in 32-bit words in its argument");
    Require(error == 0, "reading one tenth back reports no error");
    long double tiny[3] = {1.0L, 0, 0};
    Require(_LDtento_nid_postfix(tiny, -5000, &error) == 0.0L && error == 1, "_LDtento underflows to zero");
    Require(*__error_nid_postfix() == RangeError, "_LDtento reports underflow as a range error");
}

void CheckLongDouble() {
    std::mt19937_64 random(7);
    std::uniform_real_distribution<long double> unit(-1000.0L, 1000.0L);
    for (int round = 0; round < 3000; ++round) {
        const long double x = unit(random);
        if (x == 0.0L) continue;
        long double p[5] = {9, 9, 9, 9, 9};
        Require(_LXp_setw_nid_postfix(p, 5, x) == p, "_LXp_setw returns its array");
        const int used = Words(p, 5);
        Require(used >= 1 && used <= 2 && Total(p, used) == x && p[used] == 0.0L && p[used + 1] == 9.0L,
                "_LXp_setw splits a long double into at most two words and one zero");
        Require(SignificantBits(p[0]) <= 32, "the leading long double word keeps half the significand");
        Require(Same(_LXp_getw_nid_postfix(p, 5), x), "_LXp_getw reads a split value back");

        long double q[5] = {};
        long double scratch[20];
        _LXp_setw_nid_postfix(q, 5, x);
        Require(_LXp_invx_nid_postfix(q, 5, scratch) == q, "_LXp_invx returns its array");
        Require(WithinUlp(_LXp_getw_nid_postfix(q, 5), 1.0L / x), "_LXp_invx reads back within an ulp of the rounded reciprocal");

        long double r[5] = {};
        const long double square = std::fabs(x);
        _LXp_setw_nid_postfix(r, 5, square);
        Require(_LXp_sqrtx_nid_postfix(r, 5, scratch) == r, "_LXp_sqrtx returns its array");
        Require(WithinUlp(_LXp_getw_nid_postfix(r, 5), std::sqrt(square)), "_LXp_sqrtx reads back within an ulp of the rounded square root");
    }

    long double p[8] = {};
    _LXp_setn_nid_postfix(p, 8, 9007199254740993LL);
    Require(_LXp_getw_nid_postfix(p, 8) == 9007199254740993.0L, "_LXp_setn holds a 64-bit integer");
    _LXp_mulh_nid_postfix(p, 8, 3.0L);
    Require(_LXp_getw_nid_postfix(p, 8) == 27021597764222979.0L, "_LXp_mulh keeps an exact product");
    _LXp_addh_nid_postfix(p, 8, 0x1p-80L);
    Require(p[Words(p, 8) - 1] == 0x1p-80L, "_LXp_addh keeps a tiny addend in its own word");
    long double copy[8];
    Require(_LXp_movx_nid_postfix(copy, 8, p) == copy && std::memcmp(copy, p, sizeof(p)) == 0, "_LXp_movx copies every word");
    _LXp_ldexpx_nid_postfix(copy, 8, -3);
    Require(Total(copy, 8) == Total(p, 8) / 8.0L, "_LXp_ldexpx scales every word");
    long double sum[8] = {};
    Require(_LXp_addx_nid_postfix(sum, 8, p, 8) == sum && Total(sum, 8) == Total(p, 8), "_LXp_addx adds every word");
    Require(_LXp_subx_nid_postfix(sum, 8, copy, 8) == sum, "_LXp_subx returns its array");
    Require(_LXp_getw_nid_postfix(sum, 8) == 23643898043695106.625L, "_LXp_subx takes every word away");

    long double a[4] = {};
    long double b[4] = {};
    long double scratch[8];
    _LXp_setw_nid_postfix(a, 4, 3.0L);
    _LXp_addh_nid_postfix(a, 4, 0x1p-70L);
    _LXp_setw_nid_postfix(b, 4, 5.0L);
    _LXp_addh_nid_postfix(b, 4, 0x1p-70L);
    Require(_LXp_mulx_nid_postfix(a, 4, b, 4, scratch) == a, "_LXp_mulx returns its first array");
    Require(_LXp_getw_nid_postfix(a, 4) == 15.0L, "_LXp_mulx rounds the product to long double on read");
    Require(a[0] == 15.0L && a[1] == 0x1p-67L && a[2] == 0x1p-140L, "_LXp_mulx keeps the cross terms in the low words");

    long double borrow[5] = {};
    _LXp_setw_nid_postfix(borrow, 5, 1.0L);
    _LXp_addh_nid_postfix(borrow, 5, -0x1p-100L);
    Require(borrow[0] == 0xffffffffp-32L && borrow[1] == 0xffffffffp-64L && borrow[2] == 0xffffffffp-96L && borrow[3] == 0xfp-100L &&
                borrow[4] == 0.0L,
            "_LXp_addh borrows across words and keeps them all of one sign");

    long double special[3] = {};
    _LXp_setw_nid_postfix(special, 3, std::numeric_limits<long double>::infinity());
    std::feclearexcept(FE_ALL_EXCEPT);
    _LXp_addh_nid_postfix(special, 3, -std::numeric_limits<long double>::infinity());
    Require(std::isnan(special[0]) && std::fetestexcept(FE_INVALID) != 0, "long double infinity minus infinity is invalid");
    _LXp_setw_nid_postfix(special, 3, -1.0L);
    long double sqrtScratch[12];
    std::feclearexcept(FE_ALL_EXCEPT);
    _LXp_sqrtx_nid_postfix(special, 3, sqrtScratch);
    Require(std::isnan(special[0]) && std::fetestexcept(FE_INVALID) != 0, "the long double square root of a negative value is invalid");
    Require(_LXp_getw_nid_postfix(special, 0) == 0.0L, "_LXp_getw of no words is zero");
    long double tie[4] = {1.0L, 0x1p-64L, 0x1p-100L, -0x1p-99L};
    Require(_LXp_getw_nid_postfix(tie, 4) == 1.0L, "_LXp_getw adds the fourth word before rounding");
    Require(_LXp_getw_nid_postfix(tie, 3) == 1.0L + 0x1p-63L, "_LXp_getw of three words ignores a fourth");
    long double negative[3] = {2.0L, 0.5L, 0.0L};
    Require(_LXp_getw_nid_postfix(negative, -1) == 2.5L, "_LXp_getw tests only for a count of zero");
}

} // namespace

int main() {
    CheckFloatSetw();
    CheckFloatSetn();
    CheckFloatAddh();
    CheckFloatMulh();
    CheckFloatMulx();
    CheckFloatMoveScaleAddSub();
    CheckFloatInvx();
    CheckFloatSqrtx();
    CheckDtento();
    CheckLongDouble();
    std::puts("libc internal extended precision: ok");
    return 0;
}
