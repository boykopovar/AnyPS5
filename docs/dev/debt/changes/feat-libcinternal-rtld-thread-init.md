# libSceLibcInternal

## add Additional callable exports

- [_rtld_thread_init](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NID `gRw4XrztJ4Q`) follows the weak fallback in FreeBSD 11.4 [dlfcn.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/dlfcn.c#L154-L160): it takes the lock-information pointer, neither reads nor calls it, and passes "Service unavailable" to `_rtld_error`, which does nothing. The other fallbacks of that file are the ones the sibling `_rtld_atfork_pre`, `_rtld_atfork_post`, `_rtld_error`, `_rtld_get_stack_prot` and `_rtld_addr_phdr` exports reproduce, but this body was not read from `libSceLibcInternal.sprx` or run on a console, and no title was seen calling it
