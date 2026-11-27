# libSceSystemService

## change [sceSystemServiceParamGetInt guest output]

- [sceSystemServiceParamGetInt guest output](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - returning SYSTEM_SERVICE_ERROR_PARAMETER for an unwritable output pointer is a host-safety fallback. A PS5 (firmware 11.60, payload process; a title process may differ) answered 0 for a valid buffer with each of the ids 1, 2, 3, 4, 5, 100, 103 and 400 (two runs); with the id 2 the call for an unmapped pointer (0x10) ended in a segmentation fault (signal 11, one run, prototype assumed), while a NULL output pointer and a read-only buffer are unmeasured.
