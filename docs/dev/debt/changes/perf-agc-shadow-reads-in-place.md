# libSceAgcDriver

## add Functional

- [Unit-shadow reads in place](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) (libSceAgcDriver) - unverified on the console: a read-only address-based build (no stores through addresses, no descriptor writes, not a space-reuse candidate) maps live unit-shadow units read-only to their slab in its BDA table instead of publishing them into the host import. It reads the slab when it executes, so a retile recorded between `UploadFinish` and the draw is visible to the draw. A CPU store into such a unit in that window is not seen, as with the old publish, which copied the slab over it. Sparse-mirror heaps and copied regions inside mapped heaps are overlaid like any table entry; clipping and table replacement for copied regions inside mapped heaps are verified in tests. `APS5_NO_SHADOW_READS=1` restores the publish.
