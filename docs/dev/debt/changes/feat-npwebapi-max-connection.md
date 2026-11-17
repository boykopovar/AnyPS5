# libSceNpWebApi

## remove | `sceNpWebApiClearAllUnusedConnection` | `KQIkDGf80PQ` |

## remove | `sceNpWebApiSetMaxConnection` | `gRiilVCvfAI` |

## add Unknown function info

- [sceNpWebApiSetMaxConnection and sceNpWebApiClearAllUnusedConnection](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - follow firmware 13.42's checks ([reading](https://github.com/boykopovar/AnyPS5/issues/21844#issuecomment-6939271372), disassembly only, not run on a console). `sceNpWebApiSetMaxConnection(int32_t libCtxId, int32_t max)` returns `0x80552902` for `max` above 16, with no lower bound, then `0x80552904` for an unknown library context, and 0. The console keeps `min(max, 16)` as a byte in the library context; nothing here opens connections, so the limit isn't kept. `sceNpWebApiClearAllUnusedConnection(int32_t userCtxId, bool flag)` returns the user-context lookup's `0x80552904` or `0x80552905`, and otherwise 0, the console's answer for a context with no connections, as every one has here. What `flag` selects wasn't read
