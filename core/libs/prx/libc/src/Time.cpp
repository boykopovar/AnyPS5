#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/SonyCrt.hpp"

extern "C" char* tzname_nid_postfix[2];
extern "C" int64_t timezone_nid_postfix;

namespace {

constexpr const char* WEEKDAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char* MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr std::size_t ASCTIME_SIZE = sizeof("Sun Jan-2147483648 -2147483648:-2147483648:-2147483648     -2147481748\n");
constexpr std::int64_t SECONDS_PER_DAY = 86400;
constexpr std::int64_t DAYS_FROM_MARCH_TO_YEAR_END = 306;
#if defined(_WIN32) || defined(__APPLE__)
constexpr std::int64_t CALENDAR_CYCLE_YEARS = 400;
constexpr std::int64_t CALENDAR_CYCLE_SECONDS = 146097 * SECONDS_PER_DAY;
#ifdef _WIN32
constexpr int HOST_ORDINARY_FIRST_YEAR = 70;
constexpr int HOST_ORDINARY_LAST_YEAR = 1100;
constexpr std::int64_t HOST_FIRST_SECOND = 0;
constexpr std::int64_t HOST_LAST_SECOND = 32535215999;
#else
constexpr int HOST_ORDINARY_FIRST_YEAR = 0;
constexpr int HOST_ORDINARY_LAST_YEAR = std::numeric_limits<int>::max();
constexpr std::int64_t HOST_FIRST_SECOND = -2208988800;
constexpr std::int64_t HOST_LAST_SECOND = std::numeric_limits<std::int64_t>::max();
#endif

std::int64_t calendarCycles(std::int64_t value, std::int64_t cycle) {
    return value >= 0 ? value / cycle : -((-(value + 1)) / cycle + 1);
}

#ifdef __APPLE__
bool timeOfLocalSeconds(std::int64_t local, std::int64_t& time) {
    if (local < std::numeric_limits<std::int64_t>::min() + SECONDS_PER_DAY) return false;
    time = local;
    for (int step = 0; step < 4; ++step) {
        const std::time_t probe = static_cast<std::time_t>(time);
        std::tm fields{};
        if (localtime_r(&probe, &fields) == nullptr) return false;
        const std::int64_t next = local - fields.tm_gmtoff;
        if (next == time) return true;
        time = next;
    }
    return true;
}
#endif

std::int64_t utcSeconds(const std::tm& time) {
    const std::int64_t years = calendarCycles(time.tm_mon, 12);
    const std::int64_t month = time.tm_mon - years * 12 + 1;
    const std::int64_t year = std::int64_t{time.tm_year} + 1900 + years - (month <= 2 ? 1 : 0);
    const std::int64_t era = calendarCycles(year, CALENDAR_CYCLE_YEARS);
    const std::int64_t yearOfEra = year - era * CALENDAR_CYCLE_YEARS;
    const std::int64_t dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const std::int64_t days = era * 146097 + dayOfEra - 719468 + time.tm_mday - 1;
    return days * SECONDS_PER_DAY + std::int64_t{time.tm_hour} * 3600 + std::int64_t{time.tm_min} * 60 + time.tm_sec;
}
#endif

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
#ifdef __APPLE__
    host.tm_zone = const_cast<char*>(guest.tm_zone);
#else
    host.tm_zone = guest.tm_zone;
#endif
#endif
    return host;
}

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

