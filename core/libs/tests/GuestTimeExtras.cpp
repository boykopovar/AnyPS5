#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <random>

struct Xtime {
    std::int64_t sec;
    std::int64_t nsec;
};

extern "C" {
GuestTm* APS5_VABI gmtime_s_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI localtime_s_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI gmtime_r_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI localtime_r_nid_postfix(const std::int64_t*, GuestTm*);
std::int64_t APS5_VABI timegm_nid_postfix(GuestTm*);
void APS5_VABI tzset_nid_postfix();
int APS5_VABI setenv_nid_postfix(const char*, const char*, int);
int APS5_VABI xtime_get_nid_postfix(Xtime*, int);
Xtime APS5_VABI _Xtime_to_ts_nid_postfix(const Xtime*);
Xtime APS5_VABI _Xtime_diff_to_ts_nid_postfix(const Xtime*);
}

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

struct TimegmCase {
    int sec, min, hour, mday, mon, year;
    std::int64_t expected;
    int normalizedMday, normalizedMon, normalizedYear, wday, yday;
};

static void CheckTimegm(const TimegmCase& test) {
    GuestTm time{};
    time.tm_sec = test.sec;
    time.tm_min = test.min;
    time.tm_hour = test.hour;
    time.tm_mday = test.mday;
    time.tm_mon = test.mon;
    time.tm_year = test.year;
    time.tm_wday = 9;
    time.tm_yday = 999;
    time.tm_isdst = 5;
    time.tm_gmtoff = 77;
    if (timegm_nid_postfix(&time) != test.expected) {
        std::fprintf(stderr, "timegm(%d-%d-%d) returned the wrong time\n", test.year, test.mon, test.mday);
        std::abort();
    }
    Require(time.tm_mday == test.normalizedMday && time.tm_mon == test.normalizedMon && time.tm_year == test.normalizedYear
        && time.tm_wday == test.wday && time.tm_yday == test.yday && time.tm_isdst == 0 && time.tm_gmtoff == 0,
        "timegm did not normalize the calendar fields");
}

static void CheckReentrantConversions() {
    constexpr std::int64_t timers[] = {0, -1, 86399, 951782400, 4107542400, 253402300799, -62135596800};
    for (const std::int64_t timer : timers) {
        GuestTm expected{};
        GuestTm actual{};
        Require(gmtime_s_nid_postfix(&timer, &expected) == &expected, "gmtime_s rejected a representable time");
        Require(gmtime_r_nid_postfix(&timer, &actual) == &actual && Equal(actual, expected), "gmtime_r differs from gmtime_s");
        Require(localtime_s_nid_postfix(&timer, &expected) == &expected, "localtime_s rejected a representable time");
        Require(localtime_r_nid_postfix(&timer, &actual) == &actual && Equal(actual, expected) && actual.tm_gmtoff == 7200,
            "localtime_r differs from localtime_s");
    }
    const std::int64_t epoch = 0;
    Require(gmtime_r_nid_postfix(&epoch, nullptr) == nullptr, "gmtime_r accepted a null result");
    Require(localtime_r_nid_postfix(&epoch, nullptr) == nullptr, "localtime_r accepted a null result");
    const std::int64_t underflow = std::numeric_limits<std::int64_t>::min();
    GuestTm scratch{};
    Require(gmtime_r_nid_postfix(&underflow, &scratch) == nullptr, "gmtime_r accepted an unrepresentable time");
}

