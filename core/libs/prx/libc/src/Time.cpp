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

extern "C" int64_t APS5_VABI libc_mktime_nid_postfix(GuestTm* timeptr);

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

enum class Padding { Default, Less, Space, Zero };

const char* paddingFormat(int width, Padding padding, bool spaced) {
    static constexpr const char* formats[][4] = {
        {"%02d", "%d", "%2d", "%02d"},
        {"%2d", "%d", "%2d", "%02d"},
        {"%03d", "%d", "%3d", "%03d"},
        {"%04d", "%d", "%4d", "%04d"},
    };
    const int row = width == 2 ? (spaced ? 1 : 0) : width == 3 ? 2 : 3;
    return formats[row][static_cast<int>(padding)];
}

bool isLeapYearSum(int year, int base) {
    return isLeapYear(year % 400 + base % 400);
}

struct FormatBuffer {
    char* cursor;
    char* limit;
};

void addText(FormatBuffer& buffer, const char* text) {
    while (buffer.cursor < buffer.limit && (*buffer.cursor = *text++) != '\0') ++buffer.cursor;
}

void addNumber(FormatBuffer& buffer, int value, const char* format) {
    char text[16];
    std::snprintf(text, sizeof(text), format, value);
    addText(buffer, text);
}

void addYear(FormatBuffer& buffer, int year, int base, bool century, bool lastDigits) {
    int trail = year % 100 + base % 100;
    int lead = year / 100 + base / 100 + trail / 100;
    trail %= 100;
    if (trail < 0 && lead > 0) {
        trail += 100;
        --lead;
    } else if (lead < 0 && trail > 0) {
        trail -= 100;
        ++lead;
    }
    if (century) {
        if (lead == 0 && trail < 0) addText(buffer, "-0");
        else addNumber(buffer, lead, "%02d");
    }
    if (lastDigits) addNumber(buffer, trail < 0 ? -trail : trail, "%02d");
}

const char* guestZoneName(const GuestTm& guest) {
    if (guest.tm_zone != nullptr) return guest.tm_zone;
    if (guest.tm_isdst < 0) return "";
#ifdef _WIN32
    return _tzname[guest.tm_isdst != 0 ? 1 : 0];
#else
    return tzname[guest.tm_isdst != 0 ? 1 : 0];
#endif
}

void addIsoWeek(FormatBuffer& buffer, char conversion, Padding padding, const GuestTm& time) {
    int base = YEAR_BASE;
    int yday = time.tm_yday;
    int week = 0;
    for (;;) {
        const int length = isLeapYearSum(time.tm_year, base) ? 366 : 365;
        const int bottom = (yday + 11 - time.tm_wday) % 7 - 3;
        int top = bottom - length % 7;
        if (top < -3) top += 7;
        top += length;
        if (yday >= top) {
            ++base;
            week = 1;
            break;
        }
        if (yday >= bottom) {
            week = 1 + (yday - bottom) / 7;
            break;
        }
        --base;
        yday += isLeapYearSum(time.tm_year, base) ? 366 : 365;
    }
    if (conversion == 'V') addNumber(buffer, week, paddingFormat(2, padding, false));
    else addYear(buffer, time.tm_year, base, conversion == 'G', true);
}

