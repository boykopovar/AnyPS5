# libkernel

## add Functional

- [mprotect range bounds](../../../core/libs/prx/libkernel/DirectMemory/PosixMemory.cpp) (libkernel) - not measured on a console beyond unmapped ranges: the part of a range below the first page or at or above `0x800000000000` is skipped, so a range at NULL or outside the user map returns 0 and changes nothing, as FreeBSD 11.4 `vm_map_protect` clamps with `VM_MAP_RANGE_CHECK`; a range whose page-rounded end wraps is `EINVAL`. A zero length at an unaligned address returns 0 without protecting its page, and a length whose rounding with the address offset overflows is `EINVAL`, where FreeBSD 11.4 `kern_mprotect` rounds the length plus the offset up to a page without an overflow check, protecting that page or giving `EINVAL` in the last page of the address space
