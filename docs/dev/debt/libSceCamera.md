# libSceCamera technical debt

### Unknown function info

- [sceCameraOpen, sceCameraClose, sceCameraStart, sceCameraStop, sceCameraGetConfig, sceCameraSetConfig, sceCameraSetAttribute, sceCameraGetDeviceInfo, sceCameraGetFrameData, sceCameraIsValidFrameData](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera) - imported by PS4 titles and added without an implementation, declared without parameters. What the console returns from them while no camera is attached is not known, so every call throws.

### Functional

- [sceCameraIsAttached](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera) - no camera is ever attached: index 0 returns 0 (not attached) and any other index returns `SCE_CAMERA_ERROR_PARAM` (0x802E0000), as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/9a8ae4417bbfc79b1a43eb8a2eb0bb602148261e/src/core/libraries/camera/camera.cpp#L187-L200) does without a configured camera, with the error value from its [camera_error.h](https://github.com/shadps4-emu/shadPS4/blob/9a8ae4417bbfc79b1a43eb8a2eb0bb602148261e/src/core/libraries/camera/camera_error.h). Not measured on a console. Closes when: a host camera is exposed to guests.
