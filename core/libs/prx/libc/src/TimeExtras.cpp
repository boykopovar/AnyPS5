#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestTime.hpp"

namespace {

constexpr int timeUtc = 1;
constexpr std::int64_t nanosPerSecond = 1000000000;
constexpr std::int64_t secondsPerDay = 86400;

std::int64_t floorDivide(std::int64_t value, std::int64_t divisor) {
    return value >= 0 ? value / divisor : -((-(value + 1)) / divisor + 1);
}

std::int64_t daysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = floorDivide(year, 400);
    const std::int64_t yearOfEra = year - era * 400;
    const std::int64_t dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

std::int64_t wrappingAdd(std::int64_t left, std::int64_t right) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
}

}  // namespace

extern "C" {

GuestTm* APS5_VABI gmtime_r_nid_postfix(const std::int64_t* timer, GuestTm* result) {
    return gmtime_s_nid_postfix(timer, result);
}

GuestTm* APS5_VABI localtime_r_nid_postfix(const std::int64_t* timer, GuestTm* result) {
    return localtime_s_nid_postfix(timer, result);
}

std::int64_t APS5_VABI timegm_nid_postfix(GuestTm* time) {
    const std::int64_t years = floorDivide(time->tm_mon, 12);
    const std::int64_t month = time->tm_mon - years * 12 + 1;
    const std::int64_t year = std::int64_t{time->tm_year} + 1900 + years;
    const std::int64_t days = daysFromCivil(year, month, 1) + time->tm_mday - 1;
    const std::int64_t seconds = days * secondsPerDay + std::int64_t{time->tm_hour} * 3600 + std::int64_t{time->tm_min} * 60 + time->tm_sec;
    GuestTm normalized{};
    if (gmtime_s_nid_postfix(&seconds, &normalized) == nullptr) return -1;
    *time = normalized;
    return seconds;
}

int APS5_VABI xtime_get_nid_postfix(Xtime* time, int base) {
    if (time == nullptr || base != timeUtc) return 0;
    const std::int64_t micros = _Xtime_get_ticks_nid_postfix();
    time->sec = floorDivide(micros, 1000000);
    time->nsec = (micros - time->sec * 1000000) * 1000;
    return timeUtc;
}

Xtime APS5_VABI _Xtime_to_ts_nid_postfix(const Xtime* time) {
    return *time;
}

Xtime APS5_VABI _Xtime_diff_to_ts_nid_postfix(const Xtime* target) {
    Xtime now{};
    xtime_get_nid_postfix(&now, timeUtc);

    std::int64_t seconds = wrappingAdd(target->sec, target->nsec / nanosPerSecond);
    std::int64_t nanos = target->nsec % nanosPerSecond;
    if (nanos < 0) {
        nanos += nanosPerSecond;
        seconds = wrappingAdd(seconds, -1);
    }
    seconds = wrappingAdd(seconds, -now.sec);
    if (nanos < now.nsec) {
        nanos += nanosPerSecond;
        seconds = wrappingAdd(seconds, -1);
    }
    nanos -= now.nsec;
    if (seconds < 0 || (seconds == 0 && nanos == 0)) return {0, 0};
    return {seconds, nanos};
}

}
