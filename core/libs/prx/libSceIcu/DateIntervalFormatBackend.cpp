#include "prx/libSceIcu/DateIntervalFormatBackend.hpp"
#include "unicode/udateintervalformat.h"
#include <cstdint>

namespace Icu::DateIntervalFormatBackend {

void* Open(const char* locale, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID,
           std::int32_t tzIDLength, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    UDateIntervalFormat* formatter = udtitvfmt_open(locale, skeleton, skeletonLength, tzID, tzIDLength, &local);
    *status = static_cast<std::int32_t>(local);
    return formatter;
}

void Close(void* formatter) {
    udtitvfmt_close(static_cast<UDateIntervalFormat*>(formatter));
}

void* OpenResult(std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    UFormattedDateInterval* result = udtitvfmt_openResult(&local);
    *status = static_cast<std::int32_t>(local);
    return result;
}

void CloseResult(void* result) {
    udtitvfmt_closeResult(static_cast<UFormattedDateInterval*>(result));
}

void FormatToResult(const void* formatter, double fromDate, double toDate, void* result, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    udtitvfmt_formatToResult(static_cast<const UDateIntervalFormat*>(formatter), fromDate, toDate,
                             static_cast<UFormattedDateInterval*>(result), &local);
    *status = static_cast<std::int32_t>(local);
}

const UFormattedValue* ResultAsValue(const void* result, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const UFormattedValue* value =
        udtitvfmt_resultAsValue(static_cast<const UFormattedDateInterval*>(result), &local);
    *status = static_cast<std::int32_t>(local);
    return value;
}

}
