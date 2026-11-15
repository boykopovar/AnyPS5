# libc

## add Functional

- [fcloseall](../../../core/libs/prx/libc/src/FileAndHeap.cpp) has no console signature, return value or edge-case measurement; only the name is established, by the public PS5 3.20 libSceLibcInternal inventory. It follows FreeBSD 11.4's `void` `__fcloseall`/`_fwalk(fclose)`: close errors are ignored and streams opened during the call are left open. Preserving the host stdout or stderr behind a guest stream takes a host descriptor, so host limits apply, and a failure throws. Concurrent ordinary I/O and stream destruction is unsupported.
