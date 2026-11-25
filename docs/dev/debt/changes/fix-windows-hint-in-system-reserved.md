# libc

## add Inventory exports declared elsewhere

- [Guest arena system reserved range](../../../core/libs/prx/libc/src/GuestArena.cpp) (libc) - how the console uses 0x7_FFFF_C000 to 0x10_0000_0000 is not measured. On Windows, a mapping whose address hint lies in that range is placed at or above the hint inside it, as ProsperoCraft's Java heap (PPSA99874) needs; mappings without a hint, hints below the range and fixed mappings are kept out of it. Linux and macOS still keep the whole range out of guest mappings, so the same heap reservation gets 0x10_0000_0000 there.
