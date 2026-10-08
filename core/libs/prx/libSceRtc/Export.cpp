#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_RTC_ERROR_INVALID_POINTER = static_cast<int>(0x80B50002);
constexpr int SCE_RTC_ERROR_INVALID_VALUE = static_cast<int>(0x80B50003);
constexpr int SCE_RTC_ERROR_BAD_PARSE = static_cast<int>(0x80B50007);
constexpr int SCE_RTC_ERROR_INVALID_YEAR = static_cast<int>(0x80B50008);
constexpr int SCE_RTC_ERROR_INVALID_MONTH = static_cast<int>(0x80B50009);
constexpr int SCE_RTC_ERROR_INVALID_DAY = static_cast<int>(0x80B5000A);
constexpr int SCE_RTC_ERROR_INVALID_HOUR = static_cast<int>(0x80B5000B);
constexpr int SCE_RTC_ERROR_INVALID_MINUTE = static_cast<int>(0x80B5000C);
constexpr int SCE_RTC_ERROR_INVALID_SECOND = static_cast<int>(0x80B5000D);
constexpr int SCE_RTC_ERROR_INVALID_MICROSECOND = static_cast<int>(0x80B5000E);

constexpr std::int64_t TICKS_PER_SECOND = 1000000;
constexpr std::int64_t TICKS_PER_MINUTE = 60 * TICKS_PER_SECOND;
constexpr std::int64_t TICKS_PER_HOUR = 60 * TICKS_PER_MINUTE;
constexpr std::int64_t TICKS_PER_DAY = 24 * TICKS_PER_HOUR;
constexpr std::int64_t UNIX_EPOCH_DAYS = 719162;
constexpr std::int64_t WIN32_EPOCH_DAYS = 584388;
constexpr std::uint64_t UNIX_EPOCH_TICK = UNIX_EPOCH_DAYS * TICKS_PER_DAY;
constexpr std::uint64_t WIN32_EPOCH_TICK = WIN32_EPOCH_DAYS * TICKS_PER_DAY;
constexpr std::uint64_t MAX_TICK = 3652059ull * TICKS_PER_DAY - 1;

bool isLeapYear(std::int64_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(std::int64_t year, int month) {
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}

std::int64_t daysFromCivil(std::int64_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const auto yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468 + UNIX_EPOCH_DAYS;
}

void civilFromDays(std::int64_t days, std::int64_t& year, unsigned& month, unsigned& day) {
    days -= UNIX_EPOCH_DAYS - 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const auto dayOfEra = static_cast<unsigned>(days - era * 146097);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const unsigned shiftedMonth = (5 * dayOfYear + 2) / 153;
    day = dayOfYear - (153 * shiftedMonth + 2) / 5 + 1;
    month = shiftedMonth < 10 ? shiftedMonth + 3 : shiftedMonth - 9;
    year = static_cast<std::int64_t>(yearOfEra) + era * 400 + (month <= 2);
}

int validate(const RtcDateTime* time) {
    if (!time) return SCE_RTC_ERROR_INVALID_POINTER;
    if (time->year < 1 || time->year > 9999) return SCE_RTC_ERROR_INVALID_YEAR;
    if (time->month < 1 || time->month > 12) return SCE_RTC_ERROR_INVALID_MONTH;
    if (time->day < 1 || time->day > daysInMonth(time->year, time->month)) return SCE_RTC_ERROR_INVALID_DAY;
    if (time->hour > 23) return SCE_RTC_ERROR_INVALID_HOUR;
    if (time->minute > 59) return SCE_RTC_ERROR_INVALID_MINUTE;
    if (time->second > 59) return SCE_RTC_ERROR_INVALID_SECOND;
    if (time->microsecond >= TICKS_PER_SECOND) return SCE_RTC_ERROR_INVALID_MICROSECOND;
    return 0;
}

std::uint64_t toTick(const RtcDateTime& time) {
    return static_cast<std::uint64_t>(daysFromCivil(time.year, time.month, time.day)) * TICKS_PER_DAY
        + time.hour * TICKS_PER_HOUR + time.minute * TICKS_PER_MINUTE + time.second * TICKS_PER_SECOND + time.microsecond;
}

RtcDateTime fromTick(std::uint64_t tick) {
    std::int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civilFromDays(static_cast<std::int64_t>(tick / TICKS_PER_DAY), year, month, day);
    const std::uint64_t timeOfDay = tick % TICKS_PER_DAY;
    return RtcDateTime{
        static_cast<std::uint16_t>(year), static_cast<std::uint16_t>(month), static_cast<std::uint16_t>(day),
        static_cast<std::uint16_t>(timeOfDay / TICKS_PER_HOUR), static_cast<std::uint16_t>(timeOfDay % TICKS_PER_HOUR / TICKS_PER_MINUTE),
        static_cast<std::uint16_t>(timeOfDay % TICKS_PER_MINUTE / TICKS_PER_SECOND), static_cast<std::uint32_t>(timeOfDay % TICKS_PER_SECOND)};
}

std::uint64_t currentTick() {
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return UNIX_EPOCH_TICK + static_cast<std::uint64_t>(now);
}

std::int64_t localOffsetSeconds(std::uint64_t utcTick) {
    const auto seconds = static_cast<std::time_t>((static_cast<std::int64_t>(utcTick) - static_cast<std::int64_t>(UNIX_EPOCH_TICK)) / TICKS_PER_SECOND);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &seconds) != 0) return 0;
