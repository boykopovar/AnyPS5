# libSceAudioIn technical debt

### Silent stubs

- [sceAudioInInit](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - returns 0 without library-wide setup, as `sceAudioOutInit` does; each `sceAudioInOpen` opens its own SDL capture device

### Unknown function info

- [sceAudioInDeviceIdOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInDeviceIdAsyncOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetUsbGain](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInIsSharedDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInDeviceIdHqOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInDeviceHqOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetConnections](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetPortConnections](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetPortStatuses](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInGetHandleStatusInfo](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInDeviceOpenEx](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInChangeAppModuleState](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetSparkParam](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetSparkSideTone](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetCompressorPreGain](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature; the PS5 12.00 module returns 0 without reading any argument, and so does this one
- [sceAudioInSetNoiseGateThreshold](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature; the PS5 12.00 module returns 0 without reading any argument, and so does this one
- [sceAudioInSetMicLevel](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetDeviceMuteStatus](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetDevConnection](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInCountPorts](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetUserMute](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInVmicCreate](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInVmicDestroy](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInVmicWrite](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInVmicGetStatus](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInVmicSetMute](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetMode](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInSetMode2](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInExtCtrl](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature
- [sceAudioInInputs](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature

### Functional

- [sceAudioInIsPadEmbeddedMic](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - returns 0 for every open port: on the console it returns 1 when the port captures from the controller's built-in microphone and 0 for another input device, but ports here capture from the host's default input device
- [sceAudioInHqOpen, sceAudioInAsyncOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) - PS5 wrapper validation requires 48000 Hz followed by exactly 128 samples for both calls; HqOpen then requires format 2. The host uses the same SDL capture path for both, so native asynchronous scheduling and high-quality processing are not reproduced. Common port validation, device selection and host capture pacing remain incomplete.
- [sceAudioInGetGain, sceAudioInGetRerouteCount](../../../core/libs/prx/libSceAudioIn/Export.cpp) - GetGain writes 1.0f before handle validation and returns 0x80260105 for a null output, as confirmed by the PS5 module. Native handle bit checks and uninitialized/inactive port errors differ from the host port table. Native reroute counters follow device reassignment; the host opens each SDL device once and reports zero.
