# libkernel

## change on Windows it is assumed to stop before the terminator

- [sceKernelGetModuleInfoForUnwind](../../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - the terminator-inclusive `eh_frame_size` follows the firmware 13.60 measurements reported in [#2419](https://github.com/boykopovar/AnyPS5/issues/2419), not reproduced here. Windows reads relinked `.ehmeta` tables, Linux uses `sceKernelGetModuleInfoFromAddr`, and macOS uses the relinked image's program headers. No tables are reported for ordinary host images. The other module selection, name and segment differences remain listed separately; this size correction has no new title-run result.