namespace {

bool setHostZone(const char* zone) {
#ifdef _WIN32
    return _putenv_s("TZ", zone == nullptr ? "" : zone) == 0;
#else
    return zone == nullptr ? ::unsetenv("TZ") == 0 : ::setenv("TZ", zone, 1) == 0;
#endif
}

const char* hostZoneFor(const char* guest) {
#ifdef _WIN32
    return guest != nullptr && guest[0] == 0 ? "UTC0" : guest;
#else
    return guest == nullptr ? ":/etc/localtime" : guest;
#endif
}

std::recursive_mutex& zoneMutex() {
    static std::recursive_mutex mutex;
    return mutex;
}

char** HostZoneNames();
char* GuestZoneName(int index);

void applyGuestZone() {
    static bool initialized = false;
    static bool present = false;
    static std::string current;
    if (!initialized) {
        const char* host = std::getenv("TZ");
        present = host != nullptr;
        if (present) current = host;
        initialized = true;
    }
    std::string guest;
    const bool guestPresent = CopyGuestEnvironment_nid_no_patch("TZ", guest);
    if (guestPresent == present && (!guestPresent || current == guest)) return;
    if (!setHostZone(hostZoneFor(guestPresent ? guest.c_str() : nullptr))) throw std::runtime_error("tzset: the host rejected the guest TZ");
    present = guestPresent;
    current = guest;
#ifdef _WIN32
    _tzset();
#else
    ::tzset();
#endif
    tzname_nid_postfix[0] = GuestZoneName(0);
    tzname_nid_postfix[1] = GuestZoneName(1);
}

int& zoneDepth() {
    static thread_local int depth = 0;
    return depth;
}

class ZoneScope {
public:
    ZoneScope() : lock(zoneMutex()) {
        if (zoneDepth() == 0) applyGuestZone();
        ++zoneDepth();
    }

    ~ZoneScope() { --zoneDepth(); }

    ZoneScope(const ZoneScope&) = delete;
    ZoneScope& operator=(const ZoneScope&) = delete;

private:
    std::lock_guard<std::recursive_mutex> lock;
};

}  // namespace

namespace {

constexpr const char* WEEKDAY_FULL_NAMES[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
constexpr const char* MONTH_FULL_NAMES[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
    "October", "November", "December"};
constexpr int YEAR_BASE = 1900;

struct FormatBuffer {
    char* cursor;
    char* limit;
    bool full;
};

void addPiece(FormatBuffer& buffer, const char* text, std::size_t length) {
    if (buffer.full) return;
    if (length > static_cast<std::size_t>(buffer.limit - buffer.cursor)) {
        buffer.full = true;
        return;
    }
    std::memcpy(buffer.cursor, text, length);
    buffer.cursor += length;
}

void addText(FormatBuffer& buffer, const char* text) {
    addPiece(buffer, text, std::strlen(text));
}

int clampedField(std::int64_t value, int modulus) {
    return value < 0 ? 0 : static_cast<int>(value % modulus);
}

void addNumber(FormatBuffer& buffer, std::int64_t value, int modulus, const char* format) {
    char text[16];
    std::snprintf(text, sizeof(text), format, clampedField(value, modulus));
    addText(buffer, text);
}

const char* tableName(int index, const char* const* names, const char* const* shortNames, int count) {
    const auto field = static_cast<std::int32_t>(static_cast<std::uint32_t>(index) * 2u + (names == shortNames ? 0u : 1u));
    if (field < 0) return shortNames[0];
    if (field >= 2 * count) return "";
    return field % 2 == 0 ? shortNames[field / 2] : names[field / 2];
}

int twoDigitYear(std::int64_t year) {
    return static_cast<int>((year % 100 + 100) % 100);
}

int wrapped(std::int64_t value) {
    return static_cast<int>(static_cast<std::uint32_t>(value));
}

int weekOfYear(const GuestTm& time, int shift) {
    return wrapped(static_cast<std::int64_t>(time.tm_yday) + 7 - wrapped(static_cast<std::int64_t>(time.tm_wday) + shift) % 7) / 7;
}

struct IsoWeek {
    int year;
    int week;
};

bool isoLeapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year / 400 == 100 || year / 400 == -300);
}

bool midweek(int weekday) {
    return weekday >= 2 && weekday <= 4;
}

