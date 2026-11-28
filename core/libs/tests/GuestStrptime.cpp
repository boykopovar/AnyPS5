#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Guest strptime test failed at line %u\n", location.line());
        std::abort();
    }
}

static void TestIsoDateTime() {
    GuestTm tm{};
    const char input[] = "2026-11-23 15:45:30";
    const char* rem = strptime_nid_postfix(input, "%Y-%m-%d %H:%M:%S", &tm);
    Require(rem != nullptr);
    Require(*rem == '\0');
    Require(tm.tm_year == 126);
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 23);
    Require(tm.tm_hour == 15);
    Require(tm.tm_min == 45);
    Require(tm.tm_sec == 30);
    Require(tm.tm_wday == 1);
    Require(tm.tm_yday == 326);

    GuestTm tmLeap{};
    const char inLeap[] = "2024-02-29 12:00:00";
    const char* remLeap = strptime_nid_postfix(inLeap, "%Y-%m-%d %H:%M:%S", &tmLeap);
    Require(remLeap != nullptr && *remLeap == '\0');
    Require(tmLeap.tm_year == 124);
    Require(tmLeap.tm_mon == 1);
    Require(tmLeap.tm_mday == 29);
    Require(tmLeap.tm_wday == 4);
    Require(tmLeap.tm_yday == 59);

    GuestTm tmMar{};
    const char* remMar = strptime_nid_postfix("2024-03-01", "%Y-%m-%d", &tmMar);
    Require(remMar != nullptr && *remMar == '\0');
    Require(tmMar.tm_wday == 5);
    Require(tmMar.tm_yday == 60);
}

static void TestMonthAndWeekdayNames() {
    GuestTm tm{};
    const char input[] = "Monday, 23 November 2026";
    const char* rem = strptime_nid_postfix(input, "%A, %d %B %Y", &tm);
    Require(rem != nullptr);
    Require(*rem == '\0');
    Require(tm.tm_wday == 1);
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 23);
    Require(tm.tm_year == 126);

    GuestTm tmShort{};
    const char inputShort[] = "mon, 23 nov 2026";
    rem = strptime_nid_postfix(inputShort, "%a, %d %b %Y", &tmShort);
    Require(rem != nullptr);
    Require(*rem == '\0');
    Require(tmShort.tm_wday == 1);
    Require(tmShort.tm_mon == 10);

    GuestTm tmUpper{};
    const char inputUpper[] = "SUNDAY 15 MAR 2026";
    rem = strptime_nid_postfix(inputUpper, "%a %d %b %Y", &tmUpper);
    Require(rem != nullptr);
    Require(*rem == '\0');
    Require(tmUpper.tm_wday == 0);
    Require(tmUpper.tm_mon == 2);
    Require(tmUpper.tm_mday == 15);
}

static void TestTwelveHourAmPm() {
    GuestTm tm{};
    const char in1[] = "12:00:00 AM";
    const char* rem = strptime_nid_postfix(in1, "%I:%M:%S %p", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 0);

    const char in2[] = "12:00:00 PM";
    rem = strptime_nid_postfix(in2, "%I:%M:%S %p", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 12);

    const char in3[] = "01:30:15 PM";
    rem = strptime_nid_postfix(in3, "%I:%M:%S %p", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 13);
    Require(tm.tm_min == 30);
    Require(tm.tm_sec == 15);

    const char in4[] = "11:59:59 PM";
    rem = strptime_nid_postfix(in4, "%r", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 23);
    Require(tm.tm_min == 59);
    Require(tm.tm_sec == 59);

    const char in5[] = "12";
    rem = strptime_nid_postfix(in5, "%I", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 12);

    const char in6[] = "PM 02";
    rem = strptime_nid_postfix(in6, "%p %I", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 14);

    const char in7[] = "AM 12";
    rem = strptime_nid_postfix(in7, "%p %I", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 0);

    const char in8[] = "PM 12";
    rem = strptime_nid_postfix(in8, "%p %I", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 12);
}

