#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cstring>
#include <limits>

#ifdef _WIN32
#include <cstdio>
#include <string>
#include <windows.h>
#endif

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestTime.hpp"

namespace {

constexpr std::int64_t SECONDS_PER_DAY = 86400;
constexpr std::int64_t DAYS_FROM_MARCH_TO_YEAR_END = 306;

void copyCalendarFields(const std::tm& host, GuestTm& guest) {
    guest.tm_sec = host.tm_sec;
    guest.tm_min = host.tm_min;
    guest.tm_hour = host.tm_hour;
    guest.tm_mday = host.tm_mday;
    guest.tm_mon = host.tm_mon;
    guest.tm_year = host.tm_year;
    guest.tm_wday = host.tm_wday;
    guest.tm_yday = host.tm_yday;
    guest.tm_isdst = host.tm_isdst;
}

std::tm toHostTm(const GuestTm& guest) {
    std::tm host{};
    host.tm_sec = guest.tm_sec;
    host.tm_min = guest.tm_min;
    host.tm_hour = guest.tm_hour;
    host.tm_mday = guest.tm_mday;
    host.tm_mon = guest.tm_mon;
    host.tm_year = guest.tm_year;
    host.tm_wday = guest.tm_wday;
    host.tm_yday = guest.tm_yday;
    host.tm_isdst = guest.tm_isdst;
#ifndef _WIN32
    host.tm_gmtoff = static_cast<long>(guest.tm_gmtoff);
    host.tm_zone = guest.tm_zone;
#endif
    return host;
}

#ifdef _WIN32
std::string windowsTimeFormat(const char* format, const GuestTm& guest) {
    std::string result;
    while (*format != '\0') {
        const char next = *format++;
        if (next != '%' || *format == '\0') {
            result += next;
            continue;
        }
        const char* directive = format;
        if (*format == '#' && format[1] != '\0') ++format;
        const char specifier = *format++;
        if (specifier == 'z') {
            if (guest.tm_isdst < 0) continue;
            const std::int64_t minutes = guest.tm_gmtoff / 60;
            const auto magnitude = static_cast<unsigned long long>(minutes < 0 ? -minutes : minutes);
            char offset[32];
            std::snprintf(offset, sizeof(offset), "%c%02llu%02llu", guest.tm_gmtoff < 0 ? '-' : '+',
                magnitude / 60, magnitude % 60);
            result += offset;
        } else if (specifier == 'Z' && guest.tm_zone != nullptr) {
            for (const char* zone = guest.tm_zone; *zone != '\0'; ++zone) {
                if (*zone == '%') result += '%';
                result += *zone;
            }
        } else if (specifier != 'Z' || guest.tm_isdst >= 0) {
            result += '%';
            result.append(directive, format);
        }
    }
    return result;
}
#endif

bool isLeapYear(std::int64_t year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool utcCalendarFields(std::int64_t time, GuestTm& result) {
    std::int64_t days = time / SECONDS_PER_DAY;
    std::int64_t secondOfDay = time % SECONDS_PER_DAY;
    if (secondOfDay < 0) {
        secondOfDay += SECONDS_PER_DAY;
        --days;
    }
    const std::int64_t shifted = days + 719468;
    const std::int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    const std::int64_t dayOfEra = shifted - era * 146097;
    const std::int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const std::int64_t dayOfMarchYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const std::int64_t marchMonth = (5 * dayOfMarchYear + 2) / 153;
    const std::int64_t month = marchMonth < 10 ? marchMonth + 3 : marchMonth - 9;
    const std::int64_t year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
    const std::int64_t tmYear = year - 1900;
    if (tmYear < std::numeric_limits<int>::min() || tmYear > std::numeric_limits<int>::max()) return false;
    result.tm_sec = static_cast<int>(secondOfDay % 60);
    result.tm_min = static_cast<int>(secondOfDay / 60 % 60);
    result.tm_hour = static_cast<int>(secondOfDay / 3600);
    result.tm_mday = static_cast<int>(dayOfMarchYear - (153 * marchMonth + 2) / 5 + 1);
    result.tm_mon = static_cast<int>(month - 1);
    result.tm_year = static_cast<int>(tmYear);
    result.tm_wday = static_cast<int>((days % 7 + 11) % 7);
    result.tm_yday = static_cast<int>(month <= 2
        ? dayOfMarchYear - DAYS_FROM_MARCH_TO_YEAR_END
        : dayOfMarchYear + 59 + (isLeapYear(year) ? 1 : 0));
    result.tm_isdst = 0;
    return true;
}

}

#ifdef __linux__
#include <cstdlib>

namespace {

const bool timeZoneFixed = [] {
    if (std::getenv("TZ") == nullptr) ::setenv("TZ", ":/etc/localtime", 0);
    ::tzset();
    return true;
}();

}  // namespace
#endif

extern "C" {

GuestTm* APS5_VABI localtime_s_nid_postfix(const int64_t* timer, GuestTm* result);
GuestTm* APS5_VABI gmtime_s_nid_postfix(const int64_t* timer, GuestTm* result);

int64_t APS5_VABI libc_time_nid_postfix(int64_t* timer) {
    std::time_t t = std::time(nullptr);
    if (timer != nullptr) *timer = static_cast<int64_t>(t);
    return static_cast<int64_t>(t);
}

int64_t APS5_VABI time_nid_postfix(int64_t* timer) {
    return libc_time_nid_postfix(timer);
}

int64_t APS5_VABI _Xtime_get_ticks_nid_postfix() {
#ifdef _WIN32
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    const uint64_t t = ((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) - 116444736000000000ULL;
    return static_cast<int64_t>(t / 10);
#else
    timespec now{};
    clock_gettime(CLOCK_REALTIME, &now);
    return static_cast<int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
#endif
}

double APS5_VABI libc_difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

double APS5_VABI difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

GuestTm* APS5_VABI libc_gmtime_nid_postfix(const int64_t* timer) {
    static thread_local GuestTm result;
    return gmtime_s_nid_postfix(timer, &result);
}

GuestTm* APS5_VABI libc_localtime_nid_postfix(const int64_t* timer) {
    static thread_local GuestTm result;
    return localtime_s_nid_postfix(timer, &result);
}

GuestTm* APS5_VABI localtime_nid_postfix(const int64_t* timer) {
    return libc_localtime_nid_postfix(timer);
}

GuestTm* APS5_VABI localtime_s_nid_postfix(const int64_t* timer, GuestTm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    const std::time_t t = static_cast<std::time_t>(*timer);
    std::tm host{};
#ifdef _WIN32
    if (localtime_s(&host, &t) != 0) return nullptr;
    copyCalendarFields(host, *result);
    long standardBias = 0;
    long daylightBias = 0;
    _get_timezone(&standardBias);
    _get_dstbias(&daylightBias);
    const bool daylight = host.tm_isdst > 0;
    result->tm_gmtoff = -static_cast<std::int64_t>(standardBias + (daylight ? daylightBias : 0));
    result->tm_zone = _tzname[daylight ? 1 : 0];
#else
    if (localtime_r(&t, &host) == nullptr) return nullptr;
    copyCalendarFields(host, *result);
    result->tm_gmtoff = host.tm_gmtoff;
    result->tm_zone = host.tm_zone;
#endif
    return result;
}

GuestTm* APS5_VABI gmtime_s_nid_postfix(const int64_t* timer, GuestTm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    if (!utcCalendarFields(*timer, *result)) return nullptr;
    result->tm_gmtoff = 0;
    result->tm_zone = "UTC";
    return result;
}

GuestTm* APS5_VABI gmtime_nid_postfix(const int64_t* timer) {
    return libc_gmtime_nid_postfix(timer);
}

int64_t APS5_VABI libc_mktime_nid_postfix(GuestTm* timeptr) {
    std::tm host = toHostTm(*timeptr);
    const std::time_t t = std::mktime(&host);
    if (t == static_cast<std::time_t>(-1)) return -1;
    const int64_t time = static_cast<int64_t>(t);
    localtime_s_nid_postfix(&time, timeptr);
    return time;
}

int64_t APS5_VABI mktime_nid_postfix(GuestTm* timeptr) {
    return libc_mktime_nid_postfix(timeptr);
}

size_t APS5_VABI libc_strftime_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    const std::tm host = toHostTm(*timeptr);
#ifdef _WIN32
    const std::string hostFormat = windowsTimeFormat(format, *timeptr);
    return std::strftime(str, count, hostFormat.c_str(), &host);
#else
    return std::strftime(str, count, format, &host);
#endif
}

char* APS5_VABI asctime_nid_postfix(const GuestTm* timeptr) {
    const std::tm host = toHostTm(*timeptr);
    return std::asctime(&host);
}

size_t APS5_VABI strftime_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    return libc_strftime_nid_postfix(str, count, format, timeptr);
}

// The guest's CLOCKS_PER_SEC is 1000000: clock() reports process CPU time in microseconds.
int64_t APS5_VABI clock_nid_postfix() {
#ifdef _WIN32
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return -1;
    const auto toTicks = [](const FILETIME& time) { return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime; };
    return static_cast<int64_t>((toTicks(kernel) + toTicks(user)) / 10);
#else
    return static_cast<int64_t>(static_cast<double>(std::clock()) * 1000000.0 / CLOCKS_PER_SEC);
#endif
}

}
