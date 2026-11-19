# libSceHidControl

## add Functional

- [sceHidControlIsForceUpdateNeeded](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - returning `0x803b3001` (`ErrorInvalidArgument`) when the `needed` pointer is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving host errno; console behavior on unmapped pointers is not measured.


