#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" {
std::int64_t APS5_VABI libc_mktime_nid_postfix(GuestTm*);
std::int64_t APS5_VABI mktime_nid_postfix(GuestTm*);
GuestTm* APS5_VABI localtime_s_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI gmtime_s_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI localtime_r_nid_postfix(const std::int64_t*, GuestTm*);
GuestTm* APS5_VABI gmtime_r_nid_postfix(const std::int64_t*, GuestTm*);
std::int64_t APS5_VABI timegm_nid_postfix(GuestTm*);
void APS5_VABI tzset_nid_postfix();
int APS5_VABI setenv_nid_postfix(const char*, const char*, int);
}

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
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst;
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

constexpr std::size_t ConsoleTmSize = 36;

static GuestTm* FillGuarded(unsigned char (&bytes)[64]) {
    std::memset(bytes, 0xAA, sizeof(bytes));
    const int calendar[9] = {0, 0, 12, 23, 9, 126, 0, 0, 0};
    std::memcpy(bytes, calendar, sizeof(calendar));
    return reinterpret_cast<GuestTm*>(bytes);
}

static void RequireGuard(const unsigned char (&bytes)[64], const char* message) {
    for (std::size_t i = ConsoleTmSize; i < sizeof(bytes); ++i) Require(bytes[i] == 0xAA, message);
}

static void CheckWritesStayInside() {
    const std::int64_t timer = 1792756800;
    alignas(8) unsigned char bytes[64];
    for (auto convert : {libc_mktime_nid_postfix, mktime_nid_postfix, timegm_nid_postfix}) {
        Require(convert(FillGuarded(bytes)) != -1, "Conversion of a valid calendar failed");
        RequireGuard(bytes, "A calendar conversion wrote past the 36-byte struct tm");
    }
    for (auto split : {localtime_s_nid_postfix, gmtime_s_nid_postfix, localtime_r_nid_postfix, gmtime_r_nid_postfix}) {
        GuestTm* time = FillGuarded(bytes);
        Require(split(&timer, time) == time, "Splitting a valid timestamp failed");
        RequireGuard(bytes, "A timestamp split wrote past the 36-byte struct tm");
    }
    GuestTm* utc = FillGuarded(bytes);
    Require(gmtime_s_nid_postfix(&timer, utc) == utc && utc->tm_isdst == -1, "gmtime_s did not store tm_isdst -1");
}

int main() {
    CheckWritesStayInside();
    CheckZone("UTC0");
    CheckZone("UTC-2");
    CheckZone("UTC5");
}
