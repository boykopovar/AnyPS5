# libSceIcu

## change [ICU date format exports]

- [ICU date format exports](../../../core/libs/prx/libSceIcu/DateFormat.cpp) use [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/udat.cpp) and its locale, calendar, numbering and time-zone data. Console output and differences from firmware using ICU 68 remain unverified. A null locale uses the guest default locale. A null zone uses the ICU backend's default time zone. The calendar returned by `udat_getCalendar` belongs to the formatter and expires on close. Foreign handles, unknown date styles and negative array indices with undefined upstream behaviour throw. Closes when console measurements cover locale defaults, time zones and firmware-specific formatting.
