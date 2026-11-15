# libScePad

## add Functional

- [scePadSetLightBar, scePadSetVibration, scePadSetVibrationForce, scePadSetVolumeGain and scePadDeviceClassParseData](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - reading the light bar, vibration, volume and device-class parameter records through `GuestCopyIn_nid_no_patch` and storing the parsed device-class record through `GuestCopyOut_nid_no_patch`, returning `PAD_ERROR_INVALID_ARG` while preserving `errno` for guest pointers that cannot be accessed, is a host-safety fallback; the firmware faults on those pointers instead and its behaviour is not measured on a console.
