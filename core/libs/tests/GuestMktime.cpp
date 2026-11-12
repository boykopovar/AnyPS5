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
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst;
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
    const GuestTm original = input;
    GuestTm expected = original;
    GuestTm reference = original;
    if (convert == timelocal_nid_postfix) {
        expected.tm_isdst = -1;
        reference.tm_isdst = -1;
    }
    errno = EDOM;
    Require(libc_mktime_nid_postfix(&reference) == -1, "mktime reference accepted an unrepresentable calendar");
    const int expectedError = errno;
    errno = EDOM;
    Require(convert(&input) == -1, "mktime accepted an unrepresentable calendar");
    Require(Equal(input, expected), "Failed conversion changed fields beyond its daylight-hint rule");
    Require(errno == expectedError, "Failed conversion changed the underlying mktime error");
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

    constexpr std::array<Converter, 3> converters{libc_mktime_nid_postfix, mktime_nid_postfix, timelocal_nid_postfix};
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

constexpr std::size_t ConsoleTmSize = 36;

static GuestTm* FillGuarded(unsigned char (&bytes)[64]) {
    std::memset(bytes, 0xAA, sizeof(bytes));
    const int calendar[9] = {0, 0, 12, 23, 9, 126, 0, 0, 0};
    std::memcpy(bytes, calendar, sizeof(calendar));
    return reinterpret_cast<GuestTm*>(bytes);
}

static void RequireGuard(const unsigned char (&bytes)[64], const char* message) {
    for (std::size_t i = ConsoleTmSize; i < sizeof(bytes); ++i) Require(bytes[i] == 0xAA, message);
}

static void CheckWritesStayInside() {
    const std::int64_t timer = 1792756800;
    alignas(8) unsigned char bytes[64];
    for (auto convert : {libc_mktime_nid_postfix, mktime_nid_postfix, timegm_nid_postfix}) {
        Require(convert(FillGuarded(bytes)) != -1, "Conversion of a valid calendar failed");
        RequireGuard(bytes, "A calendar conversion wrote past the 36-byte struct tm");
    }
    for (auto split : {localtime_s_nid_postfix, gmtime_s_nid_postfix, localtime_r_nid_postfix, gmtime_r_nid_postfix}) {
        GuestTm* time = FillGuarded(bytes);
        Require(split(&timer, time) == time, "Splitting a valid timestamp failed");
        RequireGuard(bytes, "A timestamp split wrote past the 36-byte struct tm");
    }
    GuestTm* utc = FillGuarded(bytes);
    Require(gmtime_s_nid_postfix(&timer, utc) == utc && utc->tm_isdst == -1, "gmtime_s did not store tm_isdst -1");
    utc = FillGuarded(bytes);
    Require(gmtime_r_nid_postfix(&timer, utc) == utc && utc->tm_isdst == 0, "gmtime_r did not store tm_isdst 0");
    const GuestTm* shared = gmtime_nid_postfix(&timer);
    Require(shared != nullptr && shared->tm_isdst == 0, "gmtime did not store tm_isdst 0");
}

static void CheckGmtimeRoundTrip() {
    Require(setenv_nid_postfix("TZ", "CET-1CEST", 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();
    constexpr std::array<std::int64_t, 2> timers{1000000000, 951825600};
    for (const auto timer : timers) {
        GuestTm copy = *gmtime_nid_postfix(&timer);
        Require(mktime_nid_postfix(&copy) == timer - 3600, "mktime of a gmtime result was not read as standard time");
    }
}

static void CheckTimelocalDst() {
    Require(setenv_nid_postfix("TZ", "EST5EDT", 1) == 0, "Could not set EST5EDT zone");
    tzset_nid_postfix();

    constexpr std::int64_t summerTimer = 995198400;
    GuestTm summer{};
    Require(localtime_s_nid_postfix(&summerTimer, &summer) == &summer, "Local conversion failed");
    Require(summer.tm_isdst == 1, "Summer isdst was not 1 under EST5EDT");

    GuestTm forMktime = summer;
    forMktime.tm_isdst = 0;
    const std::int64_t mktimeResult = mktime_nid_postfix(&forMktime);

    GuestTm forTimelocal = summer;
    forTimelocal.tm_isdst = 0;
    const std::int64_t timelocalResult = timelocal_nid_postfix(&forTimelocal);

    Require(mktimeResult != -1 && timelocalResult != -1, "Conversion failed");
    Require(timelocalResult == summerTimer, "timelocal did not determine DST with tm_isdst=-1");
#ifndef _WIN32
    Require(mktimeResult == summerTimer + 3600, "mktime did not respect forced standard time tm_isdst=0");
#endif
    Require(forTimelocal.tm_isdst == 1, "timelocal normalized tm_isdst was not 1 in summer DST");

    GuestTm failure{};
    failure.tm_year = std::numeric_limits<int>::max();
    failure.tm_mon = std::numeric_limits<int>::max();
    failure.tm_mday = 1;
    failure.tm_wday = 4;
    failure.tm_yday = 12;
    failure.tm_isdst = 5;

    GuestTm expectedFailure = failure;
    expectedFailure.tm_isdst = -1;

    Require(timelocal_nid_postfix(&failure) == -1, "timelocal accepted unrepresentable date");
    Require(Equal(failure, expectedFailure), "timelocal failure modified fields other than tm_isdst");
}

static void CheckTimegm() {
    constexpr std::array<std::int64_t, 6> timers{0, 1, 86400, 946684800, 1577836800, 2000000000};
    for (const auto timer : timers) {
        GuestTm expected{};
        Require(gmtime_s_nid_postfix(&timer, &expected) == &expected, "gmtime conversion failed");
        GuestTm input = expected;
        expected.tm_isdst = 0;
        input.tm_sec += 60;
        --input.tm_min;
        input.tm_wday = -1;
        input.tm_yday = -1;
        Require(timegm_nid_postfix(&input) == timer, "timegm returned wrong timestamp");
        Require(Equal(input, expected), "timegm did not normalize UTC fields");
    }

    GuestTm leap2000{};
    leap2000.tm_year = 100;
    leap2000.tm_mon = 1;
    leap2000.tm_mday = 29;
    const std::int64_t leapTimer = timegm_nid_postfix(&leap2000);
    Require(leapTimer != -1, "timegm rejected leap day in year 2000");
    Require(leap2000.tm_year == 100 && leap2000.tm_mon == 1 && leap2000.tm_mday == 29, "timegm distorted leap day");

    GuestTm monthOverflow{};
    monthOverflow.tm_year = 121;
    monthOverflow.tm_mon = 12;
    monthOverflow.tm_mday = 5;
    const std::int64_t normTimer = timegm_nid_postfix(&monthOverflow);
    Require(normTimer != -1, "timegm rejected month overflow");
    Require(monthOverflow.tm_year == 122 && monthOverflow.tm_mon == 0 && monthOverflow.tm_mday == 5,
        "timegm failed to normalize month overflow");
}

int main() {
    CheckWritesStayInside();
    CheckGmtimeRoundTrip();
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
    CheckTimelocalDst();
    CheckTimegm();
}
