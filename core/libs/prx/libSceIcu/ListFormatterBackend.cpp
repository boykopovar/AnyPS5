#include "prx/libSceIcu/ListFormatterBackend.hpp"
#include "unicode/ulistformatter.h"
#include <cstdint>

namespace Icu::ListFormatterBackend {

void* OpenForType(const char* locale, std::int32_t type, std::int32_t width, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    void* formatter = ulistfmt_openForType(locale, static_cast<UListFormatterType>(type),
                                           static_cast<UListFormatterWidth>(width), &local);
    *status = static_cast<std::int32_t>(local);
    return formatter;
}

void Close(void* formatter) {
    ulistfmt_close(static_cast<UListFormatter*>(formatter));
}

void* OpenResult(std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    void* result = ulistfmt_openResult(&local);
    *status = static_cast<std::int32_t>(local);
    return result;
}

void CloseResult(void* result) {
    ulistfmt_closeResult(static_cast<UFormattedList*>(result));
}

const void* ResultAsValue(const void* result, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const void* value = ulistfmt_resultAsValue(static_cast<const UFormattedList*>(result), &local);
    *status = static_cast<std::int32_t>(local);
    return value;
}

std::int32_t Format(const void* formatter, const char16_t* const* strings, const std::int32_t* stringLengths,
                    std::int32_t stringCount, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = ulistfmt_format(
        static_cast<const UListFormatter*>(formatter), strings, stringLengths, stringCount, result, resultCapacity, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

void FormatStringsToResult(const void* formatter, const char16_t* const* strings, const std::int32_t* stringLengths,
                           std::int32_t stringCount, void* result, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    ulistfmt_formatStringsToResult(
        static_cast<const UListFormatter*>(formatter), strings, stringLengths, stringCount,
        static_cast<UFormattedList*>(result), &local);
    *status = static_cast<std::int32_t>(local);
}

}
