# libSceCamera

## add [sceCameraIsConnectedForSystem]

- [sceCameraIsConnectedForSystem](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `68S3GKWhY6o`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60) before any `sceCameraOpen`: handles 1, 2, 3, 4, 5 and 1000 answer 0, handles 0, -1 and -2 log `invalid handle` and answer `0x802E0000`; implemented as a positive-handle check returning 0, the debug print not reproduced. Handles above 1000 were not swept; what an opened camera changes is unverified.

## add [sceCameraGetCalibDataForEve]

- [sceCameraGetCalibDataForEve](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `4IGO2LPD3h8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60) before any `sceCameraOpen`: ids 2 and 3 answer `0x802E0010`, ids 0, 1, 4 and 5 log `is not support id` and answer `0x802E0000`; implemented as that accepted set with those answers, the print not reproduced. The meaning of the id set and ids beyond 5 are unverified.

## add [sceCameraGetCalibDataFromDeviceForEve]

- [sceCameraGetCalibDataFromDeviceForEve](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `H-7tWq8gfKs`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60) before any `sceCameraOpen`: ids 2 and 3 answer `0x802E0002`, ids 0, 1 and 4 log `is not support id` and answer `0x802E0000`; implemented as that accepted set with those answers, the print not reproduced. Ids beyond 4 are unverified.

## add [sceCameraGetConnectedDeviceAndDriverMode]

- [sceCameraGetConnectedDeviceAndDriverMode](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `2dBOiluwiMw`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60) before any `sceCameraOpen`: null answers `0x802E0000`; a struct whose first u32 is 12, 20 or 32 logs `struct version error` and answers `0x802E0000` without writing; sizeThis 16 answers 0 and writes the sixteen bytes `10 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00`, leaving the rest of a 64-byte buffer untouched. Implemented with that check and copy-out; the meaning of the fields is unknown and post-open behavior is unverified.

## add [sceCameraGetVrCaptureInfo]

- [sceCameraGetVrCaptureInfo](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `IAz2HgZQWzE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60) before any `sceCameraOpen`: null and structs with sizeThis 4, 8, 12, 16, 20, 24, 32, 40, 48, 64 and 128 all answer `0x802E0000` without writing (the `ForVrTracker struct version error` print names the read value but not the expected one). The accepted size is unknown, so it is declared without parameters and returns `0x802E0000`; a title process and post-open answers are unverified.

## add [sceCameraAccGetData]

- [sceCameraAccGetData](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `QhjrPkRPUZQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): the call never returns - the payload was killed at 40 and at 75 seconds in two sessions without printing again. It still throws; treat any probe of it as blocking, like `sceCameraWaitForEveReady`.

## add [sceCamera2GetCalibrationData]

- [sceCamera2GetCalibrationData](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `emZBXv7cuAI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): with first argument 0 and with an unspecified register it answers `0x8A760000` immediately, with first argument 7 it never returns (two sessions). The safe argument set is unknown, so it still throws; `0x8A76` is not the `0x802E` error domain of this library.
