# libSceAgcDriver

## change [Unused pixel programs]

- [Unused pixel programs](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver): a bound pixel program is skipped when the color masks enable no channel of a target that has a format and DB export and execution enables are clear, regardless of retained SPI_SHADER_Z_FORMAT. Not measured on the console, also for a mask on a FORMAT 0 target and for DB_ALPHA_TO_MASK, which always retains the program. Sources: [PAL DbShaderControl](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/hw/gfxip/gfx9/gfx9AbiToPipelineRegisters.h), [GFX10 registers](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/hw/gfxip/gfx9/chip/gfx9_plus_merged_registers.h), [radeonsi on FORMAT 0](https://github.com/mihawk-99/PS5_Mesa/blob/0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8/src/gallium/drivers/radeonsi/si_state.c#L43-L45).
