#include "prx/libSceIcu/NumberFormatterBackend.hpp"
#include "unicode/unumberformatter.h"
#include <cstdint>

namespace Icu::NumberFormatterBackend {

void* OpenForSkeletonAndLocale(const char16_t* skeleton, std::int32_t skeletonLen, const char* locale,
                               std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    UNumberFormatter* formatter = unumf_openForSkeletonAndLocale(skeleton, skeletonLen, locale, &local);
    *status = static_cast<std::int32_t>(local);
    return formatter;
}

void Close(void* formatter) {
    unumf_close(static_cast<UNumberFormatter*>(formatter));
}

void* OpenResult(std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    UFormattedNumber* result = unumf_openResult(&local);
    *status = static_cast<std::int32_t>(local);
    return result;
}

void CloseResult(void* result) {
    unumf_closeResult(static_cast<UFormattedNumber*>(result));
}

void FormatDecimal(const void* formatter, const char* value, std::int32_t valueLen, void* result,
                   std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    unumf_formatDecimal(static_cast<const UNumberFormatter*>(formatter), value, valueLen,
                        static_cast<UFormattedNumber*>(result), &local);
    *status = static_cast<std::int32_t>(local);
}

void FormatDouble(const void* formatter, double value, void* result, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    unumf_formatDouble(static_cast<const UNumberFormatter*>(formatter), value,
                       static_cast<UFormattedNumber*>(result), &local);
    *status = static_cast<std::int32_t>(local);
}

std::int32_t ResultToString(const void* result, char16_t* buffer, std::int32_t bufferCapacity, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length =
        unumf_resultToString(static_cast<const UFormattedNumber*>(result), buffer, bufferCapacity, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

void ResultGetAllFieldPositions(const void* result, UFieldPositionIterator* fpositer, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    unumf_resultGetAllFieldPositions(static_cast<const UFormattedNumber*>(result), fpositer, &local);
    *status = static_cast<std::int32_t>(local);
}

}
