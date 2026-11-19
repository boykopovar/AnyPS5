# libkernel

## add Functional

- On Linux, each [flexible memory](../../../core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp) map is a sealed memfd of its own, mapped shared at the guest address and sized up to a multiple of 64 MiB, so the [GPU imports](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) take a flexible range through udmabuf as they take direct memory, instead of a host-pointer buffer object that amdgpu walks at every submission. The memory is asked for transparent huge pages with `MADV_HUGEPAGE`, which shared memory gets only on request where `shmem_enabled` is `advise`. Unmapping part of a flexible map punches that part out of the file and keeps the rest resolvable; a live import of the punched range keeps the old pages for the GPU until it is released. Plain anonymous `mmap` memory still has no shared backing.
