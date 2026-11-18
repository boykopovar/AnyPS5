#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include <array>
#include <cmath>
#include <cstdint>
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

static void TestTtotm() {
    struct Row {
        std::int64_t seconds;
        std::array<std::int32_t, 8> fields;
    };
    static constexpr std::array<Row, 11> Rows{{
        {0LL, {0, 0, 0, 1, 0, 70, 4, 0}},
        {951827696LL, {56, 34, 12, 29, 1, 100, 2, 59}},
        {-2208988800LL, {0, 0, 0, 1, 0, 0, 1, 0}},
        {-2203891201LL, {59, 59, 23, 28, 1, 0, 3, 58}},
        {-2203891200LL, {0, 0, 0, 1, 2, 0, 4, 59}},
        {3981312000LL, {0, 0, 0, 29, 1, 196, 3, 59}},
        {4107478028LL, {8, 7, 6, 28, 1, 200, 0, 58}},
        {4107542400LL, {0, 0, 0, 29, 1, 200, 1, 59}},
        {4133980800LL, {0, 0, 0, 31, 11, 200, 6, 365}},
        {13569465600LL, {0, 0, 0, 29, 11, 499, 6, 362}},
        {-2208988801LL, {15, 0, 7, -206716581, 11, 826865099, 1, -206716248}},
    }};
    for (const Row& row : Rows) {
        for (const int isdst : {0, 1, -1}) {
            std::array<std::int32_t, 14> time{};
            time.fill(0x5a5a5a5a);
            Require(_Ttotm_nid_postfix(time.data(), row.seconds, isdst) == time.data(), "_Ttotm did not return its argument");
            for (std::size_t field = 0; field < row.fields.size(); ++field) Require(time[field] == row.fields[field], "_Ttotm wrote a wrong field");
            Require(time[8] == isdst, "_Ttotm did not store isdst unchanged");
            for (std::size_t field = 9; field < time.size(); ++field) Require(time[field] == 0x5a5a5a5a, "_Ttotm wrote past tm_isdst");
        }
    }
    auto* const own = static_cast<std::int32_t*>(_Ttotm_nid_postfix(nullptr, 951827696LL, 0));
    Require(own != nullptr && own[3] == 29 && own[4] == 1 && own[5] == 100, "_Ttotm with no structure did not fill its own");
    Require(_Ttotm_nid_postfix(nullptr, 0, 0) == own && own[5] == 70, "_Ttotm with no structure did not reuse its own");
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
    TestTtotm();
    TestXpGetw();
    return 0;
}
