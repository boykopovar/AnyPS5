# libSceSystemService

## add Functional

- [Social screen status guest output](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - `sceShellCoreUtilGetSocialScreenStatus` writes its output through `WriteStatusOrRefuse`, so an inaccessible non-null pointer returns `0x80e30003` instead of faulting the host and a null output returns the same code where it threw before. The copy is covered by guest tests but not measured on a console, and neither is whether the console checks the output pointer before returning the inactive answer; closes when measured on a PS5.
