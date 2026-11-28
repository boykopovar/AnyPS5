# libSceUlt

## change [Status query exports]

- [Status query exports](../../../core/libs/prx/libSceUlt/Export.cpp) fill waiter records for mutexes, semaphores, condition variables, queues and reader-writer locks: each recorded waiter reports the host thread block's first 8 bytes, type 1, and a wait value of 0 for a blocked lock, acquire or condition wait, 1 for a queue pop or a read lock and 2 for a queue push or a write lock; a query writes at most the supplied capacity, in push then pop order for a queue, and still validates every supplied record size first. A waiter running inside an ulthread has no recorded identity yet, so it is counted but not written. `sceUltGetUlthreadRuntimeInfo` succeeds when `numWorkerThread` is 0 and throws for a non-empty worker array when nonzero because no workers start. Layouts, wait records, worker scheduling and priorities lack console validation.

## change [sceUltGetUlthreadInfo and sceUltGetUlthreadRuntimeInfo]

- [sceUltGetUlthreadInfo and sceUltGetUlthreadRuntimeInfo](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - return `ULT_ERROR_NULL` when destination pointers are inaccessible via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, preserving guest `errno`; `sceUltGetUlthreadRuntimeInfo` stages active ulthreads and restores `info` if copying the array fails. Hardware fault signaling on unmapped memory is unverified.
