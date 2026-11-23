# libSceNpWebApi

## remove `KQIkDGf80PQ`

## add Functional

- [sceNpWebApiClearAllUnusedConnection](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - not measured on a console: HTTP connections are not modelled, so after the user context lookup (`LIB_CONTEXT_NOT_FOUND`, `USER_CONTEXT_NOT_FOUND`, as `sceNpWebApiClearUnusedConnection`) there is nothing to clear and it returns 0. The `(int, bool)` prototype is [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/np/np_web_api/np_web_api.cpp)'s, whose body is a stub; the lookup is assumed from the sibling and not read from firmware.
