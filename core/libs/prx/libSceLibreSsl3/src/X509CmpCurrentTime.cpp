#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <ctime>
#include <stdexcept>

namespace {

constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;

std::int64_t DaysFromCivil(int year, int month, int day) {
    year -= month <= 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const int yearOfEra = year - static_cast<int>(era) * 400;
    const int dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

bool Parse(const Asn1String& time, std::int64_t& seconds) {
    if (!time.data || (time.length != 13 && time.length != 15)) return false;
    const auto* data = time.data;
    if (data[time.length - 1] != 'Z') return false;
    for (int index = 0; index < time.length - 1; ++index)
        if (data[index] < '0' || data[index] > '9') return false;

    int offset;
    int type;
    int year;
    if (time.length == 15) {
        if (time.type == UtcTimeType) return false;
        year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + data[3] - '0';
        offset = 4;
        type = GeneralizedTimeType;
    } else {
        if (time.type == GeneralizedTimeType) return false;
        const int twoDigitYear = (data[0] - '0') * 10 + data[1] - '0';
        year = twoDigitYear + (twoDigitYear < 50 ? 2000 : 1900);
        offset = 2;
        type = UtcTimeType;
    }
    const int month = (data[offset] - '0') * 10 + data[offset + 1] - '0';
    const int day = (data[offset + 2] - '0') * 10 + data[offset + 3] - '0';
    const int hour = (data[offset + 4] - '0') * 10 + data[offset + 5] - '0';
    const int minute = (data[offset + 6] - '0') * 10 + data[offset + 7] - '0';
    const int second = (data[offset + 8] - '0') * 10 + data[offset + 9] - '0';
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    if (hour > 23 || minute > 59 || second > 59) return false;
    if (year < 2050 && type != UtcTimeType) return false;
    if (year >= 2050 && type != GeneralizedTimeType) return false;
    seconds = DaysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
    return seconds != -1;
}

}

extern "C" int APS5_VABI X509_cmp_current_time_nid_postfix(const Asn1String* time) {
    if (!time) throw std::invalid_argument("X509_cmp_current_time requires a time");
    std::int64_t seconds;
    if (!Parse(*time, seconds)) return 0;
    return seconds <= static_cast<std::int64_t>(std::time(nullptr)) ? -1 : 1;
}
