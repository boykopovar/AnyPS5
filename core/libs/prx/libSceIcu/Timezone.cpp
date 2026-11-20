// Derived from icu4c/source/common/putil.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2)
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*/

#include "prx/libSceIcu/Timezone.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

bool IsNonDigit(char character) {
    return character < '0' || '9' < character;
}

bool IsValidOlsonID(const char* id) {
    std::int32_t index = 0;
    while (id[index] != '\0' && IsNonDigit(id[index]) && id[index] != ',') ++index;
    return id[index] == '\0'
        || std::strcmp(id, "PST8PDT") == 0
        || std::strcmp(id, "MST7MDT") == 0
        || std::strcmp(id, "CST6CDT") == 0
        || std::strcmp(id, "EST5EDT") == 0;
}

void SkipZoneIDPrefix(const char** id) {
    if (std::strncmp(*id, "posix/", 6) == 0 || std::strncmp(*id, "right/", 6) == 0) *id += 6;
}

const char* PlatformTimeZoneName(std::int32_t index) {
#if defined(_WIN32)
    return _tzname[index];
#else
    return tzname[index];
#endif
}

const char* FormatGmtOffset(std::int32_t secondsWest) {
    static thread_local char text[16];
    const std::int32_t east = -secondsWest;
    const char sign = east < 0 ? '-' : '+';
    const std::int32_t magnitude = east < 0 ? -east : east;
    std::snprintf(text, sizeof(text), "GMT%c%02d:%02d", sign, magnitude / 3600, magnitude / 60 % 60);
    return text;
}

}

extern "C" {

void APS5_VABI uprv_tzset_nid_postfix() {
    tzset_nid_postfix();
}

const char* APS5_VABI uprv_tzname_nid_postfix(std::int32_t index) {
    const char* tzid = getenv_nid_postfix("TZ");
    if (tzid != nullptr && IsValidOlsonID(tzid)) {
        if (tzid[0] == ':') ++tzid;
        SkipZoneIDPrefix(&tzid);
        return tzid;
    }
    const char* name = PlatformTimeZoneName(index);
    if (name != nullptr && name[0] != '\0') return name;
    return FormatGmtOffset(uprv_timezone_nid_postfix());
}

std::int32_t APS5_VABI uprv_timezone_nid_postfix() {
#if defined(_WIN32)
    return static_cast<std::int32_t>(_timezone);
#else
    std::time_t now;
    std::time(&now);
    const std::tm* local = std::localtime(&now);
    if (local == nullptr) return 0;
    std::tm record = *local;
    const bool daylight = record.tm_isdst != 0;
    const std::time_t localSeconds = std::mktime(&record);
    const std::tm* universal = std::gmtime(&now);
    if (universal == nullptr) return 0;
    record = *universal;
    const std::time_t universalSeconds = std::mktime(&record);
    std::int32_t difference = static_cast<std::int32_t>(universalSeconds - localSeconds);
    if (daylight) difference += 3600;
    return difference;
#endif
}

}
