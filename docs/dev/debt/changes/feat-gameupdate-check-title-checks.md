# libSceGameUpdate

## change [sceGameUpdateCheckTitle]

- [sceGameUpdateCheckTitle](../../../core/libs/prx/libSceGameUpdate/Export.cpp) - `(int requestId, int title, GameUpdateCheckResult* result)` makes firmware 13.42's checks in order ([reading](https://github.com/boykopovar/AnyPS5/issues/21871#issuecomment-6942828554), disassembly only, not run on a console): `REQUEST_NOT_FOUND` for an id of 0 or less, `INVALID_ARGUMENT` for a `title` of -1 or a null `result`, `INVALID_SIZE` unless `result`'s size is `0x30`, then `NOT_INITIALIZED` and `REQUEST_NOT_FOUND` for an unknown request. Past those the console asks its update service with `title`, which isn't implemented, so a call that passes them throws; an unreadable `result` gives `INVALID_ARGUMENT`, a host-safety fallback. What `title` means isn't settled by the module
