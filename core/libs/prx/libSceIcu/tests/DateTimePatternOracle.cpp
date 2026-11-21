#include "prx/libSceIcu/tests/DateTimePatternOracle.hpp"
#include "unicode/udatpg.h"

void* PatternOracleOpen(const char* locale, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = udatpg_open(locale, &error);
    *status = error;
    return result;
}

void PatternOracleClose(void* handle) {
    udatpg_close(static_cast<UDateTimePatternGenerator*>(handle));
}

std::int32_t PatternOracleConvert(void* handle, std::int32_t operation, const char16_t* input, std::int32_t length,
                                 std::int32_t options, std::int32_t field, std::int32_t width,
                                 char16_t* output, std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* generator = static_cast<UDateTimePatternGenerator*>(handle);
    std::int32_t result;
    if (operation == 0) result = udatpg_getBestPattern(generator, input, length, output, capacity, &error);
    else if (operation == 1) result = udatpg_getBestPatternWithOptions(generator, input, length,
        static_cast<UDateTimePatternMatchOptions>(options), output, capacity, &error);
    else if (operation == 2) result = udatpg_getSkeleton(generator, input, length, output, capacity, &error);
    else result = udatpg_getFieldDisplayName(generator, static_cast<UDateTimePatternField>(field),
        static_cast<UDateTimePGDisplayWidth>(width), output, capacity, &error);
    *status = error;
    return result;
}
