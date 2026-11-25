#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
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

std::optional<std::int64_t> CertificateTime(const Asn1String* time, const char* caller) {
    if (!time) return std::nullopt;
    Asn1String guestTime{};
    if (!GuestCopyIn_nid_no_patch(&guestTime, time, sizeof(guestTime))) NotImplemented_nid_no_patch(caller);
    if (!guestTime.data) return std::nullopt;
    int format;
    if (guestTime.length == UtcTimeLength) {
        format = UtcTime;
    } else if (guestTime.length == GeneralizedTimeLength) {
        format = GeneralizedTime;
    } else {
        return std::nullopt;
    }
    if (guestTime.type != 0 && guestTime.type != format) return std::nullopt;
    std::array<unsigned char, GeneralizedTimeLength> buffer{};
    if (!GuestCopyIn_nid_no_patch(buffer.data(), guestTime.data, static_cast<std::size_t>(guestTime.length))) {
        NotImplemented_nid_no_patch(caller);
    }
    const unsigned char* data = buffer.data();
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
    const auto certificate = CertificateTime(time, __func__);
    if (!certificate) return 0;
    return *certificate <= now ? -1 : 1;
}
