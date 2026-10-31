#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using Converter = std::int64_t (APS5_VABI *)(GuestTm*);

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static bool Equal(const GuestTm& left, const GuestTm& right) {
    return left.tm_sec == right.tm_sec && left.tm_min == right.tm_min && left.tm_hour == right.tm_hour
        && left.tm_mday == right.tm_mday && left.tm_mon == right.tm_mon && left.tm_year == right.tm_year
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst
        && left.tm_gmtoff == right.tm_gmtoff && left.tm_zone != nullptr && right.tm_zone != nullptr
        && std::strcmp(left.tm_zone, right.tm_zone) == 0;
}

static void CheckNormalization(Converter convert, std::int64_t timer) {
    GuestTm expected{};
    Require(localtime_s_nid_postfix(&timer, &expected) == &expected, "Local conversion failed");
    GuestTm input = expected;
    input.tm_sec += 60;
    --input.tm_min;
    input.tm_wday = -1;
    input.tm_yday = -1;
    input.tm_isdst = -1;
    input.tm_gmtoff = -1;
    input.tm_zone = nullptr;
    errno = EOVERFLOW;
    Require(convert(&input) == timer, "mktime returned the wrong timestamp");
    Require(Equal(input, expected), "mktime did not normalize a valid timestamp");
}

static void CheckUnavailableDaylightHint(Converter convert, int year) {
    for (const int month : {0, 6}) {
        GuestTm canonical{};
        canonical.tm_year = year;
        canonical.tm_mon = month;
        canonical.tm_mday = 15;
        canonical.tm_hour = 12;
        const auto expected = convert(&canonical);
        Require(expected != -1, "standard calendar conversion failed");
        for (const int hint : {1, 2, std::numeric_limits<int>::max()}) {
            GuestTm input = canonical;
            input.tm_isdst = hint;
            *__error_nid_postfix() = 7;
            Require(convert(&input) == expected, "unavailable daylight hint changed the timestamp");
            Require(Equal(input, canonical), "unavailable daylight hint changed normalized fields");
            Require(*__error_nid_postfix() == 7, "daylight lookup changed errno");
        }
    }
}

