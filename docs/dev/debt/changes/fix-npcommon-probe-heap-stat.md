# libSceNpCommon

## add Unknown function info

- [sceNpHeapGetStat guest pointers](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) - the heap is copied in and the statistics are written with checked guest copies, with `errno` preserved around each copy. An unreadable heap or unwritable statistics output returns `0x80558009` (invalid argument), as `sceNpThreadGetId` does for its output, instead of faulting the host. The console's result for unmapped pointers was not measured.