IsoWeek isoWeek(const GuestTm& time) {
    const int leap = isoLeapYear(time.tm_year) ? 1 : 0;
    const int span = wrapped(static_cast<std::int64_t>(time.tm_wday) - time.tm_yday);
    const int firstWeekday = wrapped(static_cast<std::int64_t>(span) + 371) % 7;
    const int nextFirstWeekday = (leap + firstWeekday + 365) % 7;
    const int nextYear = wrapped(static_cast<std::int64_t>(time.tm_year) + 1);
    if (midweek(nextFirstWeekday) && wrapped(static_cast<std::int64_t>(time.tm_yday) - leap) >= 366 - nextFirstWeekday) return {nextYear, 1};
    const int mondayWeekday = wrapped(static_cast<std::int64_t>(time.tm_wday) + 6) % 7;
    const int week = wrapped(static_cast<std::int64_t>(time.tm_yday) - mondayWeekday + 7) / 7 + (midweek(firstWeekday) ? 1 : 0);
    if (week < 0) return {nextYear, 1};
    if (week > 0) return {time.tm_year, week};
    const int previousYear = wrapped(static_cast<std::int64_t>(time.tm_year) - 1);
    const int previousLeap = isoLeapYear(previousYear) ? 1 : 0;
    const int previousFirstWeekday = wrapped(static_cast<std::int64_t>(span) + 13 - previousLeap) % 7;
    if (midweek((previousLeap + previousFirstWeekday + 365) % 7)) return {previousYear, 0};
    const int lastWeekday = wrapped(static_cast<std::int64_t>(span) + 13) % 7;
    return {previousYear, (372 + previousLeap - lastWeekday) / 7 + (midweek(previousFirstWeekday) ? 1 : 0)};
}

void formatTime(FormatBuffer& buffer, const char* format, const GuestTm& time);

void addZoneOffset(FormatBuffer& buffer, const GuestTm& time) {
    std::int64_t east = -timezone_nid_postfix + (time.tm_isdst > 0 ? 3600 : 0);
    const char sign = east < 0 ? '-' : '+';
    if (east < 0) east = -east;
    char text[32];
    std::snprintf(text, sizeof(text), "%c%02lld%02lld", sign, static_cast<long long>(east / 3600), static_cast<long long>(east / 60 % 60));
    addText(buffer, text);
}

bool addConversion(FormatBuffer& buffer, char conversion, const GuestTm& time) {
    const std::int64_t year = static_cast<std::int64_t>(time.tm_year) + YEAR_BASE;
    switch (conversion) {
    case 'a': addText(buffer, tableName(time.tm_wday, WEEKDAY_NAMES, WEEKDAY_NAMES, 7)); return true;
    case 'A': addText(buffer, tableName(time.tm_wday, WEEKDAY_FULL_NAMES, WEEKDAY_NAMES, 7)); return true;
    case 'b':
    case 'h': addText(buffer, tableName(time.tm_mon, MONTH_NAMES, MONTH_NAMES, 12)); return true;
    case 'B': addText(buffer, tableName(time.tm_mon, MONTH_FULL_NAMES, MONTH_NAMES, 12)); return true;
    case 'c': formatTime(buffer, "%a %b %e %H:%M:%S %Y", time); return true;
    case 'C': addNumber(buffer, time.tm_year / 100 + YEAR_BASE / 100, 100, "%02d"); return true;
    case 'd': addNumber(buffer, time.tm_mday, 100, "%02d"); return true;
    case 'D':
    case 'x': formatTime(buffer, "%m/%d/%y", time); return true;
    case 'e': addNumber(buffer, time.tm_mday, 100, "%2d"); return true;
    case 'F': formatTime(buffer, "%Y-%m-%d", time); return true;
    case 'g': addNumber(buffer, twoDigitYear(isoWeek(time).year), 100, "%02d"); return true;
    case 'G': addNumber(buffer, static_cast<std::int64_t>(isoWeek(time).year) + YEAR_BASE, 10000, "%04d"); return true;
    case 'H': addNumber(buffer, time.tm_hour, 100, "%02d"); return true;
    case 'I': addNumber(buffer, (static_cast<std::int64_t>(time.tm_hour) + 11) % 12 + 1, 100, "%02d"); return true;
    case 'j': addNumber(buffer, static_cast<std::int64_t>(time.tm_yday) + 1, 1000, "%03d"); return true;
    case 'm': addNumber(buffer, static_cast<std::int64_t>(time.tm_mon) + 1, 100, "%02d"); return true;
    case 'M': addNumber(buffer, time.tm_min, 100, "%02d"); return true;
    case 'n': addText(buffer, "\n"); return true;
    case 'p': addText(buffer, time.tm_hour < 12 ? "AM" : "PM"); return true;
    case 'r': formatTime(buffer, "%I:%M:%S %p", time); return true;
    case 'R': formatTime(buffer, "%H:%M", time); return true;
    case 'S': addNumber(buffer, time.tm_sec, 100, "%02d"); return true;
    case 't': addText(buffer, "\t"); return true;
    case 'T':
    case 'X': formatTime(buffer, "%H:%M:%S", time); return true;
    case 'u': addNumber(buffer, time.tm_wday == 0 ? 7 : time.tm_wday, 10, "%d"); return true;
    case 'U': addNumber(buffer, weekOfYear(time, 7), 100, "%02d"); return true;
    case 'V': addNumber(buffer, isoWeek(time).week, 100, "%02d"); return true;
    case 'w': addNumber(buffer, time.tm_wday, 10, "%d"); return true;
    case 'W': addNumber(buffer, weekOfYear(time, 6), 100, "%02d"); return true;
    case 'y': addNumber(buffer, twoDigitYear(year), 100, "%02d"); return true;
    case 'Y': addNumber(buffer, year, 10000, "%04d"); return true;
    case 'z': addZoneOffset(buffer, time); return true;
    case 'Z': {
        const char* name = tzname_nid_postfix[time.tm_isdst > 0 ? 1 : 0];
        addText(buffer, name != nullptr ? name : "");
        return true;
    }
    case '%': addText(buffer, "%"); return true;
    default: return false;
    }
}

