#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using Converter = std::int64_t (APS5_VABI *)(GuestTm*);

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static bool Equal(const GuestTm& left, const GuestTm& right) {
    return left.tm_sec == right.tm_sec && left.tm_min == right.tm_min && left.tm_hour == right.tm_hour
        && left.tm_mday == right.tm_mday && left.tm_mon == right.tm_mon && left.tm_year == right.tm_year
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst
        && left.tm_gmtoff == right.tm_gmtoff && left.tm_zone != nullptr && right.tm_zone != nullptr
        && std::strcmp(left.tm_zone, right.tm_zone) == 0;
}

static void CheckNormalization(Converter convert, std::int64_t timer) {
    GuestTm expected{};
    Require(localtime_s_nid_postfix(&timer, &expected) == &expected, "Local conversion failed");
    GuestTm input = expected;
    input.tm_sec += 60;
    --input.tm_min;
    input.tm_wday = -1;
    input.tm_yday = -1;
    input.tm_isdst = -1;
    input.tm_gmtoff = -1;
    input.tm_zone = nullptr;
    errno = EOVERFLOW;
    Require(convert(&input) == timer, "mktime returned the wrong timestamp");
    Require(Equal(input, expected), "mktime did not normalize a valid timestamp");
}

static void CheckFailure(Converter convert) {
    GuestTm input{};
    input.tm_year = std::numeric_limits<int>::max();
    input.tm_mon = std::numeric_limits<int>::max();
    input.tm_mday = 1;
    input.tm_wday = 4;
    input.tm_yday = 12;
    input.tm_isdst = -1;
    input.tm_gmtoff = 123;
    input.tm_zone = "unchanged";
    const GuestTm original = input;
    Require(convert(&input) == -1, "mktime accepted an unrepresentable calendar");
    Require(Equal(input, original), "Failed mktime changed the guest calendar");
}

static void CheckZone(const char* zone) {
    Require(setenv_nid_postfix("TZ", zone, 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();

    constexpr std::array<Converter, 2> converters{libc_mktime_nid_postfix, mktime_nid_postfix};
    constexpr std::array<std::int64_t, 8> timers{-86401, -2, -1, 0, 1, 59, 60, 86400};
    for (auto convert : converters) {
        for (const auto timer : timers) {
            CheckNormalization(convert, timer);
        }
        CheckFailure(convert);
    }
}

int main() {
    CheckZone("UTC0");
    CheckZone("UTC-2");
    CheckZone("UTC5");
}
