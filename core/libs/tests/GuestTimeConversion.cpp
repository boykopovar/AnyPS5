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
#include <limits>
#include <thread>

extern "C" {
GuestTm* APS5_VABI libc_gmtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI gmtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI libc_localtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI localtime_nid_postfix(const std::int64_t*);
GuestTm* APS5_VABI gmtime_s_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI localtime_s_nid_postfix(const std::int64_t*, GuestTm*);
std::int64_t APS5_VABI mktime_nid_postfix(GuestTm*);
std::size_t APS5_VABI strftime_nid_postfix(char*, std::size_t, const char*, const GuestTm*);
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
            for (int iteration = 0; iteration < 50000; ++iteration) {
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

static void CheckFormat(const GuestTm& time, const char* format, const char* expected) {
    char buffer[96]{};
    const std::size_t size = strftime_nid_postfix(buffer, sizeof(buffer), format, &time);
    if (size != std::strlen(expected) || std::strcmp(buffer, expected) != 0)
        std::fprintf(stderr, "Format '%s': expected '%s', got '%s' (%zu bytes)\n", format, expected, buffer, size);
    Require(size == std::strlen(expected) && std::strcmp(buffer, expected) == 0, "strftime did not format a FreeBSD conversion");
}

static void CheckFreeBsdConversions() {
    const std::int64_t timer = 1700000000;
    GuestTm time{};
    Require(gmtime_s_nid_postfix(&timer, &time) == &time, "UTC conversion failed");
    CheckFormat(time, "%k|%l", "22|10");
    CheckFormat(time, "%v", "14-Nov-2023");
    CheckFormat(time, "%+", "Tue Nov 14 22:13:20 UTC 2023");
    CheckFormat(time, "%s", "1699992800");
    CheckFormat(time, "%%k %%l %%s %%v %%+", "%k %l %s %v %+");
    CheckFormat(time, "[%k][%l]", "[22][10]");
    time.tm_hour = 0;
    CheckFormat(time, "%k|%l", " 0|12");
    time.tm_hour = 5;
    time.tm_mday = 5;
    CheckFormat(time, "%k|%l|%v", " 5| 5| 5-Nov-2023");
    time.tm_hour = 13;
    CheckFormat(time, "%k|%l", "13| 1");
    char tooSmall[4]{};
    Require(strftime_nid_postfix(tooSmall, sizeof(tooSmall), "%k:%l", &time) == 0, "strftime accepted an undersized buffer");
}

int main() {
#ifdef _WIN32
    _putenv_s("TZ", "UTC-2");
    _tzset();
#else
    setenv("TZ", "UTC-2", 1);
    tzset();
#endif
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

    GuestTm utc{};
    Require(gmtime_s_nid_postfix(&epoch, &utc) == &utc, "UTC epoch conversion failed");
    char formatted[64]{};
    Require(strftime_nid_postfix(formatted, sizeof(formatted), "%Y-%m-%d %H:%M:%S", &utc) == 19
        && std::strcmp(formatted, "1970-01-01 00:00:00") == 0, "strftime did not read the guest tm");

    CheckFreeBsdConversions();

    CheckConcurrent(libc_gmtime_nid_postfix, gmtime_s_nid_postfix, "Concurrent libc_gmtime returned another thread's date");
    CheckConcurrent(gmtime_nid_postfix, gmtime_s_nid_postfix, "Concurrent gmtime returned another thread's date");
    CheckConcurrent(libc_localtime_nid_postfix, localtime_s_nid_postfix, "Concurrent libc_localtime returned another thread's date");
    CheckConcurrent(localtime_nid_postfix, localtime_s_nid_postfix, "Concurrent localtime returned another thread's date");
}
