# libSceCamera

## add Additional PS5 3.20 imports

- [sceCameraGetDeviceID](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `jTJCdyv9GLU`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers `0x802E0002` for first arguments -1, 0 and 7 as with an unspecified register, so it stays declared without parameters and returns that constant; the answer with an open handle was not measured and `sceCameraOpen` succeeds in this port, so it throws while a camera is open; a title process may differ.

- [sceCameraIsConfigChangeDone](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `wQfd7kfRZvo`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers 0 for first arguments 0 and 7 as with an unspecified register, so it stays declared without parameters and returns that constant; what an opened camera or a title process changes is unverified.

## change [sceCameraGetDeviceIDWithoutOpen]

- [sceCameraGetDeviceIDWithoutOpen](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `-H3UwGQvNZI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers `0x802E0010` for first arguments 0 and 7 as with an unspecified register, so it stays declared without parameters and returns that constant; what an opened camera or a title process changes is unverified.

## add Silent stubs

- [sceCameraGetCalibData](../../../core/libs/prx/libSceCamera/Export.cpp) (libSceCamera, NID `hHA1frlMxYE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCamera.c); measured on a PS5 Pro payload process (13.60): answers `0x802E0010` for first arguments 0 and 7 as with an unspecified register, so it stays declared without parameters and returns that constant; what an opened camera or a title process changes is unverified.
