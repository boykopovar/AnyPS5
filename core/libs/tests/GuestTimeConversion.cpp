#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
GuestTm* APS5_VABI libc_gmtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI gmtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI libc_localtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI localtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI gmtime_s_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI localtime_s_nid_postfix(const std::int64_t*, GuestTm*);
char* APS5_VABI asctime_nid_postfix(const GuestTm*);
std::int64_t APS5_VABI mktime_nid_postfix(GuestTm*);
std::size_t APS5_VABI strftime_nid_postfix(char*, std::size_t, const char*, const GuestTm*);
void APS5_VABI tzset_nid_postfix(void);
extern char* tzname_nid_postfix[2];
extern std::int64_t timezone_nid_postfix;
extern int daylight_nid_postfix;
char* APS5_VABI ctime_nid_postfix(const std::int64_t*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI setenv_nid_postfix(const char*, const char*, int);
double APS5_VABI libc_difftime_nid_postfix(std::int64_t, std::int64_t);
double APS5_VABI difftime_nid_postfix(std::int64_t, std::int64_t);
}

using Converter = GuestTm* (APS5_VABI *)(const std::int64_t*);
using BufferedConverter = GuestTm* (APS5_VABI *)(const std::int64_t*, GuestTm*);

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

static void CheckConcurrent(Converter convert, BufferedConverter buffered, const char* name) {
    constexpr std::array<std::int64_t, 8> timers{0, 86400, 946684800, 1078012800, 1609459200, 1709164800, 1893456000, 2145916799};
    std::array<GuestTm, timers.size()> expected{};
    for (std::size_t index = 0; index < timers.size(); ++index)
        Require(buffered(&timers[index], &expected[index]) == &expected[index], "Buffered conversion failed");
    std::barrier start(static_cast<std::ptrdiff_t>(timers.size()));
    std::atomic<bool> matches{true};
    std::array<std::thread, timers.size()> workers;
    for (std::size_t index = 0; index < workers.size(); ++index) {
        workers[index] = std::thread([&, index] {
            start.arrive_and_wait();
            for (int iteration = 0; iteration < 10000; ++iteration) {
                const auto* result = convert(&timers[index]);
                if (result == nullptr || !Equal(*result, expected[index])) {
                    matches = false;
                    break;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(matches, name);
    const std::int64_t invalid = std::numeric_limits<std::int64_t>::max();
    Require(convert(&invalid) == nullptr, "Unrepresentable time was accepted");
}

static GuestTm UtcTime(std::int64_t timer) {
    GuestTm utc{};
    Require(gmtime_s_nid_postfix(&timer, &utc) == &utc, "UTC conversion failed");
    utc.tm_isdst = 0;
    return utc;
}

static void CheckAsctime(const GuestTm& brokenDown, const char* expected) {
    const char* text = asctime_nid_postfix(&brokenDown);
    Require(text != nullptr && std::strcmp(text, expected) == 0, "asctime did not format the broken-down time");
}

struct UtcCase {
    std::int64_t timer;
    int year;
    int mon;
    int mday;
    int hour;
    int min;
    int sec;
    int wday;
    int yday;
};

static void CheckUtc(const UtcCase& expected) {
    GuestTm utc{};
    std::memset(&utc, 0xAA, sizeof(utc));
    Require(gmtime_s_nid_postfix(&expected.timer, &utc) == &utc, "UTC conversion failed");
    Require(utc.tm_year == expected.year && utc.tm_mon == expected.mon && utc.tm_mday == expected.mday
        && utc.tm_hour == expected.hour && utc.tm_min == expected.min && utc.tm_sec == expected.sec
        && utc.tm_wday == expected.wday && utc.tm_yday == expected.yday && utc.tm_isdst == -1,
        "UTC calendar fields are wrong");
}

struct DifferenceCase {
    std::int64_t time1;
    std::int64_t time0;
    double seconds;
};

static void CheckDifference(const DifferenceCase& expected) {
    Require(libc_difftime_nid_postfix(expected.time1, expected.time0) == expected.seconds
        && difftime_nid_postfix(expected.time1, expected.time0) == expected.seconds, "difftime did not return time1 - time0");
}

static void CheckFormat(const GuestTm& guest, const char* format, const char* expected) {
    char buffer[96]{};
    const std::size_t size = strftime_nid_postfix(buffer, sizeof(buffer), format, &guest);
    if (size != std::strlen(expected) || std::strcmp(buffer, expected) != 0)
        std::fprintf(stderr, "Format '%s': expected '%s', got '%s' (%zu bytes)\n", format, expected, buffer, size);
    Require(size == std::strlen(expected) && std::strcmp(buffer, expected) == 0, "strftime did not format a FreeBSD conversion");
}

static void CheckBufferSize(const GuestTm& guest, const char* format, std::size_t length) {
    char buffer[96];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(strftime_nid_postfix(buffer, length + 1, format, &guest) == length, "strftime rejected an exact-fit buffer");
    Require(std::strlen(buffer) == length, "strftime did not terminate an exact-fit buffer");
    Require(strftime_nid_postfix(buffer, length, format, &guest) == 0, "strftime accepted a buffer one byte short");
    Require(strftime_nid_postfix(buffer, 1, format, &guest) == 0, "strftime accepted a one byte buffer");
}

static const char* HostZoneName(int isdst) {
#ifdef _WIN32
    return _tzname[isdst != 0 ? 1 : 0];
#else
    return tzname[isdst != 0 ? 1 : 0];
#endif
}

static void CheckFreeBsdConversions() {
    const std::int64_t timer = 1700000000;
    GuestTm guest{};
    Require(gmtime_s_nid_postfix(&timer, &guest) == &guest, "UTC conversion failed");
    CheckFormat(guest, "%k|%l", "22|10");
    CheckFormat(guest, "%v", "14-Nov-2023");
    CheckFormat(guest, "%+", "Tue Nov 14 22:13:20  2023");
    CheckFormat(guest, "%s", "1699992800");
    CheckFormat(guest, "%%k %%l %%s %%v %%+", "%k %l %s %v %+");
    CheckFormat(guest, "%%%k", "%22");
    CheckFormat(guest, "[%k][%l]", "[22][10]");
    CheckFormat(guest, "%Ek|%Ol|%Os|%Ev|%O+", "22|10|1699992800|14-Nov-2023|Tue Nov 14 22:13:20  2023");
    CheckFormat(guest, "%E%k", "%k");
    CheckFormat(guest, "%O%l", "%l");
    CheckFormat(guest, "%E%s", "%s");
    CheckFormat(guest, "%E%v", "%v");
    CheckFormat(guest, "%O%+", "%+");
    CheckFormat(guest, "%k|%E%l|%l", "22|%l|10");
    CheckFormat(guest, "%E%", "%");
    CheckFormat(guest, "%O%%k", "%22");
    CheckFormat(guest, "%E%k%k", "%k22");
    CheckBufferSize(guest, "%k", 2);
    CheckBufferSize(guest, "%k:%l", 5);
    CheckBufferSize(guest, "%+", 25);
    CheckBufferSize(guest, "%Ek", 2);

    guest.tm_isdst = 0;
    char expected[96];
    std::snprintf(expected, sizeof(expected), "Tue Nov 14 22:13:20 %s 2023", HostZoneName(0));
    CheckFormat(guest, "%+", expected);
    guest.tm_isdst = 1;
    std::snprintf(expected, sizeof(expected), "Tue Nov 14 22:13:20 %s 2023", HostZoneName(1));
    CheckFormat(guest, "%+", expected);
    guest.tm_isdst = 0;

    GuestTm local{};
    Require(localtime_s_nid_postfix(&timer, &local) == &local, "Local conversion failed");
    GuestTm normalized = local;
    const std::int64_t mktimeResult = mktime_nid_postfix(&normalized);
    Require(mktimeResult == timer, "mktime did not invert localtime");
    std::snprintf(expected, sizeof(expected), "%lld", static_cast<long long>(mktimeResult));
    CheckFormat(local, "%s", expected);
    CheckFormat(local, "%Os", expected);

    guest.tm_hour = 0;
    CheckFormat(guest, "%k|%l", " 0|12");
    guest.tm_hour = 12;
    CheckFormat(guest, "%k|%l", "12|12");
    guest.tm_hour = 5;
    guest.tm_mday = 5;
    CheckFormat(guest, "%k|%l|%v", " 5| 5| 5-Nov-2023");
    guest.tm_hour = 13;
    CheckFormat(guest, "%k|%l", "13| 1");
    char tooSmall[4]{};
    Require(strftime_nid_postfix(tooSmall, sizeof(tooSmall), "%k:%l", &guest) == 0, "strftime accepted an undersized buffer");
}

static void CheckLiteralConversions() {
    const std::int64_t timer = 1700000000;
    GuestTm guest{};
    Require(gmtime_s_nid_postfix(&timer, &guest) == &guest, "UTC conversion failed");
    struct FormatCase {
        const char* format;
        const char* expected;
    };
    constexpr FormatCase cases[]{
        {"%Q", "Q"},
        {"%P|%f|%N|%1|%#Y", "P|f|N|1|#Y"},
        {"[%Q] %Y-%m-%d", "[Q] 2023-11-14"},
        {"%EQ|%OQ|%_Q|%-Q|%0Q", "Q|Q|Q|Q|Q"},
        {"%E_Q|%_EQ|%O0Q|%-OQ", "Q|Q|Q|Q"},
        {"%EEY|%EOY|%OEY|%OOY", "EY|OY|EY|OY"},
        {"%EE%Y|%OO%k", "E2023|O22"},
        {"%--Y|%__Y|%00Y|%_0Y|%0-Y", "-Y|_Y|0Y|0Y|-Y"},
        {"%E_EY|%_EOY|%O0OY", "EY|OY|OY"},
        {"%", "%"},
        {"text%", "text%"},
        {"%E", "E"},
        {"%O", "O"},
        {"%_", "_"},
        {"%-", "-"},
        {"%0", "0"},
        {"%E_", "_"},
        {"%_E", "E"},
        {"%Q%", "Q%"},
        {"%k%", "22%"},
        {"%%Q|%%%Q|%%%%Q", "%Q|%Q|%%Q"},
        {"%E%Q|%O%Q|%_%Q|%0%Q|%-%Q", "%Q|%Q|%Q|%Q|%Q"},
        {"%E%%Q|%O%%%Q", "%Q|%%Q"},
        {"%Q|%Ek|%Ol|%Ev|%Y", "Q|22|10|14-Nov-2023|2023"},
        {"%\xc3\xa5", "\xc3\xa5"},
        {"%%", "%"},
        {"literal text", "literal text"},
    };
    for (const auto& test : cases) {
        CheckFormat(guest, test.format, test.expected);
        CheckBufferSize(guest, test.format, std::strlen(test.expected));
    }
    CheckFormat(guest, "", "");
    char buffer[2]{'x', 'y'};
    Require(strftime_nid_postfix(buffer, 0, "%Q", &guest) == 0, "strftime accepted a zero-size buffer");
    Require(buffer[0] == 'x' && buffer[1] == 'y', "strftime wrote to a zero-size buffer");
}

static void CheckLocalRange() {
    constexpr std::array<std::int64_t, 10> timers{-1, -7200, -7201, -86400, -2208988800, -12622780801, -62135596800, 32535216000, 40000000000, 253402300799};
    for (const std::int64_t timer : timers) {
        GuestTm local{};
        *__error_nid_postfix() = 0;
        Require(localtime_s_nid_postfix(&timer, &local) == &local, "Local conversion rejected a time outside 1970 to 3000");
        Require(Equal(local, UtcTime(timer + 7200)), "Local conversion outside 1970 to 3000 has the wrong calendar fields");
        GuestTm normalized = local;
        normalized.tm_wday = -1;
        normalized.tm_yday = -1;
        Require(mktime_nid_postfix(&normalized) == timer && Equal(normalized, local), "mktime did not invert localtime outside 1970 to 3000");
        Require(*__error_nid_postfix() == 0, "A local conversion outside 1970 to 3000 set errno");
        const GuestTm* shared = localtime_nid_postfix(&timer);
        Require(shared != nullptr && Equal(*shared, local), "localtime rejected a time outside 1970 to 3000");
        Require(ctime_nid_postfix(&timer)[0] != '?', "ctime rejected a time outside 1970 to 3000");
    }
    GuestTm beforeEpoch{};
    beforeEpoch.tm_year = 70;
    beforeEpoch.tm_mon = -1;
    beforeEpoch.tm_mday = 1;
    beforeEpoch.tm_isdst = -1;
    Require(mktime_nid_postfix(&beforeEpoch) == -2685600 && beforeEpoch.tm_year == 69 && beforeEpoch.tm_mon == 11 && beforeEpoch.tm_mday == 1
        && beforeEpoch.tm_wday == 1 && beforeEpoch.tm_yday == 334, "mktime did not normalize a month before 1970");
    GuestTm afterLimit{};
    afterLimit.tm_year = 1200;
    afterLimit.tm_mon = 17;
    afterLimit.tm_mday = 15;
    afterLimit.tm_hour = 12;
    afterLimit.tm_isdst = -1;
    const std::int64_t late = mktime_nid_postfix(&afterLimit);
    Require(late == 35705181600 && afterLimit.tm_year == 1201 && afterLimit.tm_mon == 5 && afterLimit.tm_mday == 15 && afterLimit.tm_hour == 12
        && Equal(afterLimit, UtcTime(late + 7200)), "mktime did not normalize a month after 3000");
    GuestTm farDay{};
    farDay.tm_year = 70;
    farDay.tm_mday = -36500;
    farDay.tm_isdst = -1;
    const std::int64_t early = mktime_nid_postfix(&farDay);
    Require(early == -3153693600 && farDay.tm_mday >= 1 && Equal(farDay, UtcTime(early + 7200)), "mktime did not normalize a day before 1970");
    const std::int64_t overflow = std::numeric_limits<std::int64_t>::min();
    GuestTm scratch{};
    Require(localtime_s_nid_postfix(&overflow, &scratch) == nullptr, "Unrepresentable negative local time was accepted");
}

#ifndef _WIN32
static void CheckRegionZones() {
    constexpr std::array<const char*, 4> zones{"America/Los_Angeles", "Europe/Berlin", "Asia/Kolkata", "America/Argentina/Buenos_Aires"};
    constexpr std::array<std::int64_t, 9> timers{-62135553600, -12609691200, -5016340800, -5000011200, -2732356800, -2209032000, -2208988800, -2208945600, -631108800};
    for (const char* zone : zones) {
        Require(setenv_nid_postfix("TZ", zone, 1) == 0, "Could not set the region time zone");
        tzset_nid_postfix();
        for (const std::int64_t timer : timers) {
            GuestTm local{};
            Require(localtime_s_nid_postfix(&timer, &local) == &local, "Local conversion in a region zone failed");
            GuestTm normalized = local;
            normalized.tm_wday = -1;
            normalized.tm_yday = -1;
            if (mktime_nid_postfix(&normalized) != timer || !Equal(normalized, local)) {
                std::fprintf(stderr, "mktime did not invert localtime in %s at %lld: got %lld\n", zone, static_cast<long long>(timer), static_cast<long long>(mktime_nid_postfix(&local)));
                std::abort();
            }
            GuestTm unknown = local;
            unknown.tm_isdst = -1;
            Require(mktime_nid_postfix(&unknown) == timer && unknown.tm_isdst == local.tm_isdst, "mktime with an unknown daylight flag did not invert localtime in a region zone");
        }
    }
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();
}
#endif

static void CheckGuestZone() {
    const std::int64_t moment = 172800;
    GuestTm result{};
    Require(setenv_nid_postfix("TZ", "UTC-5", 1) == 0, "Could not set the guest time zone");
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 5,
        "localtime ignored a guest TZ change made without tzset");
    GuestTm calendar{};
    calendar.tm_year = 70;
    calendar.tm_mday = 3;
    calendar.tm_hour = 5;
    calendar.tm_isdst = -1;
    Require(mktime_nid_postfix(&calendar) == moment, "mktime ignored the guest TZ");
    Require(setenv_nid_postfix("TZ", "UTC+3", 1) == 0, "Could not change the guest time zone");
    tzset_nid_postfix();
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 21 && result.tm_mday == 2,
        "tzset ignored the guest TZ");
    Require(timezone_nid_postfix == 10800 && daylight_nid_postfix == 1, "tzset did not set timezone and daylight for UTC+3");
    Require(setenv_nid_postfix("TZ", "", 1) == 0, "Could not empty the guest time zone");
    tzset_nid_postfix();
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 0 && result.tm_mday == 3,
        "An empty guest TZ was not treated as UTC");
    Require(timezone_nid_postfix == 0 && daylight_nid_postfix == 1, "tzset did not set timezone and daylight for UTC");
    Require(setenv_nid_postfix("TZ", "EST5EDT", 1) == 0, "Could not set a daylight saving guest time zone");
    tzset_nid_postfix();
    Require(timezone_nid_postfix == 18000 && daylight_nid_postfix == 1, "timezone is not the standard offset of a daylight saving zone");
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();
    Require(setenv_nid_postfix("TZ", "EST5", 1) == 0, "Could not set the guest time zone without tzset");
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 19,
        "localtime ignored a guest TZ change made without tzset");
    Require(std::strcmp(tzname_nid_postfix[0], "EST") == 0 && std::strcmp(tzname_nid_postfix[1], "   ") == 0,
        "Implicit guest TZ update did not refresh tzname");
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();
}

static bool ZoneFieldsConsistent(const GuestTm& time, std::int64_t instant, int hour) {
    const std::int64_t seconds = time.tm_hour * 3600 + time.tm_min * 60 + time.tm_sec;
    bool inOneZone = false;
    for (const std::int64_t offset : {std::int64_t{18000}, std::int64_t{-10800}})
        inOneZone = inOneZone || seconds == ((instant + offset) % 86400 + 86400) % 86400;
    if (!inOneZone || time.tm_hour != hour) return false;
    char expected[4];
    std::snprintf(expected, sizeof(expected), "%02d", hour);
    char formatted[4]{};
    return strftime_nid_postfix(formatted, sizeof(formatted), "%H", &time) == 2 && std::strcmp(formatted, expected) == 0;
}

static void CheckZoneRace() {
    const std::int64_t moment = 172800;
    std::atomic<bool> stop{false};
    std::atomic<int> torn{0};
    std::thread flipper([&] {
        for (bool west = false; !stop.load(); west = !west) setenv_nid_postfix("TZ", west ? "UTC-5" : "UTC+3", 1);
    });
    std::array<std::thread, 6> readers;
    for (auto& reader : readers) {
        reader = std::thread([&] {
            for (int iteration = 0; iteration < 2500; ++iteration) {
                GuestTm result{};
                if (localtime_s_nid_postfix(&moment, &result) != &result || !ZoneFieldsConsistent(result, moment, result.tm_hour)) ++torn;
                GuestTm calendar{};
                calendar.tm_year = 70;
                calendar.tm_mday = 3;
                calendar.tm_hour = 12;
                calendar.tm_isdst = -1;
                const std::int64_t instant = mktime_nid_postfix(&calendar);
                if (instant == -1 || calendar.tm_mday != 3 || !ZoneFieldsConsistent(calendar, instant, 12)) ++torn;
            }
        });
    }
    for (auto& reader : readers) reader.join();
    stop = true;
    flipper.join();
    Require(torn == 0, "A time conversion mixed the fields of two time zones while TZ changed");
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();
}

#ifdef _WIN32
static void CheckRejectedZone() {
    const std::int64_t moment = 172800;
    const std::string oversized = "UTC-5" + std::string(40000, 'x');
    Require(setenv_nid_postfix("TZ", oversized.c_str(), 1) == 0, "The guest environment rejected a long TZ");
    GuestTm result{};
    bool thrown = false;
    try {
        localtime_s_nid_postfix(&moment, &result);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown, "localtime accepted a TZ the host cannot hold");
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 2, "localtime did not recover after a rejected TZ");
}
#endif

