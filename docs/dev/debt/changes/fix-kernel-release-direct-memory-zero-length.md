# libkernel

## change [sceKernelReleaseDirectMemory](

- [sceKernelReleaseDirectMemory](../../../core/libs/prx/libkernel/DirectMemory/MemoryPool.cpp) (libkernel) - releases the allocated pages of the range and skips pages that are not allocated, returning 0 (also for a length of 0 at an aligned start, as a PS5 payload process (firmware 11.60) did on 2026-11-07; a title process, a misaligned start with a length of 0 and a start at the end of direct memory were not measured), while `sceKernelCheckedReleaseDirectMemory` returns `ENOENT` for them, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/c206f35a546230d9c9808b823c0f36f5850cd4af/src/core/memory.cpp#L295-L313) does; not measured on a console. Absolum (PPSA28311) releases such a range when it disposes of its video logos
