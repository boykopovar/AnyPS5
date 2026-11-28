# shader

## add Functional

- [PS4 (GCN) on-chip tessellation](../../../core/shader/recompiler/Optimization/src/TessellationLowering.cpp) (shader recompiler, PS4) - not measured on a console. LS, HS and VS-stage domain routing (`VGT_SHADER_STAGES_EN` 0x45 or 0x145) is drawn through the off-chip ring lowering. With `SPI_SHADER_PGM_RSRC2_HS.OC_LDS_EN` clear the hull shader keeps its control points and patch constants in LDS and the domain shader reads them there, at offsets a tessellation constant buffer bound by the title computes at run time; a hull LDS write or a domain LDS read then throws. An on-chip hull that writes only tessellation factors and a domain shader that reads no hull output are drawn.

# libSceAgcDriver

## add Functional

- [PS4 (GCN) tessellation routing](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not measured on a console. LS, HS, ES and GS routing, `SPI_SHADER_PGM_RSRC2_HS.TG_SIZE_EN`, domain stream-out (`SPI_SHADER_PGM_RSRC2_VS.SO_EN` or `SO_BASE*_EN`) and a local shader that loads InstanceID / `VGT_INSTANCE_STEP_RATE_0` other than 1 throw. `DYNAMIC_HS` and the `VGT_TF_PARAM` fields `NUM_DS_WAVES_PER_SIMD` and `RDREQ_POLICY` are taken as scheduling and cache hints with no effect on the result.
