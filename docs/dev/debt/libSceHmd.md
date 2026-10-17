# libSceHmd technical debt

### Silent stubs

- [libSceHmd](../../../core/libs/prx/libSceHmd/Export.cpp) implements only the disconnected-headset path: initialization succeeds, device queries report `NotDetected`, and opening a device returns `DeviceDisconnected`. Headset support, tracking, rendering and additional HMD exports are not implemented; SDK-level ABI compatibility and in-game behaviour remain unverified.

### Unknown function info

- [libSceHmd distortion and field of view](../../../core/libs/prx/libSceHmd/Export.cpp) (libSceHmd) - `sceHmdDistortionInitialize`, `sceHmdDistortionGet2dVrCommand`, `sceHmdDistortionGetCorrectionCommand`, `sceHmdDistortionGetWorkMemorySize`, `sceHmdDistortionGetWorkMemoryAlign`, `sceHmdFillDistortionBuffer`, `sceHmdGet2DEyeOffset`, `sceHmdGet2dDistortionMap`, `sceHmdGet2dVrCommand`, `sceHmdGetDistortionCorrectionCommand`, `sceHmdGetDistortionMap`, `sceHmdGetDistortionParams`, `sceHmdGetDistortionWorkMemoryAlign`, `sceHmdGetDistortionWorkMemorySize`, `sceHmdGetEyeStatus`, `sceHmdGetFieldOfView` are declared without parameters from the firmware export database (aerolib.csv); their guest signatures are unverified on hardware and every call throws
