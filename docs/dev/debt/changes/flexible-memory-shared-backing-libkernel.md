# libkernel

## add Functional

- On Linux, each [flexible memory](../../../core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp) map is its own sealed memfd, mapped shared from the file offset equal to its address modulo 2 MiB and sized to a multiple of 64 MiB, so [GPU imports](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) take it through udmabuf like direct memory instead of a host-pointer buffer object. It is marked `MADV_HUGEPAGE`, since shared memory gets huge pages only on request where `shmem_enabled` is `advise`. Unmapping part of a map punches that part out of the file; a live import keeps the old pages until released, and a failed punch leaves them until the file closes. If creating, sizing or mapping the memfd fails (for example at the descriptor limit), the map stays anonymous, as before. Plain anonymous `mmap` memory still has no shared backing.
