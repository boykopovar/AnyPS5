# libSceVcodec

## change [Ac6aYjr1pY8]

- [Ac6aYjr1pY8](../../../core/libs/prx/libSceVcodec/Export.cpp) - the output layout and its conversions follow 13.42 disassembly, not console measurements. An unwritable output returns `0x80BF0001` through `GuestCopyOut_nid_no_patch` instead of faulting the host; this pointer-safety result is unmeasured on console.
