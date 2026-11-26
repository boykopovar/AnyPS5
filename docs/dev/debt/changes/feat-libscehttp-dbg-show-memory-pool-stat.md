# libSceHttp

## remove [sceHttpDbgShowMemoryPoolStat]

## add Functional

- [sceHttpDbgShowMemoryPoolStat](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - follows firmware 4.03's `libSceHttp.sprx` (SHA-256 `7e78e4b2de9d5d708b52c68585ad7dda58c2e3b0be9c5793c0942ce45b34d8d4`), read from disassembly to the return and not measured on a console; the other entries of this file follow later firmware, and whether the function changed between them is unknown. It takes no arguments and returns nothing: after `printf` the module reloads the stack guard into `rax`, so no result is defined. It calls `sceHttpGetMemoryPoolStats(0, &stats)` without looking at the result, prints the line ` current      max` with `puts`, then the used and the peak size as `%8x %8x` with `printf`, each truncated to 32 bits. The module leaves its local statistics uninitialised, so when context 0 is not live the console prints whatever the stack held; this build zeroes them and prints 0 and 0. Context ids here are handed out from 1 upwards, so context 0 is only live after the shared handle counter wraps; whether context 0 can be live on a console is unknown.
