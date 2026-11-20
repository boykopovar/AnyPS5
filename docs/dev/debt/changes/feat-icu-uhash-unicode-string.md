# libSceIcu

## add Functional

- [uhash_hashUnicodeString, uhash_hashCaselessUnicodeString, uhash_compareUnicodeString and uhash_compareCaselessUnicodeString](../../../core/libs/prx/libSceIcu/UnicodeStringHash.cpp) have no console ABI or behavior verification. They read the guest `icu::UnicodeString` through checked copies using the public ICU 68 64-bit layout from [unistr.h](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/unicode/unistr.h) (flags at offset 8, stack buffer at 10, long length at 12, array at 24) and hash, fold and compare a host copy with the bundled ICU, following [uhash_us.cpp](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/uhash_us.cpp) and [unistr_case.cpp](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/unistr_case.cpp). Unreadable objects or text, a negative length and a stack length over 27 throw through `NotImplemented_nid_no_patch`. Closes when: compared with the console.
