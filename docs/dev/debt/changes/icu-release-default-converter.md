# libSceIcu

## add Functional

- [`u_getDefaultConverter` and `u_releaseDefaultConverter`](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) use the cache and default encoding of the bundled ICU, following [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustr_cnv.cpp) and [ICU 68.2](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/ustr_cnv.cpp). The guest acquire export leaves the cache untouched on an incoming failure. An unreadable status faults through the guest-memory helpers. Release accepts null and raises `invalid_argument` for non-null pointers absent from the set of outstanding guest acquisitions. The export signatures, default encoding and behavior still need PS5 verification.
