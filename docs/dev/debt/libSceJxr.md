# libSceJxr technical debt

### Unknown function info

- [libSceJxr pending exports](../../../core/libs/prx/libSceJxr/Export.cpp) - 4 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJxr.c) and were cross-checked against the same module in PS5 firmware 13.60. `PixelFormatLookup`, `readIS` and `writeIS` have unverified signatures and behaviour; their declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Functional

- [PKFree](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) follows Microsoft's jxrlib [JXRGlue.c](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRGlue.c#L53-L62), whose four exported names the module shares: a non-null `ppv` has `*ppv` released with the guest `free` and set to null, and the result is always `WMP_errSuccess` (0) as a 64-bit `ERR` (`long`). That the console's module is built from jxrlib, and that its `PKFree` releases through libc's `free`, is inferred from the export names and not checked against the console's code. Closes when: the console's `PKFree` is disassembled or measured on a PS5
