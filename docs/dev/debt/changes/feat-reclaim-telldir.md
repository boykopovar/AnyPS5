# libSceLibcInternal

## remove [_reclaim_telldir](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp)

## add Functional

- [_reclaim_telldir](../../../core/libs/prx/libSceLibcInternal/Directory.cpp) (libSceLibcInternal, NID `7ZFy2m9rc5A`) - every `telldir` cookie stays valid after it, while FreeBSD 11.4, which the console is assumed to follow, frees the cookies of positions that do not fit FreeBSD's packed cookie (on LP64 a `getdirentries` seek of 2^32 or more, or a buffer offset of 2^31 or more) are freed and `seekdir` then ignores them. The signature `void _reclaim_telldir(DIR *)` and the behaviour come from FreeBSD 11.4 libc ([telldir.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/telldir.c) `_reclaim_telldir`, [telldir.h](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/telldir.h)); `telldir` here returns the indexed cursor, the descriptor offset or the host `telldir` value and allocates no cookie of its own, so there is nothing to free. A null `DIR` throws, as FreeBSD dereferences it. Closes when: `telldir` issues FreeBSD's packed and list cookies.
