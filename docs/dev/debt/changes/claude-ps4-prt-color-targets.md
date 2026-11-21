# libSceAgcDriver

## add Unknown function info

- [PS4 (GCN) PRT color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not checked on a console. A color target in GB_TILE_MODE 11, 12, 16 or 17 uses the tiling the texture path gives that mode, with slices one whole surface apart and, as for PRT texture layers, no per-slice bank rotation. The whole surface must be mapped: partly resident PRT surfaces are not modeled. GB_TILE_MODE 15 and 18 to 30 throw.
