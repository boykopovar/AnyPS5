#pragma once
#include <cstdint>

struct UEnumeration;

namespace Icu::CalendarBackend {

void* Open(const char16_t* zoneId, std::int32_t length, const char* locale, std::int32_t calendarType, std::int32_t* status);
void Close(void* calendar);
void* Clone(const void* calendar, std::int32_t* status);
std::int32_t Get(const void* calendar, std::int32_t field, std::int32_t* status);
std::int32_t GetAttribute(const void* calendar, std::int32_t attr);
std::int32_t GetDayOfWeekType(const void* calendar, std::int32_t dayOfWeek, std::int32_t* status);
std::int32_t GetDefaultTimeZone(char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
std::int32_t GetCanonicalTimeZoneID(const char16_t* id, std::int32_t len, char16_t* result, std::int32_t resultCapacity,
                                    std::int8_t* isSystemID, std::int32_t* status);
std::int32_t GetTimeZoneDisplayName(const void* calendar, std::int32_t displayNameType, const char* locale,
                                    char16_t* result, std::int32_t resultLength, std::int32_t* status);
void SetGregorianChange(void* calendar, double date, std::int32_t* status);
void SetMillis(void* calendar, double dateTime, std::int32_t* status);
UEnumeration* OpenTimeZones(std::int32_t* status);
UEnumeration* OpenTimeZoneIDEnumeration(std::int32_t zoneType, const char* region, const std::int32_t* rawOffset,
                                        std::int32_t* status);
UEnumeration* GetKeywordValuesForLocale(const char* key, const char* locale, std::int8_t commonlyUsed,
                                        std::int32_t* status);

}
