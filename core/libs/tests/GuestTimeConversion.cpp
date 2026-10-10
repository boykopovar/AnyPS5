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
    CheckFormat(guest, "%+", "Tue Nov 14 22:13:20 UTC 2023");
    CheckFormat(guest, "%s", "1699992800");
    CheckFormat(guest, "%%k %%l %%s %%v %%+", "%k %l %s %v %+");
    CheckFormat(guest, "%%%k", "%22");
    CheckFormat(guest, "[%k][%l]", "[22][10]");
    CheckFormat(guest, "%Ek|%Ol|%Os|%Ev|%O+", "22|10|1699992800|14-Nov-2023|Tue Nov 14 22:13:20 UTC 2023");
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
    CheckBufferSize(guest, "%+", 28);
    CheckBufferSize(guest, "%Ek", 2);

    guest.tm_zone = "XYZ";
    CheckFormat(guest, "%+", "Tue Nov 14 22:13:20 XYZ 2023");
    CheckFormat(guest, "%O+", "Tue Nov 14 22:13:20 XYZ 2023");
    guest.tm_zone = "A%B";
    CheckFormat(guest, "%+", "Tue Nov 14 22:13:20 A%B 2023");
    guest.tm_zone = nullptr;
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
