# libSceIcu

## add Functional

- [`u_releaseDefaultConverter`](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) releases handles owned by the bundled ICU to its default-converter cache or closes them when the cache is full. The implementation follows [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustr_cnv.cpp) and [ICU 68.2](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/ustr_cnv.cpp). The guest `u_getDefaultConverter` export remains a stub. The release export's ABI and behavior still need PS5 verification.
