# libSceNpWebApi

## remove `KQIkDGf80PQ`

## add Functional

- [sceNpWebApiClearAllUnusedConnection](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - follows firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21844#issuecomment-6939271372), disassembly only, not run on a console): `(int32_t userCtxId, bool flag)` returns `LIB_CONTEXT_NOT_FOUND` (`0x80552904`) for an unknown library context `userCtxId >> 16`, then `USER_CONTEXT_NOT_FOUND` (`0x80552905`), then the result of clearing each connection in the user context's list, which is 0 for an empty list. HTTP connections are not modelled, so every list is empty and it returns 0. What `flag` selects in the per-connection clear wasn't read.
