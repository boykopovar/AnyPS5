# libc

## add Functional

- [Guest arena](../../../core/libs/prx/libc/src/GuestArena.cpp) (macOS) - under Rosetta 2, guest addresses 0xF_C000_0000 to 0x70_0000_0000 cannot be mapped: every x86-64 process, `/bin/sleep` included, has a 385 GiB `Guard` region there with maximum protection none, which the arena seeds as a host region. In such a process (Apple M5 Pro, macOS 27.3), `mmap` with `MAP_FIXED` inside it fails with `EACCES`, and `vm_allocate` with `VM_FLAGS_FIXED` fails with `KERN_NO_SPACE`, also after `vm_deallocate` of part of it. A fixed guest mapping there throws "overlaps a host region in the guest arena": GTA V (PPSA04264) maps direct memory with `sceKernelBatchMap2` at 0x15_B140_0000 without reserving it first and stops at startup. Guest pointers are host pointers, so such a mapping cannot be placed elsewhere.