#else
    if (!localtime_r(&seconds, &local)) return 0;
#endif
    const std::int64_t localSeconds = (daysFromCivil(local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1), static_cast<unsigned>(local.tm_mday)) - UNIX_EPOCH_DAYS) * 86400
        + local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
    return localSeconds - static_cast<std::int64_t>(seconds);
}

constexpr const char* WEEKDAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char* MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int localOffsetMinutes(const RtcTick& utc) {
    return static_cast<int>(localOffsetSeconds(utc.tick) / 60);
}

int addTicks(RtcTick* dst, const RtcTick* src, std::int64_t count, std::int64_t unit) {
    if (!dst || !src) return SCE_RTC_ERROR_INVALID_POINTER;
    if (src->tick > MAX_TICK) return SCE_RTC_ERROR_INVALID_VALUE;
    if (count != 0 && (count > std::numeric_limits<std::int64_t>::max() / unit || count < std::numeric_limits<std::int64_t>::min() / unit)) return SCE_RTC_ERROR_INVALID_VALUE;
    const std::int64_t delta = count * unit;
    if (delta < 0 ? src->tick < static_cast<std::uint64_t>(-(delta + 1)) + 1 : MAX_TICK - src->tick < static_cast<std::uint64_t>(delta)) return SCE_RTC_ERROR_INVALID_VALUE;
    dst->tick = src->tick + static_cast<std::uint64_t>(delta);
    return 0;
}

int addMonths(RtcTick* dst, const RtcTick* src, std::int64_t months) {
    if (!dst || !src) return SCE_RTC_ERROR_INVALID_POINTER;
    if (src->tick > MAX_TICK) return SCE_RTC_ERROR_INVALID_VALUE;
    RtcDateTime time = fromTick(src->tick);
    const std::int64_t monthIndex = time.year * 12 + (time.month - 1) + months;
    const std::int64_t year = monthIndex / 12;
    if (monthIndex < 12 || year > 9999) return SCE_RTC_ERROR_INVALID_VALUE;
    time.year = static_cast<std::uint16_t>(year);
    time.month = static_cast<std::uint16_t>(monthIndex % 12 + 1);
    const int lastDay = daysInMonth(time.year, time.month);
    if (time.day > lastDay) time.day = static_cast<std::uint16_t>(lastDay);
    dst->tick = toTick(time);
    return 0;
}

bool parseDigits(const char*& cursor, int count, int& value) {
    value = 0;
    for (int index = 0; index < count; ++index, ++cursor) {
        if (*cursor < '0' || *cursor > '9') return false;
        value = value * 10 + (*cursor - '0');
    }
    return true;
}

RtcDateTime makeDateTime(int year, int month, int day, int hour, int minute, int second, std::uint32_t microsecond) {
    return RtcDateTime{static_cast<std::uint16_t>(year), static_cast<std::uint16_t>(month), static_cast<std::uint16_t>(day),
        static_cast<std::uint16_t>(hour), static_cast<std::uint16_t>(minute), static_cast<std::uint16_t>(second), microsecond};
}

