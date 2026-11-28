# libSceIcu

## add Functional

- [EBCDIC helpers](../../../core/libs/prx/libSceIcu/InvariantChars.cpp) have no console ABI or behavior verification. `uprv_ebcdicToAscii`, `uprv_ebcdicToLowercaseAscii`, `uprv_aestrncpy`, `uprv_eastrncpy`, `uprv_compareInvEbcdicAsAscii` and `uprv_compareEBCDICPropertyNames` take the signatures of `uinvchar.h` and `propname.h` in [ICU 68.2](https://github.com/unicode-org/icu/tree/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common) and call bundled ICU 78.1, whose code and tables for them match. A null string that ICU would dereference throws `std::invalid_argument`. Closes when: compared with the console.
