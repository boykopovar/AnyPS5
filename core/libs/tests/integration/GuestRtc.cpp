#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>

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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalidPointer = static_cast<int>(0x80B50002);
constexpr int invalidValue = static_cast<int>(0x80B50003);
constexpr int badParse = static_cast<int>(0x80B50007);
constexpr int invalidYear = static_cast<int>(0x80B50008);
constexpr int invalidMonth = static_cast<int>(0x80B50009);
constexpr int invalidDay = static_cast<int>(0x80B5000A);
constexpr int invalidHour = static_cast<int>(0x80B5000B);
constexpr int invalidSecond = static_cast<int>(0x80B5000D);
constexpr int invalidMicrosecond = static_cast<int>(0x80B5000E);
constexpr std::uint64_t unixEpochTick = 62135596800000000ull;
constexpr std::uint64_t leapDayTick = 63844806896789000ull;
constexpr std::uint64_t leapDaySecondTick = leapDayTick - 789000ull;
constexpr std::uint64_t maxTick = 315537897599999999ull;
constexpr std::uint64_t untouched = 123;
constexpr RtcDateTime leapDay{2024, 2, 29, 12, 34, 56, 789000};
constexpr RtcDateTime unixEpoch{1970, 1, 1, 0, 0, 0, 0};
constexpr RtcDateTime millennium{2000, 1, 1, 0, 0, 0, 0};
constexpr std::size_t textSize = 32;

class TimeZoneScope {
public:
    explicit TimeZoneScope(const char* zone) {
        if (const char* current = std::getenv("TZ")) saved = current;
        Apply(zone);
    }

    ~TimeZoneScope() {
        if (saved) {
            Apply(saved->c_str());
            return;
        }
#ifdef _WIN32
        _putenv_s("TZ", "");
        _tzset();
#else
        unsetenv("TZ");
        tzset();
#endif
    }

    TimeZoneScope(const TimeZoneScope&) = delete;
    TimeZoneScope& operator=(const TimeZoneScope&) = delete;

    void Apply(const char* zone) {
#ifdef _WIN32
        _putenv_s("TZ", zone);
        _tzset();
#else
        setenv("TZ", zone, 1);
        tzset();
#endif
    }

private:
    std::optional<std::string> saved;
};

std::string Format(const RtcDateTime& time) {
    return std::to_string(time.year) + "-" + std::to_string(time.month) + "-" + std::to_string(time.day) + " "
        + std::to_string(time.hour) + ":" + std::to_string(time.minute) + ":" + std::to_string(time.second) + "."
        + std::to_string(time.microsecond);
}

void RequireDateTime(const RtcDateTime& actual, const RtcDateTime& expected, const std::string& message) {
    RequireEqual(Format(actual), Format(expected), message);
}

RtcDateTime DateTimeOf(std::uint64_t value) {
    const RtcTick tick{value};
    RtcDateTime time{};
    RequireEqual(sceRtcSetTick(&time, &tick), 0, "convert tick " + std::to_string(value) + " to a date");
    return time;
}

std::uint64_t TickOf(int year, int month, int day, int hour, int minute, int second) {
    const RtcDateTime time{static_cast<std::uint16_t>(year), static_cast<std::uint16_t>(month), static_cast<std::uint16_t>(day),
        static_cast<std::uint16_t>(hour), static_cast<std::uint16_t>(minute), static_cast<std::uint16_t>(second), 0};
    RtcTick result{};
    RequireEqual(sceRtcGetTick(&time, &result), 0, "convert the reference date to a tick");
    return result.tick;
}

std::uint64_t ShiftedByMinutes(std::uint64_t tick, std::int64_t minutes) {
    return tick - static_cast<std::uint64_t>(minutes * 60000000);
}

const Case tickResolution{"Rtc_GetTickResolution_Called_ReturnsMicroseconds", [] {
    RequireEqual(sceRtcGetTickResolution(), 1000000, "tick resolution");
}};

const Case leapYear{"Rtc_IsLeapYear_ValidYears_ReturnsLeapFlag", [] {
    RequireEqual(sceRtcIsLeapYear(2000), 1, "2000 is a leap year");
    RequireEqual(sceRtcIsLeapYear(1900), 0, "1900 is not a leap year");
    RequireEqual(sceRtcIsLeapYear(2024), 1, "2024 is a leap year");
}};

const Case leapYearZero{"Rtc_IsLeapYear_YearZero_FailsWithInvalidYear", [] {
    RequireEqual(sceRtcIsLeapYear(0), invalidYear, "year 0");
}};

const Case daysInMonth{"Rtc_GetDaysInMonth_ValidMonths_ReturnsDayCount", [] {
    RequireEqual(sceRtcGetDaysInMonth(2023, 2), 28, "February 2023");
    RequireEqual(sceRtcGetDaysInMonth(2024, 2), 29, "February 2024");
    RequireEqual(sceRtcGetDaysInMonth(2024, 4), 30, "April 2024");
}};

const Case daysInMonthThirteen{"Rtc_GetDaysInMonth_MonthThirteen_FailsWithInvalidMonth", [] {
    RequireEqual(sceRtcGetDaysInMonth(2024, 13), invalidMonth, "month 13");
}};

const Case dayOfWeek{"Rtc_GetDayOfWeek_ValidDates_ReturnsWeekday", [] {
    RequireEqual(sceRtcGetDayOfWeek(1, 1, 1), 1, "0001-01-01 is a Monday");
    RequireEqual(sceRtcGetDayOfWeek(2026, 9, 26), 6, "2026-09-26 is a Saturday");
}};

const Case dayOfWeekInvalidDay{"Rtc_GetDayOfWeek_NonLeapFebruary29_FailsWithInvalidDay", [] {
    RequireEqual(sceRtcGetDayOfWeek(2023, 2, 29), invalidDay, "2023-02-29");
}};

const Case checkValidLeapDay{"Rtc_CheckValid_LeapDay_Succeeds", [] {
    RequireEqual(sceRtcCheckValid(&leapDay), 0, "2024-02-29 is valid");
}};

const Case checkValidNull{"Rtc_CheckValid_NullPointer_FailsWithInvalidPointer", [] {
    RequireEqual(sceRtcCheckValid(nullptr), invalidPointer, "null date");
}};

