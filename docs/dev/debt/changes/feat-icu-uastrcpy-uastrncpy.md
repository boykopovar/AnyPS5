# libSceIcu

## change [Default-converter string copy helpers]

- [Default-converter string copy helpers](../../../core/libs/prx/libSceIcu/Invariant.cpp) `u_austrcpy` and `u_austrncpy` throw `NotImplemented_nid_no_patch` for code units above `0x7f`. Their names and NIDs come from the PS5 3.20 inventory. The signatures and ASCII behavior follow ICU 78.1's [ustr_cnv.cpp](https://github.com/unicode-org/icu/blob/release-78.1/icu4c/source/common/ustr_cnv.cpp). `u_austrncpy` writes one NUL when the source ends before the count. Null buffers with `u_austrcpy` or a positive count throw an invalid-argument exception. The console's default converter for non-ASCII units and its argument handling remain unverified.
