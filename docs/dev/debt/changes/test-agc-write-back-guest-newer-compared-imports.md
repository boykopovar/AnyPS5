# libSceAgcDriver

## add Functional

- [Guest writes after a draw](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp) (libSceAgcDriver) are kept out of a storage image's write-back only while its memory is write-watched. Where host imports are compared, not watched (the `[write-watch]` probe line says so, as on Linux with lavapipe), an imported target's pages are unwatched from the import on, so no pending snapshot is taken and no later guest write is found: the write-back still stores the drawn spans over bytes the guest wrote after the draw. Comparing whole layers against a snapshot would also count GPU stores through the import that are older than the draw. `agc_driver_write_back_guest_newer` skips its after-draw cases there.
