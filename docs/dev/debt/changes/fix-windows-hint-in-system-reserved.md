# libc

## add Inventory exports declared elsewhere

- [Guest arena system reserved range](../../../core/libs/prx/libc/src/GuestArena.cpp) (libc) - how the console uses 0x7_FFFF_C000 to 0x10_0000_0000 is not measured. On Windows, a mapping whose address hint lies in that range may use it: the arena first looks for free space from the hint upward, so the mapping can start inside the range, and if none fits it falls back to the default search, as for any other hint. ProsperoCraft's Java heap (PPSA99874) needs this. Mappings without a hint, hints below the range and fixed mappings are kept out of it, and so is the AGC driver block at 0xF_E000_0000, which the driver commits when it starts. Linux and macOS still keep the whole range out of guest mappings, so the same heap reservation gets 0x10_0000_0000 there.
