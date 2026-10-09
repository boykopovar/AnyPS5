#include "SceTypes.hpp"
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" {
double APS5_VABI __powidf2_nid_postfix(double, int);
unsigned long long APS5_VABI _WStoul_nid_postfix(const char16_t*, char16_t**, int);
}

static int failures = 0;

static void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static std::uint64_t Bits(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void CheckPowi(double base, int exponent, std::uint64_t expected, const char* what) {
    const double result = __powidf2_nid_postfix(base, exponent);
    if (Bits(result) != expected) {
        std::fprintf(stderr, "FAIL: %s: got 0x%016llx, expected 0x%016llx\n", what,
            static_cast<unsigned long long>(Bits(result)), static_cast<unsigned long long>(expected));
        ++failures;
    }
}

static void CheckPowidf2() {
    const double infinity = std::numeric_limits<double>::infinity();
    CheckPowi(2.0, 10, Bits(1024.0), "2^10");
    CheckPowi(2.0, -3, Bits(0.125), "2^-3");
    CheckPowi(-2.0, 3, Bits(-8.0), "-2^3");
    CheckPowi(10.0, -5, 0x3ee4f8b588e368f1ull, "10^-5");
    CheckPowi(1.1, 13, 0x400b9e405ed5ecb6ull, "1.1^13 squares instead of multiplying 13 times");
    CheckPowi(1.1, -7, 0x3fe06bca92ef4a05ull, "1.1^-7");
    CheckPowi(-1.7, -13, 0xbf508ab663bb7905ull, "-1.7^-13 differs from pow");
    CheckPowi(1.0000001, 1000000, 0x3ff1aec7b1e247a3ull, "1.0000001^1000000");
    CheckPowi(std::numeric_limits<double>::quiet_NaN(), 0, Bits(1.0), "NaN^0");
    CheckPowi(infinity, 0, Bits(1.0), "inf^0");
    CheckPowi(0.0, -1, Bits(infinity), "0^-1");
    CheckPowi(-0.0, -1, Bits(-infinity), "-0^-1");
    CheckPowi(-0.0, -2, Bits(infinity), "-0^-2");
    CheckPowi(-0.0, 3, Bits(-0.0), "-0^3");
    CheckPowi(2.0, INT_MIN, Bits(0.0), "2^INT_MIN");
    CheckPowi(0.5, INT_MIN, Bits(infinity), "0.5^INT_MIN");
    CheckPowi(-1.0, INT_MIN, Bits(1.0), "-1^INT_MIN");
    CheckPowi(-1.0, INT_MAX, Bits(-1.0), "-1^INT_MAX");
    CheckPowi(1.5, INT_MAX, Bits(infinity), "1.5^INT_MAX");
}

static void CheckWStoul() {
    char16_t* end = nullptr;
    const char16_t* text = u"  123abc";
    Require(_WStoul_nid_postfix(text, &end, 10) == 123 && end == text + 5, "decimal stops at the first non-digit");
    text = u"0x1F!";
    Require(_WStoul_nid_postfix(text, &end, 0) == 31 && end == text + 4, "base 0 detects hexadecimal");
    text = u"0755";
    Require(_WStoul_nid_postfix(text, &end, 0) == 0755 && end == text + 4, "base 0 detects octal");
    text = u"zz";
    Require(_WStoul_nid_postfix(text, &end, 36) == 1295 && end == text + 2, "base 36");
    text = u"-1";
    Require(_WStoul_nid_postfix(text, &end, 10) == 0xffffffffffffffffull && end == text + 2, "negative values wrap");
    text = u"18446744073709551615";
    errno = 0;
    Require(_WStoul_nid_postfix(text, &end, 10) == 0xffffffffffffffffull && errno == 0, "64-bit maximum");
    text = u"18446744073709551616x";
    errno = 0;
    Require(_WStoul_nid_postfix(text, &end, 10) == 0xffffffffffffffffull && errno == ERANGE && end == text + 20,
        "overflow saturates and sets ERANGE");
    text = u"7\u0661";
    Require(_WStoul_nid_postfix(text, &end, 10) == 7 && end == text + 1, "only ASCII digits");
    text = u"\u00a05";
    Require(_WStoul_nid_postfix(text, &end, 10) == 0 && end == text, "no conversion leaves the end at the start");
    const char16_t units[] = {u'4', u'2', 0};
    Require(_WStoul_nid_postfix(units, nullptr, 16) == 0x42, "null end pointer");
}

int main() {
    CheckPowidf2();
    CheckWStoul();
    if (failures != 0) return 1;
    std::puts("guest numeric helpers passed");
    return 0;
}