int storeParsedTick(RtcTick* utc, const RtcDateTime& time, std::int64_t offsetMinutes) {
    if (validate(&time) == 0) utc->tick = toTick(time);
    utc->tick -= static_cast<std::uint64_t>(offsetMinutes * TICKS_PER_MINUTE);
    return 0;
}

int parseRfc3339(RtcTick* utc, const char* cursor) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!parseDigits(cursor, 4, year) || *cursor++ != '-' || !parseDigits(cursor, 2, month) || *cursor++ != '-' || !parseDigits(cursor, 2, day)) return SCE_RTC_ERROR_BAD_PARSE;
    if (*cursor != 'T' && *cursor != 't') return SCE_RTC_ERROR_BAD_PARSE;
    ++cursor;
    if (!parseDigits(cursor, 2, hour) || *cursor++ != ':' || !parseDigits(cursor, 2, minute) || *cursor++ != ':' || !parseDigits(cursor, 2, second)) return SCE_RTC_ERROR_BAD_PARSE;
    std::uint32_t microsecond = 0;
    if (*cursor == '.') {
        ++cursor;
        for (std::uint32_t scale = 100000; *cursor >= '0' && *cursor <= '9'; ++cursor, scale /= 10) microsecond += static_cast<std::uint32_t>(*cursor - '0') * scale;
    }
    std::int64_t offsetMinutes = 0;
    if (*cursor == '+' || *cursor == '-') {
        const int sign = *cursor++ == '-' ? -1 : 1;
        int offsetHours = 0, offsetMinute = 0;
        if (!parseDigits(cursor, 2, offsetHours) || *cursor++ != ':' || !parseDigits(cursor, 2, offsetMinute)) return SCE_RTC_ERROR_BAD_PARSE;
        offsetMinutes = sign * (offsetHours * 60 + offsetMinute);
    } else if (*cursor != 'Z' && *cursor != 'z') {
        return SCE_RTC_ERROR_BAD_PARSE;
    }
    const RtcDateTime time = makeDateTime(year, month, day, hour, minute, second, microsecond);
    if (const int result = validate(&time); result != 0 && (result != SCE_RTC_ERROR_INVALID_SECOND || second != 60)) return result;
    return storeParsedTick(utc, time, offsetMinutes);
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

