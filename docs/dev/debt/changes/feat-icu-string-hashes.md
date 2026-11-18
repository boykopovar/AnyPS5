# libSceIcu

## add Functional

- [ICU string hash helpers](../../../core/libs/prx/libSceIcu/CString.cpp) have no console ABI or behavior verification. `u_asciiToUpper`, `ustr_hashUCharsN`, `ustr_hashCharsN` and `ustr_hashICharsN` are internal ICU C functions whose names hash to the existing PS5 3.20 NIDs; signatures and behavior follow ICU 78.1's [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp), whose `STRING_HASH` macro is unchanged since 68.2 apart from the `NULL` and `UChar` spellings. The hashes multiply by 37 per unit, sample every `(length - 32) / 32 + 1`th unit, and return zero for a null string or a nonpositive length. `ustr_hashICharsN` folds only ASCII `A`–`Z`, as `uprv_tolower` does in an ASCII-family build. Closes when: compared with the console.