static void TestTwoDigitYearAndCentury() {
    GuestTm tm{};
    const char in1[] = "99-05-10";
    const char* rem = strptime_nid_postfix(in1, "%y-%m-%d", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 99);
    Require(tm.tm_mon == 4);
    Require(tm.tm_mday == 10);

    const char in2[] = "26-05-10";
    rem = strptime_nid_postfix(in2, "%y-%m-%d", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 126);

    const char in3[] = "00-01-01";
    rem = strptime_nid_postfix(in3, "%y-%m-%d", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 100);

    const char in4[] = "19 85";
    rem = strptime_nid_postfix(in4, "%C %y", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 85);

    const char in68[] = "68";
    rem = strptime_nid_postfix(in68, "%y", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 168);

    const char in69[] = "69";
    rem = strptime_nid_postfix(in69, "%y", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 69);

    GuestTm tmSpaceDay{};
    rem = strptime_nid_postfix(" 5", "%d", &tmSpaceDay);
    Require(rem != nullptr && *rem == '\0');
    Require(tmSpaceDay.tm_mday == 5);
}

static void TestCompoundFormats() {
    GuestTm tm{};
    const char inD[] = "11/23/26";
    const char* rem = strptime_nid_postfix(inD, "%D", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 23);
    Require(tm.tm_year == 126);

    const char inF[] = "2026-11-23";
    rem = strptime_nid_postfix(inF, "%F", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 126);
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 23);

    const char inT[] = "18:20:45";
    rem = strptime_nid_postfix(inT, "%T", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 18);
    Require(tm.tm_min == 20);
    Require(tm.tm_sec == 45);

    const char inR[] = "09:15";
    rem = strptime_nid_postfix(inR, "%R", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 9);
    Require(tm.tm_min == 15);
}

static void TestTimezone() {
    GuestTm tm{};
    const char in1[] = "2026-11-23 +0200";
    const char* rem = strptime_nid_postfix(in1, "%Y-%m-%d %z", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_gmtoff == 7200);

    const char in2[] = "2026-11-23 -0500";
    rem = strptime_nid_postfix(in2, "%Y-%m-%d %z", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_gmtoff == -18000);

    const char in3[] = "2026-11-23 +03:30";
    rem = strptime_nid_postfix(in3, "%Y-%m-%d %z", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_gmtoff == 12600);

    const char in4[] = "2026-11-23 Z";
    rem = strptime_nid_postfix(in4, "%Y-%m-%d %z", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_gmtoff == 0);

    const char in5[] = "2026-11-23 UTC";
    rem = strptime_nid_postfix(in5, "%Y-%m-%d %Z", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_gmtoff == 0);
}

static void TestWhitespaceAndRemainder() {
    GuestTm tm{};
    const char input[] = "2026-11-23   extra trailing text";
    const char* rem = strptime_nid_postfix(input, "%Y-%m-%d", &tm);
    Require(rem != nullptr);
    Require(std::strcmp(rem, "   extra trailing text") == 0);
    Require(tm.tm_year == 126);

    const char inputSpace[] = "2026 \t\n 11 \t 23";
    rem = strptime_nid_postfix(inputSpace, "%Y %m %d", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 126);
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 23);
}

static void TestModifiersAndEscapes() {
    GuestTm tm{};
    const char input[] = "100% 2026-11-23";
    const char* rem = strptime_nid_postfix(input, "100%% %EY-%Om-%Od", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 126);
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 23);
}

static void TestInvalidAndNullInputs() {
    GuestTm tm{};
    Require(strptime_nid_postfix(nullptr, "%Y", &tm) == nullptr);
    Require(strptime_nid_postfix("2026", nullptr, &tm) == nullptr);
    Require(strptime_nid_postfix("2026", "%Y", nullptr) == nullptr);

    Require(strptime_nid_postfix("not-a-number", "%Y-%m-%d", &tm) == nullptr);
    Require(strptime_nid_postfix("2026-13-01", "%Y-%m-%d", &tm) == nullptr);
    Require(strptime_nid_postfix("2026-11-32", "%Y-%m-%d", &tm) == nullptr);
    Require(strptime_nid_postfix("2026-11-23 25:00:00", "%Y-%m-%d %H:%M:%S", &tm) == nullptr);
    Require(strptime_nid_postfix("2026-11-23 12:60:00", "%Y-%m-%d %H:%M:%S", &tm) == nullptr);
    Require(strptime_nid_postfix("2026-11-23 12:00:61", "%Y-%m-%d %H:%M:%S", &tm) == nullptr);
    Require(strptime_nid_postfix("2026-11-23", "%Y/%m/%d", &tm) == nullptr);
    Require(strptime_nid_postfix("+20261124", "%Y%m%d", &tm) == nullptr);
    Require(strptime_nid_postfix("-20261124", "%Y%m%d", &tm) == nullptr);
    Require(strptime_nid_postfix("123", "%q", &tm) == nullptr);
}

