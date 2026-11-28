# libSceCamera

## add Functional

- [sceCameraIsVrCaptureEnable, sceCameraGetUsageInfo, sceCameraGetUsageInfoWithoutCountProcess, sceCameraOpenByModuleIdWithMode, sceCameraSetForceDriverMode, AnqFx-nAxZM, azeU8qML4VM, kgRnfleNYNo](../../../core/libs/prx/libSceCamera/InventoryExports.cpp) - updated from 13.60 `libSceCamera.sprx` disassembly: `sceCameraIsVrCaptureEnable` validates the output pointer and writes `0` when VR capture tool memory is unallocated; `sceCameraGetUsageInfo` and `WithoutCountProcess` validate non-null and `12 <= sizeThis <= 16`; `sceCameraOpenByModuleIdWithMode` checks `userId == 0xFF`, `type == 0`, `index == 0`; `sceCameraSetForceDriverMode` checks app category (`0x802E0011`) and mode `0`/`1`; `AnqFx-nAxZM`, `azeU8qML4VM`, and `kgRnfleNYNo` gate null pointers and unopened state (`0x802E0002`); remaining kernel ioctl paths throw via `NotImplemented_nid_no_patch`.
