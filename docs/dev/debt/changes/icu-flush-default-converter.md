# libSceIcu

## add Functional

- [`u_flushDefaultConverter`](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) flushes the bundled ICU default-converter cache through [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustr_cnv.cpp), whose signature and behavior agree with [ICU 68.2](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/ustr_cnv.cpp). The guest `u_getDefaultConverter` and `u_releaseDefaultConverter` exports are still stubs. The export's ABI and cache behavior have not been verified on a PS5 console.
