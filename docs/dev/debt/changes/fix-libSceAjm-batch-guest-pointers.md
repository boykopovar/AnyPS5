# libSceAjm.native

## add Functional

- [sceAjmBatchStart, sceAjmBatchStartBuffer and sceAjmBatchWait guest pointers](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - reading `AjmBatchInfo`, packed batch jobs and console batch chunks via `GuestCopyIn_nid_no_patch` and writing `AjmBatchError` and batch IDs via `GuestCopyOut_nid_no_patch` with caller `errno` preserved, returning `SCE_AJM_ERROR_INVALID_PARAMETER` for unreadable or unwritable non-null guest pointers without registering or retiring the batch, is a host-safety fallback; whether the console faults or returns an error on unmapped memory is unmeasured.
