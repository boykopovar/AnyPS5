# libSceBemp2sys technical debt

### Unknown function info

- [libSceBemp2sys pending exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 27 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceBemp2sys.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Functional

- [bemp2sys_logprintf_getlevel, bemp2sys_logprintf_setfunc, bemp2sys_logprintf_setlevel and bemp2sys_memmanager_set](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - the 11.00 wrappers at `0x5cc0`, `0x5ca0`, `0x5cb0` and `0x5c90` keep a process-global log level, log function pointer and three memory-manager pointers. `setlevel` accepts 0..3 with an unsigned compare, so larger and negative values are ignored; `getlevel` starts at 0. The pointers are stored but have no consumer until the parsers are implemented. The rest of the library keeps throwing.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `Unknown` | `1L1WwRMiys8` |
| `Unknown` | `B1nK+wb5rME` |
