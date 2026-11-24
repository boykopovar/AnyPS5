# libSceAgcDriver

## change [NGG vertex workgroups]

- [NGG vertex workgroups](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp) (libSceAgcDriver) - type-2 PRIMGEN programs using DS instructions or `s_barrier` are prepared with the draw's routing and primitive registers on the existing mesh path. The workgroup requirement follows [Mesa's NGG lowering](https://github.com/mihawk-99/PS5_Mesa/blob/b3588f78dd7fd46fbadc7ecd487e5f14d4213e4b/src/amd/common/nir/ac_nir_lower_ngg.c#L1135); the mesh path supplies LDS and two guest lanes per host invocation for wave64 on a 32-lane host subgroup. A line list exports two-index lines, as RADV sets `VGT_GS_OUT_PRIM_TYPE` (`radv_conv_prim_to_gs_out`); points and other inputs' lines throw. Console routing has not been measured. Passthrough and tessellation keep their existing paths; unsupported mesh assembly and subgroup budgets throw.
