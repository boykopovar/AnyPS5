# libSceVideoRecordingP

## add Silent stubs

- [sceVideoRecordingOpen, sceVideoRecordingQueryMemSize](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP) - the host has no recorder, so a call that passes every parameter check fails with `0x80A80002`, the code `sceVideoRecordingSetInfo` returns for a volume change it cannot apply, instead of reporting a heap size or opening a session. No console run measured this code for these two calls; the checks before it are measured, as the Functional entry below says
- [videoRecordingUnknown_iQS6DUtLybE](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP, NID `iQS6DUtLybE`) - its signature and behavior are unknown; it takes no arguments here and returns `0x80A80002`, a choice with no source, so a title that calls it learns that recording is unavailable

## change never open a recorder: both throw

- [sceVideoRecordingOpen, sceVideoRecordingQueryMemSize](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP) never open a recorder: both fail with `0x80A80002` for a block with a video format, and Open fails so for any block it accepts, because neither the heap size nor the recording session is implemented. The checks before that follow a PS5 (firmware 13.40, measured inside PPSA01317): QueryMemSize returns 0x80A80003 for a block shorter than 16 bytes, a duration outside 1 to 3600 seconds, a mode or option above 1 and an unknown format, 0x80A80008 for the option without a format and 0x1000 for neither, and Open refuses null arguments and a negative or too small heap with 0x80A80003. Read and not measured: Open checks the affinity mask, thread priority and output mode after the heap size
