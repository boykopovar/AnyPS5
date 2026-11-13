#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

using Words = std::array<double, 4>;

constexpr double Sentinel = 99.0;
constexpr int GuestEdom = 33;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

template<typename TCall>
void RequireInvalid(TCall call, const char* message) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

double Power(int exponent) {
    return std::ldexp(1.0, exponent);
}

bool Same(const Words& value, const Words& expected) {
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != expected[index] || std::signbit(value[index]) != std::signbit(expected[index])) return false;
    }
    return true;
}

Words Zeroes() {
    return Words{0.0, 0.0, 0.0, 0.0};
}

void CheckGetw() {
    const double parts[] = {5.0, 7.0, 11.0, 13.0};
    Require(_Xp_getw_nid_postfix(parts, 0) == 0.0, "_Xp_getw of no words is 0");
    Require(_Xp_getw_nid_postfix(parts, 1) == 5.0, "_Xp_getw of one word returns it");
    Require(_Xp_getw_nid_postfix(parts, 2) == 12.0, "_Xp_getw adds two words");
    Require(_Xp_getw_nid_postfix(parts, 3) == 23.0, "_Xp_getw adds three exact words");
    Require(_Xp_getw_nid_postfix(parts, 4) == 36.0, "_Xp_getw adds the fourth word to the third");
    const double zeroSecond[] = {5.0, 0.0, 11.0};
    Require(_Xp_getw_nid_postfix(zeroSecond, 3) == 5.0, "a zero second word ends the sum");
    const double carry[] = {Power(53), 1.0, 1.0, 2.0};
    Require(_Xp_getw_nid_postfix(carry, 3) == Power(53) + 2.0, "a second word lost in the first sum is folded with the third");
    Require(_Xp_getw_nid_postfix(carry, 4) == Power(53) + 4.0, "a second word lost in the first sum is folded with the third and fourth");
    RequireInvalid([&] { _Xp_getw_nid_postfix(parts, -1); }, "_Xp_getw rejects a negative count");
}

void CheckSetw() {
    const double value = 1.0 + Power(-26) + Power(-52);
    Words words{Sentinel, Sentinel, Sentinel, Sentinel};
    Require(_Xp_setw_nid_postfix(words.data(), 4, value) == words.data(), "_Xp_setw returns its array");
    Require(Same(words, Words{1.0, Power(-26), Power(-52), 0.0}), "_Xp_setw splits 53 bits into 26, 26 and 1");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 3, value);
    Require(Same(words, Words{1.0, Power(-26), Power(-52), Sentinel}), "_Xp_setw writes no terminator past three words");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 2, value);
    Require(Same(words, Words{1.0, Power(-26) + Power(-52), Sentinel, Sentinel}), "_Xp_setw with two words keeps the low 27 bits together");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 1, value);
    Require(Same(words, Words{value, Sentinel, Sentinel, Sentinel}), "_Xp_setw with one word stores the value");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 4, 1.5);
    Require(Same(words, Words{1.5, 0.0, 0.0, Sentinel}), "_Xp_setw ends a short value with zeroes");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 4, -0.0);
    Require(Same(words, Words{-0.0, Sentinel, Sentinel, Sentinel}), "_Xp_setw stores a zero alone");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 4, -std::numeric_limits<double>::infinity());
    Require(Same(words, Words{-std::numeric_limits<double>::infinity(), 0.0, Sentinel, Sentinel}), "_Xp_setw backs an infinity with a zero");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setw_nid_postfix(words.data(), 0, 1.0);
    Require(Same(words, Words{Sentinel, Sentinel, Sentinel, Sentinel}), "_Xp_setw with no words writes nothing");
}

void CheckSetn() {
    Words words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setn_nid_postfix(words.data(), 4, (std::int64_t{1} << 40) + 1);
    Require(Same(words, Words{Power(40), 1.0, 0.0, Sentinel}), "_Xp_setn splits a 41-bit integer");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setn_nid_postfix(words.data(), 4, -123);
    Require(Same(words, Words{-123.0, 0.0, 0.0, Sentinel}), "_Xp_setn loads a small negative integer");
    words = Words{Sentinel, Sentinel, Sentinel, Sentinel};
    _Xp_setn_nid_postfix(words.data(), 4, (std::int64_t{1} << 53) + 1);
    Require(Same(words, Words{Power(53), 0.0, 0.0, Sentinel}), "_Xp_setn rounds an integer wider than a double first");
}