static void CheckTimegmCases() {
    constexpr TimegmCase cases[] = {
        {0, 0, 0, 1, 0, 70, 0, 1, 0, 70, 4, 0},
        {59, 59, 23, 31, 11, 69, -1, 31, 11, 69, 3, 364},
        {7, 14, 3, 19, 9, 126, 1792379647, 19, 9, 126, 1, 291},
        {0, 0, 0, 0, 0, 70, -86400, 31, 11, 69, 3, 364},
        {0, 0, 0, 1, 12, 69, 0, 1, 0, 70, 4, 0},
        {0, 0, 0, 1, -1, 71, 28857600, 1, 11, 70, 2, 334},
        {0, 0, 0, 1, -13, 71, -2678400, 1, 11, 69, 1, 334},
        {0, 0, 0, 32, 1, 100, 952041600, 3, 2, 100, 5, 62},
        {31536000, 0, 0, 1, 0, 70, 31536000, 1, 0, 71, 5, 0},
        {0, 0, 0, 1, 2, 200, 4107542400, 1, 2, 200, 1, 59},
        {59, 59, 23, 31, 11, 8099, 253402300799, 31, 11, 8099, 5, 364},
        {0, 0, 0, 1, 0, -1899, -62135596800, 1, 0, -1899, 1, 0},
    };
    for (const auto& test : cases) CheckTimegm(test);

    GuestTm beyond{};
    beyond.tm_year = std::numeric_limits<int>::max();
    beyond.tm_mon = 12;
    beyond.tm_mday = 1;
    const GuestTm before = beyond;
    Require(timegm_nid_postfix(&beyond) == -1 && Equal(beyond, before), "timegm accepted a year past the int range");

    std::mt19937_64 random(7);
    for (int iteration = 0; iteration < 100000; ++iteration) {
        const std::int64_t timer = static_cast<std::int64_t>(random() % 600000000000ull) - 300000000000;
        GuestTm fields{};
        Require(gmtime_r_nid_postfix(&timer, &fields) == &fields, "gmtime_r rejected a representable time");
        GuestTm copy = fields;
        Require(timegm_nid_postfix(&copy) == timer && Equal(copy, fields), "timegm did not invert gmtime_r");
    }
}

static void CheckXtime() {
    Xtime time{-5, -5};
    Require(xtime_get_nid_postfix(&time, 0) == 0 && time.sec == -5 && time.nsec == -5, "xtime_get accepted base 0");
    Require(xtime_get_nid_postfix(&time, 2) == 0 && time.sec == -5 && time.nsec == -5, "xtime_get accepted base 2");
    Require(xtime_get_nid_postfix(nullptr, 1) == 0, "xtime_get accepted a null xtime");
    const std::int64_t before = static_cast<std::int64_t>(std::time(nullptr));
    Require(xtime_get_nid_postfix(&time, 1) == 1, "xtime_get rejected TIME_UTC");
    Require(time.sec >= before - 1 && time.sec <= before + 2 && time.nsec >= 0 && time.nsec < 1000000000 && time.nsec % 1000 == 0,
        "xtime_get did not return the wall clock in microseconds");

    const Xtime unnormalized{7, -3};
    const Xtime copied = _Xtime_to_ts_nid_postfix(&unnormalized);
    Require(copied.sec == 7 && copied.nsec == -3, "_Xtime_to_ts changed the time");

    Xtime now{};
    Require(xtime_get_nid_postfix(&now, 1) == 1, "xtime_get rejected TIME_UTC");
    const Xtime past{now.sec - 1, now.nsec};
    const Xtime remaining = _Xtime_diff_to_ts_nid_postfix(&past);
    Require(remaining.sec == 0 && remaining.nsec == 0, "_Xtime_diff_to_ts did not clamp a past time to zero");
    const Xtime epoch{0, 0};
    const Xtime none = _Xtime_diff_to_ts_nid_postfix(&epoch);
    Require(none.sec == 0 && none.nsec == 0, "_Xtime_diff_to_ts did not clamp the epoch to zero");
    const Xtime later{now.sec + 100, now.nsec};
    const Xtime wait = _Xtime_diff_to_ts_nid_postfix(&later);
    Require(wait.sec >= 98 && wait.sec <= 100 && wait.nsec >= 0 && wait.nsec < 1000000000, "_Xtime_diff_to_ts returned the wrong wait");
    const Xtime carried{now.sec, now.nsec + 5000000000};
    const Xtime carriedWait = _Xtime_diff_to_ts_nid_postfix(&carried);
    Require(carriedWait.sec >= 3 && carriedWait.sec <= 5 && carriedWait.nsec >= 0 && carriedWait.nsec < 1000000000,
        "_Xtime_diff_to_ts did not carry whole seconds out of nsec");
    const Xtime borrowed{now.sec + 10, -1};
    const Xtime borrowedWait = _Xtime_diff_to_ts_nid_postfix(&borrowed);
    Require(borrowedWait.sec >= 8 && borrowedWait.sec <= 9 && borrowedWait.nsec >= 0 && borrowedWait.nsec < 1000000000,
        "_Xtime_diff_to_ts did not borrow a second for a negative nsec");
}

int main() {
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();
    CheckReentrantConversions();
    CheckTimegmCases();
    CheckXtime();
    return 0;
}
