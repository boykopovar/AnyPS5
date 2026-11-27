# libSceAgcDriver

## add Silent stubs

- [sceAgcDriverAllocateToolMemoryForGpuReset and sceAgcDriverGetAllocatedToolMemoryForGpuReset guest ranges](../../../core/libs/prx/libSceAgcDriver/Unimplemented.cpp) (libSceAgcDriver) - the 12-byte range is cleared through the guest copy helper with `errno` preserved; an unwritable range is left alone and the call still returns `0x8A6D0001`, as for a null range, which is a host-safety fallback; whether the console faults on unmapped memory is unmeasured.
