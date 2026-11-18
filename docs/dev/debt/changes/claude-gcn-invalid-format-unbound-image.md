# shader

## add Unknown function info

- [GCN T#s with DATA_FORMAT INVALID](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) (shader, PS4) - a directly bound, sampled, read-only GCN T# whose DATA_FORMAT (word 1 bits [25:20]) is 0 is bound as a null image whatever its TYPE, following the GCN3 ISA (section 8.1.4: INVALID "has special meaning (unbound resource)") and shadPS4 `143b58aa48` `Rasterizer::BindTextures`, which binds a null image for `FormatInvalid`. A T# whose DST_SEL picks the constant 1 keeps its rejection, because what an unbound resource returns through that select is not documented; written, atomic and image-table slots also keep their rejection. None of it is measured on a PS4.
