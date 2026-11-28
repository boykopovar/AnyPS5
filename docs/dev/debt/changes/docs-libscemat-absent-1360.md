# libSceMat

## change [sceMatMemoryPoolReserve]

- [sceMatMemoryPoolReserve](../../../core/libs/prx/libSceMat/Export.cpp) (libSceMat) - unknown signature (NID +v5r0-k24j0). The module cannot be reached on a real console, so nothing is measured about any name in this library: a PS5 payload process (11.60; two runs of two attempts each) returned NULL from `dlopen` of libSceMat.sprx, and a PS5 Pro payload process (13.60, `kernel_get_fw_version` = `0x13600007`) got NULL from `dlopen` of both libSceMat.sprx and libSceMat.prx before and after `sceSysmoduleLoadModule(0x80000048)` - that call answers `0x805A1000` (`SCE_SYSMODULE_ERROR_INVALID_VALUE`), so the id in our module table is not loadable on 13.60 either. A title process may differ.
