# libSceNgs2.native

## add Functional

- [sceNgs2SystemGetInfo, sceNgs2SystemEnumHandles and sceNgs2SystemEnumRackHandles guest pointers](../../../core/libs/prx/libSceNgs2.native/src/System.cpp) (libSceNgs2.native) - the system info is built on the host and the enumerated handles are stored one at a time through the guest copy helpers; returning `INVALID_OUT_ADDRESS` for an unwritable info and the invalid argument error `0x804A0053` for an unwritable handle array is a host-safety fallback (handles before the failing slot stay written), and whether the console faults or returns an argument error on unmapped memory is unmeasured.
