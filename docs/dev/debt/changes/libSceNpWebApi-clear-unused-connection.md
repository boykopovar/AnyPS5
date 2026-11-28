# libSceNpWebApi

## change [sceNpWebApiClearUnusedConnection and sceNpWebApiGetConnectionStats]

- [sceNpWebApiGetConnectionStats](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - HTTP connections are not modelled, so a call that passes the checks throws. Only the argument widths come from the bodies; names and pointee types are unverified. In firmware 13.42's `libSceNpWebApi.sprx` (SHA-256 `682be03706848ada18ac61ba04b39bc1fbee29e6f61ef189dd6371d21463de6b`), read from disassembly and not run on a console: `0xbf0` returns `INVALID_ARGUMENT` for a null second or third argument before any lookup; then it looks up the library context named by the top 16 bits of the 32-bit first argument (`LIB_CONTEXT_NOT_FOUND`) and the user context (`USER_CONTEXT_NOT_FOUND`), as `findUserContext` does.

## add Functional

- Console behavior is not measured for [sceNpWebApiClearUnusedConnection](../../../core/libs/prx/libSceNpWebApi/Export.cpp): with no HTTP connections modelled it returns 0 after the argument and context checks, and the third argument, a byte, is not read.
