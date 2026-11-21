# libSceIcu

## add Functional

- [Invariant character helpers](../../../core/libs/prx/libSceIcu/InvariantChars.cpp) have no console ABI or behavior verification. `u_asciiToUpper`, `uprv_toupper`, `uprv_asciitolower`, `uprv_ebcdictolower`, `uprv_stricmp`, `uprv_strnicmp`, `uprv_isInvariantString` and `uprv_isInvariantUString` take the signatures of ICU 68.2's internal [cstring.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.h) and [uinvchar.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uinvchar.h) and call bundled ICU 78.1, whose code and invariant table match 68.2. A null string with a nonzero length for the `uprv_isInvariant*` exports throws `std::invalid_argument` where ICU dereferences it; the comparisons keep ICU's ordering of null strings. Closes when: compared with the console.
