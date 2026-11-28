# libSceDepth2

## change libSceDepth2 pending exports

- [libSceDepth2 pending exports](../../../core/libs/prx/libSceDepth2/Export.cpp) - 9 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDepth2.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention, except the ones under Functional that take the handle, and apart from those answers throw through `NotImplemented_nid_no_patch`.

## add Functional

- [sceDepth2QueryMemory](../../../core/libs/prx/libSceDepth2/Export.cpp) (libSceDepth2) - `sceDepth2QueryMemory` signature, parameter record validation checks, error codes, and memory size calculation are implemented according to firmware 13.42 disassembly (`libSceDepth2.sprx` sha256 `12bb81d8370920f886ba7470548b812fbd940e2f867bf070cf41363c3dd5848d`) with 116 verified test sets.