int main() {
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not set the guest time zone");
    tzset_nid_postfix();
    constexpr std::array<UtcCase, 7> utcCases{{
        {0, 70, 0, 1, 0, 0, 0, 4, 0},
        {-1, 69, 11, 31, 23, 59, 59, 3, 364},
        {-31536000, 69, 0, 1, 0, 0, 0, 3, 0},
        {951782400, 100, 1, 29, 0, 0, 0, 2, 59},
        {32535216000, 1101, 0, 1, 0, 0, 0, 4, 0},
        {253402300799, 8099, 11, 31, 23, 59, 59, 5, 364},
        {-62135596800, -1899, 0, 1, 0, 0, 0, 1, 0},
    }};
    for (const auto& utcCase : utcCases) CheckUtc(utcCase);
    const std::int64_t underflow = std::numeric_limits<std::int64_t>::min();
    GuestTm scratch{};
    Require(gmtime_s_nid_postfix(&underflow, &scratch) == nullptr, "Unrepresentable negative time was accepted");
    const std::int64_t overflow = std::numeric_limits<std::int64_t>::max();
    for (const std::int64_t* timer : {&underflow, &overflow}) {
        *__error_nid_postfix() = 1234;
        Require(gmtime_s_nid_postfix(timer, &scratch) == nullptr && *__error_nid_postfix() == 1234,
            "gmtime_s of an unrepresentable time changed errno");
        Require(gmtime_nid_postfix(timer) == nullptr && *__error_nid_postfix() == 1234,
            "gmtime of an unrepresentable time changed errno");
        Require(localtime_s_nid_postfix(timer, &scratch) == nullptr && *__error_nid_postfix() == 1234,
            "localtime_s of an unrepresentable time changed errno");
        Require(localtime_nid_postfix(timer) == nullptr && *__error_nid_postfix() == 1234,
            "localtime of an unrepresentable time changed errno");
    }

    CheckAsctime(UtcTime(0), "Thu Jan  1 00:00:00 1970\n");
    CheckAsctime(UtcTime(-2208988801), "Sun Dec 31 23:59:59 1899\n");
    CheckAsctime(UtcTime(-30610224000), "Wed Jan  1 00:00:00 1000\n");
    GuestTm pastMonthEnd = UtcTime(983404800);
    pastMonthEnd.tm_mon = 1;
    pastMonthEnd.tm_mday = 29;
    CheckAsctime(pastMonthEnd, "Thu Feb 29 00:00:00 2001\n");

    const std::int64_t epoch = 0;
    GuestTm local{};
    std::memset(&local, 0xAA, sizeof(local));
    Require(localtime_s_nid_postfix(&epoch, &local) == &local && local.tm_year == 70 && local.tm_mon == 0
        && local.tm_mday == 1 && local.tm_hour == 2, "Local epoch conversion failed");

    GuestTm roundTrip = local;
    roundTrip.tm_wday = -1;
    roundTrip.tm_yday = -1;
    Require(mktime_nid_postfix(&roundTrip) == 0, "mktime did not invert localtime");
    Require(roundTrip.tm_wday == 4 && roundTrip.tm_yday == 0, "mktime did not normalize the calendar fields");

    Require(std::strcmp(ctime_nid_postfix(&epoch), "Thu Jan  1 02:00:00 1970\n") == 0, "ctime did not format local time");
    const std::int64_t leap = 951782400 + 13 * 3600 + 4 * 60 + 5;
    Require(std::strcmp(ctime_nid_postfix(&leap), "Tue Feb 29 15:04:05 2000\n") == 0, "ctime formatted the wrong date");
    *__error_nid_postfix() = 0;
    Require(std::strcmp(ctime_nid_postfix(&underflow), "??? ??? ?? ??:??:?? ????\n") == 0 && *__error_nid_postfix() == 22,
        "ctime of an unrepresentable time did not report it");

    constexpr std::int64_t latest = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t earliest = std::numeric_limits<std::int64_t>::min();
    constexpr std::array<DifferenceCase, 9> differenceCases{{
        {10, 3, 7.0},
        {3, 10, -7.0},
        {0, -1, 1.0},
        {-1, 0, -1.0},
        {-86400, -172800, 86400.0},
        {latest, latest - 1, 1.0},
        {earliest + 1, earliest, 1.0},
        {latest, earliest, 0x1p64},
        {earliest, latest, -0x1p64},
    }};
    for (const auto& differenceCase : differenceCases) CheckDifference(differenceCase);

    GuestTm utc{};
    Require(gmtime_s_nid_postfix(&epoch, &utc) == &utc, "UTC epoch conversion failed");
    char formatted[64]{};
    Require(strftime_nid_postfix(formatted, sizeof(formatted), "%Y-%m-%d %H:%M:%S", &utc) == 19
        && std::strcmp(formatted, "1970-01-01 00:00:00") == 0, "strftime did not read the guest tm");

    CheckFreeBsdConversions();
    CheckLocalRange();
#ifndef _WIN32
    CheckRegionZones();
#endif
    CheckLiteralConversions();

    CheckConcurrent(libc_gmtime_nid_postfix, gmtime_s_nid_postfix, "Concurrent libc_gmtime returned another thread's date");
    CheckConcurrent(gmtime_nid_postfix, gmtime_s_nid_postfix, "Concurrent gmtime returned another thread's date");
    CheckConcurrent(libc_localtime_nid_postfix, localtime_s_nid_postfix, "Concurrent libc_localtime returned another thread's date");
    CheckConcurrent(localtime_nid_postfix, localtime_s_nid_postfix, "Concurrent localtime returned another thread's date");
    CheckGuestZone();
    CheckZoneRace();
#ifdef _WIN32
    CheckRejectedZone();
#endif
    Require(setenv_nid_postfix("TZ", "UTC-5", 1) == 0, "Could not set the guest time zone");
    tzset_nid_postfix();
    GuestTm shifted{};
    Require(localtime_s_nid_postfix(&epoch, &shifted) == &shifted && shifted.tm_hour == 5,
        "tzset did not pick up the new TZ");
    Require(tzname_nid_postfix[0] != nullptr && std::strcmp(tzname_nid_postfix[0], "UTC") == 0 && tzname_nid_postfix[1] != nullptr &&
            std::strcmp(tzname_nid_postfix[1], "   ") == 0,
        "tzname does not name the zone tzset set and leave the daylight name blank");
    Require(setenv_nid_postfix("TZ", "EST5EDT", 1) == 0, "Could not set the guest time zone");
    tzset_nid_postfix();
    Require(std::strcmp(tzname_nid_postfix[0], "EST") == 0 && std::strcmp(tzname_nid_postfix[1], "EDT") == 0,
        "tzname does not name the standard and daylight zones");
}
