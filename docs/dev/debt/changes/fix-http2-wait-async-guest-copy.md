# libSceHttp2

## change [sceHttp2WaitAsync]

- [sceHttp2WaitAsync](../../../core/libs/prx/libSceHttp2/Export.cpp) (libSceHttp2) - copies the queued async result to `result` through `GuestCopyOut_nid_no_patch` while preserving `errno`, returning `0x817B1225` (`ERROR_NULL_POINTER`) without dequeuing the completion if the output pointer is unwritable; whether the console returns an error or faults on unmapped memory is unmeasured.
