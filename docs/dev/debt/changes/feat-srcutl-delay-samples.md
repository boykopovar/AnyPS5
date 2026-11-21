# libSceSrcUtl

## change [sceSrcUtilityFinalize, sceSrcUtilityFlush, sceSrcUtilityGetDelaySamples, sceSrcUtilityInitialize, sceSrcUtilityInitializeEx and sceSrcUtilityResample]

- [sceSrcUtilityFinalize, sceSrcUtilityFlush, sceSrcUtilityInitialize, sceSrcUtilityInitializeEx and sceSrcUtilityResample](../../../core/libs/prx/libSceSrcUtl/Export.cpp) (libSceSrcUtl) - throw past the checks under Functional: in firmware 13.42 they drive AJM codec 19 (0x13), which this project's AJM lacks, and the conversion runs inside AJM, not in this module. `sceSrcUtilityInitializeEx` initializes an AJM context and registers codec 19; `sceSrcUtilityInitialize` checks its arguments like `sceSrcUtilityGetHandleSize`, clears the 0x9678-byte handle, creates a codec 19 instance, sends it a control job and marks the handle (0xDEADBEEFBAADF00D at 0x9470, priority 0x28 at 0x2C).

## add Unknown function info

- [sceSrcUtilityGetDelaySamples](../../../core/libs/prx/libSceSrcUtl/Export.cpp) (libSceSrcUtl) - `int (int inputRate, int outputRate)` returns the delay in samples from firmware 13.42's 44-entry table ([reading](https://github.com/boykopovar/AnyPS5/issues/21839#issuecomment-6939058941), disassembly only, not run on a console): `0x815D0004` for an input rate not in it, then `0x815D0005` for an output rate not listed with that input rate. A PS4 process gets the table value. Otherwise a failing `sceKernelGetProsperoCompiledSdkVersion` returns its error, and a title built with SDK 5.00 or later gets the module's six overrides (22050 to 96000 160, 22050 to 192000 324, 24000 to 44100 60, 44100 to 192000 160, 176400 to 44100 22, 192000 to 48000 22). `sceSrcUtilityTerminateEx` still throws: past its null check, whose result the module leaves undefined, it calls two unread `libSceAjmi` exports.
