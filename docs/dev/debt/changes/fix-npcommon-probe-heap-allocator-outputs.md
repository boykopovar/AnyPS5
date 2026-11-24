# libSceNpCommon

## add Unknown function info

- [Heap allocator getters](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) - `sceNpHeapGetAllocator`, `sceNpMemoryHeapGetAllocator`, `sceNpMemoryHeapGetAllocatorEx`, `sceNpGlobalHeapGetAllocator` and `sceNpGlobalHeapGetAllocatorEx` build the allocator on the host and write it with one checked guest write. These functions return nothing, so for an output pointing at unmapped memory (or null) nothing is written and `errno` is left alone instead of faulting the host. The console's handling of such pointers was not measured.
