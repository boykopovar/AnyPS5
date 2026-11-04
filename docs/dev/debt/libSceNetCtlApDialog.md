# libSceNetCtlApDialog technical debt

### Silent stubs

- [libSceNetCtlApDialog](../../../core/libs/prx/libSceNetCtlApDialog/Export.cpp) - the access-point dialog shows no UI and reports user cancellation without bringing up a connection. Parameter size, reserved-byte checks, option-string limit and lifecycle errors are recovered from the supplied module. The meaning of the input word at offset 56 and service-side connection behavior remain unresolved.
