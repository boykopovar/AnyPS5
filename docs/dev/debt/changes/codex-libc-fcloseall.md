# libc

## add Functional

- [fcloseall](../../../core/libs/prx/libc/src/FileAndHeap.cpp) has no console signature, return value or edge-case measurement; only the name is established, by the public PS5 3.20 libSceLibcInternal inventory. It follows FreeBSD 11.4's `void` `__fcloseall`/`_fwalk(fclose)`: close errors are ignored and streams opened during the call are left open. It closes the guest stdin, stdout and stderr streams too. Before closing stdout and stderr it keeps a host descriptor for each so host logging continues; stdin keeps none. Keeping one takes a host descriptor, so host limits apply; when host descriptor 1 or 2 is already closed, or on Windows when the copy fails, nothing is kept and the stream is closed anyway, as in `freopen`. Any other failure to keep one on Linux throws. Concurrent ordinary I/O and stream destruction is unsupported.
- [fclose](../../../core/libs/prx/libc/src/FileAndHeap.cpp) of a null pointer returns -1 with errno 9, like any pointer that is not a registered stream, instead of throwing "null stream"; the console result for a null pointer is unmeasured.

## change _Files and fcloseall

- [_Files](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc, NID `qG50MWOiS-Q`, exported at `0x1a0650` by the `libc.prx` of PPSA01341 v01.007.000) is not implemented. It is that module's array of stream pointers and its length is unmeasured; a table shorter than the console's would let a guest that indexes it read past the end, which is worse than a missing export that fails to resolve. Implementing it needs the console table's length and layout; the stream registry that `fcloseall` walks is not that table.
