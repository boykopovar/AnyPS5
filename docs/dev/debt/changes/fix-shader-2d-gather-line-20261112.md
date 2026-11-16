# shader

## add Functional

- [2D level-zero gathers through a 1D T#](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) ignore the second coordinate and use the existing 1D gather lowering. Measured with the [hardware oracle](../HW_ORACLE.md) on RDNA2 (RX 6650 XT, gfx1032, HSA 7.2.4; wave32, IEEE off, DX10 clamp on, f32 denormals flushed, f16/f64 denormals preserved, round to nearest even, fp16 overflow to infinity): DIM 2D and DIM 1D `image_gather4_lz` matched on all 1152 rows for widths 1 and 8, R16_UNORM and RGBA8_UNORM, and independent U/V wrap, edge and border addressing. The PS5 gfx1013 is not measured; comparison, offsets, horizontal or packed gathers, and 2D-array accesses do not gain this compatibility mode.
