#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <initializer_list>
#include <limits>

extern "C" {
int APS5_VABI sceRtcCheckValid(const RtcDateTime*);
int APS5_VABI sceRtcIsLeapYear(int);
int APS5_VABI sceRtcGetDaysInMonth(int, int);
int APS5_VABI sceRtcGetDayOfWeek(int, int, int);
int APS5_VABI sceRtcGetTickResolution(void);
int APS5_VABI sceRtcGetTick(const RtcDateTime*, RtcTick*);
int APS5_VABI sceRtcSetTick(RtcDateTime*, const RtcTick*);
int APS5_VABI sceRtcGetCurrentTick(RtcTick*);
int APS5_VABI sceRtcConvertUtcToLocalTime(const RtcTick*, RtcTick*);
int APS5_VABI sceRtcConvertLocalTimeToUtc(const RtcTick*, RtcTick*);
int APS5_VABI sceRtcGetTime_t(const RtcDateTime*, std::int64_t*);
int APS5_VABI sceRtcSetTime_t(RtcDateTime*, std::int64_t);
int APS5_VABI sceRtcGetWin32FileTime(const RtcDateTime*, std::uint64_t*);
int APS5_VABI sceRtcSetWin32FileTime(RtcDateTime*, std::uint64_t);
int APS5_VABI sceRtcGetDosTime(const RtcDateTime*, std::uint32_t*);
int APS5_VABI sceRtcSetDosTime(RtcDateTime*, std::uint32_t);
int APS5_VABI sceRtcFormatRFC3339(char*, const RtcTick*, int);
int APS5_VABI sceRtcParseRFC3339(RtcTick*, const char*);
int APS5_VABI sceRtcParseDateTime(RtcTick*, const char*);
int APS5_VABI sceRtcTickAddTicks(RtcTick*, const RtcTick*, std::int64_t);
int APS5_VABI sceRtcTickAddSeconds(RtcTick*, const RtcTick*, std::int64_t);
int APS5_VABI sceRtcTickAddDays(RtcTick*, const RtcTick*, std::int32_t);
int APS5_VABI sceRtcTickAddMonths(RtcTick*, const RtcTick*, std::int32_t);
int APS5_VABI sceRtcTickAddYears(RtcTick*, const RtcTick*, std::int16_t);
int APS5_VABI sceRtcFormatRFC3339LocalTime(char*, const RtcTick*);
int APS5_VABI sceRtcFormatRFC2822(char*, const RtcTick*, int);
int APS5_VABI sceRtcFormatRFC2822LocalTime(char*, const RtcTick*);
}

static void Require(bool value) { if (!value) std::abort(); }

static void SetTimeZone(const char* zone) {
#ifdef _WIN32
    _putenv_s("TZ", zone);
    _tzset();
#else
    setenv("TZ", zone, 1);
    tzset();
#endif
}

static bool Equal(const RtcDateTime& left, const RtcDateTime& right) {
    return left.year == right.year && left.month == right.month && left.day == right.day && left.hour == right.hour
        && left.minute == right.minute && left.second == right.second && left.microsecond == right.microsecond;
}