void CheckAddh() {
    Words words = Zeroes();
    _Xp_addh_nid_postfix(words.data(), 4, 1.0);
    Require(Same(words, Words{1.0, 0.0, 0.0, 0.0}), "_Xp_addh stores into an empty value");
    _Xp_addh_nid_postfix(words.data(), 4, Power(-30));
    Require(Same(words, Words{1.0, Power(-30), 0.0, 0.0}), "_Xp_addh appends a word 30 bits down");
    _Xp_addh_nid_postfix(words.data(), 4, Power(-30));
    Require(Same(words, Words{1.0, Power(-29), 0.0, 0.0}), "_Xp_addh adds into the matching word");
    _Xp_addh_nid_postfix(words.data(), 4, 0.0);
    Require(Same(words, Words{1.0, Power(-29), 0.0, 0.0}), "_Xp_addh of zero changes nothing");
    words = Words{1.0, Power(-26), 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 4, Power(-26));
    Require(Same(words, Words{1.0 + Power(-25), 0.0, 0.0, 0.0}), "_Xp_addh carries a word that grew into the one above");
    words = Words{1.0, 0.0, 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 4, 1.0);
    Require(Same(words, Words{2.0, 0.0, 0.0, 0.0}), "_Xp_addh adds equal leading words");
    words = Words{1.0, Power(-60), 0.0, Sentinel};
    _Xp_addh_nid_postfix(words.data(), 4, Power(-30));
    Require(Same(words, Words{1.0, Power(-30), Power(-60), 0.0}), "_Xp_addh inserts a word between larger and smaller ones");
    words = Words{1.0, Power(-30), 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 4, std::numeric_limits<double>::infinity());
    Require(words[0] == std::numeric_limits<double>::infinity() && words[1] == Power(-30), "_Xp_addh stores an infinity over a finite value");
    words = Words{std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 4, std::numeric_limits<double>::infinity());
    Require(words[0] == std::numeric_limits<double>::infinity(), "_Xp_addh keeps an infinity of the same sign");
    *__error_nid_postfix() = 0;
    words = Words{std::numeric_limits<double>::infinity(), Sentinel, 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 4, -std::numeric_limits<double>::infinity());
    Require(std::isnan(words[0]) && words[1] == 0.0, "_Xp_addh of opposite infinities is NaN");
    Require(*__error_nid_postfix() == GuestEdom, "_Xp_addh raises invalid for opposite infinities");
    words = Words{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 4, 1.0);
    Require(std::isnan(words[0]), "_Xp_addh leaves a NaN value alone");
    words = Words{1.0, 0.0, 0.0, 0.0};
    _Xp_addh_nid_postfix(words.data(), 0, 1.0);
    Require(Same(words, Words{1.0, 0.0, 0.0, 0.0}), "_Xp_addh with no words writes nothing");
}

void CheckMulh() {
    Words words{1.0, Power(-30), 0.0, 0.0};
    Require(_Xp_mulh_nid_postfix(words.data(), 4, 3.0) == words.data(), "_Xp_mulh returns its array");
    Require(Same(words, Words{3.0, 3.0 * Power(-30), 0.0, 0.0}), "_Xp_mulh multiplies every word");
    words = Words{3.0, 0.0, 0.0, 0.0};
    _Xp_mulh_nid_postfix(words.data(), 4, 1.0 + Power(-40));
    Require(Same(words, Words{3.0, 3.0 * Power(-40), 0.0, 0.0}), "_Xp_mulh splits a product wider than a word");
    words = Words{1.0, Power(-30), 0.0, 0.0};
    _Xp_mulh_nid_postfix(words.data(), 4, 0.0);
    Require(Same(words, Words{0.0, Power(-30), 0.0, 0.0}), "_Xp_mulh by zero rewrites only the leading word");
    words = Words{DBL_MAX, Sentinel, 0.0, 0.0};
    _Xp_mulh_nid_postfix(words.data(), 4, 2.0);
    Require(words[0] == std::numeric_limits<double>::infinity() && words[1] == 0.0, "_Xp_mulh stores an overflowing product with a zero backstop");
    words = Words{DBL_MAX, Sentinel, Sentinel, Sentinel};
    _Xp_mulh_nid_postfix(words.data(), 2, -2.0);
    Require(Same(words, Words{-std::numeric_limits<double>::infinity(), 0.0, Sentinel, Sentinel}), "_Xp_mulh with two words backs an overflow with a zero");
    words = Words{DBL_MAX, Sentinel, Sentinel, Sentinel};
    _Xp_mulh_nid_postfix(words.data(), 1, 2.0);
    Require(Same(words, Words{std::numeric_limits<double>::infinity(), Sentinel, Sentinel, Sentinel}), "_Xp_mulh with one word writes no backstop");
}

