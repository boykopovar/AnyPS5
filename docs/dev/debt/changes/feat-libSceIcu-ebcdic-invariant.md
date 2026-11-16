# libSceIcu

## add Functional

- [EBCDIC invariant characters](../../../core/libs/prx/libSceIcu/EbcdicInvariant.cpp) - `uprv_ebcdicToAscii`, `uprv_ebcdicToLowercaseAscii`, `uprv_compareInvEbcdicAsAscii`, `uprv_aestrncpy` and `uprv_eastrncpy` follow ICU 68.2 [uinvchar.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uinvchar.cpp#L551-L625) and carry its three 256-byte tables, including the one entry where they disagree beyond letter case: byte 0xE0 is 0x5C in `asciiFromEbcdic` and 0x7C in `lowercaseAsciiFromEbcdic`. `uprv_ebcdicFromAscii`, which needs a `UDataSwapper`, stays a stub. ICU dereferences a null string and reads the first source byte of a copy whose count is not positive; here a null string throws `NotImplemented_nid_no_patch` and such a copy reads nothing.
