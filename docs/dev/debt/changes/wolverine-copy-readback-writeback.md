# libSceAgcDriver

## add Functional

- [Copy write-back through a GPU readback](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) - a writable copy region of at least `APS5_COPY_READBACK_MIN_KIB` (64 KiB) is GPU-copied at completion into a staging buffer (`copyReadbacks`) that write-back diffs instead of the copy buffer, so a large descriptor write-back does not read write-combined memory at ~10 MiB/s. The staging uses default `HOST_VISIBLE|HOST_COHERENT` properties: `Context::BufferMemoryType` prefers cached GTT but falls back to the non-cached VRAM aperture when that heap's budget is exhausted, so a high `APS5_HOST_IMPORT_MIB` that fills GTT makes the diff slow again (Wolverine PPSA03671 stalled at frame ~490 at 6000 MiB, fast at 5000). Forcing cached staging or reserving GTT headroom is unimplemented. Console equivalence unmeasured.
