# libSceSystemService

## change [sceShellCoreUtilLaunchByUriParamInit]

- [sceShellCoreUtilLaunchByUriParamInit](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - the layout of the parameter block and its size are unknown. It follows firmware 13.42's libSceSystemService.sprx, read from its disassembly and run natively on x86-64 (not a console): it takes one pointer and returns nothing; a null pointer is ignored; otherwise it writes 0 as a 32-bit value at byte 0 and 0xFFFFFFFF at byte 4 and leaves later bytes unchanged. The host now uses a checked guest write, preserving errno, so an unmapped non-null pointer no longer crashes the host. Because this export returns void, a failed write cannot be reported to the caller; the console's response to unmapped guest output and possible partial writes across memory mappings are unmeasured.
