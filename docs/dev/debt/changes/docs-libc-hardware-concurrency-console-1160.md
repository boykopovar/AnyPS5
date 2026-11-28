# libc

## change [std::thread::hardware_concurrency]

- [std::thread::hardware_concurrency](../../../core/libs/prx/libc/src/Threads.cpp) (libc, NID `CHrhwd8QSBs`) - the starting mask of a title's process, and so the value a title sees, are not measured, and neither is whether the console's count follows a process-wide `cpuset_setaffinity`. This returns 13, the number of CPUs in libkernel's default thread mask (`0x1FFF`), and a process-wide `cpuset_setaffinity` does not change it. In payload processes whose threads started with mask `0xea00` it returned 5 on PS5 firmware 13.60 and 12.70, while `hw.ncpu` read 16 and `sceKernelGetAvailableCpumask` `0x1fff` (both on 13.60). On 11.60 it also returned 5, but the mask could not be read there. On 12.70 it still returned 5 when called first after `scePthreadSetaffinity` had narrowed the calling thread to one CPU, after the mask was restored, and in a thread created with a two-CPU mask. PPSA34015 imports it.
