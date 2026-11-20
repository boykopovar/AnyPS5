# libScePerf

## change [scePerfTraceIoControllerEnable]
- [scePerfTraceIoControllerEnable](../../../core/libs/prx/libScePerf/Export.cpp) (libScePerf) - unknown signature (NID +AX8QoImHeQ). A PS5 (firmware 11.60, payload process; a title process may differ) did not load the module: `dlopen` of libScePerf.sprx returned NULL in four attempts in three runs; in the last run `dlerror` was read after each attempt and reported only the code `0xffffffff`, so nothing was measured about the names of the module.