static void TestCompactDate() {
    GuestTm tm{};
    const char input[] = "20261124";
    const char* rem = strptime_nid_postfix(input, "%Y%m%d", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_year == 126);
    Require(tm.tm_mon == 10);
    Require(tm.tm_mday == 24);
}

static void TestSpacePaddedHours() {
    GuestTm tm{};
    const char* rem = strptime_nid_postfix(" 5:07", "%k:%M", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 5);
    Require(tm.tm_min == 7);

    rem = strptime_nid_postfix("00:15", "%k:%M", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 0);
    Require(tm.tm_min == 15);

    rem = strptime_nid_postfix("23", "%k", &tm);
    Require(rem != nullptr && *rem == '\0');
    Require(tm.tm_hour == 23);

    GuestTm tm12{};
    rem = strptime_nid_postfix(" 7:30 PM", "%l:%M %p", &tm12);
    Require(rem != nullptr && *rem == '\0');
    Require(tm12.tm_hour == 19);
    Require(tm12.tm_min == 30);

    rem = strptime_nid_postfix("12:05 AM", "%l:%M %p", &tm12);
    Require(rem != nullptr && *rem == '\0');
    Require(tm12.tm_hour == 0);
    Require(tm12.tm_min == 5);

    Require(strptime_nid_postfix("24", "%k", &tm) == nullptr);
    Require(strptime_nid_postfix("0", "%l", &tm) == nullptr);
    Require(strptime_nid_postfix("13", "%l", &tm) == nullptr);
}

static void TestWeekNumbers() {
    GuestTm tmU{};
    const char* rem = strptime_nid_postfix("2026-01-3", "%Y-%U-%w", &tmU);
    Require(rem != nullptr && *rem == '\0');
    Require(tmU.tm_year == 126);
    Require(tmU.tm_mon == 0);
    Require(tmU.tm_mday == 7);
    Require(tmU.tm_yday == 6);
    Require(tmU.tm_wday == 3);

    GuestTm tmU0{};
    rem = strptime_nid_postfix("2026-00-5", "%Y-%U-%w", &tmU0);
    Require(rem != nullptr && *rem == '\0');
    Require(tmU0.tm_year == 126);
    Require(tmU0.tm_mon == 0);
    Require(tmU0.tm_mday == 2);
    Require(tmU0.tm_yday == 1);
    Require(tmU0.tm_wday == 5);

    GuestTm tmW{};
    rem = strptime_nid_postfix("2026-01-1", "%Y-%W-%u", &tmW);
    Require(rem != nullptr && *rem == '\0');
    Require(tmW.tm_year == 126);
    Require(tmW.tm_mon == 0);
    Require(tmW.tm_mday == 5);
    Require(tmW.tm_yday == 4);
    Require(tmW.tm_wday == 1);

    GuestTm tmW0{};
    rem = strptime_nid_postfix("2026-00-4", "%Y-%W-%u", &tmW0);
    Require(rem != nullptr && *rem == '\0');
    Require(tmW0.tm_year == 126);
    Require(tmW0.tm_mon == 0);
    Require(tmW0.tm_mday == 1);
    Require(tmW0.tm_yday == 0);
    Require(tmW0.tm_wday == 4);

    GuestTm tmStandalone{};
    rem = strptime_nid_postfix("53", "%U", &tmStandalone);
    Require(rem != nullptr && *rem == '\0');
    rem = strptime_nid_postfix("53", "%W", &tmStandalone);
    Require(rem != nullptr && *rem == '\0');

    GuestTm tmYearZero{};
    rem = strptime_nid_postfix("0000-01-0", "%Y-%U-%w", &tmYearZero);
    Require(rem != nullptr && *rem == '\0');
    Require(tmYearZero.tm_year == -1900);
    Require(tmYearZero.tm_mon == 0);
    Require(tmYearZero.tm_mday == 2);
    Require(tmYearZero.tm_yday == 1);
    Require(tmYearZero.tm_wday == 0);

    GuestTm tmYearZeroWeek0{};
    rem = strptime_nid_postfix("0000-00-6", "%Y-%U-%w", &tmYearZeroWeek0);
    Require(rem != nullptr && *rem == '\0');
    Require(tmYearZeroWeek0.tm_year == -1900);
    Require(tmYearZeroWeek0.tm_mon == 0);
    Require(tmYearZeroWeek0.tm_mday == 1);
    Require(tmYearZeroWeek0.tm_yday == 0);
    Require(tmYearZeroWeek0.tm_wday == 6);

    Require(strptime_nid_postfix("2023-00-0", "%Y-%U-%w", &tmU) == nullptr);
    Require(strptime_nid_postfix("2026-00-1", "%Y-%U-%w", &tmU) == nullptr);
    Require(strptime_nid_postfix("2024-00-1", "%Y-%W-%u", &tmW) == nullptr);
    Require(strptime_nid_postfix("54", "%U", &tmU) == nullptr);
    Require(strptime_nid_postfix("54", "%W", &tmW) == nullptr);
}

