# libSceHidControl

## add Functional

- [sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData guest output](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - returning `0x803b3001` (`ErrorInvalidArgument`) for an unwritable telemetry destination is a host-safety fallback; console behavior on inaccessible output memory is unmeasured. The checked 64-byte copy preserves errno from the ioctl, but a failed boundary-crossing copy may partially write the destination. The HID device and telemetry reply semantics remain unimplemented and unverified as described above.