const Case checkValidInvalidFields{"Rtc_CheckValid_InvalidField_FailsWithFieldError", [] {
    RtcDateTime nonLeapYear = leapDay;
    nonLeapYear.year = 2023;
    RequireEqual(sceRtcCheckValid(&nonLeapYear), invalidDay, "2023-02-29");
    RtcDateTime hour24 = leapDay;
    hour24.hour = 24;
    RequireEqual(sceRtcCheckValid(&hour24), invalidHour, "hour 24");
    RtcDateTime fullSecond = leapDay;
    fullSecond.microsecond = 1000000;
    RequireEqual(sceRtcCheckValid(&fullSecond), invalidMicrosecond, "microsecond 1000000");
}};

const Case getTickLeapDay{"Rtc_GetTick_LeapDay_ReturnsExpectedTick", [] {
    RtcTick tick{};
    RequireEqual(sceRtcGetTick(&leapDay, &tick), 0, "result");
    RequireEqual(tick.tick, leapDayTick, "tick");
}};

const Case setTickLeapDay{"Rtc_SetTick_LeapDayTick_ReturnsLeapDay", [] {
    const RtcTick tick{leapDayTick};
    RtcDateTime converted{};
    RequireEqual(sceRtcSetTick(&converted, &tick), 0, "result");
    RequireDateTime(converted, leapDay, "date");
}};

const Case setTickMax{"Rtc_SetTick_MaxTick_ReturnsLastRepresentableInstant", [] {
    const RtcTick tick{maxTick};
    RtcDateTime converted{};
    RequireEqual(sceRtcSetTick(&converted, &tick), 0, "result");
    RequireDateTime(converted, RtcDateTime{9999, 12, 31, 23, 59, 59, 999999}, "date");
}};

const Case setTickPastMax{"Rtc_SetTick_PastMaxTick_FailsWithInvalidValue", [] {
    const RtcTick tick{maxTick + 1};
    RtcDateTime converted{};
    RequireEqual(sceRtcSetTick(&converted, &tick), invalidValue, "max tick + 1");
}};

const Case getTimeT{"Rtc_GetTimeT_KnownDates_ReturnsUnixSeconds", [] {
    std::int64_t seconds = -1;
    RequireEqual(sceRtcGetTime_t(&unixEpoch, &seconds), 0, "epoch result");
    RequireEqual(seconds, std::int64_t{0}, "epoch seconds");
    RequireEqual(sceRtcGetTime_t(&millennium, &seconds), 0, "2000-01-01 result");
    RequireEqual(seconds, std::int64_t{946684800}, "2000-01-01 seconds");
}};

const Case setTimeT{"Rtc_SetTimeT_MillenniumSeconds_ReturnsMillennium", [] {
    RtcDateTime converted{};
    RequireEqual(sceRtcSetTime_t(&converted, 946684800), 0, "result");
    RequireDateTime(converted, millennium, "date");
}};

const Case setTimeTNegative{"Rtc_SetTimeT_Negative_FailsWithInvalidValue", [] {
    RtcDateTime converted{};
    RequireEqual(sceRtcSetTime_t(&converted, -1), invalidValue, "time_t -1");
}};

const Case getFileTime{"Rtc_GetWin32FileTime_UnixEpoch_ReturnsFileTimeOffset", [] {
    std::uint64_t fileTime = 0;
    RequireEqual(sceRtcGetWin32FileTime(&unixEpoch, &fileTime), 0, "result");
    RequireEqual(fileTime, std::uint64_t{116444736000000000ull}, "file time");
}};

const Case setFileTime{"Rtc_SetWin32FileTime_UnixEpochOffset_ReturnsUnixEpoch", [] {
    RtcDateTime converted{};
    RequireEqual(sceRtcSetWin32FileTime(&converted, 116444736000000000ull), 0, "result");
    RequireDateTime(converted, unixEpoch, "date");
}};

const Case getDosTime{"Rtc_GetDosTime_ValidDates_ReturnsPackedValue", [] {
    const struct {
        RtcDateTime date;
        std::uint32_t expected;
    } dates[] = {
        {{2024, 2, 29, 12, 34, 57, 789000}, 0x585d645cu},
        {{1980, 1, 1, 0, 0, 0, 0}, 0x00210000u},
        {{2107, 12, 31, 23, 59, 59, 0}, 0xff9fbf7du},
    };
    for (const auto& entry : dates) {
        std::uint32_t dosTime = 0xffffffffu;
        RequireEqual(sceRtcGetDosTime(&entry.date, &dosTime), 0, "result for " + Format(entry.date));
        RequireEqual(dosTime, entry.expected, "DOS time for " + Format(entry.date));
    }
}};

const Case getDosTimeNull{"Rtc_GetDosTime_NullArguments_FailsWithInvalidPointer", [] {
    const RtcDateTime date{2024, 2, 29, 12, 34, 57, 789000};
    std::uint32_t dosTime = 0;
    RequireEqual(sceRtcGetDosTime(&date, nullptr), invalidPointer, "null output");
    RequireEqual(sceRtcGetDosTime(nullptr, &dosTime), invalidPointer, "null date");
}};

const Case getDosTimeBadMonth{"Rtc_GetDosTime_MonthThirteen_FailsWithInvalidMonth", [] {
    const RtcDateTime date{2024, 13, 1, 0, 0, 0, 0};
    std::uint32_t dosTime = 0;
    RequireEqual(sceRtcGetDosTime(&date, &dosTime), invalidMonth, "month 13");
}};

const Case getDosTimeEarly{"Rtc_GetDosTime_YearBefore1980_FailsWithInvalidYearAndZeroOutput", [] {
    const RtcDateTime date{1979, 12, 31, 0, 0, 0, 0};
    std::uint32_t dosTime = 0xff9fbf7du;
    RequireEqual(sceRtcGetDosTime(&date, &dosTime), invalidYear, "result");
    RequireEqual(dosTime, std::uint32_t{0}, "DOS time");
}};

const Case getDosTimeLate{"Rtc_GetDosTime_YearAfter2107_FailsWithInvalidYearAndClampedOutput", [] {
    const RtcDateTime date{2108, 1, 1, 0, 0, 0, 0};
    std::uint32_t dosTime = 0;
    RequireEqual(sceRtcGetDosTime(&date, &dosTime), invalidYear, "result");
    RequireEqual(dosTime, std::uint32_t{0xff9fbf7du}, "DOS time");
}};

