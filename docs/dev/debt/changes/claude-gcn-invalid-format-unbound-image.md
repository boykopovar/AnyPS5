# shader

## add Unknown function info

- [GCN T#s with DATA_FORMAT INVALID](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) (shader, PS4) - a directly bound, sampled, read-only GCN T# whose DATA_FORMAT (word 1 bits [25:20]) is 0 and none of whose DST_SELs is the constant 1 is bound as a null image whatever its TYPE, so it reads zeros. The GCN3 ISA says in its buffer section (8.1.4) that the INVALID data format "has special meaning (unbound resource)", and its MIMG table makes the texture addresser return 0 by forcing the format to invalid when DMASK is 0; it does not say what a reserved TYPE with that format does. shadPS4 `143b58aa48` `Rasterizer::BindTextures` binds a null image for any T# whose data format is `FormatInvalid`. Other T#s go through the existing rules unchanged, so such a T# with a DST_SEL of 1 is still refused when its TYPE is reserved, and written, atomic and image-table slots keep their own rules. Nothing here is measured on a PS4.
