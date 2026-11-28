#include "prx/libSceIcu/CalendarBackend.hpp"
#include "prx/libSceIcu/HostEnumeration.hpp"
#include "unicode/ucal.h"
#include <cstdint>

namespace Icu::CalendarBackend {

void* Open(const char16_t* zoneId, std::int32_t length, const char* locale, std::int32_t calendarType, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    void* calendar = ucal_open(zoneId, length, locale, static_cast<UCalendarType>(calendarType), &local);
    *status = static_cast<std::int32_t>(local);
    return calendar;
}

void Close(void* calendar) {
    ucal_close(static_cast<UCalendar*>(calendar));
}

void* Clone(const void* calendar, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    void* cloned = ucal_clone(static_cast<const UCalendar*>(calendar), &local);
    *status = static_cast<std::int32_t>(local);
    return cloned;
}

std::int32_t Get(const void* calendar, std::int32_t field, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t value = ucal_get(static_cast<const UCalendar*>(calendar), static_cast<UCalendarDateFields>(field), &local);
    *status = static_cast<std::int32_t>(local);
    return value;
}

std::int32_t GetAttribute(const void* calendar, std::int32_t attr) {
    return ucal_getAttribute(static_cast<const UCalendar*>(calendar), static_cast<UCalendarAttribute>(attr));
}

std::int32_t GetDayOfWeekType(const void* calendar, std::int32_t dayOfWeek, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const auto value = ucal_getDayOfWeekType(static_cast<const UCalendar*>(calendar), static_cast<UCalendarDaysOfWeek>(dayOfWeek), &local);
    *status = static_cast<std::int32_t>(local);
    return static_cast<std::int32_t>(value);
}

std::int32_t GetDefaultTimeZone(char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = ucal_getDefaultTimeZone(result, resultCapacity, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

std::int32_t GetCanonicalTimeZoneID(const char16_t* id, std::int32_t len, char16_t* result, std::int32_t resultCapacity,
                                    std::int8_t* isSystemID, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    UBool system = 0;
    const std::int32_t length = ucal_getCanonicalTimeZoneID(id, len, result, resultCapacity, isSystemID ? &system : nullptr, &local);
    if (isSystemID) *isSystemID = static_cast<std::int8_t>(system ? 1 : 0);
    *status = static_cast<std::int32_t>(local);
    return length;
}

std::int32_t GetTimeZoneDisplayName(const void* calendar, std::int32_t displayNameType, const char* locale,
                                    char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = ucal_getTimeZoneDisplayName(
        static_cast<const UCalendar*>(calendar), static_cast<UCalendarDisplayNameType>(displayNameType), locale, result, resultLength, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

void SetGregorianChange(void* calendar, double date, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    ucal_setGregorianChange(static_cast<UCalendar*>(calendar), date, &local);
    *status = static_cast<std::int32_t>(local);
}

void SetMillis(void* calendar, double dateTime, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    ucal_setMillis(static_cast<UCalendar*>(calendar), dateTime, &local);
    *status = static_cast<std::int32_t>(local);
}

::UEnumeration* OpenTimeZones(std::int32_t* status) {
    return OpenHostEnumeration(status, [](UErrorCode* ec) { return ucal_openTimeZones(ec); }, __func__);
}

::UEnumeration* OpenTimeZoneIDEnumeration(std::int32_t zoneType, const char* region, const std::int32_t* rawOffset,
                                          std::int32_t* status) {
    return OpenHostEnumeration(
        status,
        [&](UErrorCode* ec) {
            return ucal_openTimeZoneIDEnumeration(static_cast<USystemTimeZoneType>(zoneType), region, rawOffset, ec);
        },
        __func__);
}

::UEnumeration* GetKeywordValuesForLocale(const char* key, const char* locale, std::int8_t commonlyUsed,
                                          std::int32_t* status) {
    return OpenHostEnumeration(
        status,
        [&](UErrorCode* ec) { return ucal_getKeywordValuesForLocale(key, locale, static_cast<UBool>(commonlyUsed != 0), ec); },
        __func__);
}

}
