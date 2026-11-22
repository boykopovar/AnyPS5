# shader

## add Functional

- [Two-bit SNORM buffer stores](../../../core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvFormatConvert.cpp) (the 2-bit component of `2_10_10_10_SNORM` and `10_10_10_2_SNORM` in formatted buffer stores, measured through `tbuffer_store_format`) - the PS5's gfx1013 is not measured. A magnitude of at least 0.5 stores 1 with the value's sign and anything smaller, and NaN, stores 0, so 0.5 and -0.5 round away from zero, unlike the nearest-even rounding of the wider SNORM components; measured on RDNA2 (gfx1036) and pinned in `agc_driver_buffer_format_store_two_bit_snorm`. Image stores to these formats are not covered.
