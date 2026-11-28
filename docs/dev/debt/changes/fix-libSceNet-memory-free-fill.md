# libSceNet

## change [sceNetMemoryAllocate and sceNetMemoryFree]

- [sceNetMemoryAllocate and sceNetMemoryFree](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - allocations use the guest heap instead of the system network pool. Pool limits and its allocation accounting are not reproduced; a freed block is filled with 0xdd, so a reused block allocated without flag 2 holds 0xdd bytes, as on the console. Flag 4 bypasses the system pool reserve threshold; with no pool there is no threshold, so it allocates as other flags do. What the console returns once the pool is exhausted is unmeasured; AnyPS5 returns null without setting an errno. Measured on a PS5 (firmware 13.60): a size of 0 or less returns null and leaves `sceNetErrnoLoc` unchanged, flag 2 zero-fills, flags 1, 4, 8, 0x10, 0x100 and 0x80000000 had no visible effect, and every block of 1 to 65536 bytes was 64-byte aligned