void formatTime(FormatBuffer& buffer, const char* format, const GuestTm& time) {
    for (; *format != '\0' && !buffer.full; ++format) {
        if (*format != '%' || format[1] == '\0') {
            addPiece(buffer, format, 1);
            continue;
        }
        ++format;
        if (*format == 'E' || *format == 'O') {
            if (format[1] == '\0') throw std::invalid_argument("strftime: the format ends after an E or O modifier");
            ++format;
        }
        if (!addConversion(buffer, *format, time)) {
            const char unknown[] = {'%', *format};
            addPiece(buffer, unknown, sizeof(unknown));
        }
    }
}

std::size_t formatGuestTime(char* destination, std::size_t size, const char* format, const GuestTm& time) {
    FormatBuffer buffer{destination, destination + size, false};
    formatTime(buffer, format == nullptr ? "%c" : format, time);
    if (buffer.full || buffer.cursor == buffer.limit) return 0;
    *buffer.cursor = '\0';
    return static_cast<std::size_t>(buffer.cursor - destination);
}

char** HostZoneNames() {
#ifdef _WIN32
    _tzset();
    return _tzname;
#else
    ::tzset();
    return ::tzname;
#endif
}

char* GuestZoneName(int index) {
    static char unnamed[] = "   ";
    char** names = HostZoneNames();
#ifdef _WIN32
    const bool daylightSaving = _daylight != 0;
#else
    const bool daylightSaving = ::daylight != 0;
#endif
    return index == 1 && !daylightSaving ? unnamed : names[index];
}
}

extern "C" {

char* tzname_nid_postfix[2] = {GuestZoneName(0), GuestZoneName(1)};
int64_t timezone_nid_postfix = 0;
int daylight_nid_postfix = 0;

void APS5_VABI tzset_nid_postfix(void) {
    const ZoneScope zone;
    tzname_nid_postfix[0] = GuestZoneName(0);
    tzname_nid_postfix[1] = GuestZoneName(1);
    const int64_t now = libc_time_nid_postfix(nullptr);
    GuestTm universal{};
    GuestTm local{};
    gmtime_s_nid_postfix(&now, &universal);
    localtime_s_nid_postfix(&now, &local);

    daylight_nid_postfix = local.tm_isdst >= 0 ? 1 : 0;
    timezone_nid_postfix = static_cast<int64_t>(libc_difftime_nid_postfix(libc_mktime_nid_postfix(&universal), libc_mktime_nid_postfix(&local)));
}

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
    if (time1 >= time0) return static_cast<double>(static_cast<uint64_t>(time1) - static_cast<uint64_t>(time0));
    return -static_cast<double>(static_cast<uint64_t>(time0) - static_cast<uint64_t>(time1));
}

