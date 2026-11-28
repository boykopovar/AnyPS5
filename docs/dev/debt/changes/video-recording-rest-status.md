# libSceVideoRecordingP

## change [Handle and support queries]

- [Handle and support queries](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - native recording handles are context pointers, and GetSupportInfo writes 18 bytes. Support payload field semantics remain incomplete. RestModeProcessStart and SendCommand consume no caller arguments in the supplied native module; their existing unused parameter slots remain because the protected bodies reference those parameter names.

## add Unknown function info

- [sceVideoRecordingRestModeProcessGetStatus and sceVideoRecordingRestModeProcessStart](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - live-client status, start and service behavior remain unsupported. The [public disassembly report](https://github.com/boykopovar/AnyPS5/issues/21884#issuecomment-6951018831) identifies no-argument exports at `0xcec0` and `0xcea0` (routines `0x2d00` and `0x2c60`) and the absent-client result `0x80a80002` at `0x2d43` and `0x2ca3`. Its source is firmware 13.42 `system/common/lib/libSceVideoRecording.native.sprx`, SHA256 `9087746497c4ecb01b80343b9daa16f663b6c0babf0722e96e6504b7378b0375`. This implementation has no client creation path. The [setup report](https://github.com/boykopovar/AnyPS5/issues/21884#issuecomment-6959113340) requires an IPMI service before storing a client; initialization and its service error remain unknown. These are reported readings, not independently inspected firmware or console measurements.