char toUpper(char c) {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

int readDigits(const char*& cursor, int maxDigits, int& value) {
    value = 0;
    int count = 0;
    for (; count < maxDigits && isDigit(*cursor); ++count, ++cursor) value = value * 10 + (*cursor - '0');
    return count;
}

bool matchWord(const char*& cursor, const char* word, std::size_t length) {
    for (std::size_t index = 0; index < length; ++index) {
        if (toUpper(cursor[index]) != word[index]) return false;
    }
    cursor += length;
    return true;
}

bool parseName(const char*& cursor, const char* const* names, int count, int& index) {
    for (index = 0; index < count; ++index) {
        if (matchWord(cursor, names[index], std::strlen(names[index])) || matchWord(cursor, names[index], 3)) return true;
    }
    return false;
}

bool parseWeekday(const char*& cursor) {
    static constexpr const char* names[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
    int index = 0;
    return parseName(cursor, names, 7, index);
}

bool parseMonth(const char*& cursor, int& month) {
    static constexpr const char* names[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
    if (!parseName(cursor, names, 12, month)) return false;
    ++month;
    return true;
}

bool parseZoneName(const char* cursor, std::int64_t& offsetMinutes) {
    static constexpr struct {
        const char* name;
        std::int64_t minutes;
    } zones[] = {
        {"GMT", 0}, {"WET", 0}, {"BST", 60}, {"WAT", -60}, {"AST", -240}, {"ADT", -180}, {"EST", -300}, {"EDT", -240},
        {"CST", -360}, {"CDT", -300}, {"MST", -420}, {"MDT", -360}, {"PST", -480}, {"PDT", -420}, {"YST", -540}, {"YDT", -480},
        {"HDT", -540}, {"CAT", -600}, {"AHST", -600}, {"NST", -150}, {"NDT", -90}, {"IDLW", -720}, {"CET", 60}, {"MET", 60},
        {"MEWT", 60}, {"MEST", 120}, {"CEST", 120}, {"SST", 120}, {"FWT", 120}, {"FST", 60}, {"EET", 120}, {"IST", 120},
        {"WAST", 420}, {"WADT", 480}, {"CCT", 480}, {"AWST", 480}, {"JST", 540}, {"KST", 540}, {"ACST", 570}, {"EAST", 600},
        {"GST", 600}, {"AEST", 600}, {"NZT", 720}, {"NZST", 720}, {"NZDT", 780}, {"IDLE", 720}, {"JT", 450},
    };
    for (const auto& zone : zones) {
        const char* name = cursor;
        if (matchWord(name, zone.name, std::strlen(zone.name))) {
            offsetMinutes = zone.minutes;
            return true;
        }
    }
    return false;
}

bool parseZone(const char* cursor, std::int64_t& offsetMinutes) {
    offsetMinutes = 0;
    if (*cursor == 0) return true;
    if (*cursor == '+' || *cursor == '-') {
        const std::int64_t sign = *cursor++ == '-' ? -1 : 1;
        int hours = 0, minutes = 0;
        if (parseDigits(cursor, 2, hours) && parseDigits(cursor, 2, minutes)) offsetMinutes = sign * (hours * 60 + minutes);
        return true;
    }
    const char letter = toUpper(*cursor);
    if (letter < 'A' || letter > 'Z') return false;
    if (cursor[0] == 'U' || cursor[1] == 'T' || parseZoneName(cursor, offsetMinutes)) return true;
    if (letter == 'J') return false;
    if (letter <= 'M') offsetMinutes = (letter - 'A') * 60;
    else if (letter < 'Z') offsetMinutes = ('N' - letter) * 60;
    return true;
}

bool parseRfc2822DateTime(const char* cursor, RtcDateTime& time, std::int64_t& offsetMinutes) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (readDigits(cursor, 2, day) == 0 || (*cursor != ' ' && *cursor != '-')) return false;
    ++cursor;
    if (!parseMonth(cursor, month) || (*cursor != ' ' && *cursor != '-')) return false;
    ++cursor;
    const int yearDigits = readDigits(cursor, 4, year);
    if ((yearDigits != 2 && yearDigits != 4) || *cursor++ != ' ') return false;
    if (yearDigits == 2) year += year < 50 ? 2000 : 1900;
    if (readDigits(cursor, 2, hour) == 0 || hour > 25 || *cursor++ != ':' || readDigits(cursor, 2, minute) == 0) return false;
    offsetMinutes = 0;
    if (*cursor == ':') {
        ++cursor;
        if (readDigits(cursor, 2, second) == 0) return false;
        if (*cursor == ' ' && !parseZone(cursor + 1, offsetMinutes)) return false;
    } else if (*cursor != 0 && *cursor != ' ') {
        return false;
    }
    time = makeDateTime(year, month, day, hour, minute, second, 0);
    return true;
}

bool parseAsctimeDateTime(const char* cursor, RtcDateTime& time) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!parseMonth(cursor, month) || *cursor++ != ' ') return false;
    if (*cursor == ' ') {
        ++cursor;
        if (readDigits(cursor, 1, day) == 0) return false;
    } else if (readDigits(cursor, 2, day) == 0) {
        return false;
    }
    if (*cursor++ != ' ' || readDigits(cursor, 2, hour) == 0 || *cursor++ != ':' || readDigits(cursor, 2, minute) == 0 || *cursor++ != ':') return false;
    if (readDigits(cursor, 2, second) == 0 || *cursor++ != ' ' || readDigits(cursor, 4, year) != 4) return false;
    time = makeDateTime(year, month, day, hour, minute, second, 0);
    return true;
}

}

