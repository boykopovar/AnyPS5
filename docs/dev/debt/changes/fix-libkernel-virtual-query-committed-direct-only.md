# libkernel

## change [sceKernelVirtualQuery size and flags]

- [sceKernelVirtualQuery size and flags](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - `is_committed` (1 only for direct memory; pooled memory was not measured), size, flags and addresses follow a PS5 payload (firmware 13.590.001): flags other than 0 and 1 are `EINVAL`, then an address from 0x800000004000 up is `EINVAL`, then an unmapped one `EACCES`, then `min(info_size, 72)` bytes are copied out (none for 0, and a null `info` of size 0 succeeds), an unwritable `info` being `EFAULT`. Not measured: a title process, and the names the console gives unnamed ranges (`anon:000`, `(NoName)`), which stay empty here. The upper 16 KiB window at `0x800000000000` returns `EACCES` before the host lookup for flags 0 and 1, matching the flags-0 and flags-1 console probes.
