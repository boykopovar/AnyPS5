# libScePerf

## change [scePerfTraceIoControllerEnable]

- [scePerfTraceIoControllerEnable](../../../core/libs/prx/libScePerf/Export.cpp) (libScePerf) - unknown signature (NID +AX8QoImHeQ). The module cannot be reached on a real console, so nothing is measured about any name in this library: a PS5 payload process (11.60; four `dlopen` attempts of libScePerf.sprx in three runs, `dlerror` reporting only the code `0xffffffff`) and a PS5 Pro payload process (13.60, `kernel_get_fw_version` = `0x13600007`; NULL from `dlopen` of libScePerf.sprx and libScePerf.prx, and `sceSysmoduleLoadModule` answers `0x805A1000` - `SCE_SYSMODULE_ERROR_INVALID_VALUE` - for both ids our module table carries, 0x80000019 and 0x5) never got a handle. A title process may differ.
