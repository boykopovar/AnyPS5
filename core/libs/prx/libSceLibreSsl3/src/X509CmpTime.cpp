#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <optional>

namespace {

constexpr int UtcTime = 23;
constexpr int GeneralizedTime = 24;
constexpr int UtcTimeLength = 13;
constexpr int GeneralizedTimeLength = 15;

bool TwoDigits(const unsigned char* data, int& value) {
    if (data[0] < '0' || data[0] > '9' || data[1] < '0' || data[1] > '9') return false;
    value = (data[0] - '0') * 10 + data[1] - '0';
    return true;
}

std::optional<std::int64_t> CertificateTime(const Asn1String* time) {
    if (!time || !time->data) return std::nullopt;
    int format;
    if (time->length == UtcTimeLength) {
        format = UtcTime;
    } else if (time->length == GeneralizedTimeLength) {
        format = GeneralizedTime;
    } else {
        return std::nullopt;
    }
    if (time->type != 0 && time->type != format) return std::nullopt;
    const unsigned char* data = time->data;
    int year;
    int value;
    if (!TwoDigits(data, year)) return std::nullopt;
    data += 2;
    if (format == GeneralizedTime) {
        if (!TwoDigits(data, value)) return std::nullopt;
        year = year * 100 + value;
        data += 2;
        if (year < 2050) return std::nullopt;
    } else {
        year += year < 50 ? 2000 : 1900;
    }
    int fields[5];
    for (int& field : fields) {
        if (!TwoDigits(data, field)) return std::nullopt;
        data += 2;
    }
    if (*data != 'Z') return std::nullopt;
    const std::chrono::year_month_day date{std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(fields[0])}, std::chrono::day{static_cast<unsigned>(fields[1])}};
    if (!date.ok() || fields[2] > 23 || fields[3] > 59 || fields[4] > 59)
        return std::nullopt;
    return static_cast<std::int64_t>(std::chrono::sys_days{date}.time_since_epoch().count()) * 86400
        + fields[2] * 3600 + fields[3] * 60 + fields[4];
}

}

extern "C" int APS5_VABI X509_cmp_current_time_nid_postfix(const Asn1String* time) {
    const std::int64_t now = std::time(nullptr);
    const auto certificate = CertificateTime(time);
    if (!certificate) return 0;
    return *certificate <= now ? -1 : 1;
}
