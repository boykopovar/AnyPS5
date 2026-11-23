# libSceIcu

## add Functional

- [uprv_isASCIILetter, uprv_toupper, uprv_asciitolower, uprv_stricmp and uprv_strnicmp](../../../core/libs/prx/libSceIcu/AsciiStrings.cpp) have no console ABI or behavior verification. They follow ICU 68.2's [cstring.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp) for the ASCII charset family, which is assumed, not measured on the console. Their NIDs match ICU 68 names. Closes when: compared with the console.