const Case setDosTime{"Rtc_SetDosTime_PackedValues_DecodesDateTime", [] {
    const struct {
        std::uint32_t dosTime;
        RtcDateTime expected;
    } values[] = {
        {0x585d645cu, {2024, 2, 29, 12, 34, 56, 0}},
        {0x7f9fbf7du, {2043, 12, 31, 23, 59, 58, 0}},
        {0, {1980, 0, 0, 0, 0, 0, 0}},
        {0xff9fbf7du, {2107, 12, 31, 23, 59, 58, 0}},
        {0x80210000u, {2044, 1, 1, 0, 0, 0, 0}},
    };
    for (const auto& entry : values) {
        RtcDateTime converted{1, 1, 1, 1, 1, 1, 1};
        const std::string input = "DOS time " + std::to_string(entry.dosTime);
        RequireEqual(sceRtcSetDosTime(&converted, entry.dosTime), 0, input + " result");
        RequireDateTime(converted, entry.expected, input + " date");
    }
}};

const Case setDosTimeNull{"Rtc_SetDosTime_NullPointer_FailsWithInvalidPointer", [] {
    RequireEqual(sceRtcSetDosTime(nullptr, 0), invalidPointer, "null date");
}};

const Case formatRfc3339{"Rtc_FormatRFC3339_Offsets_FormatsShiftedTime", [] {
    const struct {
        int offset;
        const char* expected;
    } offsets[] = {
        {0, "2024-02-29T12:34:56.78Z"},
        {90, "2024-02-29T14:04:56.78+01:30"},
        {-300, "2024-02-29T07:34:56.78-05:00"},
        {1439, "2024-03-01T12:33:56.78+23:59"},
        {-1439, "2024-02-28T12:35:56.78-23:59"},
    };
    const RtcTick tick{leapDayTick};
    for (const auto& entry : offsets) {
        char text[textSize];
        const std::string input = "offset " + std::to_string(entry.offset);
        RequireEqual(sceRtcFormatRFC3339(text, &tick, entry.offset), 0, input + " result");
        RequireEqual(std::string(text), std::string(entry.expected), input + " text");
    }
}};

const Case formatRfc3339RoundTrip{"Rtc_FormatRFC3339_ValidOffsets_RoundTripsThroughParse", [] {
    const RtcTick tick{leapDayTick};
    for (int offset : {0, 1, -1, 59, -59, 60, -60, 90, -300, 1439, -1439}) {
        char text[textSize];
        RtcTick parsed{};
        const std::string input = "offset " + std::to_string(offset);
        RequireEqual(sceRtcFormatRFC3339(text, &tick, offset), 0, input + " format result");
        RequireEqual(sceRtcParseRFC3339(&parsed, text), 0, input + " parse result");
        RequireEqual(parsed.tick, leapDayTick - 9000, input + " parsed tick");
    }
}};

const Case formatRfc3339BadOffset{"Rtc_FormatRFC3339_OutOfRangeOffset_FailsAndLeavesBufferUntouched", [] {
    const RtcTick tick{leapDayTick};
    for (int offset : {1440, -1440, 6000, -6000, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        char text[textSize];
        std::memset(text, 'x', sizeof(text));
        char original[sizeof(text)];
        std::memcpy(original, text, sizeof(text));
        const std::string input = "offset " + std::to_string(offset);
        RequireEqual(sceRtcFormatRFC3339(text, &tick, offset), invalidValue, input + " result");
        Require(std::memcmp(text, original, sizeof(text)) == 0, input + " left the buffer untouched");
    }
}};

const Case formatRfc3339Null{"Rtc_FormatRFC3339_NullArguments_FailsWithInvalidPointer", [] {
    const RtcTick tick{leapDayTick};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC3339(nullptr, &tick, 1440), invalidPointer, "null buffer");
    RequireEqual(sceRtcFormatRFC3339(text, nullptr, 1440), invalidPointer, "null tick");
}};

const Case parseRfc3339Valid{"Rtc_ParseRFC3339_ValidTimestamps_ReturnsTick", [] {
    const struct {
        const char* text;
        std::uint64_t expected;
    } inputs[] = {
        {"2024-02-29T14:04:56.789+01:30", leapDayTick},
        {"2024-02-29t12:34:56.789z", leapDayTick},
        {"1970-01-01T00:00:00Z", unixEpochTick},
        {"2024-02-29T12:34:56.789+00:00", leapDayTick},
        {"2024-02-29T12:34:56.789-00:00", leapDayTick},
        {"2024-02-29T12:34:56.789+00:59", leapDayTick - 3540000000ull},
        {"2024-02-29T12:34:56.789-00:59", leapDayTick + 3540000000ull},
        {"2024-02-29T12:34:56.789+23:59", leapDayTick - 86340000000ull},
        {"2024-02-29T12:34:56.789-23:59", leapDayTick + 86340000000ull},
    };
    for (const auto& entry : inputs) {
        RtcTick tick{};
        RequireEqual(sceRtcParseRFC3339(&tick, entry.text), 0, std::string(entry.text) + " result");
        RequireEqual(tick.tick, entry.expected, std::string(entry.text) + " tick");
    }
}};

const Case parseRfc3339NonLeap{"Rtc_ParseRFC3339_NonLeapFebruary29_FailsWithInvalidDay", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseRFC3339(&tick, "2023-02-29T00:00:00Z"), invalidDay, "2023-02-29");
}};

const Case parseRfc3339NoZone{"Rtc_ParseRFC3339_MissingZone_FailsWithBadParse", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56"), badParse, "no zone designator");
}};

const Case parseRfc3339Lenient{"Rtc_ParseRFC3339_TrailingTextAndFractionVariants_ReturnsTick", [] {
    const struct {
        const char* text;
        std::uint64_t expected;
    } inputs[] = {
        {"2024-02-29T12:34:56Zjunk", leapDaySecondTick},
        {"2024-02-29T14:34:56+02:00 trailing", leapDaySecondTick},
        {"2024-02-29T12:34:56.Z", leapDaySecondTick},
        {"2024-02-29T12:34:56.1234567Z", leapDaySecondTick + 123456ull},
    };
    for (const auto& entry : inputs) {
        RtcTick tick{};
        RequireEqual(sceRtcParseRFC3339(&tick, entry.text), 0, std::string(entry.text) + " result");
        RequireEqual(tick.tick, entry.expected, std::string(entry.text) + " tick");
    }
}};

