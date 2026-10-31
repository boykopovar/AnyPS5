# libSceSystemLogger technical debt

### Silent stubs

- [sceSystemLoggerSetLogData](../../../core/libs/prx/libSceSystemLogger/Export.cpp) (libSceSystemLogger) - returns 0 when initialized without recording or dispatching telemetry data.

### Functional

- [sceSystemLoggerInitialize, sceSystemLoggerTerminate](../../../core/libs/prx/libSceSystemLogger/Export.cpp) (libSceSystemLogger) - Initialize marks the logger initialized and Terminate clears it.

### Unknown function info

- [sceSystemLoggerInitialize, sceSystemLoggerSetLogData and sceSystemLoggerTerminate](../../../core/libs/prx/libSceSystemLogger/Export.cpp) (libSceSystemLogger) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemLogger.c), but no credible prototypes were found; callers must not call them until signatures are verified.
