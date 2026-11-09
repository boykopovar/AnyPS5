#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <limits>

namespace {

constexpr std::int32_t UtcTime = 23;
constexpr std::int32_t GeneralizedTime = 24;
constexpr std::int64_t Borrowed = 0x10;
constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int IllegalTimeValue = 184;
constexpr std::int64_t First1950 = -631152000LL;
constexpr std::int64_t First10000 = 253402300800LL;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

struct BrokenTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

BrokenTime BreakDown(std::int64_t seconds) {
    std::int64_t days = seconds / 86400;
    std::int64_t daySeconds = seconds % 86400;
    if (daySeconds < 0) {
        daySeconds += 86400;
        --days;
    }
    const std::chrono::year_month_day date{std::chrono::sys_days{std::chrono::days{days}}};
    return {static_cast<int>(date.year()), static_cast<int>(static_cast<unsigned>(date.month())),
            static_cast<int>(static_cast<unsigned>(date.day())), static_cast<int>(daySeconds / 3600),
            static_cast<int>(daySeconds / 60 % 60), static_cast<int>(daySeconds % 60)};
}

}

extern "C" Asn1String* APS5_VABI X509_gmtime_adj_nid_postfix(Asn1String* time, std::int64_t adjustment) {
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    if ((adjustment > 0 && now > std::numeric_limits<std::int64_t>::max() - adjustment) ||
        (adjustment < 0 && now < std::numeric_limits<std::int64_t>::min() - adjustment)) {
        PutError(IllegalTimeValue);
        return nullptr;
    }
    const auto seconds = now + adjustment;
    if (seconds < First1950 || seconds >= First10000) {
        PutError(IllegalTimeValue);
        return nullptr;
    }
    const auto broken = BreakDown(seconds);
    const bool utc = broken.year < 2050;
    const int length = (utc ? 2 : 4) + 11;
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length) + 1));
    if (!data) {
        PutError(IllegalTimeValue);
        return nullptr;
    }
    if (utc)
        std::snprintf(reinterpret_cast<char*>(data), static_cast<std::size_t>(length) + 1, "%02d%02d%02d%02d%02d%02dZ",
            broken.year % 100, broken.month, broken.day, broken.hour, broken.minute, broken.second);
    else
        std::snprintf(reinterpret_cast<char*>(data), static_cast<std::size_t>(length) + 1, "%04d%02d%02d%02d%02d%02dZ",
            broken.year, broken.month, broken.day, broken.hour, broken.minute, broken.second);
    auto* target = time ? time : ASN1_TIME_new_nid_postfix();
    if (!target) {
        free_nid_postfix(data);
        return nullptr;
    }
    if (time && time->data && !(time->flags & Borrowed)) free_nid_postfix(time->data);
    target->data = data;
    target->length = length;
    target->type = utc ? UtcTime : GeneralizedTime;
    target->flags &= ~Borrowed;
    return target;
}
