# libSceJxr

## change [Factories]

- [Factories](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) follow [JXRGlue.c](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRGlue.c#L334-L480) with LP64 layouts of `PKFactory`, `PKCodecFactory` and `PKFormatConverter`; console layouts, slot order and the ignored `uVersion` are unverified except that `PKCodecFactory` is 40 bytes with `Release` at offset 0x20, as reported by BunnyFl from the firmware 13.42 disassembly of `PKCreateCodecFactory`. Slots hold the guest-ABI exports of the same names, so those not yet implemented throw when called. Null or unwritable output slots throw `std::invalid_argument`; allocations are freed again when an output cannot be published. `PKCodecFactory_CreateDecoderFromFile` still throws. Closes when: the console's factory structures are disassembled or measured on a PS5

## change [PKFree]

- [PKFree](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) follows Microsoft's jxrlib [JXRGlue.c](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRGlue.c#L53-L62), whose four exported names the module shares: a non-null `ppv` is copied from guest memory, cleared through `GuestCopyOut_nid_no_patch`, then released with guest `free`; unreadable or unwritable slots throw before the allocation is released, and guest probes preserve host `errno`. A null `ppv` still succeeds, and the result is always `WMP_errSuccess` (0) as a 64-bit `ERR` (`long`). Console provenance and invalid-pointer behavior are unverified. Closes when: the console's `PKFree` is disassembled or measured on a PS5
