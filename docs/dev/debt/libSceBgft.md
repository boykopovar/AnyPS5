# libSceBgft technical debt

### Unknown function info

- [sceBgftServiceDownloadGetProgress](../../../core/libs/prx/libSceBgft/Export.cpp), `sceBgftServiceIntDownloadGetProgress`, `sceBgftServiceIntDownloadGetGameAndGameAcProgress` and `sceBgftServiceIntDownloadGetPlayGoProgress`: signatures and the shared 88-byte output are confirmed. The library forwards the output through IPC without accessing individual fields; their types remain unresolved.
- [sceBgftServiceIntDownloadRegisterTaskByStorageEx](../../../core/libs/prx/libSceBgft/Export.cpp): confirmed parameter pointer and 32-bit task ID output. The input contains 32-bit scalars at offsets 0, 4, 56 and 104, strings at offsets 8, 16, 32, 40, 48, 64, 72, 80 and 88, and a 64-bit scalar at offset 96. The typed layout preserves the unused eight bytes at offset 24; scalar meanings and several string roles remain unresolved.
- [sceBgftServiceIntDownloadGetPatchProgress](../../../core/libs/prx/libSceBgft/Export.cpp) and `sceBgftServiceIntDownloadGetTaskInfo`: neither export nor a caller was found in the supplied decompiled library corpus. Their signatures remain unresolved.
