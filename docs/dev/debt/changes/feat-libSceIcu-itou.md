# libSceIcu

## add Functional

- [`uprv_itou`](../../../core/libs/prx/libSceIcu/IntegerToUChars.cpp) - follows ICU 68.2 [ustrfmt.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ustrfmt.cpp), including its writes past `capacity`: the first digit is stored even when `capacity` is not positive, and zero padding up to `minwidth` is not bounded by `capacity`. ICU divides by a zero `radix` and dereferences a null `buffer`; here both throw `NotImplemented_nid_no_patch`.
