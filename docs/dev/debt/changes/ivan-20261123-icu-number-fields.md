# libSceIcu

## add Functional

- The [unum_formatDoubleForFields](../../../core/libs/prx/libSceIcu/NumberFormat.cpp) guest export uses the [pinned ICU 78.1 formatter](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/unum.cpp) and the existing checked formatter and field-iterator handles. Checked guest output and status writes precede the replacement of numeric UTF-16 field ranges. A native staging allocation failure returns the failed formatter call’s length and ICU allocation error before guest output or field publication. If the pinned ICU backend cannot allocate the final field list, the previous iterator survives and the function reports an allocation error after the guest text was written. Exact field ranges, locale data and allocation-failure behavior on console remain unmeasured. Closes when compared with the console.
