# libSceHidControl

## add Functional

- [sceHidControlGetAuthInformation guest output fallback](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - clears and writes its `0x24`-byte `output` buffer through `GuestCopyOut_nid_no_patch` and returns `0x803b3001` (`ErrorInvalidArgument`) with caller `errno` preserved on an unwritable guest pointer as a host-safety fallback; console behavior on unmapped pointers was not measured.
