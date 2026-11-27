# libSceFlexContent

## change [sceFlexContentTerminate, sceFlexContentMount, sceFlexContentGetDownloadProgress, sceFlexContentDelete, sceFlexContentGetLatestVersion, sceFlexContentGetInstalledVersion and the 5 NID-bound exports]

- [sceFlexContentTerminate, sceFlexContentMount, sceFlexContentGetDownloadProgress, sceFlexContentDelete, sceFlexContentGetLatestVersion, sceFlexContentGetInstalledVersion and the 5 NID-bound exports](../../../core/libs/prx/libSceFlexContent/Export.cpp) (libSceFlexContent) - they return `0x81B60001` before initialization. In firmware 13.42 each first tests the library's IPMI client and returns `0x81B60001` while it does not exist; `sceFlexContentInitialize` tracks initialization, returning `0x81B60002` when already initialized, and `sceFlexContentTerminate` clears client state returning 0.
