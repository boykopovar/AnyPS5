# libSceHttp

## change Aps5HttpClient_Read

- [sceHttpGetCookie](../../../core/libs/prx/libSceHttp/src/Cookies.cpp), [sceHttpParseResponseHeader](../../../core/libs/prx/libSceHttp/Export.cpp), [URI functions](../../../core/libs/prx/libSceHttp/src/Uri.cpp) and [Aps5HttpClient_Read](../../../core/libs/prx/libSceHttp/src/Client.cpp) still access some guest buffers in place, so an unmapped input or an unchecked output in the remaining functions can crash the host instead of returning `ERROR_INVALID_VALUE`. `sceHttpParseResponseHeader` still reads the guest header in place and returns pointers into it, so its input cannot be copied without changing its result, but its `fieldValue` and `valueLen` output stores now use checked guest copies. `sceHttpGetCookie` needs a stored cookie, which the tests cannot create offline. The Uri and Client functions overlap open pull requests.
