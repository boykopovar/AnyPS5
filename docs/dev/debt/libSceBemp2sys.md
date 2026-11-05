# libSceBemp2sys technical debt

### Unknown function info

- [libSceBemp2sys pending exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 31 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceBemp2sys.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `Unknown` | `1L1WwRMiys8` |
| `Unknown` | `B1nK+wb5rME` |
