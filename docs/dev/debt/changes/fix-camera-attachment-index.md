# libSceCamera

## change [sceCameraIsAttached]

- [sceCameraIsAttached](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera) - no host camera is exposed. Indices 0 and 1 return 0 (not attached) and out-of-range indices return `SCE_CAMERA_ERROR_PARAM` (`0x802E0000`). A PS5 [payload probe on firmware 11.60](https://github.com/boykopovar/AnyPS5/pull/24428#issuecomment-7044665102) measured those answers for 0, 1, -1 and 2 without opening or initializing the camera. Closes when: a host camera is exposed to guests.
