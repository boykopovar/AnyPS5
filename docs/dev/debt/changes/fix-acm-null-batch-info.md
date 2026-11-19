# libSceAcm

## change sceAcmBatchStartBuffers requires batch_error to be null

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) - in Firmware 13.42, sceAcmBatchStartBuffers requires batch_info to be non-null and batch_error to be null, returning SCE_ACM_ERROR_BAD_POINTER otherwise, in one test with the null batch check and before the context (sceAcmBatchStartBuffer has no checks of its own and passes batch_error unchanged to the same combined test), and stores directly through context and batch pointers after null checks (read from disassembly, not measured on a console), so the console faults on an inaccessible non-null pointer; AnyPS5 returns SCE_ACM_ERROR_BAD_POINTER instead.
