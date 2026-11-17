# shader

## add Functional

- [PS4 (GCN) on-chip tessellation](../../../core/shader/recompiler/Optimization/src/TessellationLowering.cpp) (shader recompiler, PS4) - not measured on a console. The tessellation constant buffer (shadPS4 `TessellationDataConstantBuffer`) is read from guest memory for every on-chip draw and compiled into its hull and domain stages, so those draws are kept in no draw cache or recipe. With `OC_LDS_EN` clear, hull LDS writes and domain LDS reads at or above `hs_output_base` go through the off-chip ring lowering at that offset less `hs_output_base`. A hull with `hs_output_base` 0 is taken as pass-through and copies `ls_stride` bytes of its input control point to its output; one with another output stride or control point count throws, as does an on-chip LDS address that reads another constant buffer or none. LS, HS and VS-stage domain routing (0x45, 0x145) uses the off-chip ring lowering.

# libSceAgcDriver

## add Functional

- [PS4 (GCN) tessellation routing](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not measured on a console. LS, HS, ES and GS routing, `SPI_SHADER_PGM_RSRC2_HS.TG_SIZE_EN`, domain stream-out (`SPI_SHADER_PGM_RSRC2_VS.SO_EN` or `SO_BASE*_EN`) and a local shader that loads InstanceID / `VGT_INSTANCE_STEP_RATE_0` other than 1 throw. `DYNAMIC_HS` and the `VGT_TF_PARAM` fields `NUM_DS_WAVES_PER_SIMD` and `RDREQ_POLICY` are taken as scheduling and cache hints with no effect on the result.
