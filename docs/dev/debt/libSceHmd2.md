# libSceHmd2 technical debt

### Silent stubs

- [libSceHmd2](../../../core/libs/prx/libSceHmd2/Export.cpp) has no PS VR2 runtime: `sceHmd2Initialize` fails, and every other export it has (and every [libSceVrTracker2](../../../core/libs/prx/libSceVrTracker2/Export.cpp) export) throws. PPSA01317 skips its whole VR set-up when initialization fails

### Unknown function info

- PPSA29866 imports declared without parameters, signatures unknown: [sceHmd2ReprojectionQuerySeeThroughBufferSizeAlign](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2). `sceVideoOutSetFlipMaster` and `sceVideoOutClearFlipMaster` return success as no-ops because the host has one display and no flip-master arbitration layer
- [sceHmd2Initialize](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - the error the console returns when the PS VR2 runtime is unavailable is unknown; it returns libSceHmd's `UNSUPPORTED_FEATURE` (0x81110016). PPSA01317 passes a pointer to 16 zeroed bytes and only tests the result for non-zero
- PPSA01317 imports declared without parameters, signatures unknown: the remaining [libSceHmd2](../../../core/libs/prx/libSceHmd2/Export.cpp) and [libSceVrTracker2](../../../core/libs/prx/libSceVrTracker2/Export.cpp) exports. Names from the aerolib NID list
- PPSA15878 imports declared without parameters, signatures unknown: the [libSceHmd2](../../../core/libs/prx/libSceHmd2/Export.cpp) reprojection and gaze exports, `sceVrTracker2Finalize`, `sceVrTracker2GetPlayAreaOrientedBoundingBox` and `sceVrTracker2LocateCoordinateSystem` in [libSceVrTracker2](../../../core/libs/prx/libSceVrTracker2/Export.cpp), and the `scePadVrController` trigger effect exports in [libScePad](../../../core/libs/prx/libScePad/Export.cpp). Names from the aerolib NID list

### Functional

- PPSA07411 PS VR2 imports declared without parameters, signatures unknown, added without an implementation because the title fails to load without them: [sceHmd2GetDeviceInformationByHandle, sceHmd2GetFieldOfView and sceHmd2Terminate](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2), [scePadVrControllerSetVibration, scePadVrControllerSetVibrationMode and scePadVrControllerReadCombinedState](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) and [sceVrTracker2ResetLocalCoordinate](../../../core/libs/prx/libSceVrTracker2/Export.cpp) (libSceVrTracker2)
