# libSceLibcInternal

## remove [_reclaim_telldir](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp)

## add Functional

- [_reclaim_telldir](../../../core/libs/prx/libSceLibcInternal/Directory.cpp) (libSceLibcInternal, NID `7ZFy2m9rc5A`) - the console module was not read; this takes FreeBSD 11.4's `void _reclaim_telldir(DIR*)` ([telldir.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/telldir.c)), which frees the stream's `ddloc` records. `telldir` here returns raw positions and keeps no records, so it frees nothing and saved positions stay valid for `seekdir`, where FreeBSD forgets them. A null stream sets `EBADF` (9)
