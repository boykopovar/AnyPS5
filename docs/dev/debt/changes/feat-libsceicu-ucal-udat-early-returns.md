# libSceIcu

## add Functional

- [ICU calendar attribute, weekday type and time zone enumeration exports](../../../core/libs/prx/libSceIcu/Calendar.cpp) implement only the early returns of ICU 68.2 [ucal.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/ucal.cpp): `ucal_getAttribute` (339-364) returns -1 for an attribute outside 0..4 without reading the calendar; `ucal_getDayOfWeekType` (625-632) returns `UCAL_WEEKDAY`, and `ucal_openTimeZoneIDEnumeration` (52-57) and `ucal_openTimeZones` (59-62) return null, for a failing `UErrorCode` (above zero) with the status untouched. Every other call, a null `UErrorCode` and every path that needs `UCalendar` objects and time zone data throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.

## add Functional

- [ICU date format symbol count and calendar exports](../../../core/libs/prx/libSceIcu/DateFormat.cpp) follow ICU 68.2 [udat.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/udat.cpp): `udat_countSymbols` (748-880) returns 0 for a null format, which neither of its `dynamic_cast`s matches; `udat_getCalendar` (447-451) has no early return, so it has its prototype and throws for every input. Every path that needs `UDateFormat` objects and date format data throws `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.
