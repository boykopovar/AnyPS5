# libkernel

## change [sceKernelSetVirtualRangeName]

- [sceKernelSetVirtualRangeName](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - the range need not be mapped, page aligned or free of other mappings: a PS5 payload process (firmware 13.590.001) named and cleared ranges inside a hole, across one, from an unaligned start and 1 MiB past a mapping, all returning 0. `EINVAL` (0x80020016) for a start below 0x4000 (0x3000 fails; 0x4000 and 0xffff pass), a start of 2^47 or above, and a range ending above 2^47 (exactly 2^47 passes), such as 0x1000, 0x7ffffffff000 with 0x4000 bytes or 0xffff800000000000. A length of 0 checks only the start: 0x20000 returns 0, while 0x1000, 0 and 2^47 return `EINVAL`. `sceKernelClearVirtualRangeName` has the same limits. On firmware 11.60 (payload, 2026-11-07) a name of 32 or more bytes returned `ENAMETOOLONG` and kept the earlier name; not re-measured here. Named-mapping calls still cut a longer name to 31 characters.
