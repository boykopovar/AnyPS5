#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstring>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static int GentimeCall(const int* time, char code, char* buffer, const char** text) {
    int length = 0x5a5a5a5a;
    *text = _Gentime_nid_postfix(time, _Getptimes_nid_postfix(), 0, code, &length, buffer);
    return length;
}

static void ExpectGentime(const int* time, char code, const char* expected, bool format = false) {
    char buffer[16];
    std::memset(buffer, 0x5a, sizeof(buffer));
    const char* text = nullptr;
    const int length = GentimeCall(time, code, buffer, &text);
    const int size = static_cast<int>(std::strlen(expected));
    Require(length == (format ? -size : size), "_Gentime reported a wrong length");
    Require(text != nullptr && std::memcmp(text, expected, static_cast<std::size_t>(size)) == 0, "_Gentime gave a wrong text");
}

static void TestGentime() {
    const int base[9] = {3, 5, 9, 17, 10, 126, 2, 320, 0};
    ExpectGentime(base, 'a', "Tue");
    ExpectGentime(base, 'A', "Tuesday");
    ExpectGentime(base, 'b', "Nov");
    ExpectGentime(base, 'B', "November");
    ExpectGentime(base, 'h', "Nov");
    ExpectGentime(base, 'p', "AM");
    ExpectGentime(base, 'd', "17");
    ExpectGentime(base, 'e', "17");
    ExpectGentime(base, 'H', "09");
    ExpectGentime(base, 'I', "09");
    ExpectGentime(base, 'M', "05");
    ExpectGentime(base, 'S', "03");
    ExpectGentime(base, 'm', "11");
    ExpectGentime(base, 'y', "26");
    ExpectGentime(base, 'Y', "2026");
    ExpectGentime(base, 'C', "20");
    ExpectGentime(base, 'j', "321");
    ExpectGentime(base, 'u', "2");
    ExpectGentime(base, 'w', "2");
    ExpectGentime(base, '%', "%");
    ExpectGentime(base, 'n', "\n");
    ExpectGentime(base, 't', "\t");
    ExpectGentime(base, 'q', "%q");
    ExpectGentime(base, 'E', "%E");
    ExpectGentime(base, 'O', "%O");
    ExpectGentime(base, 'Z', "");
    ExpectGentime(base, 'c', "%a %b %e %T %Y", true);
    ExpectGentime(base, 'x', "%m/%d/%y", true);
    ExpectGentime(base, 'X', "%H:%M:%S", true);
    ExpectGentime(base, 'r', "%I:%M:%S %p", true);
    ExpectGentime(base, 'D', "%m/%d/%y", true);
    ExpectGentime(base, 'F', "%Y-%m-%d", true);
    ExpectGentime(base, 'R', "%H:%M", true);
    ExpectGentime(base, 'T', "%H:%M:%S", true);

    char buffer[16];
    const char* text = nullptr;
    Require(GentimeCall(base, 'a', buffer, &text) == 3 && text >= _Getptimes_nid_postfix()->days && text < _Getptimes_nid_postfix()->days + std::strlen(_Getptimes_nid_postfix()->days), "_Gentime did not point into the day names");
    Require(GentimeCall(base, 'd', buffer, &text) == 2 && text == buffer, "_Gentime did not put a number in the buffer");

    const int noon[9] = {0, 0, 12, 17, 10, 126, 2, 320, 0};
    ExpectGentime(noon, 'p', "PM");
    ExpectGentime(noon, 'I', "12");
    const int midnight[9] = {0, 0, 0, 1, 0, 100, 6, 0, 0};
    ExpectGentime(midnight, 'I', "12");
    ExpectGentime(midnight, 'V', "52");
    ExpectGentime(midnight, 'G', "1999");
    ExpectGentime(midnight, 'g', "99");
    ExpectGentime(midnight, 'e', " 1");
    const int leap[9] = {60, 59, 23, 31, 11, 99, 5, 364, 0};
    ExpectGentime(leap, 'S', "60");
    const int sunday[9] = {0, 7, 13, 3, 0, 121, 0, 2, 0};
    ExpectGentime(sunday, 'V', "53");
    ExpectGentime(sunday, 'G', "2020");
    ExpectGentime(sunday, 'u', "7");
    const int leapDay[9] = {45, 30, 12, 29, 1, 124, 4, 59, 0};
    ExpectGentime(leapDay, 'U', "08");
    ExpectGentime(leapDay, 'W', "09");
    ExpectGentime(leapDay, 'V', "09");

    const auto with = [&](int field, int value) {
        static int time[9];
        std::memcpy(time, base, sizeof(time));
        time[field] = value;
        return static_cast<const int*>(time);
    };
    ExpectGentime(with(6, 7), 'a', "");
    ExpectGentime(with(6, 7), 'A', "");
    ExpectGentime(with(6, 7), 'u', "7");
    ExpectGentime(with(6, 8), 'w', "8");
    ExpectGentime(with(6, 100), 'w', "0");
    ExpectGentime(with(6, -1), 'a', "Sun");
    ExpectGentime(with(6, -1), 'A', "Sun");
    ExpectGentime(with(6, -1), 'u', "0");
    ExpectGentime(with(4, 12), 'b', "");
    ExpectGentime(with(4, 12), 'm', "13");
    ExpectGentime(with(4, 13), 'm', "14");
    ExpectGentime(with(4, -1), 'B', "Jan");
    ExpectGentime(with(4, -1), 'm', "00");
    ExpectGentime(with(2, 24), 'H', "24");
    ExpectGentime(with(2, 24), 'I', "12");
    ExpectGentime(with(2, 24), 'p', "PM");
    ExpectGentime(with(2, 25), 'I', "01");
    ExpectGentime(with(2, -1), 'H', "00");
    ExpectGentime(with(2, -1), 'I', "11");
    ExpectGentime(with(2, -1), 'p', "AM");
    ExpectGentime(with(3, 0), 'd', "00");
    ExpectGentime(with(3, 0), 'e', " 0");
    ExpectGentime(with(3, -1), 'e', " 0");
    ExpectGentime(with(3, 100), 'd', "00");
    ExpectGentime(with(3, 32), 'd', "32");
    ExpectGentime(with(1, -1), 'M', "00");
    ExpectGentime(with(0, 99), 'S', "99");
    ExpectGentime(with(5, -1), 'Y', "1899");
    ExpectGentime(with(5, -1), 'C', "19");
    ExpectGentime(with(5, -1), 'y', "99");
    ExpectGentime(with(5, -1900), 'Y', "0000");
    ExpectGentime(with(5, -2000), 'C', "00");
    ExpectGentime(with(5, 7999), 'Y', "9899");
    ExpectGentime(with(5, 8099), 'Y', "9999");
    ExpectGentime(with(5, 8099), 'C', "99");
    ExpectGentime(with(7, -1), 'j', "000");
    ExpectGentime(with(7, 400), 'j', "401");

    std::int64_t utc = 0;
    KernelTimesec zone{};
    std::int32_t daylight = 0;
    Require(sceKernelConvertLocaltimeToUtc(1794906303, 0, &utc, &zone, &daylight) == 0, "the local time could not be converted");
    const int offset = static_cast<int>(zone.west_sec);
    char expected[8];
    std::snprintf(expected, sizeof(expected), "%+03d%02d", offset / 3600, std::abs(offset / 60) % 60);
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(GentimeCall(base, 'z', buffer, &text) == 5 && text == buffer && std::memcmp(buffer, expected, 6) == 0, "_Gentime gave a wrong zone offset");
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
    TestGentime();
    Require(_Isdst_nid_postfix() == -1, "_Isdst did not return -1");
    TestDaysto();
    TestTtotm();
    TestXpGetw();
    return 0;
}
