#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestDaysto() {
    Require(_Daysto_nid_postfix(1, 0) == 0, "_Daysto(1, 0)");
    Require(_Daysto_nid_postfix(1, 1) == 31, "_Daysto(1, 1)");
    Require(_Daysto_nid_postfix(4, 2) == 60, "a positive multiple of four uses the leap table");
    Require(_Daysto_nid_postfix(5, 2) == 60, "year 5 adds one leap day to the common table");
    Require(_Daysto_nid_postfix(0, 2) == 59, "year 0 uses the common table with no leap days");
    Require(_Daysto_nid_postfix(-1, 2) == 59, "years -1 to -3 add no leap days");
    Require(_Daysto_nid_postfix(-3, 11) == 334, "_Daysto(-3, 11)");
    Require(_Daysto_nid_postfix(-4, 2) == 63, "year -4 adds 1 + 8 / 4 days to the leap table");
    Require(_Daysto_nid_postfix(-5, 0) == 3, "year -5 adds 1 + 9 / 4 days to the common table");
    Require(_Daysto_nid_postfix(100, 11) == 359, "every fourth year uses the leap table, centuries included");
    Require(_Daysto_nid_postfix(2023, 11) == 334 + 505, "_Daysto(2023, 11)");
    Require(_Daysto_nid_postfix(2024, 11) == 335 + 505, "_Daysto(2024, 11)");
    constexpr int Lowest = std::numeric_limits<int>::min();
    constexpr int Highest = std::numeric_limits<int>::max();
    Require(_Daysto_nid_postfix(Lowest, 0) == 536870914 && _Daysto_nid_postfix(Lowest, 11) == 536871249, "the lowest year wraps 4 - year as an unsigned 32-bit value");
    Require(_Daysto_nid_postfix(Lowest + 1, 0) == 536870913 && _Daysto_nid_postfix(Lowest + 3, 11) == 536871247, "years just above the lowest wrap 4 - year");
    Require(_Daysto_nid_postfix(Lowest + 4, 0) == 536870913 && _Daysto_nid_postfix(Lowest + 4, 11) == 536871248, "the last wrapping year");
    Require(_Daysto_nid_postfix(Lowest + 5, 0) == 536870912 && _Daysto_nid_postfix(Lowest + 5, 11) == 536871246, "the first year whose 4 - year fits in int");
    Require(_Daysto_nid_postfix(Highest, 0) == 536870911 && _Daysto_nid_postfix(Highest, 11) == 536871245, "the highest year");
}

static void TestXpGetw() {
    const float one[] = {5.0f, 7.0f, 11.0f, 13.0f};
    Require(_FXp_getw_nid_postfix(one, 0) == 0.0f, "no parts sum to 0");
    Require(_FXp_getw_nid_postfix(one, 1) == 5.0f, "one part is returned as it is");
    const float zeroHead[] = {0.0f, 7.0f, 11.0f};
    Require(_FXp_getw_nid_postfix(zeroHead, 3) == 0.0f, "a zero first part ends the sum");
    const float zeroSecond[] = {5.0f, 0.0f, 11.0f};
    Require(_FXp_getw_nid_postfix(zeroSecond, 3) == 5.0f, "a zero second part ends the sum");
    Require(_FXp_getw_nid_postfix(one, 2) == 12.0f, "two parts are added");
    const float zeroThird[] = {5.0f, 7.0f, 0.0f, 13.0f};
    Require(_FXp_getw_nid_postfix(zeroThird, 4) == 12.0f, "a zero third part ends the sum");
    Require(_FXp_getw_nid_postfix(one, 3) == 23.0f, "three exact parts are added");
    Require(_FXp_getw_nid_postfix(one, 4) == 36.0f, "the fourth part joins the third");
    const float carry[] = {16777216.0f, 1.0f, 1.0f, 2.0f};
    Require(_FXp_getw_nid_postfix(carry, 3) == 16777218.0f, "a second part lost in the first sum is folded with the third first");
    Require(_FXp_getw_nid_postfix(carry, 4) == 16777220.0f, "a second part lost in the first sum is folded with the third and fourth first");
    const float nan[] = {std::nanf(""), 1.0f};
    Require(std::isnan(_FXp_getw_nid_postfix(nan, 2)), "a NaN first part is summed, not compared away");
}

int main() {
    Require(_Isdst_nid_postfix() == -1, "_Isdst did not return -1");
    TestDaysto();
    TestXpGetw();
    return 0;
}
