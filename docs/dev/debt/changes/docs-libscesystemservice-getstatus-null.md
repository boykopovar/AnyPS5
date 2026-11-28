# libSceSystemService

## change [sceSystemServiceGetStatus guest output]

- [sceSystemServiceGetStatus guest output](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - returning SYSTEM_SERVICE_ERROR_PARAMETER for a null or unwritable output pointer is a host-safety fallback. A PS5 (firmware 11.60, payload process; a title process may differ) answered 0 for a valid 256-byte buffer, and the call with a NULL output pointer ended in a fault signal (SIGSEGV or SIGBUS, the probe did not record which; one run); what the console does for an unmapped non-null pointer is unmeasured.
