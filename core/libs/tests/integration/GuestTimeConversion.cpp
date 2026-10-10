#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using Converter = GuestTm* (APS5_VABI *)(const std::int64_t*);
using BufferedConverter = GuestTm* (APS5_VABI *)(const std::int64_t*, GuestTm*);

class TimeZoneFixture {
public:
    TimeZoneFixture() {
        if (const char* current = std::getenv("TZ")) previous = current;
        Apply("UTC-2");
    }

    ~TimeZoneFixture() {
        if (previous) {
            Apply(previous->c_str());
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

    TimeZoneFixture(const TimeZoneFixture&) = delete;
    TimeZoneFixture& operator=(const TimeZoneFixture&) = delete;

private:
    static void Apply(const char* zone) {
#ifdef _WIN32
        _putenv_s("TZ", zone);
        _tzset();
#else
        setenv("TZ", zone, 1);
        tzset();
#endif
    }

    std::optional<std::string> previous;
};

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

constexpr std::array<UtcCase, 7> utcCases{{
    {0, 70, 0, 1, 0, 0, 0, 4, 0},
    {-1, 69, 11, 31, 23, 59, 59, 3, 364},
    {-31536000, 69, 0, 1, 0, 0, 0, 3, 0},
    {951782400, 100, 1, 29, 0, 0, 0, 2, 59},
    {32535216000, 1101, 0, 1, 0, 0, 0, 4, 0},
    {253402300799, 8099, 11, 31, 23, 59, 59, 5, 364},
    {-62135596800, -1899, 0, 1, 0, 0, 0, 1, 0},
}};

bool Equal(const GuestTm& left, const GuestTm& right) {
    return left.tm_sec == right.tm_sec && left.tm_min == right.tm_min && left.tm_hour == right.tm_hour
        && left.tm_mday == right.tm_mday && left.tm_mon == right.tm_mon && left.tm_year == right.tm_year
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst;
}

GuestTm ConvertUtc(std::int64_t timer, const std::string& label) {
    GuestTm utc{};
    std::memset(&utc, 0xAA, sizeof(utc));
    Require(gmtime_s_nid_postfix(&timer, &utc) == &utc, label + ": conversion returns the buffer");
    return utc;
}

GuestTm LocalEpoch() {
    const std::int64_t epoch = 0;
    GuestTm local{};
    std::memset(&local, 0xAA, sizeof(local));
    Require(localtime_s_nid_postfix(&epoch, &local) == &local, "local epoch conversion returns the buffer");
    return local;
}

GuestTm RoundTripInput() {
    GuestTm roundTrip = LocalEpoch();
    roundTrip.tm_wday = -1;
    roundTrip.tm_yday = -1;
    roundTrip.tm_gmtoff = -1;
    roundTrip.tm_zone = nullptr;
    return roundTrip;
}

void RequireConcurrentConversions(Converter convert, BufferedConverter buffered) {
    constexpr std::array<std::int64_t, 8> timers{0, 86400, 946684800, 1078012800, 1609459200, 1709164800, 1893456000, 2145916799};
    std::array<GuestTm, timers.size()> expected{};
    for (std::size_t index = 0; index < timers.size(); ++index) {
        Require(buffered(&timers[index], &expected[index]) == &expected[index],
            "buffered conversion of " + std::to_string(timers[index]));
    }
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
    Require(matches, "every thread saw its own date");
}

void RequireUnrepresentableRejected(Converter convert) {
    const std::int64_t invalid = std::numeric_limits<std::int64_t>::max();
    Require(convert(&invalid) == nullptr, "INT64_MAX is rejected");
}

const Case utcFields{"GmtimeS_RepresentableTimes_FillsCalendarFields", [] {
    for (const auto& expected : utcCases) {
        const std::string label = "timer " + std::to_string(expected.timer);
        const GuestTm utc = ConvertUtc(expected.timer, label);
        RequireEqual(utc.tm_year, expected.year, label + " tm_year");
        RequireEqual(utc.tm_mon, expected.mon, label + " tm_mon");
        RequireEqual(utc.tm_mday, expected.mday, label + " tm_mday");
        RequireEqual(utc.tm_hour, expected.hour, label + " tm_hour");
        RequireEqual(utc.tm_min, expected.min, label + " tm_min");
        RequireEqual(utc.tm_sec, expected.sec, label + " tm_sec");
        RequireEqual(utc.tm_wday, expected.wday, label + " tm_wday");
        RequireEqual(utc.tm_yday, expected.yday, label + " tm_yday");
        RequireEqual(utc.tm_isdst, 0, label + " tm_isdst");
    }
}};

const Case utcZone{"GmtimeS_RepresentableTimes_SetsUtcZoneFields", [] {
    for (const auto& expected : utcCases) {
        const std::string label = "timer " + std::to_string(expected.timer);
        const GuestTm utc = ConvertUtc(expected.timer, label);
        RequireEqual(utc.tm_gmtoff, std::int64_t{0}, label + " tm_gmtoff");
        Require(utc.tm_zone != nullptr, label + " tm_zone is set");
        RequireEqual(std::string_view(utc.tm_zone), std::string_view("UTC"), label + " tm_zone");
    }
}};

const Case utcUnderflow{"GmtimeS_Int64Min_ReturnsNull", [] {
    const std::int64_t underflow = std::numeric_limits<std::int64_t>::min();
    GuestTm scratch{};
    Require(gmtime_s_nid_postfix(&underflow, &scratch) == nullptr, "INT64_MIN is rejected");
}};

const Case localFields{"LocaltimeS_EpochInUtcMinusTwo_ShiftsTwoHoursAhead", [] {
    const TimeZoneFixture zone;
    const GuestTm local = LocalEpoch();
    RequireEqual(local.tm_year, 70, "tm_year");
    RequireEqual(local.tm_mon, 0, "tm_mon");
    RequireEqual(local.tm_mday, 1, "tm_mday");
    RequireEqual(local.tm_hour, 2, "tm_hour");
}};

const Case localZone{"LocaltimeS_EpochInUtcMinusTwo_SetsZoneFields", [] {
    const TimeZoneFixture zone;
    const GuestTm local = LocalEpoch();
    RequireEqual(local.tm_gmtoff, std::int64_t{7200}, "tm_gmtoff");
    Require(local.tm_zone != nullptr && std::strlen(local.tm_zone) > 0, "tm_zone is a non-empty name");
}};

const Case mktimeInverse{"Mktime_LocalEpochFields_ReturnsZero", [] {
    const TimeZoneFixture zone;
    GuestTm roundTrip = RoundTripInput();
    RequireEqual(mktime_nid_postfix(&roundTrip), std::int64_t{0}, "mktime inverts localtime");
}};

const Case mktimeNormalize{"Mktime_InvalidDayFields_NormalizesWeekdayAndYearday", [] {
    const TimeZoneFixture zone;
    GuestTm roundTrip = RoundTripInput();
    RequireEqual(mktime_nid_postfix(&roundTrip), std::int64_t{0}, "mktime result");
    RequireEqual(roundTrip.tm_wday, 4, "tm_wday");
    RequireEqual(roundTrip.tm_yday, 0, "tm_yday");
}};

const Case mktimeZone{"Mktime_ClearedZoneFields_SetsZoneFields", [] {
    const TimeZoneFixture zone;
    GuestTm roundTrip = RoundTripInput();
    RequireEqual(mktime_nid_postfix(&roundTrip), std::int64_t{0}, "mktime result");
    RequireEqual(roundTrip.tm_gmtoff, std::int64_t{7200}, "tm_gmtoff");
    Require(roundTrip.tm_zone != nullptr, "tm_zone is set");
}};

const Case strftimeGuest{"Strftime_GuestUtcEpoch_FormatsDateAndTime", [] {
    const GuestTm utc = ConvertUtc(0, "epoch");
    char formatted[64]{};
    RequireEqual(strftime_nid_postfix(formatted, sizeof(formatted), "%Y-%m-%d %H:%M:%S", &utc), std::size_t{19}, "length");
    RequireEqual(std::string_view(formatted), std::string_view("1970-01-01 00:00:00"), "text");
}};

const Case libcGmtimeConcurrent{"LibcGmtime_ConcurrentThreads_ReturnOwnDate", [] {
    RequireConcurrentConversions(libc_gmtime_nid_postfix, gmtime_s_nid_postfix);
}};

const Case libcGmtimeInvalid{"LibcGmtime_Int64Max_ReturnsNull", [] {
    RequireUnrepresentableRejected(libc_gmtime_nid_postfix);
}};

const Case gmtimeConcurrent{"Gmtime_ConcurrentThreads_ReturnOwnDate", [] {
    RequireConcurrentConversions(gmtime_nid_postfix, gmtime_s_nid_postfix);
}};

const Case gmtimeInvalid{"Gmtime_Int64Max_ReturnsNull", [] {
    RequireUnrepresentableRejected(gmtime_nid_postfix);
}};

const Case libcLocaltimeConcurrent{"LibcLocaltime_ConcurrentThreads_ReturnOwnDate", [] {
    const TimeZoneFixture zone;
    RequireConcurrentConversions(libc_localtime_nid_postfix, localtime_s_nid_postfix);
}};

const Case libcLocaltimeInvalid{"LibcLocaltime_Int64Max_ReturnsNull", [] {
    const TimeZoneFixture zone;
    RequireUnrepresentableRejected(libc_localtime_nid_postfix);
}};

const Case localtimeConcurrent{"Localtime_ConcurrentThreads_ReturnOwnDate", [] {
    const TimeZoneFixture zone;
    RequireConcurrentConversions(localtime_nid_postfix, localtime_s_nid_postfix);
}};

const Case localtimeInvalid{"Localtime_Int64Max_ReturnsNull", [] {
    const TimeZoneFixture zone;
    RequireUnrepresentableRejected(localtime_nid_postfix);
}};

} // namespace
