# libSceAddressSanitizer

## add Functional

- [ASan heap, fake stack and containers](../../../core/libs/prx/libSceAddressSanitizer/src/AsanAllocator.cpp) (libSceAddressSanitizer) - 100 exports follow compiler-rt 16.0.6: the `__asan_malloc` family over the guest heap, the 256 MiB quarantine, heap reports, statistics, `__asan_stack_*` and the [fake stack](../../../core/libs/prx/libSceAddressSanitizer/src/AsanStack.cpp), fibers, and the [container annotations](../../../core/libs/prx/libSceAddressSanitizer/src/AsanContainers.cpp). Not measured on a console: the signatures of the `__asan_malloc` family, `__asan_malloc_stats` and the `exp`/`noabort` mixes, taken from libc's allocator table. Chunk metadata lives on the host, and a recycled block returns to the guest heap unpoisoned. Stacks keep one frame, threads print as `T-1`, and only threads that called into the runtime are searched for stack addresses.
