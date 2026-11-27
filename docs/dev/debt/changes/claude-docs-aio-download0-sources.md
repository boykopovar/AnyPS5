# libkernel

## change [AIO request ownership]

- [AIO request ownership](../../../core/libs/prx/libkernel/Aio/src/Aio.cpp) - cancelling or deleting a processing request throws: the console's answer and native cancellation guarantees are unverified, and the Lapse exploit's decompiled [`_aio_multi_delete`](https://github.com/Al-Azif/psfree-lapse/blob/b6bccb39a9e5a1a2d64e99c0d9404aad523fa4f0/src/lapse.mjs#L558-L604) does not show that path. Requests run inside their submit, so only an id whose submit has not returned is processing. The registry holds at most 511 undeleted submissions, reserves a Multiple batch together and uses positive identities without reuse; exhaustion throws. A thrown transfer releases its unpublished reservations without undoing earlier transfers or result stores. Console capacity, identity allocation and exhaustion are unmeasured. Background execution needs atomic retention of the native file and archive/shared-memory metadata across descriptor close and replacement, which the duplication path does not provide

# libSceAppContent

## change [sceAppContentDownload0Expand, sceAppContentDownload0Shrink, sceAppContentDownload1Shrink and sceAppContentDownload2Shrink]

- [sceAppContentDownload0Expand, sceAppContentDownload0Shrink, sceAppContentDownload1Shrink and sceAppContentDownload2Shrink](../../../core/libs/prx/libSceAppContent/Export.cpp) (libSceAppContent) - how the system service treats the size is unverified on a console. The `libSceAppContent.sprx` in the `fakelib/` of a PPSA01649 01.055.000 dump (SHA-256 `b3e8cbe8f324ca1b`..., firmware unknown, read from disassembly) returns `NOT_INITIALIZED`, then forwards one 8-byte argument to the service; only the `/download2` pair answers `PARAMETER` for 0 first. PPSA01649 passes a size in GiB shifted left by 10 and retries after `0x80d90008`; [dlc_emu](https://github.com/drakmor/dlc_emu/blob/fcf69b16d46a9850578bce19dc2c1b4dc867cbb5/src/dlc_modules/sceappcontent_exports.cpp#L233-L247) declares a mount point instead and forwards it unchecked. `Download0Expand` raises the `/download0` quota to that many MiB, never answers `0x80d90008` and throws for 0; the shrink functions throw after their checks.
