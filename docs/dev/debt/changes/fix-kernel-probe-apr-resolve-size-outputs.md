# libkernel

## add Functional

- [sceKernelAprResolveFilepathsToIdsAndFileSizes and sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes guest outputs](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - each id, size and the error index are resolved into locals and written with GuestCopyOut_nid_no_patch in the same order as before; an unwritable output fails with `EFAULT`, as `sceKernelAprGetFileSize` and `sceKernelAprGetFileStat` do, a host-safety fallback with console behaviour unmeasured. The path array and strings are still read in place.
