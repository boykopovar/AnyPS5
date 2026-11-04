#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int UndefinedType = -1;
constexpr std::int64_t MultiStringFlag = 0x040;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;
constexpr int UtcTimeLength = 13;
constexpr int GeneralizedTimeLength = 15;
constexpr std::int64_t MinimumPosixTime = -62167219200;
constexpr std::int64_t MaximumPosixTime = 253402300799;

}

extern "C" {

Asn1String* APS5_VABI ASN1_TIME_new_nid_postfix() {
    auto* time = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!time) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        return nullptr;
    }
    time->type = UndefinedType;
    time->flags = MultiStringFlag;
    return time;
}

Asn1String* APS5_VABI X509_gmtime_adj_nid_postfix(Asn1String* time, std::int64_t adjustment) {
    std::int64_t seconds = std::time(nullptr);
    if (seconds < MinimumPosixTime || seconds > MaximumPosixTime) return nullptr;
    if (adjustment > MaximumPosixTime - seconds || adjustment < MinimumPosixTime - seconds) return nullptr;
    seconds += adjustment;
    const auto days = std::chrono::floor<std::chrono::days>(std::chrono::sys_seconds{std::chrono::seconds{seconds}});
    const std::chrono::year_month_day date{days};
    const auto clock = seconds - static_cast<std::int64_t>(days.time_since_epoch().count()) * 86400;
    const int year = static_cast<int>(date.year());
    const bool utcTime = year >= 1950 && year < 2050;
    const unsigned fields[6] {static_cast<unsigned>(utcTime ? year % 100 : year), static_cast<unsigned>(date.month()),
        static_cast<unsigned>(date.day()), static_cast<unsigned>(clock / 3600), static_cast<unsigned>(clock / 60 % 60),
        static_cast<unsigned>(clock % 60)};
    char text[GeneralizedTimeLength + 1];
    std::snprintf(text, sizeof(text), utcTime ? "%02u%02u%02u%02u%02u%02uZ" : "%04u%02u%02u%02u%02u%02uZ",
        fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]);
    const int length = utcTime ? UtcTimeLength : GeneralizedTimeLength;
    auto* target = time ? time : ASN1_TIME_new_nid_postfix();
    if (!target) return nullptr;
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(length + 1));
    if (!data) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        if (target != time) free_nid_postfix(target);
        return nullptr;
    }
    std::memcpy(data, text, length + 1);
    free_nid_postfix(target->data);
    target->data = data;
    target->length = length;
    target->type = utcTime ? UtcTimeType : GeneralizedTimeType;
    return target;
}

}