const Case parseRfc3339Unranged{"Rtc_ParseRFC3339_UnrangedOffsets_AppliesOffsetMinutes", [] {
    const struct {
        const char* text;
        std::int64_t minutes;
    } offsets[] = {
        {"2024-02-29T12:34:56.789+00:99", 99},
        {"2024-02-29T12:34:56.789-00:99", -99},
        {"2024-02-29T12:34:56.789+00:60", 60},
        {"2024-02-29T12:34:56.789-00:60", -60},
        {"2024-02-29T12:34:56.789+24:00", 1440},
        {"2024-02-29T12:34:56.789-24:00", -1440},
        {"2024-02-29T12:34:56.789+99:59", 5999},
        {"2024-02-29T12:34:56.789-99:99", -6039},
    };
    for (const auto& offset : offsets) {
        RtcTick tick{};
        RequireEqual(sceRtcParseRFC3339(&tick, offset.text), 0, std::string(offset.text) + " result");
        RequireEqual(tick.tick, ShiftedByMinutes(leapDayTick, offset.minutes), std::string(offset.text) + " tick");
    }
}};

const Case parseRfc3339Malformed{"Rtc_ParseRFC3339_MalformedText_FailsWithBadParseAndKeepsTick", [] {
    for (const char* text : {"2024-02-29 12:34:56Z", "2024-02-29T12:34:56+0100", "2024-02-29T12:34:56+01", "2024-02-29T12:34:56 Z",
             " 2024-02-29T12:34:56Z", "2024-02-29T12:34:56..Z", "2024-2-29T12:34:56Z"}) {
        RtcTick tick{untouched};
        RequireEqual(sceRtcParseRFC3339(&tick, text), badParse, std::string(text) + " result");
        RequireEqual(tick.tick, untouched, std::string(text) + " tick");
    }
}};

const Case parseRfc3339LeapSecond{"Rtc_ParseRFC3339_LeapSecondUtc_SucceedsWithoutChangingTick", [] {
    RtcTick tick{untouched};
    RequireEqual(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:60Z"), 0, "result");
    RequireEqual(tick.tick, untouched, "tick");
}};

const Case parseRfc3339LeapSecondOffset{"Rtc_ParseRFC3339_LeapSecondWithOffset_ShiftsExistingTickByOffset", [] {
    RtcTick tick{untouched};
    RequireEqual(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:60+01:00"), 0, "result");
    RequireEqual(tick.tick, untouched - 3600000000ull, "tick");
}};

const Case parseRfc3339SecondSixtyOne{"Rtc_ParseRFC3339_SecondSixtyOne_FailsWithInvalidSecond", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:61Z"), invalidSecond, "second 61");
}};

const Case parseRfc3339YearZero{"Rtc_ParseRFC3339_YearZeroWithInvalidFields_FailsWithInvalidYear", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseRFC3339(&tick, "0000-13-40T99:99:99Z"), invalidYear, "year 0");
}};

const Case parseRfc3339FirstInstant{"Rtc_ParseRFC3339_FirstInstantWithPositiveOffset_WrapsBelowZero", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseRFC3339(&tick, "0001-01-01T00:00:00+01:00"), 0, "result");
    RequireEqual(tick.tick, std::uint64_t{0xFFFFFFFF296C5C00ull}, "tick");
}};

const Case parseRfc3339Null{"Rtc_ParseRFC3339_NullTick_FailsWithInvalidPointer", [] {
    RequireEqual(sceRtcParseRFC3339(nullptr, "1970-01-01T00:00:00Z"), invalidPointer, "null tick");
}};

const Case parseDateTimeMalformed{"Rtc_ParseDateTime_MalformedRfc3339_FailsWithBadParseAndKeepsTick", [] {
    for (const char* text : {"2024-02-29T12:34:56.789", "2024-02-29 12:34:56", "2024-02-29 12:34:56Z", "2024-02-29", "2024/02/29T12:34:56Z",
             "2024-02-29T12:34:56."}) {
        RtcTick tick{untouched};
        RequireEqual(sceRtcParseDateTime(&tick, text), badParse, std::string(text) + " result");
        RequireEqual(tick.tick, untouched, std::string(text) + " tick");
    }
}};

const Case parseDateTimeRfc3339{"Rtc_ParseDateTime_Rfc3339Inputs_ReturnsTick", [] {
    const struct {
        const char* text;
        std::uint64_t expected;
    } inputs[] = {
        {" \t 2024-02-29T12:34:56.789Z", leapDayTick},
        {"2024-02-29T12:34:56.789Zjunk", leapDayTick},
        {"2024-02-29T12:34:56.789+99:99", leapDayTick - 6039ull * 60000000ull},
        {"2024-02-29T14:04:56.789+01:30", leapDayTick},
        {"2024-02-29t12:34:56.789z", leapDayTick},
        {"1970-01-01T00:00:00Z", unixEpochTick},
    };
    for (const auto& entry : inputs) {
        RtcTick tick{};
        RequireEqual(sceRtcParseDateTime(&tick, entry.text), 0, std::string(entry.text) + " result");
        RequireEqual(tick.tick, entry.expected, std::string(entry.text) + " tick");
    }
}};

const Case parseDateTimeLeapSecond{"Rtc_ParseDateTime_LeapSecondWithOffset_ShiftsExistingTickByOffset", [] {
    RtcTick tick{untouched};
    RequireEqual(sceRtcParseDateTime(&tick, "2024-02-29T12:34:60-01:00"), 0, "result");
    RequireEqual(tick.tick, untouched + 3600000000ull, "tick");
}};

const Case parseDateTimeMonth{"Rtc_ParseDateTime_MonthThirteen_FailsWithInvalidMonth", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseDateTime(&tick, "2024-13-01T00:00:00Z"), invalidMonth, "month 13");
}};

const Case parseDateTimeNonLeap{"Rtc_ParseDateTime_NonLeapFebruary29_FailsWithInvalidDay", [] {
    RtcTick tick{};
    RequireEqual(sceRtcParseDateTime(&tick, "2023-02-29T00:00:00Z"), invalidDay, "2023-02-29");
}};

const Case parseDateTimeNull{"Rtc_ParseDateTime_NullTick_FailsWithInvalidPointer", [] {
    RequireEqual(sceRtcParseDateTime(nullptr, "1970-01-01T00:00:00Z"), invalidPointer, "null tick");
}};