int main() {
    constexpr int invalidPointer = static_cast<int>(0x80B50002);
    constexpr int invalidValue = static_cast<int>(0x80B50003);
    constexpr int badParse = static_cast<int>(0x80B50007);
    constexpr int invalidYear = static_cast<int>(0x80B50008);
    constexpr int invalidMonth = static_cast<int>(0x80B50009);
    constexpr int invalidDay = static_cast<int>(0x80B5000A);
    constexpr int invalidHour = static_cast<int>(0x80B5000B);
    constexpr int invalidMicrosecond = static_cast<int>(0x80B5000E);
    constexpr std::uint64_t unixEpochTick = 62135596800000000ull;
    constexpr std::uint64_t leapDayTick = 63844806896789000ull;
    constexpr std::uint64_t maxTick = 315537897599999999ull;

    Require(sceRtcGetTickResolution() == 1000000);
    Require(sceRtcIsLeapYear(2000) == 1 && sceRtcIsLeapYear(1900) == 0 && sceRtcIsLeapYear(2024) == 1);
    Require(sceRtcIsLeapYear(0) == invalidYear);
    Require(sceRtcGetDaysInMonth(2023, 2) == 28 && sceRtcGetDaysInMonth(2024, 2) == 29 && sceRtcGetDaysInMonth(2024, 4) == 30);
    Require(sceRtcGetDaysInMonth(2024, 13) == invalidMonth);
    Require(sceRtcGetDayOfWeek(1, 1, 1) == 1 && sceRtcGetDayOfWeek(2026, 9, 26) == 6);
    Require(sceRtcGetDayOfWeek(2023, 2, 29) == invalidDay);

    RtcDateTime leapDay{2024, 2, 29, 12, 34, 56, 789000};
    Require(sceRtcCheckValid(&leapDay) == 0);
    Require(sceRtcCheckValid(nullptr) == invalidPointer);
    RtcDateTime invalid = leapDay;
    invalid.year = 2023;
    Require(sceRtcCheckValid(&invalid) == invalidDay);
    invalid = leapDay;
    invalid.hour = 24;
    Require(sceRtcCheckValid(&invalid) == invalidHour);
    invalid = leapDay;
    invalid.microsecond = 1000000;
    Require(sceRtcCheckValid(&invalid) == invalidMicrosecond);

    RtcTick tick{};
    Require(sceRtcGetTick(&leapDay, &tick) == 0 && tick.tick == leapDayTick);
    RtcDateTime converted{};
    Require(sceRtcSetTick(&converted, &tick) == 0 && Equal(converted, leapDay));
    tick.tick = maxTick;
    Require(sceRtcSetTick(&converted, &tick) == 0 && Equal(converted, RtcDateTime{9999, 12, 31, 23, 59, 59, 999999}));
    tick.tick = maxTick + 1;
    Require(sceRtcSetTick(&converted, &tick) == invalidValue);

    RtcDateTime epoch{1970, 1, 1, 0, 0, 0, 0};
    std::int64_t seconds = -1;
    Require(sceRtcGetTime_t(&epoch, &seconds) == 0 && seconds == 0);
    RtcDateTime millennium{2000, 1, 1, 0, 0, 0, 0};
    Require(sceRtcGetTime_t(&millennium, &seconds) == 0 && seconds == 946684800);
    Require(sceRtcSetTime_t(&converted, 946684800) == 0 && Equal(converted, millennium));
    Require(sceRtcSetTime_t(&converted, -1) == invalidValue);
    std::uint64_t fileTime = 0;
    Require(sceRtcGetWin32FileTime(&epoch, &fileTime) == 0 && fileTime == 116444736000000000ull);
    Require(sceRtcSetWin32FileTime(&converted, 116444736000000000ull) == 0 && Equal(converted, epoch));
    std::uint32_t dosTime = 0xffffffffu;
    const RtcDateTime dosDate{2024, 2, 29, 12, 34, 57, 789000};
    Require(sceRtcGetDosTime(&dosDate, &dosTime) == 0 && dosTime == 0x585d645cu);
    const RtcDateTime dosFirst{1980, 1, 1, 0, 0, 0, 0};
    Require(sceRtcGetDosTime(&dosFirst, &dosTime) == 0 && dosTime == 0x00210000u);
    const RtcDateTime dosLast{2107, 12, 31, 23, 59, 59, 0};
    Require(sceRtcGetDosTime(&dosLast, &dosTime) == 0 && dosTime == 0xff9fbf7du);
    Require(sceRtcGetDosTime(&dosDate, nullptr) == invalidPointer);
    Require(sceRtcGetDosTime(nullptr, &dosTime) == invalidPointer);
    const RtcDateTime dosBadMonth{2024, 13, 1, 0, 0, 0, 0};
    Require(sceRtcGetDosTime(&dosBadMonth, &dosTime) == invalidMonth);
    const RtcDateTime dosEarly{1979, 12, 31, 0, 0, 0, 0};
    bool dosEarlyThrew = false;
    try { sceRtcGetDosTime(&dosEarly, &dosTime); } catch (const std::exception&) { dosEarlyThrew = true; }
    Require(dosEarlyThrew);
    converted = RtcDateTime{1, 1, 1, 1, 1, 1, 1};
    Require(sceRtcSetDosTime(&converted, 0x585d645cu) == 0 && Equal(converted, RtcDateTime{2024, 2, 29, 12, 34, 56, 0}));
    Require(sceRtcSetDosTime(&converted, 0x7f9fbf7du) == 0 && Equal(converted, RtcDateTime{2043, 12, 31, 23, 59, 58, 0}));
    Require(sceRtcSetDosTime(&converted, 0) == 0 && Equal(converted, RtcDateTime{1980, 0, 0, 0, 0, 0, 0}));
    Require(sceRtcSetDosTime(nullptr, 0) == invalidPointer);
    Require(sceRtcSetDosTime(&converted, 0xff9fbf7du) == 0 && Equal(converted, RtcDateTime{2107, 12, 31, 23, 59, 58, 0}));
    Require(sceRtcSetDosTime(&converted, 0x80210000u) == 0 && Equal(converted, RtcDateTime{2044, 1, 1, 0, 0, 0, 0}));

    char text[32];
    tick.tick = leapDayTick;
    Require(sceRtcFormatRFC3339(text, &tick, 0) == 0 && std::strcmp(text, "2024-02-29T12:34:56.78Z") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, 90) == 0 && std::strcmp(text, "2024-02-29T14:04:56.78+01:30") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, -300) == 0 && std::strcmp(text, "2024-02-29T07:34:56.78-05:00") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, 1439) == 0 && std::strcmp(text, "2024-03-01T12:33:56.78+23:59") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, -1439) == 0 && std::strcmp(text, "2024-02-28T12:35:56.78-23:59") == 0);
    for (int offset : {0, 1, -1, 59, -59, 60, -60, 90, -300, 1439, -1439}) {
        RtcTick parsed{};
        Require(sceRtcFormatRFC3339(text, &tick, offset) == 0);
        Require(sceRtcParseRFC3339(&parsed, text) == 0 && parsed.tick == leapDayTick - 9000);
    }
    for (int offset : {1440, -1440, 6000, -6000, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        std::memset(text, 'x', sizeof(text));
        char original[sizeof(text)];
        std::memcpy(original, text, sizeof(text));
        Require(sceRtcFormatRFC3339(text, &tick, offset) == invalidValue);
        Require(std::memcmp(text, original, sizeof(text)) == 0);
    }
    Require(sceRtcFormatRFC3339(nullptr, &tick, 1440) == invalidPointer);
    Require(sceRtcFormatRFC3339(text, nullptr, 1440) == invalidPointer);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T14:04:56.789+01:30") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29t12:34:56.789z") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "1970-01-01T00:00:00Z") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseRFC3339(&tick, "2023-02-29T00:00:00Z") == invalidDay);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56") == badParse);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56Zjunk") == badParse);
    const char* invalidOffsets[] = {
        "2024-02-29T12:34:56.789+00:99",
        "2024-02-29T12:34:56.789-00:99",
        "2024-02-29T12:34:56.789+00:60",
        "2024-02-29T12:34:56.789-00:60",
        "2024-02-29T12:34:56.789+24:00",
        "2024-02-29T12:34:56.789-24:00",
        "2024-02-29T12:34:56.789+99:59",
        "2024-02-29T12:34:56.789-99:59",
    };
    for (const char* text : invalidOffsets) {
        tick.tick = 123;
        Require(sceRtcParseRFC3339(&tick, text) == badParse);
        Require(tick.tick == 123);
    }
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789+00:00") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789-00:00") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789+00:59") == 0 && tick.tick == leapDayTick - 3540000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789-00:59") == 0 && tick.tick == leapDayTick + 3540000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789+23:59") == 0 && tick.tick == leapDayTick - 86340000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789-23:59") == 0 && tick.tick == leapDayTick + 86340000000ull);
    Require(sceRtcParseRFC3339(nullptr, "1970-01-01T00:00:00Z") == invalidPointer);

    Require(sceRtcParseDateTime(&tick, "2024-02-29T12:34:56.789") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "2024-02-29 12:34:56") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "2024-02-29T14:04:56.789+01:30") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "2024-02-29t12:34:56.789z") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "1970-01-01T00:00:00Z") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseDateTime(&tick, "2023-02-29T00:00:00") == invalidDay);
    Require(sceRtcParseDateTime(nullptr, "1970-01-01T00:00:00Z") == invalidPointer);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 12:34:56") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 12:34:56 GMT") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 14:04:56 +0130") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 11:04:56 -0130") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 01 Jan 1970 00:00:00 +0000") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseDateTime(&tick, "Thu, 30 Feb 2024 00:00:00") == invalidDay);
    Require(sceRtcParseDateTime(&tick, "Thu Feb 29 12:34:56 2024") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu Jan  1 00:00:00 1970") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseDateTime(&tick, "Thu Feb 30 00:00:00 2024") == invalidDay);
    const char* unparseable[] = {
        "2024-02-29",
        "2024/02/29T12:34:56",
        "2024-02-29T12:34:56Zjunk",
        "2024-02-29T12:34:56+99:99",
        "2024-02-29T12:34:56.",
        "Thu, 29 Feb 2024 12:34:56 +01:30",
        "Thu, 29 Feb 2024 12:34:56 PST",
        "Thu, 29 Xyz 2024 12:34:56",
        "Xyz, 29 Feb 2024 12:34:56",
        "Thu, 29 Feb 2024 12:34:56 +2400",
        "Thu Feb 29 12:34:56 2024\n",
        "Thu Feb 29 12:34:56 24",
        "Thu Feb  29 12:34:56 2024",
    };
    for (const char* text : unparseable) {
        bool thrown = false;
        try {
            sceRtcParseDateTime(&tick, text);
        } catch (const std::exception&) {
            thrown = true;
        }
        Require(thrown);
    }

    RtcTick source{leapDayTick};
    RtcTick result{};
    Require(sceRtcTickAddSeconds(&result, &source, 3600) == 0 && result.tick == leapDayTick + 3600000000ull);
    Require(sceRtcTickAddDays(&result, &source, 1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && converted.month == 3 && converted.day == 1);
    Require(sceRtcTickAddYears(&result, &source, 1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && converted.year == 2025 && converted.month == 2 && converted.day == 28);
    RtcDateTime endOfJanuary{2024, 1, 31, 8, 0, 0, 0};
    Require(sceRtcGetTick(&endOfJanuary, &source) == 0);
    Require(sceRtcTickAddMonths(&result, &source, 1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && Equal(converted, RtcDateTime{2024, 2, 29, 8, 0, 0, 0}));
    Require(sceRtcTickAddMonths(&result, &source, -12 * 2024) == invalidValue);
    source.tick = 5;
    Require(sceRtcTickAddTicks(&result, &source, -6) == invalidValue);
    Require(sceRtcTickAddTicks(&result, &source, -5) == 0 && result.tick == 0);
    source.tick = maxTick;
    Require(sceRtcTickAddTicks(&result, &source, 1) == invalidValue);
    Require(sceRtcTickAddTicks(nullptr, &source, 1) == invalidPointer);

    const std::uint64_t invalidTicks[] = {maxTick + 1, std::numeric_limits<std::uint64_t>::max()};
    for (std::uint64_t invalidTick : invalidTicks) {
        source.tick = invalidTick;
        result.tick = 123;
        Require(sceRtcTickAddTicks(&result, &source, 0) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddTicks(&result, &source, 1) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddTicks(&result, &source, -1) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddSeconds(&result, &source, 0) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddDays(&result, &source, -1) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddMonths(&result, &source, 0) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddYears(&result, &source, -1) == invalidValue && result.tick == 123);
        Require(sceRtcTickAddTicks(&source, &source, 0) == invalidValue && source.tick == invalidTick);
        Require(sceRtcTickAddMonths(&source, &source, -1) == invalidValue && source.tick == invalidTick);
        Require(sceRtcTickAddTicks(nullptr, &source, 0) == invalidPointer);
        Require(sceRtcTickAddMonths(nullptr, &source, 0) == invalidPointer);
    }

    source.tick = maxTick;
    Require(sceRtcTickAddTicks(&source, &source, 0) == 0 && source.tick == maxTick);
    Require(sceRtcTickAddTicks(&source, &source, -1) == 0 && source.tick == maxTick - 1);
    source.tick = maxTick;
    Require(sceRtcTickAddMonths(&source, &source, 0) == 0 && source.tick == maxTick);
    Require(sceRtcTickAddYears(&result, &source, -1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && Equal(converted, RtcDateTime{9998, 12, 31, 23, 59, 59, 999999}));
    result.tick = 123;
    Require(sceRtcTickAddMonths(&result, &source, 1) == invalidValue && result.tick == 123);
    Require(sceRtcTickAddTicks(&result, &source, std::numeric_limits<std::int64_t>::min()) == invalidValue && result.tick == 123);
    Require(sceRtcTickAddSeconds(&result, &source, std::numeric_limits<std::int64_t>::max()) == invalidValue && result.tick == 123);
    source.tick = 0;
    Require(sceRtcTickAddMonths(&source, &source, 0) == 0 && source.tick == 0);
    Require(sceRtcTickAddMonths(&result, &source, -1) == invalidValue && result.tick == 123);

    RtcTick now{};
    Require(sceRtcGetCurrentTick(&now) == 0 && now.tick > leapDayTick);
    RtcTick local{};
    RtcTick back{};
    Require(sceRtcConvertUtcToLocalTime(&now, &local) == 0);
    Require(sceRtcConvertLocalTimeToUtc(&local, &back) == 0 && back.tick == now.tick);

    char rfc2822[32];
    tick.tick = leapDayTick;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 12:34:56 +0000") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 90) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 14:04:56 +0130") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -300) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 07:34:56 -0500") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -30) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 12:04:56 -0030") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 65) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 13:39:56 +0105") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 1439) == 0 && std::strcmp(rfc2822, "Fri, 01 Mar 2024 12:33:56 +2359") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -1439) == 0 && std::strcmp(rfc2822, "Wed, 28 Feb 2024 12:35:56 -2359") == 0);
    const char* week[] = {"Sun, 03 Mar", "Mon, 04 Mar", "Tue, 05 Mar", "Wed, 06 Mar", "Thu, 07 Mar", "Fri, 08 Mar", "Sat, 09 Mar"};
    for (int day = 0; day < 7; ++day) {
        const RtcDateTime date{2024, 3, static_cast<std::uint16_t>(3 + day), 0, 0, 0, 0};
        Require(sceRtcGetTick(&date, &tick) == 0 && sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0);
        Require(std::strncmp(rfc2822, week[day], std::strlen(week[day])) == 0);
    }
    tick.tick = 0;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0 && std::strcmp(rfc2822, "Mon, 01 Jan 0001 00:00:00 +0000") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -1) == invalidValue);
    tick.tick = maxTick;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0 && std::strcmp(rfc2822, "Fri, 31 Dec 9999 23:59:59 +0000") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 1) == invalidValue);
    tick.tick = leapDayTick;
    for (int offset : {1440, -1440, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        std::memset(rfc2822, 'x', sizeof(rfc2822));
        char original[sizeof(rfc2822)];
        std::memcpy(original, rfc2822, sizeof(rfc2822));
        Require(sceRtcFormatRFC2822(rfc2822, &tick, offset) == invalidValue);
        Require(std::memcmp(rfc2822, original, sizeof(rfc2822)) == 0);
    }
    Require(sceRtcFormatRFC2822(nullptr, &tick, 0) == invalidPointer);
    Require(sceRtcFormatRFC2822(rfc2822, nullptr, 0) == invalidPointer);

    SetTimeZone("XXX-5:30");
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 18:04:56 +0530") == 0);
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "2024-02-29T18:04:56.78+05:30") == 0);
    SetTimeZone("XXX+3");
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 09:34:56 -0300") == 0);
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "2024-02-29T09:34:56.78-03:00") == 0);
    SetTimeZone("UTC0");
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 12:34:56 +0000") == 0);
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "2024-02-29T12:34:56.78Z") == 0);
    Require(sceRtcFormatRFC2822LocalTime(nullptr, &tick) == invalidPointer);
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, nullptr) == invalidPointer);
    Require(sceRtcFormatRFC3339LocalTime(nullptr, &tick) == invalidPointer);
    Require(sceRtcFormatRFC3339LocalTime(text, nullptr) == invalidPointer);
}
