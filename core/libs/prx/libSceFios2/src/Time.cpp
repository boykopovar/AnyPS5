#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstring>

namespace {

constexpr std::int64_t NanosecondsPerSecond = 1000000000;
constexpr std::int64_t SecondsPerDay = 86400;

std::int64_t DaysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yearOfEra = year - era * 400;
    const std::int64_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

void CivilFromDays(std::int64_t days, std::int64_t& year, std::int64_t& month, std::int64_t& day) {
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t dayOfEra = days - era * 146097;
    const std::int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const std::int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const std::int64_t shifted = (5 * dayOfYear + 2) / 153;
    day = dayOfYear - (153 * shifted + 2) / 5 + 1;
    month = shifted < 10 ? shifted + 3 : shifted - 9;
    year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
}

}

extern "C" {

SceFiosDate APS5_VABI sceFiosDateGetCurrent() {
    const auto since = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<SceFiosDate>(std::chrono::duration_cast<std::chrono::nanoseconds>(since).count());
}

SceFiosDate APS5_VABI sceFiosDateFromComponents(const SceFiosTm* components) {
    if (components == nullptr) return 0;
    SceFiosTm tm{};
    if (!Fios::CopyIn(&tm, components, sizeof(tm))) return 0;
    std::int64_t year = static_cast<std::int64_t>(tm.tm_year) + 1900;
    std::int64_t month = tm.tm_mon;
    year += month / 12;
    month %= 12;
    if (month < 0) {
        month += 12;
        --year;
    }
    const std::int64_t days = DaysFromCivil(year, month + 1, 1) + tm.tm_mday - 1;
    const std::int64_t seconds = days * SecondsPerDay + static_cast<std::int64_t>(tm.tm_hour) * 3600 + static_cast<std::int64_t>(tm.tm_min) * 60 + tm.tm_sec;
    return seconds < 0 ? 0 : static_cast<SceFiosDate>(seconds) * NanosecondsPerSecond;
}

SceFiosTm* APS5_VABI sceFiosDateToComponents(SceFiosDate date, SceFiosTm* outComponents) {
    if (outComponents == nullptr) return nullptr;
    const auto seconds = static_cast<std::int64_t>(date / NanosecondsPerSecond);
    const std::int64_t days = seconds / SecondsPerDay;
    const std::int64_t rest = seconds % SecondsPerDay;
    std::int64_t year = 0, month = 0, day = 0;
    CivilFromDays(days, year, month, day);
    SceFiosTm tm{};
    tm.tm_sec = static_cast<std::int32_t>(rest % 60);
    tm.tm_min = static_cast<std::int32_t>(rest / 60 % 60);
    tm.tm_hour = static_cast<std::int32_t>(rest / 3600);
    tm.tm_mday = static_cast<std::int32_t>(day);
    tm.tm_mon = static_cast<std::int32_t>(month - 1);
    tm.tm_year = static_cast<std::int32_t>(year - 1900);
    tm.tm_wday = static_cast<std::int32_t>((days + 4) % 7);
    tm.tm_yday = static_cast<std::int32_t>(days - DaysFromCivil(year, 1, 1));
    tm.tm_isdst = 0;
    return Fios::CopyOut(outComponents, &tm, sizeof(tm)) ? outComponents : nullptr;
}

SceFiosTimeInterval APS5_VABI sceFiosTimeIntervalFromNanoseconds(std::int64_t nanoseconds) {
    return nanoseconds;
}

std::int64_t APS5_VABI sceFiosTimeIntervalToNanoseconds(SceFiosTimeInterval interval) {
    return interval;
}

}
