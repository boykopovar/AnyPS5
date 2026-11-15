# libSceNet

## add Unknown function info

- [sceNetGetSystemTime](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - the process time is written with a checked guest write; for a non-null output pointing at unmapped memory nothing is written and the net and libc errno values are left alone, as for a null output, instead of faulting the host. The function returns nothing, so the console's handling of unmapped memory there was not measured.
