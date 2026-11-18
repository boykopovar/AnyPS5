# libSceAgcDriver

## add Functional

- [Initial viewport control and color masks](../../../core/libs/prx/libSceAgcDriver/Execution/include/QueueState.hpp) (libSceAgcDriver) - the console's own starting values of `PA_CL_VTE_CNTL` (context `0x206`) and `CB_TARGET_MASK`/`CB_SHADER_MASK` (`0x8e`/`0x8f`) are not measured. A queue starts, and returns after `CLEAR_STATE`, at 0x43f and 0, the values of every public [register-default table](../../../core/libs/prx/libSceAgc/Command/include/RegisterDefaultsData.hpp); Linux's GFX10 clear-state values 0 and 0xffffffff left titles that never write these registers drawing without the viewport transform, upside down. A title that writes 0 to `PA_CL_VTE_CNTL` still gets a viewport covering its render target.
