# libSceHttp

## change [sceHttpDbgShowStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp)

- [sceHttpDbgShowStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the native diagnostic consumes no arguments; it prints library state. A PS5 Pro payload process (13.60) printed `**** libhttp debug information ****` with a MEMORY POOL table and the active-connection and current-request counts, and returned a value that was identical across all four `sceHttpDbgShow*Stat` calls of each run but differed between runs (`0x86e22b39`, `0x4bce3005`, `0x3d87fc21`) - the return is undefined, not a status. The throwing implementation remains unchanged.

## change [sceHttpDbgShowConnectionStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp)

- [sceHttpDbgShowConnectionStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the native diagnostic consumes no arguments; it prints library state: a PS5 Pro payload process (13.60) printed only the active-connection count line, and returned the same undefined per-run value as `sceHttpDbgShowStat`. The throwing implementation remains unchanged.

## change [sceHttpDbgShowRequestStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp)

- [sceHttpDbgShowRequestStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the native diagnostic consumes no arguments; it prints library state: a PS5 Pro payload process (13.60) printed only the current-request count line, and returned the same undefined per-run value as `sceHttpDbgShowStat`. The throwing implementation remains unchanged.

## change [sceHttpDbgShowMemoryPoolStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp)

- [sceHttpDbgShowMemoryPoolStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the native diagnostic consumes no arguments; it prints library state: a PS5 Pro payload process (13.60) printed only the MEMORY POOL current/max table, and returned the same undefined per-run value as `sceHttpDbgShowStat`. The throwing implementation remains unchanged.

## add Functional

- [sceHttpDbgGetConnectionStat, sceHttpDbgGetRequestStat, sceHttpRequestGetAllHeaders and sceHttpAddRequestHeaderRaw](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - an unknown id answers before any output is written: on a PS5 Pro payload process (13.60), unloaded library, `sceHttpDbgGetConnectionStat(-1, ·)` and `sceHttpDbgGetRequestStat(-1, ·)` returned `0x80431100` with the sentinel-filled stat buffer untouched, `sceHttpRequestGetAllHeaders(0, ·)` returned `0x80431100` leaving the caller's count and content-length buffers untouched, and `sceHttpAddRequestHeaderRaw(0, "A: b", 0)` - a header string the call cannot be blamed for - returned `0x804311FE`, so that export validates ids as values rather than handles. Valid-id behavior is unmeasured; the exports throw.
- [sceHttpAuthCacheImport and sceHttpAuthCacheExport](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - calling either on a PS5 Pro payload process (13.60) with context id 0 before `sceHttpInit` never returned: the probe stopped at `sceHttpAuthCacheExport(0, buffer, 64, &size)` and no later line or timeout output followed, in two sessions; treat these exports as blocking until initialization semantics are known. They throw here.