extern "C" {

int APS5_VABI sceRtcCheckValid(const RtcDateTime* time) {
    return validate(time);
}

int APS5_VABI sceRtcIsLeapYear(int year) {
    if (year < 1) return SCE_RTC_ERROR_INVALID_YEAR;
    return isLeapYear(year) ? 1 : 0;
}

int APS5_VABI sceRtcGetDaysInMonth(int year, int month) {
    if (year < 1) return SCE_RTC_ERROR_INVALID_YEAR;
    if (month < 1 || month > 12) return SCE_RTC_ERROR_INVALID_MONTH;
    return daysInMonth(year, month);
}

int APS5_VABI sceRtcGetDayOfWeek(int year, int month, int day) {
    if (year < 1 || year > 9999) return SCE_RTC_ERROR_INVALID_YEAR;
    if (month < 1 || month > 12) return SCE_RTC_ERROR_INVALID_MONTH;
    if (day < 1 || day > daysInMonth(year, month)) return SCE_RTC_ERROR_INVALID_DAY;
    return static_cast<int>((daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) + 1) % 7);
}

int APS5_VABI sceRtcGetTickResolution(void) {
    return static_cast<int>(TICKS_PER_SECOND);
}

int APS5_VABI sceRtcGetTick(const RtcDateTime* time, RtcTick* tick) {
    if (!tick) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const int result = validate(time); result != 0) return result;
    tick->tick = toTick(*time);
    return 0;
}

int APS5_VABI sceRtcSetTick(RtcDateTime* time, const RtcTick* tick) {
    if (!time || !tick) return SCE_RTC_ERROR_INVALID_POINTER;
    if (tick->tick > MAX_TICK) return SCE_RTC_ERROR_INVALID_VALUE;
    *time = fromTick(tick->tick);
    return 0;
}

int APS5_VABI sceRtcGetCurrentTick(RtcTick* tick) {
    if (!tick) return SCE_RTC_ERROR_INVALID_POINTER;
    tick->tick = currentTick();
    return 0;
}

int APS5_VABI sceRtcGetCurrentNetworkTick(RtcTick* tick) {
    return sceRtcGetCurrentTick(tick);
}

int APS5_VABI sceRtcGetCurrentClock(RtcDateTime* time, int time_zone_minutes) {
    if (!time) return SCE_RTC_ERROR_INVALID_POINTER;
    RtcTick tick{currentTick()};
    if (const int result = addTicks(&tick, &tick, time_zone_minutes, TICKS_PER_MINUTE); result != 0) return result;
    *time = fromTick(tick.tick);
    return 0;
}

int APS5_VABI sceRtcGetCurrentClockLocalTime(RtcDateTime* time) {
    if (!time) return SCE_RTC_ERROR_INVALID_POINTER;
    const std::uint64_t utc = currentTick();
    *time = fromTick(utc + static_cast<std::uint64_t>(localOffsetSeconds(utc) * TICKS_PER_SECOND));
    return 0;
}

int APS5_VABI sceRtcConvertUtcToLocalTime(const RtcTick* utc, RtcTick* local_time) {
    if (!utc || !local_time) return SCE_RTC_ERROR_INVALID_POINTER;
    return addTicks(local_time, utc, localOffsetSeconds(utc->tick), TICKS_PER_SECOND);
}

int APS5_VABI sceRtcConvertLocalTimeToUtc(const RtcTick* local_time, RtcTick* utc) {
    if (!local_time || !utc) return SCE_RTC_ERROR_INVALID_POINTER;
    const std::int64_t offset = localOffsetSeconds(local_time->tick - static_cast<std::uint64_t>(localOffsetSeconds(local_time->tick) * TICKS_PER_SECOND));
    return addTicks(utc, local_time, -offset, TICKS_PER_SECOND);
}

int APS5_VABI sceRtcGetTime_t(const RtcDateTime* time, int64_t* seconds) {
    if (!seconds) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const int result = validate(time); result != 0) return result;
    const std::uint64_t tick = toTick(*time);
    if (tick < UNIX_EPOCH_TICK) return SCE_RTC_ERROR_INVALID_VALUE;
    *seconds = static_cast<int64_t>((tick - UNIX_EPOCH_TICK) / TICKS_PER_SECOND);
    return 0;
}

int APS5_VABI sceRtcSetTime_t(RtcDateTime* time, int64_t seconds) {
    if (!time) return SCE_RTC_ERROR_INVALID_POINTER;
    if (seconds < 0 || static_cast<std::uint64_t>(seconds) > (MAX_TICK - UNIX_EPOCH_TICK) / TICKS_PER_SECOND) return SCE_RTC_ERROR_INVALID_VALUE;
    *time = fromTick(UNIX_EPOCH_TICK + static_cast<std::uint64_t>(seconds) * TICKS_PER_SECOND);
    return 0;
}

