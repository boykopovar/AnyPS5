# libSceAgcDriver

## add Unknown function info

- [DMA_DATA GDS address controls](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) (AGC driver) - GDS selector 1 accepts both clear controls and register address space with no increment (SAS/SAIC or DAS/DAIC both 1), transferring consecutive bytes as [Mesa 24.0.0 si_cp_dma.c lines 72-86](https://github.com/chaotic-cx/mesa-mirror/blob/03ecd8b0a517e528d790a3d252194f7b898d0e8f/src/gallium/drivers/radeonsi/si_cp_dma.c#L72-L86) specifies; this is not measured on PS5. Partial GDS controls and non-GDS register-space or no-increment endpoints still throw.
