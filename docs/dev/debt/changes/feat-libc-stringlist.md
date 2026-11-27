# libc

## add Unknown function info

- [sl_init, sl_add, sl_free and sl_find](../../../core/libs/prx/libc/src/StringList.cpp) (libc) - follows FreeBSD 11.4's [`stringlist.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/stringlist.c) at the pinned commit: the list grows by 20 pointers, `sl_free` frees the stored strings only when `all` is nonzero, and `sl_find` compares with `strcmp`. When an allocation fails, `sl_init` prints a message to stderr and exits with status 1, as FreeBSD's `err` does; the console's message text and exit path are not measured.
