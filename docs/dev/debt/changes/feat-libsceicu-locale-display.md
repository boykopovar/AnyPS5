# libSceIcu

## add Functional

- [Locale display exports](../../../core/libs/prx/libSceIcu/LocaleDisplay.cpp) (libSceIcu) - eight `uloc_*` exports, the display name, language, script, country, variant, keyword, keyword value and line orientation, forward to the host ICU entry points, reading guest locale strings through `CopyGuestString` and copying the `UChar` result and the `UErrorCode` word in and out through `GuestCopyIn`/`GuestCopyOut`. A null output with a positive capacity, a negative capacity and a capacity above 8192 report `U_ILLEGAL_ARGUMENT_ERROR`. This build ships no `icudt` data, so the test covers those argument checks and the null-status paths only.
