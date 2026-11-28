# libSceVideoRecordingP

## change [sceVideoRecordingCloseByHandle, sceVideoRecordingGetInfoByHandle]

- [sceVideoRecordingCloseByHandle, sceVideoRecordingGetInfoByHandle](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - native handles are context pointers. `sceVideoRecordingCloseByHandle` follows firmware 13.42's `libSceVideoRecording.native.sprx` ([disassembly](https://github.com/boykopovar/AnyPS5/issues/21884#issuecomment-6947645425), not measured on a console): a null handle gives `0x80a80003`, and any handle that is neither the built-in recorder with recordings open nor the object the module keeps at `0x75188` gives `0x80a80006`. No recorder is ever opened here (`sceVideoRecordingOpen`, `Open2` and `OpenByRAFeatureId` never return a handle), so every non-null handle gets `0x80a80006`, the answer `sceVideoRecordingClose` already gives. Recording state and per-handle queries remain unimplemented.

## add Functional

- [sceVideoRecordingCloseWithTruncationByHandle](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP, NID `oryBI-2iyxw`, the hash of the name) - takes `(handle, discard, truncation, mode)` and checks them as firmware 13.42 does ([disassembly](https://github.com/boykopovar/AnyPS5/issues/21884#issuecomment-6947645425), not measured on a console): a mode above 3, a null handle, a positive truncation, or one below -60 (-60000 when bit 1 of the mode is set) gives `0x80a80003`. Past the checks it throws: the reading gives the built-in recorder and stored-object paths but not the answer for any other handle, which is every handle here.
