# libkernel

## add Functional

- [sceKernelLoadStartModule and sceKernelStopUnloadModule results](../../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - `res` is written through a checked copy. An unwritable `res` returns `EFAULT` (0x8002000e): `sceKernelLoadStartModule` before loading, and `sceKernelStopUnloadModule` after its argument checks and before unloading, so the module stays loaded. The console's result and its order against the other errors are not measured.