double APS5_VABI difftime_nid_postfix(int64_t time1, int64_t time0) {
    return libc_difftime_nid_postfix(time1, time0);
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
    const ZoneScope zone;
    std::tm host{};
    const int callerError = errno;
#ifdef _WIN32
    const std::int64_t cycles = *timer >= 0 && *timer <= HOST_LAST_SECOND ? 0 : calendarCycles(*timer, CALENDAR_CYCLE_SECONDS);
    const std::int64_t offset = *timer % CALENDAR_CYCLE_SECONDS;
    const std::time_t t = static_cast<std::time_t>(cycles == 0 ? *timer : offset < 0 ? offset + CALENDAR_CYCLE_SECONDS : offset);
    if (localtime_s(&host, &t) != 0) {
        errno = callerError;
        return nullptr;
    }
    const std::int64_t year = host.tm_year + cycles * CALENDAR_CYCLE_YEARS;
    if (year < std::numeric_limits<int>::min() || year > std::numeric_limits<int>::max()) {
        errno = callerError;
        return nullptr;
    }
    host.tm_year = static_cast<int>(year);
    copyCalendarFields(host, *result);
    long standardBias = 0;
    long daylightBias = 0;
    _get_timezone(&standardBias);
    _get_dstbias(&daylightBias);
    const bool daylight = host.tm_isdst > 0;
    result->tm_gmtoff = -static_cast<std::int64_t>(standardBias + (daylight ? daylightBias : 0));
    result->tm_zone = _tzname[daylight ? 1 : 0];
#else
    const std::time_t t = static_cast<std::time_t>(*timer);
    if (localtime_r(&t, &host) == nullptr) {
        errno = callerError;
        return nullptr;
    }
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
    const ZoneScope zone;
    std::tm host = toHostTm(*timeptr);
    host.tm_wday = -1;
    int64_t moved = 0;
#if defined(_WIN32) || defined(__APPLE__)
    const bool ordinary = host.tm_year > HOST_ORDINARY_FIRST_YEAR && host.tm_year < HOST_ORDINARY_LAST_YEAR && host.tm_mon >= 0 && host.tm_mon < 12
        && host.tm_mday >= 1 && host.tm_mday <= 31 && host.tm_hour >= 0 && host.tm_hour < 24 && host.tm_min >= 0 && host.tm_min < 60
        && host.tm_sec >= 0 && host.tm_sec <= 60;
    constexpr std::int64_t firstOrdinary = HOST_FIRST_SECOND + SECONDS_PER_DAY;
    const std::int64_t approximate = ordinary ? firstOrdinary : utcSeconds(host);
#ifdef __APPLE__
    if (approximate < firstOrdinary) {
        std::int64_t time = 0;
        GuestTm normalized{};
        if (!timeOfLocalSeconds(approximate, time) || localtime_s_nid_postfix(&time, &normalized) == nullptr) return -1;
        *timeptr = normalized;
        return time;
    }
#endif
    if (approximate < firstOrdinary || approximate > HOST_LAST_SECOND - SECONDS_PER_DAY) {
        moved = calendarCycles(approximate - firstOrdinary, CALENDAR_CYCLE_SECONDS) * CALENDAR_CYCLE_SECONDS;
        GuestTm window{};
        if (!utcCalendarFields(approximate - moved, window)) return -1;
        window.tm_isdst = host.tm_isdst;
        host = toHostTm(window);
        host.tm_wday = -1;
    }
#endif
    const std::time_t t = std::mktime(&host);
    if (t == static_cast<std::time_t>(-1) && host.tm_wday == -1) return -1;
    const int64_t time = static_cast<int64_t>(t) + moved;
    GuestTm normalized{};
    if (localtime_s_nid_postfix(&time, &normalized) == nullptr) return -1;
    *timeptr = normalized;
    return time;
}

int64_t APS5_VABI mktime_nid_postfix(GuestTm* timeptr) {
    return libc_mktime_nid_postfix(timeptr);
}

