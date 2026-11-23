# libSceAudioIn

## change [sceAudioInOpen]

- [sceAudioInOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - the 4.03 module applies an extra gate when `sceKernelIsPs4Process` reports a PS4 title: frequency must be 16000, granularity 256, and param <= 1. The PS5 path remains unchanged. The regression test exercises the branch with a CUSA `param.sfo` in a child process. This follows the 4.03 module; it has not been measured on PS4 hardware.
