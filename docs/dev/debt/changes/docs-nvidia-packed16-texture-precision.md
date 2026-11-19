# libSceAgcDriver

## add Functional

- [Packed 16-bit texture reads on NVIDIA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) - the R5G6B5 and A1R5G5B5 views of `5_6_5` (133), `5_5_5_1` (134) and `1_5_5_5` (135) return each 5- or 6-bit field c widened to 8 bits by bit replication, as replicate8(c) / 255, not c / 31 or c / 63, so 25/31 reads 206/255 (0.80784 against 0.80645). One-bit fields and every field of `4_4_4_4` (136) are exact. Measured with the hardware oracle against `agc_oracle_runner` through `image_load` over 32 texels per format covering every 5-bit value: NVIDIA GeForce RTX 5080 (driver 610.74, Windows) gives the replicated value for 90 of the 96 wide fields per format and c / (2^n - 1) where the two agree; RDNA2 (gfx1036 iGPU, AMD Windows driver 32.0.21045.16010) returns c / (2^n - 1) for all of them, through the hardware and through the same views. The PS5's gfx1013 is not measured.