static void TestIsoWeekDate() {
    GuestTm tmPrevYear{};
    const char* rem = strptime_nid_postfix("2026-W01-1", "%G-W%V-%u", &tmPrevYear);
    Require(rem != nullptr && *rem == '\0');
    Require(tmPrevYear.tm_year == 125);
    Require(tmPrevYear.tm_mon == 11);
    Require(tmPrevYear.tm_mday == 29);
    Require(tmPrevYear.tm_yday == 362);
    Require(tmPrevYear.tm_wday == 1);

    GuestTm tmSameYear{};
    rem = strptime_nid_postfix("26-W01-4", "%g-W%V-%u", &tmSameYear);
    Require(rem != nullptr && *rem == '\0');
    Require(tmSameYear.tm_year == 126);
    Require(tmSameYear.tm_mon == 0);
    Require(tmSameYear.tm_mday == 1);
    Require(tmSameYear.tm_yday == 0);
    Require(tmSameYear.tm_wday == 4);

    GuestTm tmNextYear{};
    rem = strptime_nid_postfix("2020-W53-5", "%G-W%V-%u", &tmNextYear);
    Require(rem != nullptr && *rem == '\0');
    Require(tmNextYear.tm_year == 121);
    Require(tmNextYear.tm_mon == 0);
    Require(tmNextYear.tm_mday == 1);
    Require(tmNextYear.tm_yday == 0);
    Require(tmNextYear.tm_wday == 5);

    GuestTm tmCentury{};
    rem = strptime_nid_postfix("19 85-W01-1", "%C %g-W%V-%u", &tmCentury);
    Require(rem != nullptr && *rem == '\0');
    Require(tmCentury.tm_year == 84);
    Require(tmCentury.tm_mon == 11);
    Require(tmCentury.tm_mday == 31);
    Require(tmCentury.tm_wday == 1);

    GuestTm tmDefaultWday{};
    rem = strptime_nid_postfix("2026-W02", "%G-W%V", &tmDefaultWday);
    Require(rem != nullptr && *rem == '\0');
    Require(tmDefaultWday.tm_year == 126);
    Require(tmDefaultWday.tm_mon == 0);
    Require(tmDefaultWday.tm_mday == 5);
    Require(tmDefaultWday.tm_wday == 1);

    GuestTm tmStandalone{};
    rem = strptime_nid_postfix("2026 99 12", "%G %g %V", &tmStandalone);
    Require(rem != nullptr && *rem == '\0');

    GuestTm tmYearZeroIso{};
    rem = strptime_nid_postfix("0000-W01-1", "%G-W%V-%u", &tmYearZeroIso);
    Require(rem != nullptr && *rem == '\0');
    Require(tmYearZeroIso.tm_year == -1900);
    Require(tmYearZeroIso.tm_mon == 0);
    Require(tmYearZeroIso.tm_mday == 3);
    Require(tmYearZeroIso.tm_yday == 2);
    Require(tmYearZeroIso.tm_wday == 1);

    Require(strptime_nid_postfix("2026-W00-1", "%G-W%V-%u", &tmPrevYear) == nullptr);
    Require(strptime_nid_postfix("2026-W54-1", "%G-W%V-%u", &tmPrevYear) == nullptr);
}

