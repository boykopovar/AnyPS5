# libSceNetCtlAp

## change [sceNetCtlApGetResult, sceNetCtlApGetState, sceNetCtlApRegisterCallback]

- [sceNetCtlApGetResult, sceNetCtlApGetState, sceNetCtlApRegisterCallback, sceNetCtlApGetInfo, sceNetCtlApGetConnectInfo](../../../core/libs/prx/libSceNetCtlAp/Export.cpp) - checked guest memory copies preserve host errno; returning `NET_CTL_AP_ERROR_INVALID_ADDR` for inaccessible guest memory is a host-safety fallback with console behavior unmeasured.
