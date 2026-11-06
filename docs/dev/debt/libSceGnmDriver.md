# libSceGnmDriver technical debt

### Silent stubs

- [sceGnmAreSubmitsAllowed](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - returns 1: submissions are never suspended here, because nothing on the host takes the GPU away from the title. shadPS4 returns 0 only while its submission lock is held.
- [sceGnmIsUserPaEnabled, sceGnmDriverCaptureInProgress, sceRazorIsLoaded](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - return false: the user performance analysis mode, GPU captures and the Razor GPU profiler belong to development tools and are not present, as shadPS4 also answers for retail firmware. A title only skips its tool hooks on these answers.

### Unknown function info

- [Command buffer writers](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the packets, register offsets and reserved sizes the `sceGnm` writers emit follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/gnmdriver/gnmdriver.cpp); they are not checked on a console. Submitted buffers run on the AGC executor: PS4 EOP, EOS and SURFACE_SYNC packets are lowered to RELEASE_MEM and ACQUIRE_MEM, constant engine buffers are merged into the draw buffer, type-2 filler packets are dropped, the CP_DMA prefetch of the PS4 SDK is a NOP, and a flip reservation for an interrupt flip throws.
- [sceGnmLogicalCuMaskToPhysicalCuMask, sceGnmLogicalCuIndexToPhysicalCuIndex](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - throw: they translate a logical compute unit mask or index of the title into the physical compute units of the console GPU. That mapping depends on which units are disabled on each chip and is not documented; shadPS4 returns the logical mask unchanged, which is not evidence of the console result.
