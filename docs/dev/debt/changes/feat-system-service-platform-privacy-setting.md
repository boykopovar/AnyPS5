# libSceSystemService

## add Functional

- [sceSystemServiceGetPlatformPrivacySetting](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - follows `libSceSystemService.sprx` of firmware 13.60, read from disassembly and not measured on a console: a process that `sceKernelIsPs4Process` does not report as a PS4 one gets 0 and its output is not written; a PS4 process gets 1 when bit 0 of the shared event flag `SceUserServiceCommonEventFlag` is set and 0 otherwise, also when the flag cannot be opened. Nothing in AnyPS5 creates that flag, which the console's system owns, so a PS4 title always reads 0; the console's value and what the two values mean are unknown. The module's `printf` of the open error is not reproduced. Closes when: measured on a console.
