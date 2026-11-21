# libSceAgcDriver

## add Unknown function info

- [PS4 (GCN) multisampled color textures](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureTiling.cpp) (libSceAgcDriver, PS4) - not checked on a console. A 2D multisampled T# in a 2D color tile mode (2, 4 or 8 samples, one level, one slice) follows addrlib's color sample order: a micro tile holds sample 0's 64 elements, then sample 1's, and so on. The tile split is the sample split times the single-sample micro tile, at least 256 bytes and at most the 1 KiB row; a larger one is split into slices a surface apart (egbaddrlib.cpp `ComputeSurfaceAddrFromCoordMacroTiled`, ciaddrlib.cpp `HwlComputeMacroModeIndex`, in [Mesa 25.0.7's addrlib](https://github.com/openbsd/xenocara/tree/483f148565a7/lib/mesa/src/amd/addrlib/src/r800)). The sample split of 2 and the row are shadPS4's (`GetSampleSplit`, `DRAM_ROW_SIZE` in [tiling.cpp](https://github.com/shadps4-emu/shadPS4/blob/8e23388af83f/src/video_core/amdgpu/tiling.cpp)). FMASK is not modeled; PRT, linear and 1D color modes throw.

## add Functional

- [PS4 Pro alternate tiling](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureTiling.cpp) (libSceAgcDriver, PS4) - the alternate tile mode of Neo-mode surfaces (the T# `alt_tile_mode` bit and CB_COLOR_INFO bit 31 in shadPS4's [resource.h](https://github.com/shadps4-emu/shadPS4/blob/ee0364a9c17a/src/video_core/amdgpu/resource.h) and [regs_color.h](https://github.com/shadps4-emu/shadPS4/blob/ee0364a9c17a/src/video_core/amdgpu/regs_color.h)) is not read, so an alternate-tiled PS4 surface gets the base layout (8 pipes and the base bank tables). inFAMOUS Second Son (CUSA00223) sets it on every color target it enables, and its DB_DEPTH_INFO gives 16 pipes and 8 banks.
