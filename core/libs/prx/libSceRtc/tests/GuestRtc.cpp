#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
int APS5_VABI sceRtcFormatRFC3339(char*, const RtcTick*, int);
int APS5_VABI sceRtcParseRFC3339(RtcTick*, const char*);
int APS5_VABI sceRtcTickAddTicks(RtcTick*, const RtcTick*, std::int64_t);
int APS5_VABI sceRtcTickAddSeconds(RtcTick*, const RtcTick*, std::int64_t);
int APS5_VABI sceRtcTickAddDays(RtcTick*, const RtcTick*, std::int32_t);
int APS5_VABI sceRtcTickAddMonths(RtcTick*, const RtcTick*, std::int32_t);
int APS5_VABI sceRtcTickAddYears(RtcTick*, const RtcTick*, std::int16_t);
}

static void Require(bool value) { if (!value) std::abort(); }

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

    char text[32];
    tick.tick = leapDayTick;
    Require(sceRtcFormatRFC3339(text, &tick, 0) == 0 && std::strcmp(text, "2024-02-29T12:34:56.78Z") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, 90) == 0 && std::strcmp(text, "2024-02-29T14:04:56.78+01:30") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, -300) == 0 && std::strcmp(text, "2024-02-29T07:34:56.78-05:00") == 0);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T14:04:56.789+01:30") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29t12:34:56.789z") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "1970-01-01T00:00:00Z") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseRFC3339(&tick, "2023-02-29T00:00:00Z") == invalidDay);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56") == badParse);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56Zjunk") == badParse);
    Require(sceRtcParseRFC3339(nullptr, "1970-01-01T00:00:00Z") == invalidPointer);

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

    RtcTick now{};
    Require(sceRtcGetCurrentTick(&now) == 0 && now.tick > leapDayTick);
    RtcTick local{};
    RtcTick back{};
    Require(sceRtcConvertUtcToLocalTime(&now, &local) == 0);
    Require(sceRtcConvertLocalTimeToUtc(&local, &back) == 0 && back.tick == now.tick);
}
