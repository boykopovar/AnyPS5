# libkernel

## add Unknown function info

- [APR resolve guest pointers](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - the eight `sceKernelAprResolveFilepaths*` forms read the `paths` table one element at a time, and each path and the prefix with `CopyGuestPath_nid_no_patch`, then write `ids`, `sizes`, `results` and `error_index` through checked copies. An unreadable table element, path or prefix, or an unwritable output, fails with `EFAULT` at that index, after the earlier indices were written. A path or prefix that does not fit the 1024-byte path limit resolves to nothing, like a missing file. This is a host-safety fallback: what the console does for these pointers is not measured.
