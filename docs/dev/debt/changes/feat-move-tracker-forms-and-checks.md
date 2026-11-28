# libSceMove

## change [sceMoveIsReproductionModel, sceMoveReadStateRecentForTracker, sceMoveSetExtensionPortOutput, sceMoveSetLightSphereForTracker, sceMoveWriteCalibrationInfo]

- [sceMoveIsReproductionModel, sceMoveReadStateRecentForTracker, sceMoveSetExtensionPortOutput, sceMoveSetLightSphereForTracker, sceMoveWriteCalibrationInfo](../../../core/libs/prx/libSceMove/Export.cpp): follow firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21752#issuecomment-6932806153), disassembly only, not run on a console), each returning 0x80EE0001 before `sceMoveInit`. `SetLightSphereForTracker` and `ReadStateRecentForTracker` run `SetLightSphere`'s and `ReadStateRecent`'s path with a flag that changes only the controller request and one bit of each copied state record, so they forward. `SetExtensionPortOutput(handle, data)` and `WriteCalibrationInfo(handle, info)` return 0x80EE0003 for a null pointer, 0x80EE0004 for an unknown handle, and 1 with no controller attached, as here. `IsReproductionModel(handle)` returns 0x80EE0006 for an unknown handle and otherwise asks the driver even with no controller, so it throws

## change a null data array is accepted for SDK versions through 0x03ffffff

- [sceMoveReadStateRecent](../../../core/libs/prx/libSceMove/Export.cpp): a null data array is accepted for SDK versions through 0x03ffffff and rejected from 0x04000000 on, as the native module does; the count pointer and a nonnegative timestamp are required for every version. An unknown handle returns 0x80EE0004 with `*count` set to 0, as firmware 13.42 does ([reading](https://github.com/boykopovar/AnyPS5/issues/21752#issuecomment-6932806153)); `ReadStateRecentForTracker` shares the path. Connected state records and device-service failures remain unimplemented.
