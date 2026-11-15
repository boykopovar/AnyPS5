# libSceNetCtlApDialog technical debt

### Functional

- [sceNetCtlApDialogOpen](../../../core/libs/prx/libSceNetCtlApDialog/Export.cpp) - returning `0x80B8000D` (`COMMON_DIALOG_ERROR_ARG_NULL`) for an unmapped or unreadable parameter pointer is a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
- [sceNetCtlApDialogGetResult](../../../core/libs/prx/libSceNetCtlApDialog/Export.cpp) - returning `COMMON_DIALOG_ERROR_ARG_NULL` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.

### Silent stubs

- [libSceNetCtlApDialog](../../../core/libs/prx/libSceNetCtlApDialog/Export.cpp) - the access-point dialog shows no UI and reports user cancellation without bringing up a connection. Parameter size, reserved-byte checks, option-string limit and lifecycle errors are recovered from the supplied module. The meaning of the input word at offset 56 and service-side connection behavior remain unresolved.
