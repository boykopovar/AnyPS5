# libSceNpCommon

## add Functional

- [sceNpDbgDumpText](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) copies its text in with `GuestCopyIn_nid_no_patch` in chunks that stay inside one 4 KiB page, and prints up to `length` bytes or the first NUL (with no limit for a negative `length`). It stops at the first chunk it cannot read, returns the number of bytes printed (-1 if a write to stdout fails, as `printf` returned a negative value), and prints nothing for a null pointer. This is a host-safety fallback; the console's behaviour for an inaccessible or null text is not measured. Closes when: compared with the console.
