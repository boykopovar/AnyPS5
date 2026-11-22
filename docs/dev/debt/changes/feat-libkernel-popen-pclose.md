# libkernel

## add Functional

- [popen and pclose](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - throw `NotImplemented`: AnyPS5 runs no shell for guest code, and the console's process model for `popen` is unverified. `system` keeps its existing result.
