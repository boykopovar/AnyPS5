#include "prx/libSceIcu/DateTimePatternGeneratorBackend.hpp"
#include "unicode/udatpg.h"
#include <type_traits>

static_assert(sizeof(UErrorCode) == 4 && sizeof(UDateTimePatternMatchOptions) == 4);
static_assert(sizeof(UDateTimePatternField) == 4 && sizeof(UDateTimePGDisplayWidth) == 4);
static_assert(std::is_same_v<UChar, char16_t>);
static_assert(UDATPG_MATCH_HOUR_FIELD_LENGTH == (1 << 11) && UDATPG_MATCH_MINUTE_FIELD_LENGTH == (1 << 12));
static_assert(UDATPG_MATCH_SECOND_FIELD_LENGTH == (1 << 13) && UDATPG_MATCH_ALL_FIELDS_LENGTH == 65535);
static_assert(UDATPG_FIELD_COUNT == 16 && UDATPG_WIDE == 0 && UDATPG_NARROW == 2);

namespace IcuPatternBackend {

void* Open_nid_no_patch(const char* locale, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = udatpg_open(locale, &error);
    *status = error;
    return result;
}

void Close_nid_no_patch(void* handle) {
    udatpg_close(static_cast<UDateTimePatternGenerator*>(handle));
}

std::int32_t Best_nid_no_patch(void* handle, const char16_t* input, std::int32_t length, std::int32_t options,
                             char16_t* output, std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = udatpg_getBestPatternWithOptions(static_cast<UDateTimePatternGenerator*>(handle), input, length,
        static_cast<UDateTimePatternMatchOptions>(options), output, capacity, &error);
    *status = error;
    return result;
}

std::int32_t Skeleton_nid_no_patch(const char16_t* input, std::int32_t length,
                                 char16_t* output, std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = udatpg_getSkeleton(nullptr, input, length, output, capacity, &error);
    *status = error;
    return result;
}

std::int32_t Field_nid_no_patch(const void* handle, std::int32_t field, std::int32_t width,
                              char16_t* output, std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = udatpg_getFieldDisplayName(static_cast<const UDateTimePatternGenerator*>(handle),
        static_cast<UDateTimePatternField>(field), static_cast<UDateTimePGDisplayWidth>(width), output, capacity, &error);
    *status = error;
    return result;
}

} // namespace IcuPatternBackend
