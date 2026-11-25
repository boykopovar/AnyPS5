# libkernel

## change [sceKernelDirectMemoryQuery]

- [sceKernelDirectMemoryQuery](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - follows a payload process on firmware 13.00: flags 2, 3, 4, 5, 8, 0x10 to 0x200, 0x1000 and 0x10000 return the block as flags 0 do (only bit 0, find-next, changes the result); bit 30 returns EINVAL and writes nothing; where no block is found, flags 0, 2, 4 and 8 return EACCES. A size of 0 returns 0 and writes nothing; 16 writes 16 bytes and 32 writes the 24-byte record. On system software 12.40 [obSCEne measured](https://github.com/project-oops/obSCEne/blob/de5efc406319ade18df00654025494804405a7fb/reports/hardware/report-20261013-172400.log#L11881-L11947) EINVAL for flags 2 and 4, so the answer depends on the firmware, which is not modeled. Bit 31 returned 0 and wrote another record (its first field 0x200000056, then 0x200000057), so it throws, as does a size of 0 where no block is found. Not measured for PS4 titles.
