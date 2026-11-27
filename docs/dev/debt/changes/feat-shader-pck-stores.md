# shader

## change [MIMG PCK2/PCK4 loads]

- [MIMG PCK2/PCK4 loads](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`image_load[_mip]_pck2/pck4`, opcodes 0x70, 0x71, 0x73 and 0x74) were measured on RDNA2 (gfx1035) with the [hardware oracle](../HW_ORACLE.md) for 2D images with identity swizzle. With data mask 0x1, they write one dword: the raw bits of the N texels (PCK2: 2, PCK4: 4) from `x & ~(N - 1)` on row y, concatenated (element k at bit `k * texel bits`) and zero-extended. The dword is zero when the mip level is past the last level, `x + N - 1` is outside the level's width (unaligned x, unsigned), or y is outside its height. Data only comes back when N texels fit in 32 bits: R8, R16 or RG8 for PCK2, R8 for PCK4. Wider texels (R32, RG16, RGBA8) and masks other than 0x1 write zeros on hardware. Those masks throw at decode, and formats outside UNORM/UINT/SINT R8, R16 and RG8 throw. #1314's earlier measurement saw only zeros because it used masks 0x3 and 0xf.

## add Functional

- [MIMG PCK2/PCK4 stores](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (opcodes 0x76, 0x77, 0x79 and 0x7a) throw for UNORM, SINT and FLOAT formats, masks other than 0x1, texels too wide for N to fit 32 bits and image tables, although RDNA2 hardware writes raw bits or nothing there; tiled surfaces are not measured, and 128-bit formats were not run (0x76 hung a GPU in #1314). Measured on gfx1036 (Radeon 610M) on linear 2D images: under the load's bounds, texel `x + k` gets bits `k * texel bits` of the dword while it stays in the aligned 4-byte group holding x, so x is not aligned down as in the loads.
