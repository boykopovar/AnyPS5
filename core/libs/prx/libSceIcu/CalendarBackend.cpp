#include "prx/libSceIcu/CalendarBackend.hpp"
#include "unicode/ucal.h"
#include <type_traits>

static_assert(sizeof(UErrorCode) == 4 && sizeof(UCalendarType) == 4 && sizeof(UCalendarDateFields) == 4);
static_assert(std::is_same_v<UChar, char16_t> && std::is_same_v<UDate, double>);
static_assert(UCAL_FIELD_COUNT == 24 && UCAL_GREGORIAN == 1);

namespace IcuCalendarBackend {

void* Open_nid_no_patch(const char16_t* zone, std::int32_t length, const char* locale, std::int32_t type, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = ucal_open(zone, length, locale, static_cast<UCalendarType>(type), &error);
    *status = error;
    return result;
}
void* Clone_nid_no_patch(const void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = ucal_clone(static_cast<const UCalendar*>(handle), &error);
    *status = error;
    return result;
}
void Close_nid_no_patch(void* handle) { ucal_close(static_cast<UCalendar*>(handle)); }
std::int32_t Get_nid_no_patch(const void* handle, std::int32_t field, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = ucal_get(static_cast<const UCalendar*>(handle), static_cast<UCalendarDateFields>(field), &error);
    *status = error;
    return result;
}
void Set_nid_no_patch(void* handle, double milliseconds, bool cutover, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    if (cutover) ucal_setGregorianChange(static_cast<UCalendar*>(handle), milliseconds, &error);
    else ucal_setMillis(static_cast<UCalendar*>(handle), milliseconds, &error);
    *status = error;
}

} // namespace IcuCalendarBackend
