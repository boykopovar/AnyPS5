# libSceIcu

## add Functional

- [`ucnv_getDefaultName` and `ucnv_setDefaultName`](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) follow the bundled [ICU converter configuration](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucnv_bld.cpp). The set export uses the bundled converter registry and ignores changes on builds with `U_CHARSET_IS_UTF8`. Resetting the name also clears the cached converter, and release closes a previously acquired converter whose name differs from the current default. The setter follows ICU's requirement that changes occur during single-threaded initialization. The guest signatures and the PS5 default codepage still need console verification.
