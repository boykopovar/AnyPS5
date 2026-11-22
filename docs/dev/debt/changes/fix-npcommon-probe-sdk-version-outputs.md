# libSceNpCommon

## add Unknown function info

- [SDK version outputs](../../../core/libs/prx/libSceNpCommon/Export.cpp) - `sceNpGetSdkVersion`, `sceNpGetNavSdkVersion` and [sceNpGetSdkVersionUInt](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) build the version on the host and write it with one checked guest write, with `errno` preserved. These functions return nothing, so for an output pointing at unmapped memory nothing is written instead of faulting the host. The console's handling of such pointers was not measured.
