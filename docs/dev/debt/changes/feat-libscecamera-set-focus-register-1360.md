# libSceCamera

## change [sceCameraSetTrackerMode, sceCameraSetRegister, sceCameraSetVideoSyncInternal, sceCameraSetCalibDataForEve, sceCameraOpenByModuleId, sceCameraSetDebugStop, sceCameraSetVideoSync]

- [sceCameraSetTrackerMode, sceCameraSetRegister, sceCameraSetVideoSyncInternal, sceCameraSetCalibDataForEve, sceCameraOpenByModuleId, sceCameraSetDebugStop, sceCameraSetVideoSync](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): all answer `SCE_CAMERA_ERROR_PARAM` (0x802E0000) without reading arguments - `sceCameraSetTrackerMode`, `sceCameraSetRegister`, `sceCameraSetVideoSyncInternal`, `sceCameraSetCalibDataForEve`, `sceCameraOpenByModuleId`, `sceCameraSetDebugStop`, `sceCameraSetVideoSync` are declared with the measured parameter shapes and return that constant.

## change [sceCameraInitializeRegistryCalibData, sceCameraSetCalibData]

- [sceCameraInitializeRegistryCalibData, sceCameraSetCalibData](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): both answer `SCE_CAMERA_ERROR_NOT_INIT` (0x802E0002) without reading arguments, declared without parameters and return that constant.

## change [sceCameraSetProcessFocusByHandle, sceCameraSetProcessFocus]

- [sceCameraSetProcessFocusByHandle, sceCameraSetProcessFocus](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): `sceCameraSetProcessFocusByHandle` answers 0 for all integer inputs (0, 1, 2, 3, -1); `sceCameraSetProcessFocus` answers 0 for (0, buffer) and (1, buffer) without writing the buffer - both declared with the measured parameter shapes and return 0.

## change [sceCameraWaitForEveReady, sceCameraSetUacModeInternal]

- [sceCameraWaitForEveReady, sceCameraSetUacModeInternal](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): both answer 0 without reading arguments, declared without parameters and return 0.

## change [sceCameraChangeAppModuleState]

- [sceCameraChangeAppModuleState](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers `SCE_CAMERA_ERROR_MBIS_ACCESS_DENIED` (0x809B0080) for first argument 0 and `SCE_CAMERA_ERROR_MBIS_FATAL` (0x809B00FF) for 1, 2, 3, -1 - the mbus control failure codes from `sceMbusAcquireControlWithState2`; declared with the measured int parameter and returns those constants.

## change [sceCameraSetAppModuleFocus]

- [sceCameraSetAppModuleFocus](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers `SCE_CAMERA_ERROR_MBIS_ACCESS_DENIED` (0x809B0080) for both (0, buffer) and (1, buffer) without writing the buffer - declared with the measured shapes and returns that constant.

## change [sceCameraSetForceActivate]

- [sceCameraSetForceActivate](../../../core/libs/prx/libSceCamera/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers `SCE_CAMERA_ERROR_PARAM` (0x802E0000) for 0 and -1, `SCE_CAMERA_ERROR_NOT_OPEN` (0x802E0004) for 1, 2, 3 - declared with the measured int parameter and returns those constants.
