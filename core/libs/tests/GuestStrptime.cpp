#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"

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

static void TestUnimplementedSpecifiers() {
    GuestTm tm{};
    const char* const formats[] = {"%U", "%W", "%s", "%G", "%g", "%V", "%k", "%l"};
    for (const char* fmt : formats) {
        bool threw = false;
        try {
            strptime_nid_postfix("123", fmt, &tm);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
    }
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
    TestUnimplementedSpecifiers();
    TestInvalidAndNullInputs();
    return 0;
}
