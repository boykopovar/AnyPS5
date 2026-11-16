# libSceIcu

## add Functional

- [Property name comparison](../../../core/libs/prx/libSceIcu/PropertyNameCompare.cpp) - `uprv_compareASCIIPropertyNames` and `uprv_compareEBCDICPropertyNames` follow ICU 68.2 [propname.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/propname.cpp#L28-L142) with the lowercasing of `uprv_asciitolower` and `uprv_ebcdictolower` from [cstring.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp#L102-L119), which stay stubs. Bytes are read as unsigned; upstream tests the ASCII ignorable range on a plain `char`, the same for every byte when `char` is signed, as on the console's x86-64. ICU dereferences a null name; here it throws `NotImplemented_nid_no_patch`.
