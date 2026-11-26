# libc

## add Functional

- [_Assert](../../../core/libs/prx/libc/src/StdioExtras.cpp) and [sceLibcInternalBacktraceForGame](../../../core/libs/prx/libc/src/HeapDiagnostics.cpp) (libc) copy their message, location and heap-name strings in with `CopyGuestString_nid_no_patch` (64 KiB limit). `_Assert` prints an unreadable or unterminated string as `?`, as it already prints a null one, and `sceLibcInternalBacktraceForGame` prints it as an empty name, as it already prints a null one. This is a host-safety fallback; the console's behaviour for an inaccessible string is not measured. Closes when: compared with the console.
