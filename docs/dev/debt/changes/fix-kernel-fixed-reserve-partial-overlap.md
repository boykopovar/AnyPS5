# libkernel

## add Functional

- [Fixed reservations over a partly mapped range](../../../core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp) (libkernel) - `sceKernelReserveVirtualRange` and `sceKernelMemoryPoolReserve` with `MAP_FIXED`, without no-overwrite, over a range that is partly mapped and partly free replace the mapped parts, following FreeBSD 11.4 [vm_map_fixed](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/vm/vm_map.c#L1443-L1467), as shadPS4 does; not measured on a console. A range that also holds a host region fails with `ENOMEM`, and one that holds registered image memory is refused as before (an exception; `EINVAL` from the pool call); both keep their mappings. `sceKernelMemoryPoolReserve` leaves the flexible memory it replaces charged to the flexible budget until the range is unmapped. Closes when: measured on a PS5 and the budget is returned
