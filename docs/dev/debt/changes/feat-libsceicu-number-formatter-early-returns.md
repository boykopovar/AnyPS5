# libSceIcu

## add Functional

- [ICU number formatter format and result exports](../../../core/libs/prx/libSceIcu/NumberFormatter.cpp) implement only the early returns of ICU 68.2 [number_capi.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/number_capi.cpp): `unumf_formatDouble` (124-134), `unumf_formatDecimal` (136-147), `unumf_resultToString` (149-161) and `unumf_resultGetAllFieldPositions` (184-198) return with the status untouched for a failing `UErrorCode` (above zero) and set `U_ILLEGAL_ARGUMENT_ERROR` for a null first handle, as `IcuCApiHelper::validate` does before it reads the handle's magic number. `unumf_openForSkeletonAndLocale` (84-96) has no early return, not even for a failing `UErrorCode`: it has its prototype and throws for every input. A non-null first handle and a null `UErrorCode` throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.

## add Functional

- [unum_formatDoubleForFields](../../../core/libs/prx/libSceIcu/NumberFormat.cpp) implements only the early returns of ICU 68.2 [unum.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/unum.cpp) (255-281): -1 with the status untouched for a failing `UErrorCode` (above zero), and -1 with `U_ILLEGAL_ARGUMENT_ERROR` for a result buffer and length that disagree, checked before the format is read. Valid calls, a null `UErrorCode` and every path that needs a `UNumberFormat` object throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.
