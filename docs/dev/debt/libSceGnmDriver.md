# libSceGnmDriver technical debt

### Unknown function info

- [Command buffer writers](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the packets, register offsets and reserved sizes the `sceGnm` writers emit follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/gnmdriver/gnmdriver.cpp); they are not checked on a console. Submitted buffers run on the AGC executor: PS4 EOP, EOS and SURFACE_SYNC packets are lowered to RELEASE_MEM and ACQUIRE_MEM, constant engine buffers are merged into the draw buffer, the CP_DMA prefetch of the PS4 SDK is a NOP, and a flip reservation for an interrupt flip throws.
