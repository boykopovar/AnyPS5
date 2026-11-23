# libSceNpCommon

## add Functional

- [sceNpDbgDumpBinary](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) copies each 16-byte line in with `GuestCopyIn_nid_no_patch` before printing it, and stops at the first line it cannot read, printing only the final empty line after the lines already dumped. This is a host-safety fallback; the console's behaviour for an inaccessible buffer is not measured. Closes when: compared with the console.