void formatTime(FormatBuffer& buffer, const char* format, const GuestTm& time) {
    for (; *format != '\0'; ++format) {
        if (*format == '%') {
            bool alternative = false;
            Padding padding = Padding::Default;
            bool handled = true;
            for (bool modifier = true; modifier;) {
                modifier = false;
                switch (*++format) {
                case '\0':
                    --format;
                    handled = false;
                    break;
                case 'A':
                    addText(buffer, time.tm_wday < 0 || time.tm_wday >= 7 ? "?" : WEEKDAY_FULL_NAMES[time.tm_wday]);
                    break;
                case 'a':
                    addText(buffer, time.tm_wday < 0 || time.tm_wday >= 7 ? "?" : WEEKDAY_NAMES[time.tm_wday]);
                    break;
                case 'B':
                    addText(buffer, time.tm_mon < 0 || time.tm_mon >= 12 ? "?" : MONTH_FULL_NAMES[time.tm_mon]);
                    break;
                case 'b':
                case 'h':
                    addText(buffer, time.tm_mon < 0 || time.tm_mon >= 12 ? "?" : MONTH_NAMES[time.tm_mon]);
                    break;
                case 'C':
                    addYear(buffer, time.tm_year, YEAR_BASE, true, false);
                    break;
                case 'c':
                    formatTime(buffer, "%a %b %e %H:%M:%S %Y", time);
                    break;
                case 'D':
                    formatTime(buffer, "%m/%d/%y", time);
                    break;
                case 'd':
                    addNumber(buffer, time.tm_mday, paddingFormat(2, padding, false));
                    break;
                case 'E':
                case 'O':
                    if (alternative) {
                        handled = false;
                        break;
                    }
                    alternative = true;
                    modifier = true;
                    break;
                case 'e':
                    addNumber(buffer, time.tm_mday, paddingFormat(2, padding, true));
                    break;
                case 'F':
                    formatTime(buffer, "%Y-%m-%d", time);
                    break;
                case 'H':
                    addNumber(buffer, time.tm_hour, paddingFormat(2, padding, false));
                    break;
                case 'I':
                    addNumber(buffer, time.tm_hour % 12 != 0 ? time.tm_hour % 12 : 12, paddingFormat(2, padding, false));
                    break;
                case 'j':
                    addNumber(buffer, time.tm_yday + 1, paddingFormat(3, padding, false));
                    break;
                case 'k':
                    addNumber(buffer, time.tm_hour, paddingFormat(2, padding, true));
                    break;
                case 'l':
                    addNumber(buffer, time.tm_hour % 12 != 0 ? time.tm_hour % 12 : 12, paddingFormat(2, padding, true));
                    break;
                case 'M':
                    addNumber(buffer, time.tm_min, paddingFormat(2, padding, false));
                    break;
                case 'm':
                    addNumber(buffer, time.tm_mon + 1, paddingFormat(2, padding, false));
                    break;
                case 'n':
                    addText(buffer, "\n");
                    break;
                case 'p':
                    addText(buffer, time.tm_hour >= 12 ? "PM" : "AM");
                    break;
                case 'R':
                    formatTime(buffer, "%H:%M", time);
                    break;
                case 'r':
                    formatTime(buffer, "%I:%M:%S %p", time);
                    break;
                case 'S':
                    addNumber(buffer, time.tm_sec, paddingFormat(2, padding, false));
                    break;
                case 's': {
                    GuestTm copy = time;
                    char text[24];
                    std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(libc_mktime_nid_postfix(&copy)));
                    addText(buffer, text);
                    break;
                }
                case 'T':
                    formatTime(buffer, "%H:%M:%S", time);
                    break;
                case 't':
                    addText(buffer, "\t");
                    break;
                case 'U':
                    addNumber(buffer, (time.tm_yday + 7 - time.tm_wday) / 7, paddingFormat(2, padding, false));
                    break;
                case 'u':
                    addNumber(buffer, time.tm_wday == 0 ? 7 : time.tm_wday, "%d");
                    break;
                case 'V':
                case 'G':
                case 'g':
                    addIsoWeek(buffer, *format, padding, time);
                    break;
                case 'v':
                    formatTime(buffer, "%e-%b-%Y", time);
                    break;
                case 'W':
                    addNumber(buffer, (time.tm_yday + 7 - (time.tm_wday != 0 ? time.tm_wday - 1 : 6)) / 7,
                        paddingFormat(2, padding, false));
                    break;
                case 'w':
                    addNumber(buffer, time.tm_wday, "%d");
                    break;
                case 'X':
                    formatTime(buffer, "%H:%M:%S", time);
                    break;
                case 'x':
                    formatTime(buffer, "%m/%d/%y", time);
                    break;
                case 'y':
                    addYear(buffer, time.tm_year, YEAR_BASE, false, true);
                    break;
                case 'Y':
                    addYear(buffer, time.tm_year, YEAR_BASE, true, true);
                    break;
                case 'Z':
                    addText(buffer, guestZoneName(time));
                    break;
                case 'z': {
                    if (time.tm_isdst < 0) break;
                    int difference = static_cast<int>(time.tm_gmtoff);
                    addText(buffer, difference < 0 ? "-" : "+");
                    if (difference < 0) difference = -difference;
                    difference /= 60;
                    addNumber(buffer, difference / 60 * 100 + difference % 60, paddingFormat(4, padding, false));
                    break;
                }
                case '+':
                    formatTime(buffer, "%a %b %e %H:%M:%S %Z %Y", time);
                    break;
                case '-':
                case '_':
                case '0':
                    if (padding != Padding::Default) {
                        handled = false;
                        break;
                    }
                    padding = *format == '-' ? Padding::Less : *format == '_' ? Padding::Space : Padding::Zero;
                    modifier = true;
                    break;
                default:
                    handled = false;
                    break;
                }
            }
            if (handled) continue;
        }
        if (buffer.cursor == buffer.limit) break;
        *buffer.cursor++ = *format;
    }
}

std::size_t formatGuestTime(char* destination, std::size_t size, const char* format, const GuestTm& time) {
    FormatBuffer buffer{destination, destination + size};
    formatTime(buffer, format == nullptr ? "%c" : format, time);
    if (buffer.cursor == buffer.limit) return 0;
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

GuestTm* APS5_VABI localtime_s_nid_postfix(const int64_t* timer, GuestTm* result);
GuestTm* APS5_VABI gmtime_s_nid_postfix(const int64_t* timer, GuestTm* result);
int64_t APS5_VABI libc_time_nid_postfix(int64_t* timer);
double APS5_VABI libc_difftime_nid_postfix(int64_t time1, int64_t time0);
std::size_t APS5_VABI wcsrtombs_nid_postfix(char* destination, const std::uint16_t** source, std::size_t capacity, void* state);
std::size_t APS5_VABI mbsrtowcs_nid_postfix(std::uint16_t* destination, const char** source, std::size_t capacity, void* state);
int APS5_VABI _Getmbcurmax_nid_postfix();

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
    const char* weekday = timeptr->tm_wday >= 0 && timeptr->tm_wday < 7 ? WEEKDAY_NAMES[timeptr->tm_wday] : "???";
    const char* month = timeptr->tm_mon >= 0 && timeptr->tm_mon < 12 ? MONTH_NAMES[timeptr->tm_mon] : "???";
    char year[16];
    formatGuestTime(year, sizeof(year), "%Y", *timeptr);
    std::snprintf(result, sizeof(result), std::strlen(year) <= 4 ? "%s %s%3d %.2d:%.2d:%.2d %-4s\n" : "%s %s%3d %.2d:%.2d:%.2d     %s\n",
        weekday, month, timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec, year);
    return result;
}

char* APS5_VABI ctime_nid_postfix(const int64_t* timer) {
    if (const GuestTm* local = localtime_nid_postfix(timer)) return asctime_nid_postfix(local);
    static char unknown[26];
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
