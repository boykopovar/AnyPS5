# libSceAgcDriver

## add Functional

- [Initial viewport control and color masks](../../../core/libs/prx/libSceAgcDriver/Execution/include/QueueState.hpp) - `PA_CL_VTE_CNTL` (context `0x206`) starts, and returns after `CLEAR_STATE`, at 0x43f, and `CB_TARGET_MASK`/`CB_SHADER_MASK` (`0x8e`/`0x8f`) at 0. These are the values in every public [register-default table](../../../core/libs/prx/libSceAgc/Command/include/RegisterDefaultsData.hpp) (V0, V4, V5 and V7-V11). Linux's GFX10 clear-state table uses 0 and 0xffffffff, and starting from those turned Kao the Kangaroo, GTA V, AEW: Fight Forever and Stray upside down or stopped them. The console's own starting values are not measured. A title that writes 0 to `PA_CL_VTE_CNTL` still gets a viewport covering its render target.