void CheckMovxAddxSubx() {
    const Words source{1.0, Power(-30), Power(-60), 0.0};
    Words words{Sentinel, Sentinel, Sentinel, Sentinel};
    Require(_Xp_movx_nid_postfix(words.data(), 3, source.data()) == words.data(), "_Xp_movx returns its array");
    Require(Same(words, Words{1.0, Power(-30), Power(-60), Sentinel}), "_Xp_movx copies n words");
    _Xp_movx_nid_postfix(words.data(), 0, nullptr);
    RequireInvalid([&] { _Xp_movx_nid_postfix(words.data(), -1, source.data()); }, "_Xp_movx rejects a negative count");
    const Words addend{Power(-30), Power(-60), 0.0, Sentinel};
    words = Words{1.0, 0.0, 0.0, 0.0};
    _Xp_addx_nid_postfix(words.data(), 4, addend.data(), 4);
    Require(Same(words, Words{1.0, Power(-30), Power(-60), 0.0}), "_Xp_addx adds words up to the terminator");
    words = Words{1.0, 0.0, 0.0, 0.0};
    _Xp_addx_nid_postfix(words.data(), 4, addend.data(), 1);
    Require(Same(words, Words{1.0, Power(-30), 0.0, 0.0}), "_Xp_addx adds at most m words");
    words = Words{1.0, Power(-30), Power(-60), 0.0};
    _Xp_subx_nid_postfix(words.data(), 4, addend.data(), 4);
    Require(Same(words, Words{1.0, 0.0, 0.0, 0.0}), "_Xp_subx removes the words it added");
}

void CheckLdexpx() {
    Words words{1.0, Power(-30), 0.0, 7.0};
    Require(_Xp_ldexpx_nid_postfix(words.data(), 4, 3) == words.data(), "_Xp_ldexpx returns its array");
    Require(Same(words, Words{8.0, Power(-27), 0.0, 7.0}), "_Xp_ldexpx scales up to the terminator");
    words = Words{1.0, Power(-30), 5.0, 0.0};
    _Xp_ldexpx_nid_postfix(words.data(), 2, -1);
    Require(Same(words, Words{0.5, Power(-31), 5.0, 0.0}), "_Xp_ldexpx scales at most n words");
}

void CheckMulx() {
    std::array<double, 8> scratch{};
    Words words{1.0, Power(-30), 0.0, 0.0};
    const Words factor{3.0, Power(-40), 0.0, 0.0};
    Require(_Xp_mulx_nid_postfix(words.data(), 4, factor.data(), 4, scratch.data()) == words.data(), "_Xp_mulx returns its array");
    Require(Same(words, Words{3.0, 3.0 * Power(-30) + Power(-40), Power(-70), 0.0}), "_Xp_mulx forms the exact product");
    scratch.fill(Sentinel);
    words = Words{1.0, Power(-30), 0.0, 0.0};
    const double single[] = {5.0};
    _Xp_mulx_nid_postfix(words.data(), 4, single, 1, scratch.data());
    Require(Same(words, Words{5.0, 5.0 * Power(-30), 0.0, 0.0}), "_Xp_mulx by one word is _Xp_mulh");
    Require(scratch[0] == Sentinel, "_Xp_mulx by one word needs no scratch");
    words = Words{1.0, Power(-30), 0.0, 0.0};
    _Xp_mulx_nid_postfix(words.data(), 4, factor.data(), 0, scratch.data());
    Require(Same(words, Words{1.0, Power(-30), 0.0, 0.0}), "_Xp_mulx by no words writes nothing");
    RequireInvalid([&] { _Xp_mulx_nid_postfix(words.data(), 4, factor.data(), -1, scratch.data()); }, "_Xp_mulx rejects a negative multiplier count");
    RequireInvalid([&] { _Xp_mulx_nid_postfix(words.data(), -1, factor.data(), 4, scratch.data()); }, "_Xp_mulx rejects a negative count");
}

