# libSceAgcDriver

## add Silent stubs

- [sceAgcDriverGetCoredumpMode guest pointer](../../../core/libs/prx/libSceAgcDriver/Unimplemented.cpp) (libSceAgcDriver) - the coredump mode (always 0) is written through the guest copy helper; returning `0x8A6D0003`, the null-output error, for an unwritable pointer is a host-safety fallback, and whether the console faults or returns an argument error on unmapped memory is unmeasured.
