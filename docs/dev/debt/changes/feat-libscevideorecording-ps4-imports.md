# libSceVideoRecording

## add Unknown function info

- [sceVideoRecordingGetStatus, sceVideoRecordingStart, sceVideoRecordingStop, sceVideoRecordingClose](../../../core/libs/prx/libSceVideoRecording/Export.cpp) (libSceVideoRecording, PS4) - answer as with no recorder open, since this library never opens one: GetStatus 0 and the other three 0x80A80006, the value `sceVideoRecordingSetInfo` returns here for the info that needs a recorder and the one `libSceVideoRecordingP` returns for these calls, measured on a PS5 (firmware 13.40). The PS4 answers are not measured, and a PS5 title throws. CUSA12982 imports them.
- [sceVideoRecordingOpen2, sceVideoRecordingQueryMemSize2](../../../core/libs/prx/libSceVideoRecording/Export.cpp) (libSceVideoRecording, PS4) - throw through `NotImplemented_nid_no_patch`: the PS4 parameter layout, heap sizes and what Open2 returns when the capture service cannot be reached are not known. They are exported so CUSA12982, which imports them, can load.
