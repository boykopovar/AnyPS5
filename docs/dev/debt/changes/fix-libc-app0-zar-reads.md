# libc

## add Functional

- [app0.zar reads through fopen and access](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) - `fopen` for reading copies the whole archive-only entry into a temporary read-only host file before the first read, so opening a large entry costs its size in time and temporary disk space. A writing mode fails with `EROFS` and an exclusive one with `EEXIST`, as the libkernel `open` does; an archived directory throws, unmeasured on the console. The copy's descriptor counts as the archived entry through `dup` until it is closed, so `fstat` reports the entry's 0444 mode and size and calls libkernel refuses on archived descriptors throw on it; this tracking follows [#7447](https://github.com/boykopovar/AnyPS5/pull/7447). [`access`](../../../core/libs/prx/libc/src/Filesystem.cpp) answers from the 0444 and 0555 modes `sceKernelStat` gives, so `X_OK` on a file fails with `EACCES`, and throws for `W_OK`, which is unmeasured on the console. PPSA21402 opens its music this way.

# libkernel

## add Functional

- [APR reads from app0.zar](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - the APR resolves give archive-only files an id and the entry's size, and reads take them from the archive in 64 KiB pieces, zero-filling past the entry's end as for files on disk; a short archive read before the end fails the command with `EIO`. An archived directory does not resolve, and `sceKernelAprGetFileStat` reports the 0444 mode and size `sceKernelStat` gives. PPSA26344 resolves and reads its 1.5 GiB `all_shaders.xpps` this way. The console reads titles from a PFS image, so none of this is measured there.

# libSceNgs2.native

## add Functional

- [sceNgs2ParseWaveformFile from app0.zar](../../../core/libs/prx/libSceNgs2.native/src/Atrac9.cpp) (libSceNgs2.native) - an archive-only waveform file is read from the archive from `offset` to the entry's end, as a file on disk is; a short archive read throws, and an archived directory throws as a directory on disk does. Balatro (PPSA21402) parses its music this way. Not measured on a console, which reads titles from a PFS image.
