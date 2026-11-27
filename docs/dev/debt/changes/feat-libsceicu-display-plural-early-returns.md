# libSceIcu

## add Functional

- [ICU locale display name queries](../../../core/libs/prx/libSceIcu/LocaleDisplayNames.cpp) implement only the early returns of ICU 68.2 [locdspnm.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/locdspnm.cpp): `uldn_localeDisplayName` (969-989), `uldn_scriptDisplayName` (1009-1025), `uldn_regionDisplayName` (1036-1052) and `uldn_keyValueDisplayName` (1090-1108) return 0 with the status untouched for a failing `UErrorCode` (above zero), and 0 with `U_ILLEGAL_ARGUMENT_ERROR` for a null handle, a null code, key or value, a null result with a positive size or a negative size. Valid calls, a null `UErrorCode` and every path that needs `ULocaleDisplayNames` objects and locale data throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.

## add Functional

- [ICU plural rules keyword and selection exports](../../../core/libs/prx/libSceIcu/PluralRules.cpp) implement only the early returns of ICU 68.2 [upluralrules.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/upluralrules.cpp): for a failing `UErrorCode` (above zero) `uplrules_getKeywords` (159-180) returns null and `uplrules_selectWithFormat` (138-157) returns 0, status untouched; a null `UPluralRules`, and for the second one a null `UNumberFormat` or a keyword buffer and capacity that disagree, set `U_ILLEGAL_ARGUMENT_ERROR`. Valid calls, a null `UErrorCode` and every path that needs the objects throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.

## add Functional

- [ureldatefmt_formatNumeric](../../../core/libs/prx/libSceIcu/RelativeDateTimeFormatter.cpp) implements only the early returns of ICU 68.2 [reldatefmt.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/reldatefmt.cpp) (1301-1327), the same as `ureldatefmt_format`: 0 with the status untouched for a failing `UErrorCode` (above zero), and 0 with `U_ILLEGAL_ARGUMENT_ERROR` for a result buffer and capacity that disagree, checked before the formatter is read. Valid calls, a null `UErrorCode` and every path that needs a `URelativeDateTimeFormatter` object throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.

## add Functional

- [udtitvfmt_resultAsValue](../../../core/libs/prx/libSceIcu/DateIntervalFormat.cpp) implements only the early returns of ICU 68.2 ([formattedval_impl.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/formattedval_impl.h) 266-270, `IcuCApiHelper::validate` in [capi_helper.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/capi_helper.h) 52-66): null with the status untouched for a failing `UErrorCode` (above zero), and null with `U_ILLEGAL_ARGUMENT_ERROR` for a null result. A non-null result, whose magic number is read next, and a null `UErrorCode` throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.

## add Functional

- [ulistfmt_resultAsValue](../../../core/libs/prx/libSceIcu/ListFormatter.cpp) implements only the early returns of ICU 68.2 ([formattedval_impl.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/formattedval_impl.h) 266-270, `IcuCApiHelper::validate` in [capi_helper.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/capi_helper.h) 52-66): null with the status untouched for a failing `UErrorCode` (above zero), and null with `U_ILLEGAL_ARGUMENT_ERROR` for a null result. A non-null result, whose magic number is read next, and a null `UErrorCode` throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.