void CheckInvx() {
    std::array<double, 16> scratch{};
    Words words{4.0, 0.0, 0.0, 0.0};
    Require(_Xp_invx_nid_postfix(words.data(), 4, scratch.data()) == words.data(), "_Xp_invx returns its array");
    Require(Same(words, Words{0.25, 0.0, 0.0, 0.0}), "_Xp_invx of 4 is 0.25");
    words = Words{3.0, 0.0, 0.0, 0.0};
    _Xp_invx_nid_postfix(words.data(), 4, scratch.data());
    Require(_Xp_getw_nid_postfix(words.data(), 4) == 1.0 / 3.0, "_Xp_invx of 3 rounds to the double nearest 1/3");
    Require(words[1] != 0.0 && words[2] != 0.0, "_Xp_invx of 3 carries more than a double's precision");
    _Xp_mulh_nid_postfix(words.data(), 4, 3.0);
    _Xp_addh_nid_postfix(words.data(), 4, -1.0);
    Require(std::fabs(_Xp_getw_nid_postfix(words.data(), 4)) < Power(-95), "_Xp_invx of 3 times 3 is 1 to beyond 95 bits");
    words = Words{0.0, 0.0, 0.0, 0.0};
    _Xp_invx_nid_postfix(words.data(), 4, scratch.data());
    Require(words[0] == std::numeric_limits<double>::infinity(), "_Xp_invx of zero is infinity");
    words = Words{-std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0};
    _Xp_invx_nid_postfix(words.data(), 4, scratch.data());
    Require(words[0] == 0.0 && !std::signbit(words[0]), "_Xp_invx of an infinity is +0");
    words = Words{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0};
    _Xp_invx_nid_postfix(words.data(), 4, scratch.data());
    Require(std::isnan(words[0]), "_Xp_invx of NaN is NaN");
    RequireInvalid([&] { _Xp_invx_nid_postfix(words.data(), -1, scratch.data()); }, "_Xp_invx rejects a negative count");
}

void CheckSqrtx() {
    std::array<double, 16> scratch{};
    Words words{4.0, 0.0, 0.0, 0.0};
    Require(_Xp_sqrtx_nid_postfix(words.data(), 4, scratch.data()) == words.data(), "_Xp_sqrtx returns its array");
    Require(Same(words, Words{2.0, 0.0, 0.0, 0.0}), "_Xp_sqrtx of 4 is 2");
    words = Words{2.0, 0.0, 0.0, 0.0};
    _Xp_sqrtx_nid_postfix(words.data(), 4, scratch.data());
    Require(_Xp_getw_nid_postfix(words.data(), 4) == std::sqrt(2.0), "_Xp_sqrtx of 2 rounds to the correctly rounded square root");
    Words square = words;
    std::array<double, 8> temp{};
    _Xp_mulx_nid_postfix(square.data(), 4, words.data(), 4, temp.data());
    _Xp_addh_nid_postfix(square.data(), 4, -2.0);
    Require(std::fabs(_Xp_getw_nid_postfix(square.data(), 4)) < Power(-95), "_Xp_sqrtx of 2 squared is 2 to beyond 95 bits");
    words = Words{-0.0, 0.0, 0.0, 0.0};
    _Xp_sqrtx_nid_postfix(words.data(), 4, scratch.data());
    Require(words[0] == 0.0 && std::signbit(words[0]), "_Xp_sqrtx keeps -0");
    *__error_nid_postfix() = 0;
    words = Words{-1.0, 0.0, 0.0, 0.0};
    _Xp_sqrtx_nid_postfix(words.data(), 4, scratch.data());
    Require(std::isnan(words[0]), "_Xp_sqrtx of a negative value is NaN");
    Require(*__error_nid_postfix() == GuestEdom, "_Xp_sqrtx raises invalid for a negative value");
    words = Words{std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0};
    _Xp_sqrtx_nid_postfix(words.data(), 4, scratch.data());
    Require(words[0] == std::numeric_limits<double>::infinity(), "_Xp_sqrtx of infinity is infinity");
    RequireInvalid([&] { _Xp_sqrtx_nid_postfix(words.data(), -1, scratch.data()); }, "_Xp_sqrtx rejects a negative count");
}

}

int main() {
    CheckGetw();
    CheckSetw();
    CheckSetn();
    CheckAddh();
    CheckMulh();
    CheckMovxAddxSubx();
    CheckLdexpx();
    CheckMulx();
    CheckInvx();
    CheckSqrtx();
    return 0;
}
