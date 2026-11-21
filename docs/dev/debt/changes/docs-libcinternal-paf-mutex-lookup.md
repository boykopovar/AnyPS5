# libSceLibcInternal

## change [sceLibcPafMspaceMallocUsableSize]

- [sceLibcPafMspaceMallocUsableSize](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6JcY5RDA4jY`) - delegates to `sceLibcPafMspaceMallocUsableSize_nid_postfix` in libc, matching the other PAF mspace forwarding entry points; console behavior is unverified.

## change [sceLibcInternalMemoryMutexEnable]

- [sceLibcInternalMemoryMutexEnable](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `h8jq9ee4h5c`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`. On a PS5 (firmware 11.60, payload process; a title process may differ) the name resolved by `dlsym` in libSceLibcInternal.sprx; nothing was called, so the signature and behavior stay unverified.

# libc

## change [sceLibcPafMspaceCreate, Destroy, Malloc, Memalign, PosixMemalign, Free, Calloc, Realloc, Reallocalign, MallocStats and MallocStatsFast]

- [sceLibcPafMspaceCreate, Destroy, Malloc, Memalign, PosixMemalign, Free, Calloc, Realloc, Reallocalign, MallocStats and MallocStatsFast](../../../core/libs/prx/libc/src/Mspace.cpp) - PAF exports delegate to `sceLibcMspace` without malloc hooks; `Memalign` and `PosixMemalign` forward to their `Mspace` counterparts with owner ignored and are unverified on console. `Create`'s options block (version, type 1 or 2 at +2, field at +0x10) throws when non-null as its meaning is unknown. Block footers are not modelled, so owners are ignored: on PS5 firmware 13.60 mismatched owners in `Reallocalign` or in-place `Realloc` kill the process and `MallocStats` returns 1 if non-heap-owned blocks exist, which is not reproduced. `Trim`, `IsHeapEmpty`, `CheckMemoryBounds`, `ReportMemoryBlocks` and `GetFooterValue` are not implemented.
