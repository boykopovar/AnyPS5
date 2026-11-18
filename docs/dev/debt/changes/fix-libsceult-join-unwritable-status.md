# libSceUlt

## add Functional

- [sceUltUlthreadJoin and sceUltUlthreadTryJoin](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - after the join succeeds they unregister the ulthread, then copy the exit status through `GuestCopyOut_nid_no_patch` and return `ULT_ERROR_NULL` with guest errno kept when `status` is unwritable; the join is not undone. That answer is a host-safety fallback: what the console returns for a status pointer on unmapped memory is unmeasured.