size_t APS5_VABI libc_strftime_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    const ZoneScope zone;
    tzset_nid_postfix();

    return formatGuestTime(str, count, format, *timeptr);
}

char* APS5_VABI asctime_nid_postfix(const GuestTm* timeptr) {
    static thread_local char result[ASCTIME_SIZE];
    if (timeptr == nullptr) {
        errno = EINVAL;
        std::memcpy(result, "??? ??? ?? ??:??:?? ????\n", sizeof("??? ??? ?? ??:??:?? ????\n"));
        return result;
    }
    const std::int64_t year = static_cast<std::int64_t>(timeptr->tm_year) + YEAR_BASE;
    std::snprintf(result, sizeof(result), "%s %s%3d %02d:%02d:%02d %04d\n", tableName(timeptr->tm_wday, WEEKDAY_NAMES, WEEKDAY_NAMES, 7), tableName(timeptr->tm_mon, MONTH_NAMES, MONTH_NAMES, 12),
        clampedField(timeptr->tm_mday, 100), clampedField(timeptr->tm_hour, 100), clampedField(timeptr->tm_min, 100), clampedField(timeptr->tm_sec, 100), clampedField(year, 10000));
    return result;
}

char* APS5_VABI ctime_nid_postfix(const int64_t* timer) {
    if (const GuestTm* local = localtime_nid_postfix(timer)) return asctime_nid_postfix(local);
    static thread_local char unknown[26];
    std::memcpy(unknown, "??? ??? ?? ??:??:?? ????\n", sizeof(unknown));
    errno = EINVAL;
    return unknown;
}

size_t APS5_VABI strftime_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    return libc_strftime_nid_postfix(str, count, format, timeptr);
}

size_t APS5_VABI wcsftime_nid_postfix(std::uint16_t* str, size_t count, const std::uint16_t* format, const GuestTm* timeptr) {
    constexpr int GuestEnomem = 12;
    constexpr int GuestEinval = 22;
    constexpr auto Failed = static_cast<std::size_t>(-1);
    if (format == nullptr) throw std::invalid_argument("wcsftime: null format");
    if (timeptr == nullptr) throw std::invalid_argument("wcsftime: null time");
    if (str == nullptr) return 0;
    const std::uint16_t* formatCursor = format;
    const std::size_t formatLength = wcsrtombs_nid_postfix(nullptr, &formatCursor, 0, nullptr);
    if (formatLength == Failed) return 0;
    const std::unique_ptr<char[]> narrowFormat(new (std::nothrow) char[formatLength + 1]);
    if (!narrowFormat) {
        errno = GuestEnomem;
        return 0;
    }
    wcsrtombs_nid_postfix(narrowFormat.get(), &formatCursor, formatLength + 1, nullptr);

    const auto maxCharacterBytes = static_cast<std::size_t>(_Getmbcurmax_nid_postfix());
    if (std::numeric_limits<std::size_t>::max() / maxCharacterBytes <= count) {
        errno = GuestEinval;
        return 0;
    }
    const std::unique_ptr<char[]> narrow(new (std::nothrow) char[count * maxCharacterBytes]);
    if (!narrow) {
        errno = GuestEnomem;
        return 0;
    }
    const int callerError = errno;
    if (strftime_nid_postfix(narrow.get(), count * maxCharacterBytes, narrowFormat.get(), timeptr) == 0) {
        errno = callerError;
        return 0;
    }
    const char* narrowCursor = narrow.get();
    const std::size_t length = mbsrtowcs_nid_postfix(nullptr, &narrowCursor, 0, nullptr);
    if (length == Failed || length >= count) return 0;
    mbsrtowcs_nid_postfix(str, &narrowCursor, count, nullptr);

    return length;
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

int APS5_VABI ctime_s_nid_postfix(char* buffer, std::size_t size, const std::int64_t* time) {
    return SonyCtimeS(buffer, size, time);
}

int APS5_VABI asctime_s_nid_postfix(char* buffer, std::size_t size, const GuestTm* time) {
    return SonyAsctimeS(buffer, size, time);
}

}
