# libSceIcu

## change [UTF-16 scalar counting and search]

- [UTF-16 scalar counting and search](../../../core/libs/prx/libSceIcu/Utf16.cpp): equivalence on console hardware is not established beyond the firmware 13.42 emulator cases below. The five APIs follow the stable public contracts and implementation of ICU 78.1 [ustring.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/unicode/ustring.h) and [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp), including isolated-surrogate matches, counted embedded NULs, truncated pairs and invalid scalar rejection. Null or unreadable text that a call has to read throws `std::invalid_argument`, and terminated counts beyond INT32_MAX throw; the public API does not define such valid inputs, and the console's behavior for them is unverified. Overlapping concurrent modification remains a caller error.
