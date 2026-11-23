# libkernel

## add Functional

- [sceKernelDebugWriteCppExceptionInfo](../../../core/libs/prx/libkernel/System/src/Coredump.cpp) (libkernel) copies its type-name and `what()` strings in with `CopyGuestString_nid_no_patch` (64 KiB limit). An unreadable or unterminated type name prints as `(unknown)` and an unreadable or unterminated `what()` string is left out, as a null one already is. This is a host-safety fallback; the console's behaviour for an inaccessible string is not measured. Closes when: compared with the console.
