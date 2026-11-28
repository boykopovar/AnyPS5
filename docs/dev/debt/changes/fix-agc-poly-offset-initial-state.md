# libSceAgcDriver

## add Functional

- [Initial depth bias scale and offset](../../../core/libs/prx/libSceAgcDriver/Execution/include/QueueState.hpp) - the front and back `PA_SU_POLY_OFFSET_*_SCALE` and `*_OFFSET` registers (context `0x2e0`-`0x2e3`) start, and return after `CLEAR_STATE`, at zero, the value of Linux's [GFX10 clear-state table](https://github.com/torvalds/linux/blob/602042bf29f6efde39cfb5fdd9289bf4854bc0c5/drivers/gpu/drm/amd/amdgpu/clearstate_gfx10.h#L747-L751). The console's reset values are not measured, and the public [register-default tables](../../../core/libs/prx/libSceAgc/Command/include/RegisterDefaultsData.hpp) do not list these registers. Hogwarts Legacy (PPSA01593 v01.000.013) enables depth bias on both faces and writes only the front pair.
