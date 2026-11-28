# libSceIcu

## add Functional

- [uprv_isASCIILetter](../../../core/libs/prx/libSceIcu/AsciiStrings.cpp) has no console ABI or behavior verification. It follows ICU 68.2's [cstring.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp) for the ASCII charset family, which is assumed, not measured on the console. Its NID matches the ICU 68 name. Closes when: compared with the console.
