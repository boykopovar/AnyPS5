# libSceNpManager

## add Unknown function info

- [sceNpPush calls before initialization](../../../core/libs/prx/libSceNpManager/PendingExports.cpp) (libSceNpManager) - 14 `sceNpPush` exports and two unnamed ones (`QZJ5fC0tP8M`, `-5ZuQ1HlZNY`) return `0x8055A002` while `sceNpPushInit` has not run (or `sceNpPushTerm` has), whatever their arguments, and throw otherwise. Firmware 13.42's `libSceNpManager.sprx` reads its push context pointer `0x62b98` first and returns that code while it is null; only `sceNpPushInit` and `sceNpManagerPrxStartVsh` create the context. `sceNpManagerPrxStartVsh` still throws here; if it is implemented, these must follow the context it creates too. Read from its disassembly and run under an emulator, not measured on a console. What they do once initialized, and their signatures, are unknown
