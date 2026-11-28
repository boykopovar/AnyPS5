# libSceUlt

## add Functional

- [sceUltGetConditionVariableInfo, sceUltGetMutexInfo, sceUltGetQueueInfo, sceUltGetSemaphoreInfo](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - preserve caller `errno` across pointer copies and restore the previous `info` destination struct if copying out waiting thread records fails with `ULT_ERROR_NULL`; returning `ULT_ERROR_NULL` and atomically preserving the info struct on waiter copy failure is a host-safety fallback with console behavior unmeasured.
