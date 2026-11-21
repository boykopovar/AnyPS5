# libSceHidControl

## add Functional

- [sceHidControlGetControllerSerialNumberInformation guest output](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - the firmware-13.42-disassembly-backed export still returns without writing on negative `ioctl`, and on nonnegative replies copies the first `0x11` reply bytes plus `0x0f` zero bytes. It now uses `GuestCopyOut_nid_no_patch` once for the `0x20`-byte output and returns `0x803b3001` (`ErrorInvalidArgument`) for an unwritable guest destination while preserving host `errno`. The invalid-pointer error is a host-safety fallback, not measured on a console.
