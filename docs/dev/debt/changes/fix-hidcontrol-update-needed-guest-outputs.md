# libSceHidControl

## change [Guest output fallback]

- [Guest output fallback](../../../core/libs/prx/libSceHidControl/Export.cpp) - `sceHidControlIsForceUpdateNeeded`, `sceHidControlPswUpdateIsUpdateNeeded`, `sceHidControlPswUpdateIsUpdateNeededForMultiDevice`, `sceHidControlPswUpdateIsReady`, `sceHidControlPswUpdateGetStatus` and `sceHidControlPswUpdateGetReconnectStatus` move their outputs through `GuestCopyOut_nid_no_patch` (and the multi-device version input through `GuestCopyIn_nid_no_patch`) and return `0x803b3001` (`ErrorInvalidArgument`) when a guest pointer is unreadable or unwritable, preserving host errno where no ioctl runs; console behavior on unmapped pointers is not measured.
