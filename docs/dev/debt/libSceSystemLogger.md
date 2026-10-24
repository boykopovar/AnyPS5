# libSceSystemLogger technical debt

### Silent stubs

- [sceSystemLoggerInitialize and sceSystemLoggerTerminate](../../../core/libs/prx/libSceSystemLogger/Export.cpp) (libSceSystemLogger) - return 0 without contacting a log service: the host runs no system logger daemon, so there is no channel to open or close. The declarations have no parameters solely to resolve known imports; the signatures are unverified host-emulation fallback assumptions, not measured console behaviour

### Unknown function info

- [sceSystemLoggerSetLogData](../../../core/libs/prx/libSceSystemLogger/Export.cpp) (libSceSystemLogger) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemLogger.c), but no credible prototypes were found. The declaration has no parameters solely to resolve known imports; this is not an ABI claim and callers must not call it until signatures are verified