const Case parseDateTimeRfc2822{"Rtc_ParseDateTime_Rfc2822AndAsctimeInputs_ReturnsTick", [] {
    const struct {
        const char* text;
        std::uint64_t expected;
    } inputs[] = {
        {"Thu, 29 Feb 2024 12:34:56", leapDaySecondTick},
        {"Thu, 29 Feb 2024 12:34:56 GMT", leapDaySecondTick},
        {"Thu, 29 Feb 2024 14:04:56 +0130", leapDaySecondTick},
        {"Thu, 29 Feb 2024 11:04:56 -0130", leapDaySecondTick},
        {"Thu, 01 Jan 1970 00:00:00 +0000", unixEpochTick},
        {"Thu Feb 29 12:34:56 2024", leapDaySecondTick},
        {"Thu Jan  1 00:00:00 1970", unixEpochTick},
    };
    for (const auto& entry : inputs) {
        RtcTick tick{};
        RequireEqual(sceRtcParseDateTime(&tick, entry.text), 0, std::string(entry.text) + " result");
        RequireEqual(tick.tick, entry.expected, std::string(entry.text) + " tick");
    }
}};

const Case parseDateTimeTable{"Rtc_ParseDateTime_FreeFormInputs_MatchExpectedResultAndTick", [] {
    const std::uint64_t march = TickOf(2024, 3, 1, 12, 34, 56);
    const struct {
        const char* text;
        int result;
        std::uint64_t local;
        std::int64_t minutes;
    } dateTimes[] = {
        {"thursday,29-feb-24 12:34:56", 0, leapDaySecondTick, 0},
        {"Mon 29 February 2024 12:34:56 -0130", 0, leapDaySecondTick, -90},
        {" \tFri, 01 Mar 2024 12:34:56 GMT", 0, march, 0},
        {"Thu, 1-Mar 2024 12:34:56 -9999", 0, march, -6039},
        {"Thu, 01 Mar 2024 12:34:56 +01:30", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 +0130x", 0, march, 90},
        {"Thu, 01 Mar 2024 12:34:56 PST8PDT", 0, march, -480},
        {"Thu, 01 Mar 2024 12:34:56 nzdt", 0, march, 780},
        {"Thu, 01 Mar 2024 12:34:56 KST", 0, march, 540},
        {"Thu, 01 Mar 2024 12:34:56 HST", 0, march, 420},
        {"Thu, 01 Mar 2024 12:34:56 Jt", 0, march, 450},
        {"Thu, 01 Mar 2024 12:34:56 ut", 0, march, -420},
        {"Thu, 01 Mar 2024 12:34:56 xT", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 Ux", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 B", 0, march, 60},
        {"Thu, 01 Mar 2024 12:34:56 m", 0, march, 720},
        {"Thu, 01 Mar 2024 12:34:56 N", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 y", 0, march, -660},
        {"Thu, 01 Mar 2024 12:34:56 Z", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 J", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34:56  +0100", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34:56 (UTC)", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34:56\tEST", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 ", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:567 EST", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34 EST", 0, TickOf(2024, 3, 1, 12, 34, 0), 0},
        {"Thu, 01 Mar 2024 12:34EST", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 1:2:3 EST", 0, TickOf(2024, 3, 1, 1, 2, 3), -300},
        {"Thu, 01 Mar 2024 26:00:00 GMT", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 25:00:00 +0100", 0, untouched, 60},
        {"Thu, 29 Feb 2023 12:34:56 EST", 0, untouched, -300},
        {"Thu, 01 Mar 49 12:34:56 GMT", 0, TickOf(2049, 3, 1, 12, 34, 56), 0},
        {"Thu, 01 Mar 50 12:34:56 GMT", 0, TickOf(1950, 3, 1, 12, 34, 56), 0},
        {"Mon, 01 Jan 0001 00:00:00 +0100", 0, 0, 60},
        {"Thu, 01 Mar 024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01 Mar 10000 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 001 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01  Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01 Mar 2024\t12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01 Sept 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thurs, 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu , 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {", 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"\nThu, 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"THURSDAY february  9 1:2:3 2024 +0100", 0, TickOf(2024, 2, 9, 1, 2, 3), 0},
        {"ThuMar  1 12:34:56 10000", 0, TickOf(1000, 3, 1, 12, 34, 56), 0},
        {"Thu,\tFeb 29 12:34:56 2024", 0, leapDaySecondTick, 0},
        {"Thu Feb 29 99:99:99 2024", 0, untouched, 0},
        {"Thu Feb  29 12:34:56 2024", badParse, untouched, 0},
        {"Thu Feb 29 12:34 2024", badParse, untouched, 0},
        {"Thu Feb 29 12:34:56 24", badParse, untouched, 0},
        {"Thu Feb 29 12:34:56\t2024", badParse, untouched, 0},
        {"Feb 29 12:34:56 2024", badParse, untouched, 0},
        {"\n2024-02-29T12:34:56Z", badParse, untouched, 0},
        {"+2024-02-29T12:34:56Z", badParse, untouched, 0},
        {"", badParse, untouched, 0},
    };
    for (const auto& dateTime : dateTimes) {
        RtcTick tick{untouched};
        const std::string input = "\"" + std::string(dateTime.text) + "\"";
        RequireEqual(sceRtcParseDateTime(&tick, dateTime.text), dateTime.result, input + " result");
        RequireEqual(tick.tick, ShiftedByMinutes(dateTime.local, dateTime.minutes), input + " tick");
    }
}};

const Case addSeconds{"Rtc_TickAddSeconds_OneHour_AdvancesTick", [] {
    const RtcTick source{leapDayTick};
    RtcTick result{};
    RequireEqual(sceRtcTickAddSeconds(&result, &source, 3600), 0, "result");
    RequireEqual(result.tick, leapDayTick + 3600000000ull, "tick");
}};

const Case addDays{"Rtc_TickAddDays_LeapDayPlusOne_ReturnsMarchFirst", [] {
    const RtcTick source{leapDayTick};
    RtcTick result{};
    RequireEqual(sceRtcTickAddDays(&result, &source, 1), 0, "result");
    const RtcDateTime converted = DateTimeOf(result.tick);
    RequireEqual(converted.month, std::uint16_t{3}, "month");
    RequireEqual(converted.day, std::uint16_t{1}, "day");
}};

const Case addYears{"Rtc_TickAddYears_LeapDayPlusOne_ClampsToFebruary28", [] {
    const RtcTick source{leapDayTick};
    RtcTick result{};
    RequireEqual(sceRtcTickAddYears(&result, &source, 1), 0, "result");
    const RtcDateTime converted = DateTimeOf(result.tick);
    RequireEqual(converted.year, std::uint16_t{2025}, "year");
    RequireEqual(converted.month, std::uint16_t{2}, "month");
    RequireEqual(converted.day, std::uint16_t{28}, "day");
}};

const Case addMonths{"Rtc_TickAddMonths_EndOfJanuaryPlusOne_ClampsToLeapDay", [] {
    const RtcDateTime endOfJanuary{2024, 1, 31, 8, 0, 0, 0};
    RtcTick source{};
    RequireEqual(sceRtcGetTick(&endOfJanuary, &source), 0, "source tick");
    RtcTick result{};
    RequireEqual(sceRtcTickAddMonths(&result, &source, 1), 0, "result");
    RequireDateTime(DateTimeOf(result.tick), RtcDateTime{2024, 2, 29, 8, 0, 0, 0}, "date");
}};

const Case addMonthsBeforeYearOne{"Rtc_TickAddMonths_BeforeYearOne_FailsWithInvalidValue", [] {
    const RtcDateTime endOfJanuary{2024, 1, 31, 8, 0, 0, 0};
    RtcTick source{};
    RequireEqual(sceRtcGetTick(&endOfJanuary, &source), 0, "source tick");
    RtcTick result{};
    RequireEqual(sceRtcTickAddMonths(&result, &source, -12 * 2024), invalidValue, "minus 2024 years");
}};

const Case addTicksBelowZero{"Rtc_TickAddTicks_BelowZero_FailsWithInvalidValue", [] {
    const RtcTick source{5};
    RtcTick result{};
    RequireEqual(sceRtcTickAddTicks(&result, &source, -6), invalidValue, "5 - 6");
}};

const Case addTicksToZero{"Rtc_TickAddTicks_ToZero_ReturnsZero", [] {
    const RtcTick source{5};
    RtcTick result{untouched};
    RequireEqual(sceRtcTickAddTicks(&result, &source, -5), 0, "result");
    RequireEqual(result.tick, std::uint64_t{0}, "tick");
}};

const Case addTicksPastMax{"Rtc_TickAddTicks_PastMaxTick_FailsWithInvalidValue", [] {
    const RtcTick source{maxTick};
    RtcTick result{};
    RequireEqual(sceRtcTickAddTicks(&result, &source, 1), invalidValue, "max tick + 1");
}};

const Case addTicksNull{"Rtc_TickAddTicks_NullResult_FailsWithInvalidPointer", [] {
    const RtcTick source{maxTick};
    RequireEqual(sceRtcTickAddTicks(nullptr, &source, 1), invalidPointer, "null result");
}};

const Case addInvalidSource{"Rtc_TickAdd_InvalidSourceTick_FailsAndLeavesOutputUntouched", [] {
    for (std::uint64_t invalidTick : {maxTick + 1, std::numeric_limits<std::uint64_t>::max()}) {
        const std::string input = "source " + std::to_string(invalidTick);
        RtcTick source{invalidTick};
        RtcTick result{untouched};
        RequireEqual(sceRtcTickAddTicks(&result, &source, 0), invalidValue, input + " add 0 ticks");
        RequireEqual(result.tick, untouched, input + " add 0 ticks output");
        RequireEqual(sceRtcTickAddTicks(&result, &source, 1), invalidValue, input + " add 1 tick");
        RequireEqual(result.tick, untouched, input + " add 1 tick output");
        RequireEqual(sceRtcTickAddTicks(&result, &source, -1), invalidValue, input + " add -1 tick");
        RequireEqual(result.tick, untouched, input + " add -1 tick output");
        RequireEqual(sceRtcTickAddSeconds(&result, &source, 0), invalidValue, input + " add 0 seconds");
        RequireEqual(result.tick, untouched, input + " add 0 seconds output");
        RequireEqual(sceRtcTickAddDays(&result, &source, -1), invalidValue, input + " add -1 day");
        RequireEqual(result.tick, untouched, input + " add -1 day output");
        RequireEqual(sceRtcTickAddMonths(&result, &source, 0), invalidValue, input + " add 0 months");
        RequireEqual(result.tick, untouched, input + " add 0 months output");
        RequireEqual(sceRtcTickAddYears(&result, &source, -1), invalidValue, input + " add -1 year");
        RequireEqual(result.tick, untouched, input + " add -1 year output");
        RequireEqual(sceRtcTickAddTicks(&source, &source, 0), invalidValue, input + " in-place add 0 ticks");
        RequireEqual(source.tick, invalidTick, input + " in-place add 0 ticks output");
        RequireEqual(sceRtcTickAddMonths(&source, &source, -1), invalidValue, input + " in-place add -1 month");
        RequireEqual(source.tick, invalidTick, input + " in-place add -1 month output");
        RequireEqual(sceRtcTickAddTicks(nullptr, &source, 0), invalidPointer, input + " null result for ticks");
        RequireEqual(sceRtcTickAddMonths(nullptr, &source, 0), invalidPointer, input + " null result for months");
    }
}};

const Case addTicksMaxInPlace{"Rtc_TickAddTicks_MaxTickInPlace_UpdatesSource", [] {
    RtcTick source{maxTick};
    RequireEqual(sceRtcTickAddTicks(&source, &source, 0), 0, "add 0 result");
    RequireEqual(source.tick, maxTick, "add 0 tick");
    RequireEqual(sceRtcTickAddTicks(&source, &source, -1), 0, "add -1 result");
    RequireEqual(source.tick, maxTick - 1, "add -1 tick");
}};

const Case addMonthsMaxZero{"Rtc_TickAddMonths_MaxTickZeroMonths_KeepsTick", [] {
    RtcTick source{maxTick};
    RequireEqual(sceRtcTickAddMonths(&source, &source, 0), 0, "result");
    RequireEqual(source.tick, maxTick, "tick");
}};

const Case addYearsMaxMinusOne{"Rtc_TickAddYears_MaxTickMinusOneYear_ReturnsYear9998", [] {
    const RtcTick source{maxTick};
    RtcTick result{};
    RequireEqual(sceRtcTickAddYears(&result, &source, -1), 0, "result");
    RequireDateTime(DateTimeOf(result.tick), RtcDateTime{9998, 12, 31, 23, 59, 59, 999999}, "date");
}};

const Case addOutOfRangeFromMax{"Rtc_TickAdd_OutOfRangeFromMaxTick_FailsAndLeavesOutputUntouched", [] {
    const RtcTick source{maxTick};
    RtcTick result{untouched};
    RequireEqual(sceRtcTickAddMonths(&result, &source, 1), invalidValue, "add 1 month");
    RequireEqual(result.tick, untouched, "add 1 month output");
    RequireEqual(sceRtcTickAddTicks(&result, &source, std::numeric_limits<std::int64_t>::min()), invalidValue, "add int64 min ticks");
    RequireEqual(result.tick, untouched, "add int64 min ticks output");
    RequireEqual(sceRtcTickAddSeconds(&result, &source, std::numeric_limits<std::int64_t>::max()), invalidValue, "add int64 max seconds");
    RequireEqual(result.tick, untouched, "add int64 max seconds output");
}};

const Case addMonthsZeroTick{"Rtc_TickAddMonths_ZeroTickZeroMonths_KeepsTick", [] {
    RtcTick source{0};
    RequireEqual(sceRtcTickAddMonths(&source, &source, 0), 0, "result");
    RequireEqual(source.tick, std::uint64_t{0}, "tick");
}};

const Case addMonthsBelowZero{"Rtc_TickAddMonths_ZeroTickMinusOneMonth_FailsAndLeavesOutputUntouched", [] {
    const RtcTick source{0};
    RtcTick result{untouched};
    RequireEqual(sceRtcTickAddMonths(&result, &source, -1), invalidValue, "result");
    RequireEqual(result.tick, untouched, "output");
}};

const Case currentTick{"Rtc_GetCurrentTick_Now_IsAfterLeapDay2024", [] {
    RtcTick now{};
    RequireEqual(sceRtcGetCurrentTick(&now), 0, "result");
    Require(now.tick > leapDayTick, "current tick " + std::to_string(now.tick) + " is after 2024-02-29");
}};

const Case currentTickLocalRoundTrip{"Rtc_ConvertUtcToLocalTime_CurrentTick_RoundTripsThroughLocalTimeToUtc", [] {
    RtcTick now{};
    RequireEqual(sceRtcGetCurrentTick(&now), 0, "current tick");
    RtcTick local{};
    RtcTick back{};
    RequireEqual(sceRtcConvertUtcToLocalTime(&now, &local), 0, "UTC to local");
    RequireEqual(sceRtcConvertLocalTimeToUtc(&local, &back), 0, "local to UTC");
    RequireEqual(back.tick, now.tick, "round-tripped tick");
}};

const Case formatRfc2822{"Rtc_FormatRFC2822_Offsets_FormatsShiftedTime", [] {
    const struct {
        int offset;
        const char* expected;
    } offsets[] = {
        {0, "Thu, 29 Feb 2024 12:34:56 +0000"},
        {90, "Thu, 29 Feb 2024 14:04:56 +0130"},
        {-300, "Thu, 29 Feb 2024 07:34:56 -0500"},
        {-30, "Thu, 29 Feb 2024 12:04:56 -0030"},
        {65, "Thu, 29 Feb 2024 13:39:56 +0105"},
        {1439, "Fri, 01 Mar 2024 12:33:56 +2359"},
        {-1439, "Wed, 28 Feb 2024 12:35:56 -2359"},
    };
    const RtcTick tick{leapDayTick};
    for (const auto& entry : offsets) {
        char text[textSize];
        const std::string input = "offset " + std::to_string(entry.offset);
        RequireEqual(sceRtcFormatRFC2822(text, &tick, entry.offset), 0, input + " result");
        RequireEqual(std::string(text), std::string(entry.expected), input + " text");
    }
}};

const Case formatRfc2822Week{"Rtc_FormatRFC2822_WeekOfMarch2024_FormatsWeekdayNames", [] {
    const char* week[] = {"Sun, 03 Mar", "Mon, 04 Mar", "Tue, 05 Mar", "Wed, 06 Mar", "Thu, 07 Mar", "Fri, 08 Mar", "Sat, 09 Mar"};
    for (int day = 0; day < 7; ++day) {
        const RtcDateTime date{2024, 3, static_cast<std::uint16_t>(3 + day), 0, 0, 0, 0};
        RtcTick tick{};
        char text[textSize];
        const std::string input = "2024-03-" + std::to_string(3 + day);
        RequireEqual(sceRtcGetTick(&date, &tick), 0, input + " tick");
        RequireEqual(sceRtcFormatRFC2822(text, &tick, 0), 0, input + " format result");
        RequireEqual(std::string(text, std::strlen(week[day])), std::string(week[day]), input + " prefix");
    }
}};

const Case formatRfc2822Zero{"Rtc_FormatRFC2822_TickZero_FormatsFirstInstant", [] {
    const RtcTick tick{0};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC2822(text, &tick, 0), 0, "result");
    RequireEqual(std::string(text), std::string("Mon, 01 Jan 0001 00:00:00 +0000"), "text");
}};

const Case formatRfc2822ZeroNegative{"Rtc_FormatRFC2822_TickZeroNegativeOffset_FailsWithInvalidValue", [] {
    const RtcTick tick{0};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC2822(text, &tick, -1), invalidValue, "offset -1");
}};

const Case formatRfc2822Max{"Rtc_FormatRFC2822_MaxTick_FormatsLastInstant", [] {
    const RtcTick tick{maxTick};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC2822(text, &tick, 0), 0, "result");
    RequireEqual(std::string(text), std::string("Fri, 31 Dec 9999 23:59:59 +0000"), "text");
}};

const Case formatRfc2822MaxPositive{"Rtc_FormatRFC2822_MaxTickPositiveOffset_FailsWithInvalidValue", [] {
    const RtcTick tick{maxTick};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC2822(text, &tick, 1), invalidValue, "offset 1");
}};

const Case formatRfc2822BadOffset{"Rtc_FormatRFC2822_OutOfRangeOffset_FailsAndLeavesBufferUntouched", [] {
    const RtcTick tick{leapDayTick};
    for (int offset : {1440, -1440, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        char text[textSize];
        std::memset(text, 'x', sizeof(text));
        char original[sizeof(text)];
        std::memcpy(original, text, sizeof(text));
        const std::string input = "offset " + std::to_string(offset);
        RequireEqual(sceRtcFormatRFC2822(text, &tick, offset), invalidValue, input + " result");
        Require(std::memcmp(text, original, sizeof(text)) == 0, input + " left the buffer untouched");
    }
}};

const Case formatRfc2822Null{"Rtc_FormatRFC2822_NullArguments_FailsWithInvalidPointer", [] {
    const RtcTick tick{leapDayTick};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC2822(nullptr, &tick, 0), invalidPointer, "null buffer");
    RequireEqual(sceRtcFormatRFC2822(text, nullptr, 0), invalidPointer, "null tick");
}};

const Case formatLocalTime{"Rtc_FormatLocalTime_TimeZones_FormatsLocalOffset", [] {
    const struct {
        const char* zone;
        const char* rfc2822;
        const char* rfc3339;
    } zones[] = {
        {"XXX-5:30", "Thu, 29 Feb 2024 18:04:56 +0530", "2024-02-29T18:04:56.78+05:30"},
        {"XXX+3", "Thu, 29 Feb 2024 09:34:56 -0300", "2024-02-29T09:34:56.78-03:00"},
        {"UTC0", "Thu, 29 Feb 2024 12:34:56 +0000", "2024-02-29T12:34:56.78Z"},
    };
    const RtcTick tick{leapDayTick};
    for (const auto& entry : zones) {
        const TimeZoneScope zone(entry.zone);
        char rfc2822[textSize];
        char rfc3339[textSize];
        const std::string input = std::string("TZ=") + entry.zone;
        RequireEqual(sceRtcFormatRFC2822LocalTime(rfc2822, &tick), 0, input + " RFC 2822 result");
        RequireEqual(std::string(rfc2822), std::string(entry.rfc2822), input + " RFC 2822 text");
        RequireEqual(sceRtcFormatRFC3339LocalTime(rfc3339, &tick), 0, input + " RFC 3339 result");
        RequireEqual(std::string(rfc3339), std::string(entry.rfc3339), input + " RFC 3339 text");
    }
}};

const Case formatLocalTimeNull{"Rtc_FormatLocalTime_NullArguments_FailsWithInvalidPointer", [] {
    const TimeZoneScope zone("UTC0");
    const RtcTick tick{leapDayTick};
    char text[textSize];
    RequireEqual(sceRtcFormatRFC2822LocalTime(nullptr, &tick), invalidPointer, "RFC 2822 null buffer");
    RequireEqual(sceRtcFormatRFC2822LocalTime(text, nullptr), invalidPointer, "RFC 2822 null tick");
    RequireEqual(sceRtcFormatRFC3339LocalTime(nullptr, &tick), invalidPointer, "RFC 3339 null buffer");
    RequireEqual(sceRtcFormatRFC3339LocalTime(text, nullptr), invalidPointer, "RFC 3339 null tick");
}};

const Case localTimeInvalidTick{"Rtc_LocalTime_InvalidTick_FailsAndLeavesOutputsUntouched", [] {
    for (const char* zoneName : {"UTC0", "XXX-5:30", "XXX+3"}) {
        const TimeZoneScope zone(zoneName);
        for (std::uint64_t invalidTick : {maxTick + 1, std::uint64_t{1} << 63u, std::numeric_limits<std::uint64_t>::max()}) {
            const std::string input = std::string("TZ=") + zoneName + " tick " + std::to_string(invalidTick);
            RtcTick source{invalidTick};
            RtcTick result{untouched};
            RequireEqual(sceRtcConvertUtcToLocalTime(&source, &result), invalidValue, input + " UTC to local");
            RequireEqual(result.tick, untouched, input + " UTC to local output");
            RequireEqual(sceRtcConvertLocalTimeToUtc(&source, &result), invalidValue, input + " local to UTC");
            RequireEqual(result.tick, untouched, input + " local to UTC output");
            RequireEqual(sceRtcConvertUtcToLocalTime(&source, &source), invalidValue, input + " in-place UTC to local");
            RequireEqual(source.tick, invalidTick, input + " in-place UTC to local output");
            RequireEqual(sceRtcConvertLocalTimeToUtc(&source, &source), invalidValue, input + " in-place local to UTC");
            RequireEqual(source.tick, invalidTick, input + " in-place local to UTC output");
            char rfc3339[textSize];
            std::memset(rfc3339, 'x', sizeof(rfc3339));
            char originalRfc3339[sizeof(rfc3339)];
            std::memcpy(originalRfc3339, rfc3339, sizeof(rfc3339));
            RequireEqual(sceRtcFormatRFC3339LocalTime(rfc3339, &source), invalidValue, input + " RFC 3339 local");
            Require(std::memcmp(rfc3339, originalRfc3339, sizeof(rfc3339)) == 0, input + " RFC 3339 local left the buffer untouched");
            char rfc2822[textSize];
            std::memset(rfc2822, 'x', sizeof(rfc2822));
            char originalRfc2822[sizeof(rfc2822)];
            std::memcpy(originalRfc2822, rfc2822, sizeof(rfc2822));
            RequireEqual(sceRtcFormatRFC2822LocalTime(rfc2822, &source), invalidValue, input + " RFC 2822 local");
            Require(std::memcmp(rfc2822, originalRfc2822, sizeof(rfc2822)) == 0, input + " RFC 2822 local left the buffer untouched");
        }
    }
}};

const Case localTimeRoundTrip{"Rtc_ConvertUtcToLocalTime_LeapDayInEachZone_RoundTripsThroughLocalTimeToUtc", [] {
    for (const char* zoneName : {"UTC0", "XXX-5:30", "XXX+3"}) {
        const TimeZoneScope zone(zoneName);
        const std::string input = std::string("TZ=") + zoneName;
        const RtcTick source{leapDayTick};
        RtcTick result{};
        RequireEqual(sceRtcConvertUtcToLocalTime(&source, &result), 0, input + " UTC to local");
        RequireEqual(sceRtcConvertLocalTimeToUtc(&result, &result), 0, input + " in-place local to UTC");
        RequireEqual(result.tick, source.tick, input + " round-tripped tick");
    }
}};

} // namespace
