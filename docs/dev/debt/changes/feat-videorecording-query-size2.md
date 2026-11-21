# libSceVideoRecordingP

## change sceVideoRecordingOpen2](

- [sceVideoRecordingOpen2](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - the 40-byte parameter variant, four argument registers and 32-bit heap size are recovered. Mode, output mode, option and format semantics remain incomplete, and Open2 remains unimplemented.

## add Functional

- [sceVideoRecordingQueryMemSize2, sceVideoRecordingQueryHddSize2](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) follow a [reading of firmware 13.42's libSceVideoRecording.sprx](https://github.com/boykopovar/AnyPS5/issues/21884#issuecomment-6947645425), not measured on a console. Both refuse a null block or a size below 0x18 with 0x80A80003 and read only the duration at +0x14, refused above 3600 and 180. The heap size depends on the compiled SDK version and the Neo-mode attribute; the HDD size is the duration, at least 10, times 0x1C0000, or 0x280000 in Neo mode. The `get_authinfo` bit 62 branch is never taken, since `get_authinfo` sets only bit 61 here. A title with param.json throws in `sceKernelIsNeoMode`, so both queries throw for it. An unreadable block gives 0x80A80003, a host-safety fallback. Unread: whether the SDK and duration comparisons are signed.