int APS5_VABI sceRtcGetWin32FileTime(const RtcDateTime* time, uint64_t* win32_time) {
    if (!win32_time) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const int result = validate(time); result != 0) return result;
    const std::uint64_t tick = toTick(*time);
    if (tick < WIN32_EPOCH_TICK) return SCE_RTC_ERROR_INVALID_VALUE;
    *win32_time = (tick - WIN32_EPOCH_TICK) * 10;
    return 0;
}

int APS5_VABI sceRtcSetWin32FileTime(RtcDateTime* time, uint64_t win32_time) {
    if (!time) return SCE_RTC_ERROR_INVALID_POINTER;
    if (win32_time / 10 > MAX_TICK - WIN32_EPOCH_TICK) return SCE_RTC_ERROR_INVALID_VALUE;
    *time = fromTick(WIN32_EPOCH_TICK + win32_time / 10);
    return 0;
}

int APS5_VABI sceRtcGetDosTime(const RtcDateTime* time, uint32_t* dos_time) {
    if (!dos_time) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const int result = validate(time); result != 0) return result;
    if (time->year < 1980 || time->year > 2107) {
        *dos_time = time->year < 1980 ? 0 : 0xff9fbf7du;
        return SCE_RTC_ERROR_INVALID_YEAR;
    }
    *dos_time = static_cast<uint32_t>(time->year - 1980) << 25 | static_cast<uint32_t>(time->month) << 21
        | static_cast<uint32_t>(time->day) << 16 | static_cast<uint32_t>(time->hour) << 11
        | static_cast<uint32_t>(time->minute) << 5 | static_cast<uint32_t>(time->second / 2);
    return 0;
}

int APS5_VABI sceRtcSetDosTime(RtcDateTime* time, uint32_t dos_time) {
    if (!time) return SCE_RTC_ERROR_INVALID_POINTER;
    time->year = static_cast<std::uint16_t>(1980 + (dos_time >> 25));
    time->month = static_cast<std::uint16_t>(dos_time >> 21 & 0x0f);
    time->day = static_cast<std::uint16_t>(dos_time >> 16 & 0x1f);
    time->hour = static_cast<std::uint16_t>(dos_time >> 11 & 0x1f);
    time->minute = static_cast<std::uint16_t>(dos_time >> 5 & 0x3f);
    time->second = static_cast<std::uint16_t>((dos_time & 0x1f) * 2);
    time->microsecond = 0;
    return 0;
}

int APS5_VABI sceRtcFormatRFC3339(char* date_time, const RtcTick* utc, int time_zone_minutes) {
    if (!date_time || !utc) return SCE_RTC_ERROR_INVALID_POINTER;
    if (time_zone_minutes < -1439 || time_zone_minutes > 1439) return SCE_RTC_ERROR_INVALID_VALUE;
    RtcTick local{};
    if (const int result = addTicks(&local, utc, time_zone_minutes, TICKS_PER_MINUTE); result != 0) return result;
    const RtcDateTime time = fromTick(local.tick);
    int written = std::snprintf(date_time, 23, "%04u-%02u-%02uT%02u:%02u:%02u.%02u", time.year, time.month, time.day, time.hour, time.minute, time.second, time.microsecond / 10000);
    if (time_zone_minutes == 0) {
        std::snprintf(date_time + written, 2, "Z");
    } else {
        const int offset = time_zone_minutes < 0 ? -time_zone_minutes : time_zone_minutes;
        std::snprintf(date_time + written, 7, "%c%02d:%02d", time_zone_minutes < 0 ? '-' : '+', offset / 60, offset % 60);
    }
    return 0;
}

int APS5_VABI sceRtcFormatRFC3339LocalTime(char* date_time, const RtcTick* utc) {
    if (!date_time || !utc) return SCE_RTC_ERROR_INVALID_POINTER;
    return sceRtcFormatRFC3339(date_time, utc, localOffsetMinutes(*utc));
}