static void CheckAvailableDaylightHints(const char* context = "host-state") {
    Require(setenv_nid_postfix("TZ", "EST5EDT", 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();
    constexpr std::array<Converter, 2> converters{libc_mktime_nid_postfix, mktime_nid_postfix};
    const struct { int month; int hint; std::int64_t expected; int hour; int daylight; } cases[]{
        {0, 1, 1705334400, 11, 0},
        {6, 0, 1721062800, 13, 1},
        {6, 1, 1721059200, 12, 1}
    };
    int mismatches = 0;
    for (auto convert : converters) {
        for (const auto& item : cases) {
            GuestTm input{};
            input.tm_year = 124;
            input.tm_mon = item.month;
            input.tm_mday = 15;
            input.tm_hour = 12;
            input.tm_isdst = item.hint;
            const auto actual = convert(&input);
            long standardBias = 0;
            long daylightBias = 0;
#ifdef _WIN32
            _get_timezone(&standardBias);
            _get_dstbias(&daylightBias);
#endif
            std::fprintf(stderr, "available-hint context=%s converter=%s month=%d hint=%d actual=%lld expected=%lld hour=%d expectedHour=%d isdst=%d expectedIsdst=%d standardBias=%ld daylightBias=%ld\n",
                         context, convert == libc_mktime_nid_postfix ? "libc_mktime" : "mktime", item.month, item.hint,
                         static_cast<long long>(actual), static_cast<long long>(item.expected), input.tm_hour, item.hour,
                         input.tm_isdst, item.daylight, standardBias, daylightBias);
            if (actual != item.expected || input.tm_hour != item.hour || input.tm_isdst != item.daylight) ++mismatches;
        }
    }
    Require(mismatches == 0, "available daylight hint cases differ; see named records");
}

#ifndef _WIN32
static void CheckHistoricalDaylightAbsence(const char* zone, int year) {
    Require(setenv_nid_postfix("TZ", zone, 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();
    CheckUnavailableDaylightHint(libc_mktime_nid_postfix, year);
    CheckUnavailableDaylightHint(mktime_nid_postfix, year);
}

#endif

static void CheckRepresentableYearEdges(Converter convert) {
    const struct { int year; int month; } cases[]{
        {std::numeric_limits<int>::min(), 0},
        {std::numeric_limits<int>::max(), 11}
    };
    for (const auto& item : cases) {
        GuestTm input{};
        input.tm_year = item.year;
        input.tm_mon = item.month;
        input.tm_mday = 15;
        input.tm_hour = 12;
        input.tm_isdst = 1;
        Require(convert(&input) != -1, "daylight hint at a representable year edge failed");
    }
}

static void CheckFailure(Converter convert, int hint = -1) {
    GuestTm input{};
    input.tm_year = std::numeric_limits<int>::max();
    input.tm_mon = std::numeric_limits<int>::max();
    input.tm_mday = 1;
    input.tm_wday = 4;
    input.tm_yday = 12;
    input.tm_isdst = hint;
    input.tm_gmtoff = 123;
    input.tm_zone = "unchanged";
    const GuestTm original = input;
    Require(convert(&input) == -1, "mktime accepted an unrepresentable calendar");
    Require(Equal(input, original), "Failed mktime changed the guest calendar");
}

static void CheckEarlyYearNormalization(Converter convert) {
    const struct {
        int year;
        int mon;
        int mday;
        int normalizedMon;
        int normalizedMday;
    } inputs[] = {{68, 24, 13, 0, 13}, {68, 25, 1, 1, 1}, {69, 11, 45, 0, 14}, {67, 36, 2, 0, 2}};
    for (const auto& item : inputs) {
        GuestTm canonical{};
        canonical.tm_year = item.year;
        canonical.tm_mon = item.mon;
        canonical.tm_mday = item.mday;
        canonical.tm_hour = 12;
        canonical.tm_isdst = -1;
        GuestTm input = canonical;
        const std::int64_t timer = convert(&input);
        Require(timer != -1, "mktime refused a calendar that normalizes into 1970");
        GuestTm expected{};
        Require(localtime_s_nid_postfix(&timer, &expected) == &expected, "Local conversion failed");
        Require(Equal(input, expected), "mktime did not normalize an early-year calendar");
        Require(input.tm_year == 70 && input.tm_mon == item.normalizedMon && input.tm_mday == item.normalizedMday && input.tm_hour == 12,
            "mktime normalized to the wrong date");
    }
}

static void CheckEquivalentFutureCalendars(const char* zone) {
    Require(setenv_nid_postfix("TZ", zone, 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();
    constexpr std::array<Converter, 2> converters{libc_mktime_nid_postfix, mktime_nid_postfix};
    for (auto convert : converters) {
        for (const int year : {420, 490, 1620}) {
            GuestTm canonical{};
            canonical.tm_year = year;
            canonical.tm_mon = 9;
            canonical.tm_mday = 22;
            canonical.tm_hour = 18;
            canonical.tm_min = 22;
            canonical.tm_sec = 37;
            canonical.tm_isdst = -1;
            GuestTm nonordinary = canonical;
            nonordinary.tm_mon = 21;
            nonordinary.tm_mday = -343;
            const auto expected = convert(&canonical);
            Require(expected != -1, "mktime refused a representable future calendar");
            Require(convert(&nonordinary) == expected, "equivalent future calendars used different time-zone rules");
            Require(Equal(canonical, nonordinary), "equivalent future calendars normalized to different fields");
        }
    }
}

static void CheckZone(const char* zone) {
    Require(setenv_nid_postfix("TZ", zone, 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();

    constexpr std::array<Converter, 2> converters{libc_mktime_nid_postfix, mktime_nid_postfix};
    constexpr std::array<std::int64_t, 8> timers{-86401, -2, -1, 0, 1, 59, 60, 86400};
    for (auto convert : converters) {
        for (const auto timer : timers) {
            CheckNormalization(convert, timer);
        }
        CheckEarlyYearNormalization(convert);
        CheckFailure(convert);
        CheckFailure(convert, 1);
        CheckRepresentableYearEdges(convert);
        for (const int year : {40, 70, 124}) CheckUnavailableDaylightHint(convert, year);
    }
}

int main() {
    CheckZone("IST-5:30");
    CheckAvailableDaylightHints();
#ifdef _WIN32
    Require(setenv_nid_postfix("TZ", "UTC0", 1) == 0, "Could not select the UTC zone");
    tzset_nid_postfix();
    long savedBias = 0;
    _get_dstbias(&savedBias);
    _dstbias = 0;
    CheckAvailableDaylightHints("zero-host-daylight-bias");
    _dstbias = savedBias;
#endif
#ifndef _WIN32
    CheckHistoricalDaylightAbsence("Australia/Lord_Howe", 40);
    CheckHistoricalDaylightAbsence("Europe/Moscow", 124);
#endif
    CheckZone("UTC0");
    CheckZone("UTC-2");
    CheckZone("UTC5");
    CheckEquivalentFutureCalendars("EST5EDT");
#ifndef _WIN32
    CheckEquivalentFutureCalendars("America/New_York");
    CheckEquivalentFutureCalendars("Pacific/Kiritimati");
#endif
}
