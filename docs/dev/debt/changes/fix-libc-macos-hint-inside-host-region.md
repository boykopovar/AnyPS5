# libc

## add Functional

- [Mapping hints inside a host region](../../../core/libs/prx/libc/src/GuestArena.cpp) (libc) - a mapping without `MAP_FIXED` whose hint lies in a host region of the guest arena, such as the range macOS keeps unmappable up to 0x70_0000_0000, is placed in the highest free range above the hint instead of the first one. The console has no such region and would place it at the hint. The first free range above the macOS region begins at 0x70_0000_0000, where Savage Wind (PPSA90212) later reserves 0x320000000 bytes with `MAP_FIXED` after its heap was given a hint of 0x20_0000_0000. Hints in the system reserved range still take the first free range above them