int APS5_VABI sceRtcFormatRFC2822(char* date_time, const RtcTick* utc, int time_zone_minutes) {
    if (!date_time || !utc) return SCE_RTC_ERROR_INVALID_POINTER;
    if (time_zone_minutes < -1439 || time_zone_minutes > 1439) return SCE_RTC_ERROR_INVALID_VALUE;
    RtcTick local{};
    if (const int result = addTicks(&local, utc, time_zone_minutes, TICKS_PER_MINUTE); result != 0) return result;
    const RtcDateTime time = fromTick(local.tick);
    const int offset = time_zone_minutes < 0 ? -time_zone_minutes : time_zone_minutes;
    std::snprintf(date_time, 32, "%s, %02u %s %04u %02u:%02u:%02u %c%02d%02d", WEEKDAY_NAMES[(local.tick / TICKS_PER_DAY + 1) % 7], time.day,
        MONTH_NAMES[time.month - 1], time.year, time.hour, time.minute, time.second, time_zone_minutes < 0 ? '-' : '+', offset / 60, offset % 60);
    return 0;
}

int APS5_VABI sceRtcFormatRFC2822LocalTime(char* date_time, const RtcTick* utc) {
    if (!date_time || !utc) return SCE_RTC_ERROR_INVALID_POINTER;
    return sceRtcFormatRFC2822(date_time, utc, localOffsetMinutes(*utc));
}

int APS5_VABI sceRtcParseRFC3339(RtcTick* utc, const char* date_time) {
    if (!utc || !date_time) return SCE_RTC_ERROR_INVALID_POINTER;
    return parseRfc3339(utc, date_time);
}

int APS5_VABI sceRtcParseDateTime(RtcTick* utc, const char* date_time) {
    if (!utc || !date_time) return SCE_RTC_ERROR_INVALID_POINTER;
    const char* cursor = date_time;
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    if (isDigit(*cursor)) return parseRfc3339(utc, cursor);
    if (!parseWeekday(cursor)) return SCE_RTC_ERROR_BAD_PARSE;
    if (*cursor == ',') ++cursor;
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    RtcDateTime time{};
    std::int64_t offsetMinutes = 0;
    if (isDigit(*cursor) ? !parseRfc2822DateTime(cursor, time, offsetMinutes) : !parseAsctimeDateTime(cursor, time)) return SCE_RTC_ERROR_BAD_PARSE;
    return storeParsedTick(utc, time, offsetMinutes);
}

int APS5_VABI sceRtcTickAddTicks(RtcTick* dst, const RtcTick* src, int64_t ticks) {
    return addTicks(dst, src, ticks, 1);
}

int APS5_VABI sceRtcTickAddMicroseconds(RtcTick* dst, const RtcTick* src, int64_t usec) {
    return addTicks(dst, src, usec, 1);
}

int APS5_VABI sceRtcTickAddSeconds(RtcTick* dst, const RtcTick* src, int64_t seconds) {
    return addTicks(dst, src, seconds, TICKS_PER_SECOND);
}

int APS5_VABI sceRtcTickAddMinutes(RtcTick* dst, const RtcTick* src, int64_t minutes) {
    return addTicks(dst, src, minutes, TICKS_PER_MINUTE);
}

int APS5_VABI sceRtcTickAddHours(RtcTick* dst, const RtcTick* src, int32_t hours) {
    return addTicks(dst, src, hours, TICKS_PER_HOUR);
}

int APS5_VABI sceRtcTickAddDays(RtcTick* dst, const RtcTick* src, int32_t days) {
    return addTicks(dst, src, days, TICKS_PER_DAY);
}

int APS5_VABI sceRtcTickAddWeeks(RtcTick* dst, const RtcTick* src, int32_t weeks) {
    return addTicks(dst, src, weeks, 7 * TICKS_PER_DAY);
}

int APS5_VABI sceRtcTickAddMonths(RtcTick* dst, const RtcTick* src, int32_t months) {
    return addMonths(dst, src, months);
}

int APS5_VABI sceRtcTickAddYears(RtcTick* dst, const RtcTick* src, int16_t years) {
    return addMonths(dst, src, static_cast<std::int64_t>(years) * 12);
}

}
