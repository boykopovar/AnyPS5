# libSceVcodec

## change [Ac6aYjr1pY8]

- [Ac6aYjr1pY8](../../../core/libs/prx/libSceVcodec/Export.cpp) - the output layout and its conversions follow 13.42 disassembly, not console measurements. An unwritable output returns `0x80BF0001` through `GuestCopyOut_nid_no_patch` instead of faulting the host; this pointer-safety result is unmeasured on console.

## add Functional

- [Export.cpp](../../../core/libs/prx/libSceVcodec/Export.cpp) (libSceVcodec) - reverting it alone still passes its tests in core/libs/tests/GuestVcodec.cpp, because the success path of the status conversion needs `/dev/uvd_bgt`, which AnyPS5 does not model; the tests reach only the `EBADF` path. Modelling the device is a separate change.
