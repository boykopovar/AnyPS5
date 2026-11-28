# libSceNpCommon

## add Functional

- [sceNpUtilXmlEscape and sceNpUtilXmlUnescape guest buffers](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the source is read once through checked copies in bounded chunks, together with up to 8 guest bytes after it that the loops may look at (taken as zeros when unreadable), and the result is built on the host and written with one checked copy before each return. An unreadable source returns `NP_UTIL_ERROR_INVALID_ARGUMENT` with `errno` kept. A host-safety fallback, not measured on the console.
