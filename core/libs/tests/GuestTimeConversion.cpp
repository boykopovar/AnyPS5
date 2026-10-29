#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
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

extern char* tzname_nid_postfix[2];
extern std::int64_t timezone_nid_postfix;
extern int daylight_nid_postfix;

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
        && utc.tm_wday == expected.wday && utc.tm_yday == expected.yday && utc.tm_isdst == 0,
        "UTC calendar fields are wrong");
    Require(utc.tm_gmtoff == 0, "UTC conversion did not reset tm_gmtoff");
    Require(utc.tm_zone != nullptr && std::strcmp(utc.tm_zone, "UTC") == 0, "UTC conversion did not set tm_zone");
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


static void CheckConsoleConversions() {
    const std::int64_t timer = 1700000000;
    GuestTm guest{};
    Require(gmtime_s_nid_postfix(&timer, &guest) == &guest, "UTC conversion failed");
    CheckFormat(guest, "%k|%l|%v|%+|%s", "%k|%l|%v|%+|%s");
    CheckFormat(guest, "%%k %%l %%s %%v %%+", "%k %l %s %v %+");
    CheckFormat(guest, "%Ek|%Ol|%Os|%Ev|%O+", "%k|%l|%s|%v|%+");
    CheckFormat(guest, "%Ec|%Ox|%EY|%Om", "Tue Nov 14 22:13:20 2023|11/14/23|2023|11");
    CheckFormat(guest, "%EE|%OO|%EO", "%E|%O|%O");
    CheckBufferSize(guest, "%k", 2);
    CheckBufferSize(guest, "%c", 24);
    CheckBufferSize(guest, "%Ek", 2);
    bool threw = false;
    try {
        char buffer[16];
        strftime_nid_postfix(buffer, sizeof(buffer), "ab%O", &guest);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "strftime accepted a format ending after a modifier");

    guest.tm_zone = "XYZ";
    guest.tm_isdst = 0;
    CheckFormat(guest, "%Z", tzname_nid_postfix[0]);
    guest.tm_isdst = 1;
    CheckFormat(guest, "%Z", tzname_nid_postfix[1]);
    guest.tm_isdst = 0;

    guest.tm_hour = 0;
    CheckFormat(guest, "%I|%p", "12|AM");
    guest.tm_hour = 12;
    CheckFormat(guest, "%I|%p", "12|PM");
    guest.tm_hour = 13;
    CheckFormat(guest, "%I|%p", "01|PM");
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
        {"%Q", "%Q"},
        {"%P|%f|%N|%1|%#Y", "%P|%f|%N|%1|%#Y"},
        {"[%Q] %Y-%m-%d", "[%Q] 2023-11-14"},
        {"%EQ|%OQ|%_Q|%-Q|%0Q", "%Q|%Q|%_Q|%-Q|%0Q"},
        {"%--Y|%__Y|%00Y", "%--Y|%__Y|%00Y"},
        {"%", "%"},
        {"text%", "text%"},
        {"%_", "%_"},
        {"%Q%", "%Q%"},
        {"%Y%", "2023%"},
        {"%%Q|%%%Q|%%%%Q", "%Q|%%Q|%%Q"},
        {"%\xc3\xa5", "%\xc3\xa5"},
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
        Require(localtime_s_nid_postfix(&timer, &local) == &local && local.tm_gmtoff == 7200, "Local conversion rejected a time outside 1970 to 3000");
        Require(Equal(local, UtcTime(timer + 7200)), "Local conversion outside 1970 to 3000 has the wrong calendar fields");
        Require(local.tm_zone != nullptr && std::strlen(local.tm_zone) > 0, "Local conversion outside 1970 to 3000 did not set tm_zone");
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

#ifdef _WIN32
extern "C" long* __cdecl __dstbias(void);

struct DaylightBiasFixture {
    long saved = *__dstbias();
    ~DaylightBiasFixture() { *__dstbias() = saved; }
    void Apply() const { *__dstbias() = -3600; }
};

struct ZoneOffset {
    const char* zone;
    std::int64_t timer;
    long offset;
    int isdst;
};

static void CheckPosixFormZoneOffsets() {
    DaylightBiasFixture bias;
    constexpr std::array<ZoneOffset, 4> expectations{{
        {"PST8PDT", 979560000, -28800, 0},
        {"PST8PDT", 995198400, -25200, 1},
        {"EST5EDT", 979560000, -18000, 0},
        {"EST5EDT", 995198400, -14400, 1},
    }};
    for (const ZoneOffset& expected : expectations) {
        Require(setenv_nid_postfix("TZ", expected.zone, 1) == 0, "Could not set the POSIX-form time zone");
        tzset_nid_postfix();
        bias.Apply();
        GuestTm local{};
        Require(localtime_s_nid_postfix(&expected.timer, &local) == &local, "Local conversion in a POSIX-form zone failed");
        if (local.tm_gmtoff != expected.offset || local.tm_isdst != expected.isdst) {
            long hostTimezone = -1;
            long hostDstBias = -1;
            _get_timezone(&hostTimezone);
            _get_dstbias(&hostDstBias);
            std::tm hostLocal{};
            const __time64_t hostTimer = expected.timer;
            _localtime64_s(&hostLocal, &hostTimer);
            std::fprintf(stderr, "%s at %lld: offset %ld daylight %d, expected %ld daylight %d (host timezone %ld dstbias %ld daylight %d)\n", expected.zone, static_cast<long long>(expected.timer), static_cast<long>(local.tm_gmtoff), local.tm_isdst, expected.offset, expected.isdst, hostTimezone, hostDstBias, hostLocal.tm_isdst);
            std::abort();
        }
    }
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();
}
#endif

static void CheckRegionZones() {
#ifdef _WIN32
    constexpr std::array<const char*, 2> zones{"PST8PDT", "EST5EDT"};
    constexpr std::array<std::int64_t, 11> timers{-62135553600, -12609691200, -5016340800, -5000011200, -2732356800, -2209032000, -2208988800, -2208945600, -631108800, 979560000, 995198400};
#else
    constexpr std::array<const char*, 4> zones{"America/Los_Angeles", "Europe/Berlin", "Asia/Kolkata", "America/Argentina/Buenos_Aires"};
    constexpr std::array<std::int64_t, 9> timers{-62135553600, -12609691200, -5016340800, -5000011200, -2732356800, -2209032000, -2208988800, -2208945600, -631108800};
#endif
#ifdef _WIN32
    DaylightBiasFixture bias;
#endif
    for (const char* zone : zones) {
        Require(setenv_nid_postfix("TZ", zone, 1) == 0, "Could not set the region time zone");
        tzset_nid_postfix();
#ifdef _WIN32
        bias.Apply();
#endif
        for (const std::int64_t timer : timers) {
            GuestTm local{};
            Require(localtime_s_nid_postfix(&timer, &local) == &local, "Local conversion in a region zone failed");
            GuestTm normalized = local;
            normalized.tm_wday = -1;
            normalized.tm_yday = -1;
            if (mktime_nid_postfix(&normalized) != timer || !Equal(normalized, local)) {
                std::fprintf(stderr, "mktime did not invert localtime in %s at %lld (offset %ld): got %lld\n", zone, static_cast<long long>(timer), static_cast<long>(local.tm_gmtoff), static_cast<long long>(mktime_nid_postfix(&local)));
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

static void CheckGuestZone() {
    const std::int64_t moment = 172800;
    GuestTm result{};
    Require(setenv_nid_postfix("TZ", "UTC-5", 1) == 0, "Could not set the guest time zone");
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 5 && result.tm_gmtoff == 18000,
        "localtime ignored a guest TZ change made without tzset");
    GuestTm calendar{};
    calendar.tm_year = 70;
    calendar.tm_mday = 3;
    calendar.tm_hour = 5;
    calendar.tm_isdst = -1;
    Require(mktime_nid_postfix(&calendar) == moment, "mktime ignored the guest TZ");
    Require(setenv_nid_postfix("TZ", "UTC+3", 1) == 0, "Could not change the guest time zone");
    tzset_nid_postfix();

    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 21 && result.tm_mday == 2 && result.tm_gmtoff == -10800,
        "tzset ignored the guest TZ");
    Require(timezone_nid_postfix == 10800 && daylight_nid_postfix == 1, "tzset did not set timezone and daylight for UTC+3");
    Require(setenv_nid_postfix("TZ", "", 1) == 0, "Could not empty the guest time zone");
    tzset_nid_postfix();

    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 0 && result.tm_mday == 3 && result.tm_gmtoff == 0,
        "An empty guest TZ was not treated as UTC");
    Require(timezone_nid_postfix == 0 && daylight_nid_postfix == 1, "tzset did not set timezone and daylight for UTC");
    Require(setenv_nid_postfix("TZ", "EST5EDT", 1) == 0, "Could not set a daylight saving guest time zone");
    tzset_nid_postfix();

    Require(timezone_nid_postfix == 18000 && daylight_nid_postfix == 1, "timezone is not the standard offset of a daylight saving zone");
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();

    Require(setenv_nid_postfix("TZ", "EST5", 1) == 0, "Could not set the guest time zone without tzset");
    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_hour == 19 && result.tm_gmtoff == -18000,
        "localtime ignored a guest TZ change made without tzset");
    Require(std::strcmp(tzname_nid_postfix[0], "EST") == 0 && std::strcmp(tzname_nid_postfix[1], "   ") == 0,
        "Implicit guest TZ update did not refresh tzname");
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not restore the guest time zone");
    tzset_nid_postfix();

}

static bool ZoneFieldsConsistent(const GuestTm& time, std::int64_t instant, int hour) {
    const std::int64_t localSeconds = ((instant + time.tm_gmtoff) % 86400 + 86400) % 86400;
    if (time.tm_hour * 3600 + time.tm_min * 60 + time.tm_sec != localSeconds || time.tm_hour != hour) return false;
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
        for (bool west = false; !stop.load(); west = !west) {
            setenv_nid_postfix("TZ", west ? "UTC-5" : "UTC+3", 1);

            std::this_thread::yield();
        }
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

    Require(localtime_s_nid_postfix(&moment, &result) == &result && result.tm_gmtoff == 7200, "localtime did not recover after a rejected TZ");
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
    Require(local.tm_gmtoff == 7200, "Local conversion did not set tm_gmtoff");
    Require(local.tm_zone != nullptr && std::strlen(local.tm_zone) > 0, "Local conversion did not set tm_zone");

    GuestTm roundTrip = local;
    roundTrip.tm_wday = -1;
    roundTrip.tm_yday = -1;
    roundTrip.tm_gmtoff = -1;
    roundTrip.tm_zone = nullptr;
    Require(mktime_nid_postfix(&roundTrip) == 0, "mktime did not invert localtime");
    Require(roundTrip.tm_wday == 4 && roundTrip.tm_yday == 0, "mktime did not normalize the calendar fields");
    Require(roundTrip.tm_gmtoff == 7200 && roundTrip.tm_zone != nullptr, "mktime did not set the zone fields");

    Require(std::strcmp(ctime_nid_postfix(&epoch), "Thu Jan  1 02:00:00 1970\n") == 0, "ctime did not format local time");
    const std::int64_t leap = 951782400 + 13 * 3600 + 4 * 60 + 5;
    Require(std::strcmp(ctime_nid_postfix(&leap), "Tue Feb 29 15:04:05 2000\n") == 0, "ctime formatted the wrong date");
    *__error_nid_postfix() = 0;
    Require(std::strcmp(ctime_nid_postfix(&underflow), "??? ??? ?? ??:??:?? ????\n") == 0 && *__error_nid_postfix() == 22,
        "ctime of an unrepresentable time did not report it");
    {
        std::barrier finish(3);
        std::array<const char*, 2> texts{};
        std::array<std::thread, 2> readers;
        for (std::size_t index = 0; index < readers.size(); ++index) {
            readers[index] = std::thread([&, index] {
                texts[index] = ctime_nid_postfix(&underflow);
                finish.arrive_and_wait();
            });
        }
        const char* own = ctime_nid_postfix(&underflow);
        finish.arrive_and_wait();
        Require(texts[0] != texts[1] && texts[0] != own && texts[1] != own, "ctime of an unrepresentable time shared one buffer between threads");
        for (auto& reader : readers) reader.join();
    }

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

    CheckConsoleConversions();
    CheckLocalRange();
    CheckRegionZones();
#ifdef _WIN32
    CheckPosixFormZoneOffsets();
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
    Require(localtime_s_nid_postfix(&epoch, &shifted) == &shifted && shifted.tm_hour == 5 && shifted.tm_gmtoff == 18000,
        "tzset did not pick up the new TZ");
    Require(tzname_nid_postfix[0] != nullptr && std::strcmp(tzname_nid_postfix[0], "UTC") == 0 && tzname_nid_postfix[1] != nullptr &&
            std::strcmp(tzname_nid_postfix[1], "   ") == 0,
        "tzname does not name the zone tzset set and leave the daylight name blank");
    Require(setenv_nid_postfix("TZ", "EST5EDT", 1) == 0, "Could not set the guest time zone");
    tzset_nid_postfix();

    Require(std::strcmp(tzname_nid_postfix[0], "EST") == 0 && std::strcmp(tzname_nid_postfix[1], "EDT") == 0,
        "tzname does not name the standard and daylight zones");
}