static void TestEpochSeconds() {
    const std::int64_t samples[] = {0, 1700000000, -86400};
    for (std::int64_t epoch : samples) {
        GuestTm expected{};
        Require(localtime_r_nid_postfix(&epoch, &expected) != nullptr);

        char input[32]{};
        std::snprintf(input, sizeof(input), "%lld", static_cast<long long>(epoch));

        GuestTm tm{};
        const char* rem = strptime_nid_postfix(input, "%s", &tm);
        Require(rem != nullptr && *rem == '\0');
        Require(tm.tm_year == expected.tm_year);
        Require(tm.tm_mon == expected.tm_mon);
        Require(tm.tm_mday == expected.tm_mday);
        Require(tm.tm_hour == expected.tm_hour);
        Require(tm.tm_min == expected.tm_min);
        Require(tm.tm_sec == expected.tm_sec);
        Require(tm.tm_wday == expected.tm_wday);
        Require(tm.tm_yday == expected.tm_yday);
        Require(tm.tm_isdst == expected.tm_isdst);
        Require(tm.tm_gmtoff == expected.tm_gmtoff);
    }

    GuestTm tmOverride{};
    const char* rem = strptime_nid_postfix("1700000000 04", "%s %H", &tmOverride);
    Require(rem != nullptr && *rem == '\0');
    Require(tmOverride.tm_hour == 4);

    GuestTm tmEpochDate{};
    rem = strptime_nid_postfix("0 2024-06-15", "%s %F", &tmEpochDate);
    Require(rem != nullptr && *rem == '\0');
    Require(tmEpochDate.tm_year == 124);
    Require(tmEpochDate.tm_mon == 5);
    Require(tmEpochDate.tm_mday == 15);
    Require(tmEpochDate.tm_wday == 6);
    Require(tmEpochDate.tm_yday == 166);

    GuestTm tmEpochExplicitWday{};
    rem = strptime_nid_postfix("0 2024-01-01 3", "%s %F %w", &tmEpochExplicitWday);
    Require(rem != nullptr && *rem == '\0');
    Require(tmEpochExplicitWday.tm_year == 124);
    Require(tmEpochExplicitWday.tm_wday == 3);
    Require(tmEpochExplicitWday.tm_yday == 0);

    GuestTm tmEpochExplicitYday{};
    rem = strptime_nid_postfix("0 2024-06-15 100", "%s %F %j", &tmEpochExplicitYday);
    Require(rem != nullptr && *rem == '\0');
    Require(tmEpochExplicitYday.tm_year == 124);
    Require(tmEpochExplicitYday.tm_wday == 6);
    Require(tmEpochExplicitYday.tm_yday == 99);

    GuestTm tmEpochIsoWeek{};
    rem = strptime_nid_postfix("0 2024-W02-1", "%s %G-W%V-%u", &tmEpochIsoWeek);
    Require(rem != nullptr && *rem == '\0');
    Require(tmEpochIsoWeek.tm_year == 124);
    Require(tmEpochIsoWeek.tm_mon == 0);
    Require(tmEpochIsoWeek.tm_mday == 8);
    Require(tmEpochIsoWeek.tm_wday == 1);
    Require(tmEpochIsoWeek.tm_yday == 7);

    GuestTm tmBad{};
    Require(strptime_nid_postfix("99999999999999999999", "%s", &tmBad) == nullptr);
    Require(strptime_nid_postfix("abc", "%s", &tmBad) == nullptr);
}

int main() {
    TestIsoDateTime();
    TestMonthAndWeekdayNames();
    TestTwelveHourAmPm();
    TestTwoDigitYearAndCentury();
    TestCompoundFormats();
    TestTimezone();
    TestWhitespaceAndRemainder();
    TestModifiersAndEscapes();
    TestCompactDate();
    TestSpacePaddedHours();
    TestWeekNumbers();
    TestIsoWeekDate();
    TestEpochSeconds();
    TestInvalidAndNullInputs();
    return 0;
}
